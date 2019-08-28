/*
 Screen layer for ugui

 Copyright 2019, S. Kevin Hester, kevinh@geeksville.com

 (MIT License)
 Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
 associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
 subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or substantial
 portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT
 LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "screen.h"
#include "lcd.h"
#include "ugui.h"
#include "fonts.h"

extern UG_GUI gui;

// If true, the scroll position changed and force a complete redraw
// FIXME - heading shouldn't be redrawn
// FIXME - currently limited to one scrollable per screen
static bool forceScrollableRelayout;

// If the user is editing an editable, this will be it
static Field *curActiveEditable = NULL;

#define MAX_SCROLLABLE_DEPTH 3 // How deep can we nest scrollables in our stack

static Field *scrollableStack[MAX_SCROLLABLE_DEPTH];
int scrollableStackPtr = 0; // Points to where to push the next entry (so if zero, stack is empty)

// Returns true if we decided to draw something
typedef bool (*FieldRenderFn)(FieldLayout *layout);

static const FieldRenderFn renderers[];

/// If true blink changed to be true or false this tick and we should redraw anything that is animated
static bool blinkChanged;
static bool blinkOn;

static uint32_t screenUpdateCounter;

#ifdef SW102
#define HEADING_FONT FONT_5X12
#else
#define HEADING_FONT TITLE_TEXT_FONT

#endif

#define SCROLLABLE_FONT CONFIGURATIONS_TEXT_FONT

const UG_FONT *editable_label_font = &SMALL_TEXT_FONT;
const UG_FONT *editable_value_font = &SMALL_TEXT_FONT;
const UG_FONT *editable_units_font = &SMALL_TEXT_FONT;

// We can optionally render by filling all with black and then drawing text with a transparent background
// This is useful on very small screens (SW102) where we might want the text to overlap.  However, this
// approach causes flickering on non memory resident framebuffers (850C)
#ifdef SW102
#define EDITABLE_BLANKALL true
#else
#define EDITABLE_BLANKALL false
#endif

// The default is for editables to be two rows tall, with the data value on the second row
// define this as 1 if you want them to be one row tall (because you have a wide enough screen)
#ifndef EDITABLE_NUM_ROWS
#define EDITABLE_NUM_ROWS 2
#endif

// The default is C_WHITE, redefine if you want something else
#ifndef EDITABLE_CURSOR_COLOR
#define EDITABLE_CURSOR_COLOR       C_WHITE
#endif

static UG_COLOR getBackColor(const FieldLayout *layout) {
	switch (layout->color) {
	case ColorInvert:
		return C_WHITE;

	case ColorHeading:
		return HEADING_BACKGROUND;

	case ColorNormal:
	default:
		return C_BLACK;
	}
}

static UG_COLOR getForeColor(const FieldLayout *layout) {
	switch (layout->color) {
	case ColorInvert:
		return C_BLACK;

	case ColorNormal:
	case ColorHeading:
	default:
		return C_WHITE;
	}
}

static void autoTextHeight(FieldLayout *layout) {
	// Allow developer to use this shorthand for one row high text fields
	if (layout->height == -1) {
		assert(layout->font); // you must specify a font to use this feature
		layout->height = layout->font->char_height;
	}
}

static bool renderDrawTextCommon(FieldLayout *layout, const char *msg) {
	autoTextHeight(layout);

	const UG_FONT *font = layout->font;
	assert(font); // dynamic font selection not yet supported

	// how many pixels does our rendered string
	UG_S16 strwidth = (font->char_width + gui.char_h_space) * strlen(msg);

	UG_S16 width = layout->width;
	UG_S16 height = layout->height;
	UG_S16 x = layout->x;

	if (strwidth < width) // If the user gave us more space than we need, center justify within that box
		x += (width - strwidth) / 2;

	UG_FontSelect(font);
	UG_COLOR back = getBackColor(layout);
	UG_SetForecolor(getForeColor(layout));

	// ug fonts include no blank space at the beginning, so we always include one col of padding
	UG_FillFrame(layout->x, layout->y, layout->x + width - 1,
			layout->y + height - 1, back);
	UG_SetBackcolor(C_TRANSPARENT);
	UG_PutString(x + 1, layout->y, (char*) msg);
	return true;
}

static bool renderDrawText(FieldLayout *layout) {
	return renderDrawTextCommon(layout, layout->field->drawText.msg);
}

static bool renderDrawTextPtr(FieldLayout *layout) {
	return renderDrawTextCommon(layout, layout->field->drawTextPtr.msg);
}

static bool renderFill(FieldLayout *layout) {
	assert(layout->width >= 1);
	assert(layout->height >= 1);

	UG_FillFrame(layout->x, layout->y, layout->x + layout->width - 1,
			layout->y + layout->height - 1, getForeColor(layout));
	return true;
}

static bool renderMesh(FieldLayout *layout) {
	assert(layout->width >= 1);
	assert(layout->height >= 1);

	UG_DrawMesh(layout->x, layout->y, layout->x + layout->width - 1,
			layout->y + layout->height - 1, getForeColor(layout));
	return true;
}

/**
 * If we are selected, highlight this item with a bar to the left (on color screens possibly draw a small
 * color pointer or at least color the line something nice.
 */
static void drawSelectionMarker(FieldLayout *layout) {
	// Only consider doing this on items that might be animated
	// we size the cursor to be slightly shorter than the box it is in

	//  && !curActiveEditable - old code when editing don't blink the selection cursor
	if (layout->field && layout->field->is_selected) {
		UG_FontSelect(&FONT_CURSORS);
		UG_PutChar('0', layout->x + layout->width - FONT_CURSORS.char_width, // draw on ride side of line
		layout->y + (layout->height - FONT_CURSORS.char_height) / 2, // draw centered vertially within the box
		blinkOn ? EDITABLE_CURSOR_COLOR : getBackColor(layout),
		C_TRANSPARENT);
	}

}

/**
 * If we have a border on this layout, drawit
 */
static void drawBorder(FieldLayout *layout) {
	UG_COLOR color = getForeColor(layout);
	int fatness = (layout->border & BorderFat) ? Yby64(1) : 1;

	if (layout->border & BorderTop)
		UG_DrawLine(layout->x, layout->y, layout->x + layout->width - 1,
				layout->y, color); // top

	if (layout->border & BorderBottom)
		UG_FillFrame(layout->x, layout->y + layout->height - fatness,
				layout->x + layout->width - 1, layout->y + layout->height - 1,
				color); // bottom

	if (layout->border & BorderLeft)
		UG_DrawLine(layout->x, layout->y, layout->x,
				layout->y + layout->height - 1, color); // left

	if (layout->border & BorderRight)
		UG_DrawLine(layout->x + layout->width - 1, layout->y,
				layout->x + layout->width - 1, layout->y + layout->height - 1,
				color); // right
}

const Coord screenWidth = SCREEN_WIDTH, screenHeight = SCREEN_HEIGHT; // FIXME, for larger devices allow screen objcts to nest inside other screens

// True while the user is holding down the m key and but not trying to edit anything
// We use a static so we can detect when state changes
// We do all this calculation only once in the main render loop, so that all fields change at once
static bool oldForceLabels;
static bool forceLabels;

/// Should we redraw this field this tick? We always render dirty items, or items that might need to show blink animations
static bool needsRender(FieldLayout *layout) {
	if (layout->field->dirty)
		return true;

	if (layout->field->blink && blinkChanged)
		return true; // this field is doing a blink animation and it is time for that to update

	if (layout->field->variant == FieldEditable)
		return true; // Editables are smart enough to do their own rendering shortcuts based on cached values

	return false;
}

const bool renderLayouts(FieldLayout *layouts, bool forceRender) {
	bool didDraw = false; // we only render to hardware if something changed

	Coord maxy = 0;

	bool didChangeForceLabels = false; // if we did label force/unforce we need to remember for the next render
	bool mpressed = SCREENFN_FORCE_LABELS;

	// For each field if that field is dirty (or the screen is) redraw it
	for (FieldLayout *layout = layouts; layout->field; layout++) {
		if (forceRender) // tell the field it must redraw itself
			layout->field->dirty = true;

		if (layout->field->variant == FieldEditable) {
			forceLabels = mpressed && layout->modifier == ModNoLabel;
			didChangeForceLabels = true;
		}

		// We always render dirty items, or items that might need to show blink animations
		if (needsRender(layout)) {
			if (layout->width == 0)
				layout->width = screenWidth - layout->x;

			if (layout->height == 0)
				layout->height = screenHeight - layout->y;

			// if user specified width in terms of characters, change it to pixels
			if (layout->width < 0) {
				assert(layout->font); // you must specify a font to use this feature
				layout->width = -layout->width
						* (layout->font->char_width + gui.char_h_space);
			}

			// a y <0 means, start just below the previous lowest point on the screen, -1 is immediately below, -2 has one blank line, -3 etc...
			if (layout->y < 0)
				layout->y = maxy + -layout->y - 1;

			didDraw |= renderers[layout->field->variant](layout);

			assert(layout->height != -1); // by the time we reach here this must be set

			// After the renderer has run, cache the highest Y we have seen (for entries that have y = -1 for auto assignment)
			if (layout->y + layout->height > maxy)
				maxy = layout->y + layout->height;

			drawSelectionMarker(layout);
			drawBorder(layout);
		}
	}

	// We clear the dirty bits in a separate pass because multiple layouts on the screen might share the same field
	for (const FieldLayout *layout = layouts; layout->field; layout++) {
		layout->field->dirty = false;
	}

	if (didChangeForceLabels)
		oldForceLabels = forceLabels;

	return didDraw;
}

// Return the scrollable we are currently showing the user, or NULL if none
// The (currently only one allowed per screen) scrollable that is currently being shown to the user.
// if the scrollable changes, we'll need to regenerate the entire render
static Field* getActiveScrollable() {
	return scrollableStackPtr ? scrollableStack[scrollableStackPtr - 1] : NULL;
}

/**
 * The user just clicked on a scrollable entry, descend down into it
 */
static void enterScrollable(Field *f) {
	assert(scrollableStackPtr < MAX_SCROLLABLE_DEPTH);
	scrollableStack[scrollableStackPtr++] = f;

	// We always set blink for scrollables, because they contain child items that might need to blink
	f->blink = true;

	// NOTE: Only the root scrollable is ever checked for 'dirty' by the main screen renderer,
	// so that's the one we set
	scrollableStack[0]->dirty = true;

	forceScrollableRelayout = true;
}

/**
 * The user just clicked to exit a scrollable entry, ascend to the entry above us or if we are the top
 * go back to the main screen
 *
 * @return true if we just selected a new scrollable
 */
static bool exitScrollable() {
	assert(scrollableStackPtr > 0);
	scrollableStackPtr--;

	Field *f = getActiveScrollable();
	if (f) {
		// Parent was a scrollable, show it
		f->dirty = true;
		forceScrollableRelayout = true;
		return true;
	} else {
		// otherwise we just leave the screen showing the top scrollable
		return false;
	}
}

#define SCROLLABLE_VPAD 4 // extra space between each row (for visual appearance)
#define SCROLLABLE_ROW_HEIGHT (SCROLLABLE_VPAD + 16) // for planning purposes - might be larger at runtime
#define MAX_SCROLLABLE_ROWS (SCREEN_HEIGHT / SCROLLABLE_ROW_HEIGHT) // Max number of rows we can show on one screen (including header)

static int maxRowsPerScreen;

static bool renderActiveScrollable(FieldLayout *layout, Field *field) {
	const Coord rowHeight = EDITABLE_NUM_ROWS
			* (SCROLLABLE_FONT.char_height + gui.char_v_space) + SCROLLABLE_VPAD;
	maxRowsPerScreen = SCREEN_HEIGHT / rowHeight; // might be less than MAX_SCROLLABLE_ROWS

	Field *scrollable = getActiveScrollable();
	bool weAreExpanded = scrollable == field;

	assert(rowHeight >= SCROLLABLE_ROW_HEIGHT); // Make sure we we don't violate our array sizes

	// If we are expanded show our heading and the current visible child elements
	// Otherwise just show our label so that the user might select us to expand
	if (weAreExpanded) {
		static FieldLayout rows[MAX_SCROLLABLE_ROWS + 1]; // Used to layout each of the currently visible rows + heading + end of rows marker

		if (forceScrollableRelayout) {
			static Field blankRows[MAX_SCROLLABLE_ROWS]; // Used to fill with blank space if necessary
			static Field heading = FIELD_DRAWTEXT();

			bool hasMoreRows = true; // Once we reach an invalid row we stop rendering and instead fill with blank space

			forceScrollableRelayout = false;
			for (int i = 0; i < maxRowsPerScreen; i++) {
				FieldLayout *r = rows + i;

				r->x = layout->x;
				r->y = layout->y + rowHeight * i;
				r->width = layout->width;
				r->border = BorderNone;

				if (i == 0) { // heading
					fieldPrintf(&heading, "%s", field->scrollable.label);
					r->field = &heading;
					r->color = ColorHeading;
					r->border = HEADING_BORDER;
					r->font = &HEADING_FONT;

					r->y = layout->y;
					r->height = r->font->char_height + gui.char_v_space
							+ SCROLLABLE_VPAD;
				} else {
					r->y = rows[i - 1].y + rows[i - 1].height;
					r->height = rowHeight; // all data rows are the same height

					// visible menu rows, starting with where the user has scrolled to
					const int entryNum = field->scrollable.first + i - 1;
					Field *entry = &field->scrollable.entries[entryNum];

					entry->dirty = true; // Force it to be redrawn
					if (entry->variant == FieldEnd)
						hasMoreRows = false;

					// if the current row is valid, render that, otherwise render blank space
					if (hasMoreRows) {
						r->field = entry;
						entry->is_selected = (entryNum
								== field->scrollable.selected);
						entry->blink = entry->is_selected; // We want to service our blink animation
					} else {
						r->field = &blankRows[i];
						r->field->variant = FieldFill;
						r->color = ColorInvert; // black box for empty slots at end
					}

					r->field->dirty = true; // Force rerender
				}

				rows[maxRowsPerScreen].field = NULL; // mark end of array (for rendering)
			}
		}

		// draw (or redraw if necessary) our current set of visible rows
		return renderLayouts(rows, false);
	} else {
		static FieldLayout rows[1 + 1]; // Used to layout each our single row + end of rows marker

		// Just draw our label (not highlighted) - show selection bar if necessary
		FieldLayout *r = &rows[0];

		r->x = layout->x;
		r->y = layout->y;
		r->width = layout->width;
		r->height = layout->height;
		r->border = BorderNone;

		static Field label = FIELD_DRAWTEXT();
		fieldPrintf(&label, "%s", field->scrollable.label);
		r->field = &label;
		r->color = ColorNormal;
		r->font = &SCROLLABLE_FONT;

		// If we are inside a scrollable and selected, blink
		if (scrollable)
			label.is_selected =
					field
							== &scrollable->scrollable.entries[scrollable->scrollable.selected];
		else
			label.is_selected = false;
		label.blink = label.is_selected; // We want to service our blink animation

		rows[1].field = NULL; // mark end of array (for rendering)

		// draw (or redraw if necessary) our current set of visible rows
		return renderLayouts(rows, false);
	}
}

static bool renderScrollable(FieldLayout *layout) {
	if (!getActiveScrollable()) // we are the first scrollable on this screen, use us to init the stack
		enterScrollable(layout->field);

	// If we are being asked to render the root scrollable, instead we want to substitute the deepest scrollable
	// in the stack
	Field *field = layout->field;
	if (scrollableStack[0] == field)
		field = getActiveScrollable();

	return renderActiveScrollable(layout, field);
}

// Get the numeric value of an editable number, properly handling different possible byte encodings
static int32_t getEditableNumber(Field *field) {
	assert(field->variant == FieldEditable);

	switch (field->editable.size) {
	case 1:
		return *(uint8_t*) field->editable.target;
	case 2:
		return *(int16_t*) field->editable.target;
	case 4:
		return *(int32_t*) field->editable.target;
	default:
		assert(0);
		return 0;
	}
}

// Set the numeric value of an editable number, properly handling different possible byte encodings
static void setEditableNumber(Field *field, uint32_t v) {
	switch (field->editable.size) {
	case 1:
		*(uint8_t*) field->editable.target = (uint8_t) v;
		break;
	case 2:
		*(uint16_t*) field->editable.target = (uint16_t) v;
		break;
	case 4:
		*(uint32_t*) field->editable.target = (uint32_t) v;
		break;
	default:
		assert(0);
	}
}

static int countEnumOptions(Field *s) {
	const char **e = s->editable.editEnum.options;

	int n = 0;
	while (*e++)
		n++;

	return n;
}

/**
 * increment/decrement an editable
 */
static void changeEditable(bool increment) {
	Field *f = curActiveEditable;
	assert(f);

	int v = getEditableNumber(f);

	switch (f->editable.typ) {
	case EditUInt: {
		int step = f->editable.number.inc_step;

		if (step == 0)
			step = 1;

		v += step * (increment ? 1 : -1);
		if (v < f->editable.number.min_value) // loop around
			v = f->editable.number.max_value;
		else if (v > f->editable.number.max_value)
			v = f->editable.number.min_value;
		setEditableNumber(f, v);
		break;
	}
	case EditEnum: {
		int numOpts = countEnumOptions(f);
		v += increment ? 1 : -1;
		if (v < 0) // loop around
			v = numOpts - 1;
		else if (v >= numOpts)
			v = 0;
		setEditableNumber(f, v);
		break;
	}
	default:
		assert(0);
		break;
	}
}

/// Given an editible extract its value as a string (max len MAX_FIELD_LEN)
static void getEditableString(Field *field, int32_t num, char *outbuf) {
	switch (field->editable.typ) {
	case EditUInt: {
		// properly handle div_digits
		int divd = field->editable.number.div_digits;
		if (divd == 0)
			snprintf(outbuf, MAX_FIELD_LEN, "%lu", num);
		else {
			int div = 1;
			while (divd--)
				div *= 10; // pwrs of 10

			if (field->editable.number.hide_fraction)
				snprintf(outbuf, MAX_FIELD_LEN, "%ld", num / div);
			else
				snprintf(outbuf, MAX_FIELD_LEN, "%ld.%0*lu", num / div,
						field->editable.number.div_digits, num % div);
		}
		break;
	}
	case EditEnum:
		strncpy(outbuf, field->editable.editEnum.options[num], MAX_FIELD_LEN);
		break;
	default:
		assert(0);
		break;
	}
}

// Center justify a string on a line of specified width
static void putStringCentered(int x, int y, int width, const UG_FONT *font,
		const char *str) {
	UG_S16 strwidth = (font->char_width + gui.char_h_space) * strlen(str);

	if (strwidth < width)
		x += (width - strwidth) / 2; // if we have extra space put half of it before the string

	UG_FontSelect(font);
	UG_PutString(x, y, (char*) str);
}

// right justify a string (printing it to the left of X and Y)
static void putStringRight(int x, int y, const UG_FONT *font, const char *str) {
	UG_S16 strwidth = (font->char_width + gui.char_h_space) * strlen(str);

	x -= strwidth;

	UG_FontSelect(font);
	UG_PutString(x, y, (char*) str);
}

/**
 * This render operator is smart enough to do its own dirty managment.  If you set dirty, it will definitely redraw.  Otherwise it will check the actual data bytes
 * of what we are trying to render and if the same as last time, it will decide to not draw.
 */
static bool renderEditable(FieldLayout *layout) {
	Field *field = layout->field;
	UG_S16 width = layout->width;
	bool isActive = curActiveEditable == field; // are we being edited right now?
	bool dirty = field->dirty;
	bool showLabel = layout->modifier != ModNoLabel;
	bool showLabelAtTop = layout->modifier == ModLabelTop;
	const UG_FONT *font = layout->font ? layout->font : editable_value_font;

	bool isTwoRows = showLabel && (EDITABLE_NUM_ROWS == 2);

	if (layout->height == -1) // We should autoset
		layout->height = (
				(isTwoRows || showLabelAtTop) ?
						editable_label_font->char_height : 0)
				+ font->char_height;

	UG_S16 height = layout->height;

	UG_COLOR back = getBackColor(layout), fore = getForeColor(layout);
	UG_SetForecolor(fore);

	// If we are blinking right now, that's a good place to poll our buttons so that the user can press and hold to change a series of values
	if (isActive && blinkChanged && !field->editable.read_only) {
		if (buttons_get_up_state()) {
			changeEditable(true);
		}

		if (buttons_get_down_state()) {
			changeEditable(false);
		}
	}

	// Get the value we are trying to show (it might be a num or an enum)
	int32_t num = getEditableNumber(field);
	bool valueChanged = num != layout->old_editable;
	char valuestr[MAX_FIELD_LEN];

	bool needBlink = blinkChanged && (isActive || field->is_selected);

	// If the value numerically changed, see if it also changed as a string (much more expensive)
	bool showValue = !forceLabels && (valueChanged || dirty || needBlink); // default to not drawing the value
	if (showValue) {
		char oldvaluestr[MAX_FIELD_LEN];
		getEditableString(field, layout->old_editable, oldvaluestr);

		layout->old_editable = num;

		getEditableString(field, num, valuestr);
		if (strlen(valuestr) != strlen(oldvaluestr))
			dirty = true; // Force a complete redraw (because alignment of str in field might have changed and we don't want to leave turds on the screen
	}

	// If not dirty, labels didn't change and we aren't animating then exit
	bool forceLabelsChanged = forceLabels != oldForceLabels;

	if (!dirty && !valueChanged && !forceLabelsChanged && !needBlink)
		return false; // We didn't actually change so don't try to draw anything

	// fill our entire box with blankspace (if we must)
	bool blankAll = EDITABLE_BLANKALL || forceLabelsChanged || dirty;
	if (blankAll)
		UG_FillFrame(layout->x, layout->y, layout->x + width - 1,
				layout->y + height - 1, back);

	// Show the label (if showing the conventional way - i.e. small and off to the top left.
	if (showLabel) {
		UG_SetBackcolor(C_TRANSPARENT); // always draw labels with transparency, because they might slightly overlap the border

		if (showLabelAtTop)
			putStringCentered(layout->x, layout->y, width, editable_label_font,
					field->editable.label);
		else {
			UG_FontSelect(editable_label_font);
			UG_PutString(layout->x + 1, layout->y,
					(char*) field->editable.label);
		}
	}

	UG_SetBackcolor(blankAll ? C_TRANSPARENT : C_BLACK); // we just cleared the background ourself, from now on allow fonts to overlap

	// Show the label in the middle of the box
	if (forceLabels)
		putStringCentered(layout->x,
				layout->y + (height - editable_label_font->char_height) / 2,
				width, editable_label_font, field->editable.label);

	// draw editable value
	if (showValue) {
		UG_FontSelect(font);

		// how many pixels does our rendered string
		UG_S16 strwidth = (font->char_width + gui.char_h_space)
				* strlen(valuestr);

		UG_S16 x = layout->x;
		UG_S16 y = layout->y;

		if (showLabel) {
			if (!showLabelAtTop) {
				x += width - strwidth - FONT_CURSORS.char_width; // move the value all the way to the right (but leave room for the cursor)

				if (isTwoRows) // put the value on the second line (if the screen is narrow)
					y += editable_label_font->char_height;
			} else {
				// Center justify
				if (strwidth < width) // If the user gave us more space than we need, center justify within that box
					x += (width - strwidth) / 2;

				y += editable_label_font->char_height; // put value just underneath
			}
		} else {
			if (strwidth < width) // If the user gave us more space than we need, center justify within that box
				x += (width - strwidth) / 2;
		}

		UG_PutString(x, y, (char*) valuestr);

		// Blinking underline cursor when editing
		if (isActive) {
			UG_S16 cursorY = y + font->char_height + 1;
			UG_DrawLine(x - 1, cursorY, layout->x + width, cursorY,
					blinkOn ? EDITABLE_CURSOR_COLOR : back);
		}
	}

	// Put units in bottom right (unless we are showing the label)
	bool showUnits = field->editable.typ == EditUInt && !showLabel
			&& !forceLabels;
	if (showUnits) {
		int ulen = strlen(field->editable.number.units);
		if (ulen) {
			const UG_FONT *font = editable_units_font;
			UG_S16 uwidth = (font->char_width + gui.char_h_space) * ulen;

			UG_FontSelect(editable_units_font);
			UG_PutString(layout->x + width - uwidth,
					layout->y + layout->height - font->char_height - 1,
					(char*) field->editable.number.units);
		}
	}

	return true;
}

static bool renderCustom(FieldLayout *layout) {
	assert(layout->field->custom.render);
	return (*layout->field->custom.render)(layout);
}

// FIXME - support multiple active graphs by assigning cache dynamically
static GraphCache caches[1];

// Add to our ring buffer and maintain invariants
static void graphAddPoint(Field *field, int32_t val) {
	GraphCache *cache = field->graph.cache;

	// add the point
	cache->points[cache->end_valid] = val;
	cache->end_valid = (cache->end_valid + 1) % GRAPH_MAX_POINTS; // inc ptr with wrap

	// discard old point if needed
	bool overfull = cache->start_valid == cache->end_valid;
	if (overfull)
		cache->start_valid = (cache->start_valid + 1) % GRAPH_MAX_POINTS;

	// update invariants
	if (val > cache->max_val)
		cache->max_val = val;
	if (val < cache->min_val && val >= field->graph.min_threshold)
		cache->min_val = val;
}

static int graphX, // upper left of graph
		graphY, // upper left of graph,
		graphWidth, // total draw area width
		graphHeight, // total draw area height
		graphXmin, // x loc of 0,0 position
		graphXmax, // x loc of rightmost data point
		graphYmin, // y loc of 0,0 position (for min value)
		graphYmax, // y loc of max value
		graphLabelY; // y loc of the label for field name

// Draw our axis lines and min/max numbers
static void graphClearAndLabelAxis(Field *field) {
	UG_SetForecolor(GRAPH_COLOR_ACCENT);
	UG_SetBackcolor(GRAPH_COLOR_BACKGROUND);

	// Only need to draw labels and axis if dirty
	Field *source = field->graph.source;
	if (field->dirty) {
		// clear all
		UG_FillFrame(graphX, graphY, graphX + graphWidth - 1,
				graphY + graphHeight - 1, GRAPH_COLOR_BACKGROUND);

		putStringCentered(graphX, graphLabelY, graphWidth, &GRAPH_LABEL_FONT,
				source->editable.label);

		// vertical axis line
		UG_DrawLine(graphXmin, graphYmin, graphXmin, graphYmax,
		GRAPH_COLOR_AXIS);

		// horiz axis line
		UG_DrawLine(graphXmin, graphYmin, graphXmax, graphYmin,
		GRAPH_COLOR_AXIS);
	}

	// draw max value
	GraphCache *cache = field->graph.cache;
	char valstr[MAX_FIELD_LEN];
	if (cache->max_val != INT32_MIN) {
		getEditableString(source, cache->max_val, valstr);
		putStringRight(graphXmin, graphYmax, &GRAPH_MAXVAL_FONT, valstr);
	}

	// draw min value
	if (cache->min_val != INT32_MAX) {
		getEditableString(source, cache->min_val, valstr);
		putStringRight(graphXmin, graphYmin - GRAPH_MAXVAL_FONT.char_height,
				&GRAPH_MAXVAL_FONT, valstr);
	}
}

// Linear  interpolated between the min/max values to generate a y coordinate for plotting a particular value x
static inline int32_t graphScaleY(GraphCache *cache, int32_t x) {
	if (cache->max_val == cache->min_val) // Until there is a span everything is at wmin
		return graphYmin;

	// We go one row up from graphymin so we don't cover over the axis
	return ((graphYmin - 1) * (cache->max_val - x)
			+ graphYmax * (x - cache->min_val))
			/ (cache->max_val - cache->min_val);
}

static void graphDrawPoints(Field *field) {
	GraphCache *cache = field->graph.cache;

	int ptr = cache->start_valid;
	if (ptr == cache->end_valid)
		return; // ring buffer is empty

	int x = graphXmin; // the vertical axis line

	int warn_threshold = field->graph.warn_threshold;
	if (warn_threshold != -1) {
		warn_threshold = graphScaleY(cache, field->graph.warn_threshold);

		// Make sure our threshold never goes below the areas we are going to draw
		if (warn_threshold > graphYmin - 1)
			warn_threshold = graphYmin - 1;
	}

	int error_threshold = field->graph.error_threshold;
	if (error_threshold != -1) {
		error_threshold = graphScaleY(cache, field->graph.error_threshold);

		// Make sure our threshold never goes below the areas we are going to draw
		if (error_threshold > graphYmin - 1)
			error_threshold = graphYmin - 1;
	}

	do {
		x++; // drawing a new vertical line now
		int val = cache->points[ptr];
		int y = graphScaleY(cache, val);

		// Draw black space above the line (so we scroll/scale properly)
		UG_DrawLine(x, graphYmax, x, y - 1, GRAPH_COLOR_BACKGROUND);

#if 0 // FIXME accent line is pretty ugly
		 // Draw the accent line
		 int accent_bottom = y + 2; // we draw the accent line three pixels high
		 if(accent_bottom >= graphYmin) // don't draw past end of graph
			 accent_bottom = graphYmin - 1;

		 UG_DrawLine(x, y, x, accent_bottom, GRAPH_COLOR_ACCENT);
		 y = accent_bottom + 1; // New segment is just below the accent
#endif

		if (error_threshold != -1 && y <= error_threshold) {
			UG_DrawLine(x, y, x, error_threshold, GRAPH_COLOR_ERROR);
			y = error_threshold + 1;
		}

		if (warn_threshold != -1 && y <= warn_threshold) {
			UG_DrawLine(x, y, x, warn_threshold, GRAPH_COLOR_WARN);
			y = warn_threshold + 1;
		}

		UG_DrawLine(x, y, x, graphYmin, GRAPH_COLOR_NORMAL);

		ptr = (ptr + 1) % GRAPH_MAX_POINTS; // increment and wrap
	} while (ptr != cache->end_valid); // we just did the last entry?
}

/**
 * Our graphs are invoked for rendering once each blink interval, but most of the time we opt to do nothing.
 */
static bool renderGraph(FieldLayout *layout) {
	bool needUpdate = (screenUpdateCounter
			% (GRAPH_INTERVAL_MS / UPDATE_INTERVAL_MS) == 0);

	Field *field = layout->field;
	assert(field);

	// If we are not dirty and we don't need an update, just return
	if (!needUpdate && !field->dirty)
		return false;

	if (!field->graph.cache) {
		GraphCache *cache = field->graph.cache = &caches[0];

		// Init cache to empty
		cache->max_val = INT32_MIN;
		cache->min_val = INT32_MAX;
		cache->start_valid = 0;
		cache->end_valid = 0;
	}

	Field *source = field->graph.source;
	assert(source);

	// Pull in the latest point (if we are our periodic update)
	if (needUpdate)
		graphAddPoint(field, getEditableNumber(source));

	// Set axis coordinates
	int axisdigits = 5;
	int axiswidth = axisdigits
			* (GRAPH_MAXVAL_FONT.char_width + gui.char_h_space);
	graphX = layout->x; // upper left of graph
	graphY = layout->y; // upper left of graph,
	graphWidth = layout->width; // total draw area width
	graphHeight = layout->height; // total draw area height
	graphXmin = graphX + axiswidth; // x loc of 0,0 position
	graphXmax = graphX + graphWidth - 1; // x loc of rightmost data point
	graphYmin = graphY + graphHeight - 1; // y loc of 0,0 position (for min value)
	graphYmax = graphY + GRAPH_LABEL_FONT.char_height; // y loc of max value
	graphLabelY = graphY; // y loc of the label for field name

	// limit max x based on the number of points we might have (so each point gets its own column
	if (graphXmin + GRAPH_MAX_POINTS < graphXmax)
		graphXmax = graphXmin + GRAPH_MAX_POINTS;

	graphClearAndLabelAxis(field);
	graphDrawPoints(field);

	return true;
}

static bool renderEnd(FieldLayout *layout) {
	assert(0); // This should never be called I think
	return true;
}

// If we are showing a scrollable redraw it
static void forceScrollableRender() {
	Field *active = getActiveScrollable();
	if (active) {
		scrollableStack[0]->dirty = true; // the gui thread only looks in the root scrollable to find dirty
		forceScrollableRelayout = true;
	}
}

// Mark a new editable as active (and that it now wants to be animated)
static void setActiveEditable(Field *clicked) {
	if (curActiveEditable)
		curActiveEditable->blink = false;

	curActiveEditable = clicked;

	if (clicked) {
		clicked->dirty = true; // force redraw with highlighting
		clicked->blink = true;
	}

	forceScrollableRender(); // FIXME, I'm not sure if this is really required
}

// Returns true if we've handled the event (and therefore it should be cleared)
static bool onPressEditable(buttons_events_t events) {
	bool handled = false;
	Field *s = curActiveEditable;

	if (events & UP_CLICK) {
		// Note: we mark that we've handled this 'event' (so that other subsystems don't think they should) but really, we have already
		// been calling changeEditable in our render function, where we check only on blinkChanged, so that users can press and hold to
		// change values.
		// changeEditable(true);
		handled = true;
	}

	if (events & DOWN_CLICK) {
		// changeEditable(false);
		handled = true;
	}

	// Mark that we are no longer editing - click pwr button to exit
	if (events & SCREENCLICK_STOP_EDIT) {
		setActiveEditable(NULL);

		handled = true;
	}

	if (handled) {
		s->dirty = true; // redraw our position

		// If we are inside a scrollable, tell the GUI that scrollable also needs to be redrawn
		Field *scrollable = getActiveScrollable();
		if (scrollable) {
			scrollableStack[0]->dirty = true; // we just changed something, make sure we get a chance to be redrawn
		}
	}

	return handled;
}

int countEntries(Field *s) {
	Field *e = s->scrollable.entries;

	int n = 0;
	while (e && e->variant != FieldEnd) {
		n++;
		e++;
	}

	return n;
}

// Returns true if we've handled the event (and therefore it should be cleared)
// if first or selected changed, mark our scrollable as dirty (so child editables can be drawn)
static bool onPressScrollable(buttons_events_t events) {
	bool handled = false;
	Field *s = getActiveScrollable();

	if (!s)
		return false; // no scrollable is active

	if (events & UP_CLICK) {
		if (s->scrollable.selected >= 1) {
			s->scrollable.selected--;
		}

		if (s->scrollable.selected < s->scrollable.first) // we need to scroll the whole list up some
			s->scrollable.first = s->scrollable.selected;

		forceScrollableRender();
		handled = true;
	}

	if (events & DOWN_CLICK) {
		int numEntries = countEntries(s);

		if (s->scrollable.selected < numEntries - 1) {
			s->scrollable.selected++;
		}

		int numDataRows = maxRowsPerScreen - 1;
		int lastVisibleRow = s->scrollable.first + numDataRows - 1;
		if (s->scrollable.selected > lastVisibleRow) // we need to scroll the whole list down some
			s->scrollable.first = s->scrollable.selected - numDataRows + 1;

		forceScrollableRender();
		handled = true;
	}

	// If we aren't already editing anything, start now (note: we will only be called if some active editable
	// hasn't already handled this button
	if (events & SCREENCLICK_START_EDIT) {
		Field *clicked = &s->scrollable.entries[s->scrollable.selected];

		switch (clicked->variant) {
		case FieldEditable:
			if (!clicked->editable.read_only) { // only start editing non read only fields
				setActiveEditable(clicked);
				handled = true;
			}
			break;

		case FieldScrollable:
			enterScrollable(clicked);
			handled = true;
			break;

		default:
			break;
		}
	}

	// click power button to exit out of menus
	if (!handled && (events & SCREENCLICK_STOP_EDIT)) {
		handled = exitScrollable(); // if we were top scrollable don't claim we handled this press (let rest of app do it)
	}

	return handled;
}

/**
 * Used to map from FieldVariant enums to rendering functions
 */
static const FieldRenderFn renderers[] = { renderDrawText, renderDrawTextPtr,
		renderFill, renderMesh, renderScrollable, renderEditable, renderCustom,
		renderGraph, renderEnd };

static Screen *curScreen;
static bool screenDirty;

bool screenOnPress(buttons_events_t events) {
	bool handled = false;

	if (curActiveEditable)
		handled |= onPressEditable(events);

	if (!handled)
		handled |= onPressScrollable(events);

	if (!handled && curScreen && curScreen->onPress)
		handled |= curScreen->onPress(events);

	return handled;
}

// A low level screen render that doesn't use soft device or call exit handlers (useful for the critical fault handler ONLY)
void panicScreenShow(Screen *screen) {
	setActiveEditable(NULL);
	scrollableStackPtr = 0; // new screen might not have one, we will find out when we render
	curScreen = screen;
	screenDirty = true;

	if (curScreen->onEnter)
		(*curScreen->onEnter)();

	screenUpdate(); // Force a draw immediately
}

void screenShow(Screen *screen) {
	if (curScreen && curScreen->onExit)
		curScreen->onExit();

	panicScreenShow(screen);
}

Screen* getCurrentScreen() {
	return curScreen;
}

void screenUpdate() {
	if (!curScreen)
		return;

	if (curScreen->onUpdate)
		(*curScreen->onUpdate)();

	bool didDraw = false; // we only render to hardware if something changed

	// Every 200ms toggle any blinking animations
	screenUpdateCounter++;
	blinkChanged = (screenUpdateCounter
			% (BLINK_INTERVAL_MS / UPDATE_INTERVAL_MS) == 0);
	if (blinkChanged) {
		blinkOn = !blinkOn;
	}

	if (screenDirty) {
		; // clear screen (to prevent turds from old screen staying around)
		UG_FillScreen(C_BLACK);
		didDraw = true;
	}

	// For each field if that field is dirty (or the screen is) redraw it
	didDraw |= renderLayouts(curScreen->fields, screenDirty);

#ifdef SW102
  // flush the screen to the hardware
  if (didDraw)
  {
    lcd_refresh();
  }
#endif

	screenDirty = false;
}

void fieldPrintf(Field *field, const char *fmt, ...) {
	va_list argp;
	va_start(argp, fmt);
	char buf[sizeof(field->drawText.msg)] = "";

	assert(field->variant == FieldDrawText);
	vsnprintf(buf, sizeof(buf), fmt, argp);
	if (strcmp(buf, field->drawText.msg) != 0) {
		strcpy(field->drawText.msg, buf);
		field->dirty = true;
	}

	va_end(argp);
}

