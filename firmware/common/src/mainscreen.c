/*
 * Bafang LCD 850C firmware
 *
 * Copyright (C) Casainho, 2018.
 *
 * Released under the GPL License, Version 3
 */

#include <math.h>
#include <string.h>
#include "stdio.h"
#include "main.h"
#include "utils.h"
#include "screen.h"
#include "rtc.h"
#include "fonts.h"
#include "uart.h"
#include "mainscreen.h"
#include "eeprom.h"
#include "buttons.h"
#include "lcd.h"
#include "adc.h"
#include "ugui.h"
#include "configscreen.h"
#include "state.h"
#include "timer.h"

uint8_t ui8_m_wheel_speed_decimal;

static uint8_t ui8_walk_assist_state = 0;

uint16_t ui16_m_battery_current_filtered_x10;
uint16_t ui16_m_motor_current_filtered_x10;
uint16_t ui16_m_battery_power_filtered;
uint16_t ui16_m_pedal_power_filtered;

void lcd_main_screen(void);
void warnings(void);
void walk_assist_state(void);
void power(void);
void time(void);
void wheel_speed(void);
void battery_soc(void);
void trip_time(void);
void wheel_speed(void);
void showNextScreen();
static bool renderWarning(FieldLayout *layout);
void DisplayResetToDefaults(void);
void onSetConfigurationBatteryTotalWh(uint32_t v);
void batteryTotalWh(void);
void batteryCurrent(void);
void motorCurrent(void);
void batteryPower(void);
void pedalPower(void);
void thresholds(void);

/// set to true if this boot was caused because we had a watchdog failure, used to show user the problem in the fault line
bool wd_failure_detected;

#define MAX_TIMESTR_LEN 8 // including nul terminator

//
// Fields - these might be shared my multiple screens
//
Field socField = FIELD_DRAWTEXT_RW();
Field timeField = FIELD_DRAWTEXT_RW();
Field assistLevelField = FIELD_READONLY_UINT("assist", &ui_vars.ui8_assist_level, "", false);
#ifdef SW102
Field wheelSpeedIntegerField = FIELD_READONLY_UINT("speed", &ui_vars.ui16_wheel_speed_x10, "kph", false, .div_digits = 1, .hide_fraction = true);
#else
Field wheelSpeedIntegerField = FIELD_READONLY_UINT("speed", &ui_vars.ui16_wheel_speed_x10, "kph", false, .div_digits = 1, .hide_fraction = true);
#endif

Field wheelSpeedDecimalField = FIELD_READONLY_UINT("", &ui8_m_wheel_speed_decimal, "kph", false);
Field wheelSpeedField = FIELD_READONLY_UINT("speed", &ui_vars.ui16_wheel_speed_x10, "kph", true, .div_digits = 1);

// Note: this field is special, the string it is pointing to must be in RAM so we can change it later
Field tripTimeField = FIELD_READONLY_STRING(_S("trip time", "trip time"), (char [MAX_TIMESTR_LEN]){ 0 });

Field tripDistanceField = FIELD_READONLY_UINT(_S("trip distance", "trip dista"), &ui_vars.ui32_trip_x10, "km", false, .div_digits = 1);
Field odoField = FIELD_READONLY_UINT("odometer", &ui_vars.ui32_odometer_x10, "km", false, .div_digits = 1);
Field cadenceField = FIELD_READONLY_UINT("cadence", &ui_vars.ui8_pedal_cadence_filtered, "rpm", true, .div_digits = 0);
Field humanPowerField = FIELD_READONLY_UINT(_S("human power", "human powr"), &ui16_m_pedal_power_filtered, "W", true, .div_digits = 0);
Field batteryPowerField = FIELD_READONLY_UINT(_S("motor power", "motor powr"), &ui16_m_battery_power_filtered, "W", true, .div_digits = 0);
Field batteryVoltageField = FIELD_READONLY_UINT(_S("batt voltage", "bat volts"), &ui_vars.ui16_battery_voltage_filtered_x10, "", true, .div_digits = 1);
Field batteryCurrentField = FIELD_READONLY_UINT(_S("batt current", "bat curren"), &ui16_m_battery_current_filtered_x10, "", true, .div_digits = 1);
Field motorCurrentField = FIELD_READONLY_UINT(_S("motor current", "mot curren"), &ui16_m_motor_current_filtered_x10, "", true, .div_digits = 1);
Field batterySOCField = FIELD_READONLY_UINT(_S("battery SOC", "bat SOC"), &ui16_g_battery_soc_watts_hour, "%", true, .div_digits = 0);
Field motorTempField = FIELD_READONLY_UINT(_S("motor temp", "mot temp"), &ui_vars.ui8_motor_temperature, "C", true, .div_digits = 0);
Field motorErpsField = FIELD_READONLY_UINT(_S("motor speed", "mot speed"), &ui_vars.ui16_motor_speed_erps, "", true, .div_digits = 0);
Field pwmDutyField = FIELD_READONLY_UINT(_S("motor pwm", "mot pwm"), &ui_vars.ui8_duty_cycle, "", true, .div_digits = 0);
Field motorFOCField = FIELD_READONLY_UINT(_S("motor foc", "mot foc"), &ui_vars.ui8_foc_angle, "", true, .div_digits = 0);

Field warnField = FIELD_CUSTOM(renderWarning);

/**
 * NOTE: The indexes into this array are stored in EEPROM, to prevent user confusion add new options only at the end.
 * If you remove old values, either warn users or bump up eeprom version to force eeprom contents to be discarded.
 */
Field *customizables[] = {
    &tripTimeField, // 0
    &tripDistanceField, // 1
    &odoField, // 2
    &wheelSpeedField, // 3
    &cadenceField, // 4
		&humanPowerField, // 5
		&batteryPowerField, // 6
    &batteryVoltageField, // 7
    &batteryCurrentField, // 8
    &motorCurrentField, // 9
    &batterySOCField, // 10
		&motorTempField, // 11
    &motorErpsField, // 12
		&pwmDutyField, // 13
		&motorFOCField, // 14
		NULL
};

// We currently don't have any graphs in the SW102, so leave them here until then
// kevinh: I think the following could be probably shared with the defs above (no need to copy and compute twice).  Also high chance of introducing bugs
// only in one place.
// Though I'm not sure why you need l2 vs l3 vars in this case.
Field wheelSpeedFieldGraph = FIELD_READONLY_UINT("speed", &rt_vars.ui16_wheel_speed_x10, "km", false, .div_digits = 1);
Field tripDistanceFieldGraph = FIELD_READONLY_UINT("trip distance", &rt_vars.ui32_trip_x10, "km", false, .div_digits = 1);
Field odoFieldGraph = FIELD_READONLY_UINT("odometer", &rt_vars.ui32_odometer_x10, "km", false, .div_digits = 1);
Field cadenceFieldGraph = FIELD_READONLY_UINT("cadence", &rt_vars.ui8_pedal_cadence_filtered, "", false);
Field humanPowerFieldGraph = FIELD_READONLY_UINT("human power", &rt_vars.ui16_pedal_power_filtered, "", false);
Field batteryPowerFieldGraph = FIELD_READONLY_UINT("motor power", &rt_vars.ui16_battery_power_filtered, "", false);
Field batteryVoltageFieldGraph = FIELD_READONLY_UINT("battery voltage", &rt_vars.ui16_battery_voltage_filtered_x10, "", false, .div_digits = 1);
Field batteryCurrentFieldGraph = FIELD_READONLY_UINT("battery current", &ui16_m_battery_current_filtered_x10, "", false, .div_digits = 1);
Field motorCurrentFieldGraph = FIELD_READONLY_UINT("motor current", &ui16_m_motor_current_filtered_x10, "", false, .div_digits = 1);
Field batterySOCFieldGraph = FIELD_READONLY_UINT("battery SOC", &ui16_g_battery_soc_watts_hour, "", false);
Field motorTempFieldGraph = FIELD_READONLY_UINT("motor temperature", &rt_vars.ui8_motor_temperature, "C", false);
Field motorErpsFieldGraph = FIELD_READONLY_UINT("motor speed", &rt_vars.ui16_motor_speed_erps, "", false);
Field pwmDutyFieldGraph = FIELD_READONLY_UINT("pwm duty-cycle", &rt_vars.ui8_duty_cycle, "", false);
Field motorFOCFieldGraph = FIELD_READONLY_UINT("motor foc", &rt_vars.ui8_foc_angle, "", false);

#ifndef SW102 // we don't have any graphs yet on SW102, possibly move this into mainscreen_850.c

Field wheelSpeedGraph = FIELD_GRAPH(&wheelSpeedFieldGraph, .min_threshold = -1, .graph_vars = &g_graphVars[VarsWheelSpeed]);
Field tripDistanceGraph = FIELD_GRAPH(&tripDistanceFieldGraph, .min_threshold = -1, .graph_vars = &g_graphVars[VarsTripDistance]);
Field odoGraph = FIELD_GRAPH(&odoFieldGraph, .min_threshold = -1, .graph_vars = &g_graphVars[VarsOdometer]);
Field cadenceGraph = FIELD_GRAPH(&cadenceFieldGraph, .min_threshold = -1, .graph_vars = &g_graphVars[VarsCadence]);
Field humanPowerGraph = FIELD_GRAPH(&humanPowerFieldGraph, .min_threshold = -1, .graph_vars = &g_graphVars[VarsHumanPower]);
Field batteryPowerGraph = FIELD_GRAPH(&batteryPowerFieldGraph, .min_threshold = -1, .graph_vars = &g_graphVars[VarsBatteryPower]);
Field batteryVoltageGraph = FIELD_GRAPH(&batteryVoltageFieldGraph, .min_threshold = -1, .graph_vars = &g_graphVars[VarsBatteryVoltage]);
Field batteryCurrentGraph = FIELD_GRAPH(&batteryCurrentFieldGraph, .filter = FilterSquare, .min_threshold = -1, .graph_vars = &g_graphVars[VarsBatteryCurrent]);
Field motorCurrentGraph = FIELD_GRAPH(&motorCurrentFieldGraph, .min_threshold = -1, .graph_vars = &g_graphVars[VarsMotorCurrent]);
Field batterySOCGraph = FIELD_GRAPH(&batterySOCFieldGraph, .min_threshold = -1, .graph_vars = &g_graphVars[VarsBatterySOC]);
Field motorTempGraph = FIELD_GRAPH(&motorTempFieldGraph, .min_threshold = -1, .graph_vars = &g_graphVars[VarsMotorTemp]);
Field motorErpsGraph = FIELD_GRAPH(&motorErpsFieldGraph, .min_threshold = -1, .graph_vars = &g_graphVars[VarsMotorERPS]);
Field pwmDutyGraph = FIELD_GRAPH(&pwmDutyFieldGraph, .min_threshold = -1, .graph_vars = &g_graphVars[VarsMotorPWM]);
Field motorFOCGraph = FIELD_GRAPH(&motorFOCFieldGraph, .min_threshold = -1, .graph_vars = &g_graphVars[VarsMotorFOC]);

// Note: the number of graphs in this collection must equal GRAPH_VARIANT_SIZE (for now)
Field graphs = FIELD_CUSTOMIZABLE(&ui_vars.field_selectors[0],
  &wheelSpeedGraph,
  &tripDistanceGraph,
  &cadenceGraph,
  &humanPowerGraph,
  &batteryPowerGraph,
  &batteryVoltageGraph,
  &batteryCurrentGraph,
  &motorCurrentGraph,
  &batterySOCGraph,
  &motorTempGraph,
  &motorErpsGraph,
  &pwmDutyGraph,
  &motorFOCGraph);
#else
const Field graphs = FIELD_CUSTOMIZABLE(&ui_vars.field_selectors[0],
  NULL);
#endif

Field *activeGraphs = NULL; // set only once graph data is safe to read

// Note: field_selectors[0] is used on the 850C for the graphs selector
Field custom1 = FIELD_CUSTOMIZABLE_PTR(&ui_vars.field_selectors[1], customizables),
 custom2 = FIELD_CUSTOMIZABLE_PTR(&ui_vars.field_selectors[2], customizables),
 custom3 = FIELD_CUSTOMIZABLE_PTR(&ui_vars.field_selectors[3], customizables),
 custom4 = FIELD_CUSTOMIZABLE_PTR(&ui_vars.field_selectors[4], customizables);

Field bootHeading = FIELD_DRAWTEXT_RO(_S("OpenSource EBike", "OS-EBike")),
 bootURL_1 = FIELD_DRAWTEXT_RO(_S("www.github.com/", "Keep pedal")),
 bootURL_2 = FIELD_DRAWTEXT_RO(_S("OpenSource-EBike-Firmware", "free")),
 bootFirmwareVersion = FIELD_DRAWTEXT_RO("850C firmware version:"),
 bootVersion = FIELD_DRAWTEXT_RO(VERSION_STRING),
 bootStatus1 = FIELD_DRAWTEXT_RO(_S("Keep pedals free and wait", "free pedal")),
 bootStatus2 = FIELD_DRAWTEXT_RW(.msg = "Booting...");

#define MIN_VOLTAGE_10X 140 // If our measured bat voltage (using ADC in the display) is lower than this, we assume we are running on a developers desk

static void bootScreenOnPreUpdate() {
	uint16_t bvolt = battery_voltage_10x_get();

	g_is_sim_motor = (bvolt < MIN_VOLTAGE_10X);
  if(g_is_sim_motor)
    fieldPrintf(&bootStatus2, _S("SIMULATING TSDZ2!", "SIMULATING"));

  if(g_has_seen_motor) {
    fieldPrintf(&bootStatus2, _S("TSDZ2 firmware: %u.%u.%u", "%u.%u.%u"),
    g_tsdz2_firmware_version.major,
    g_tsdz2_firmware_version.minor,
    g_tsdz2_firmware_version.patch);
  } else {
    fieldPrintf(&bootStatus2, _S("Waiting TSDZ2 - (%u.%uV)", "Wait %u.%uV"), bvolt / 10, bvolt % 10);
  }

  // Stop showing only after we release on/off button and we are commutication with motor
  if(buttons_get_onoff_state() == 0 && (g_has_seen_motor || g_is_sim_motor))
    showNextScreen();
}

Screen bootScreen = {
  .onPreUpdate = bootScreenOnPreUpdate,

  .fields = {
    {
      .x = 0, .y = YbyEighths(1), .height = -1,
      .field = &bootHeading,
      .font = &REGULAR_TEXT_FONT,
    },
    {
      .x = 0, .y = -20, .height = -1,
      .field = &bootURL_1,
      .font = &SMALL_TEXT_FONT,
    },

    {
      .x = 0, .y = -6, .height = -1,
      .field = &bootURL_2,
      .font = &SMALL_TEXT_FONT,
    },
#ifndef SW102
    {
      .x = 0, .y = YbyEighths(4), .height = -1,
      .field = &bootStatus1,
      .font = &SMALL_TEXT_FONT,
    },
    {
      .x = 0, .y = YbyEighths(6), .height = -1,
      .field = &bootFirmwareVersion,
      .font = &SMALL_TEXT_FONT,
    },
#endif
    {
      .x = 0, .y = -8, .height = -1,
      .field = &bootVersion,
      .font = &SMALL_TEXT_FONT,
    },
    {
      .x = 0, .y = YbyEighths(7), .height = -1,
      .field = &bootStatus2,
      .font = &SMALL_TEXT_FONT,
    },
    {
      .field = NULL
    }
  }
};

// Allow common operations (like walk assist and headlights) button presses to work on any page
bool anyscreen_onpress(buttons_events_t events) {
  if ((events & DOWN_LONG_CLICK) && ui_vars.ui8_walk_assist_feature_enabled) {
    ui8_walk_assist_state = 1;
    return true;
  }

  // long up to turn on headlights
  if (events & UP_LONG_CLICK) {
    ui_vars.ui8_lights = !ui_vars.ui8_lights;
    set_lcd_backlight();

    return true;
  }

  return false;
}

bool mainscreen_onpress(buttons_events_t events) {
	if(anyscreen_onpress(events))
	  return true;

	if (events & UP_CLICK /* &&
	 m_lcd_vars.ui8_lcd_menu_max_power == 0 */) {
		ui_vars.ui8_assist_level++;

		if (ui_vars.ui8_assist_level > ui_vars.ui8_number_of_assist_levels) {
			ui_vars.ui8_assist_level = ui_vars.ui8_number_of_assist_levels;
		}

		return true;
	}

	if (events & DOWN_CLICK /* &&
	 m_lcd_vars.ui8_lcd_menu_max_power == 0 */) {
		if (ui_vars.ui8_assist_level > 0)
			ui_vars.ui8_assist_level--;

		return true;
	}

	return false;
}


void set_conversions() {
  screenConvertMiles = ui_vars.ui8_units_type != 0; // Set initial value on unit conversions (FIXME, move this someplace better)
  screenConvertFarenheit = screenConvertMiles; // FIXME, should be based on a different eeprom config value
  screenConvertPounds = screenConvertMiles;
}

void lcd_main_screen(void) {
	time();
	walk_assist_state();
//  offroad_mode();
	power();
	battery_soc();
	battery_display();
	warnings();
	trip_time();
	wheel_speed();
}

void wheel_speed(void)
{
  // limit otherwise at startup this value goes crazy
  if(ui_vars.ui16_wheel_speed_x10 > 999) {
    ui_vars.ui16_wheel_speed_x10 = 999;
  }

  // Note: no need to check for 'wheel speed previous' because this math is so cheap
  ui8_m_wheel_speed_decimal = (uint8_t) (ui_vars.ui16_wheel_speed_x10 % 10);
}

void power(void) {
#if 0

  if(!m_lcd_vars.ui8_lcd_menu_max_power)
  {
    _ui16_battery_power_filtered = ui_vars.ui16_battery_power;

    if((_ui16_battery_power_filtered != ui16_battery_power_filtered_previous) ||
        m_lcd_vars.ui32_main_screen_draw_static_info ||
        ui8_target_max_battery_power_state == 0)
    {
      ui16_battery_power_filtered_previous = _ui16_battery_power_filtered;
      ui8_target_max_battery_power_state = 1;

      if (_ui16_battery_power_filtered > 9999) { _ui16_battery_power_filtered = 9999; }

      power_number.ui32_number = _ui16_battery_power_filtered;
      power_number.ui8_refresh_all_digits = m_lcd_vars.ui32_main_screen_draw_static_info;
      lcd_print_number(&power_number);
      power_number.ui8_refresh_all_digits = 0;
    }
    else
    {

    }
  }
  else
  {
    // because this click envent can happens and will block the detection of button_onoff_long_click_event
    buttons_clear_onoff_click_event();

    // leave this menu with a button_onoff_long_click
    if(buttons_get_onoff_long_click_event())
    {
      buttons_clear_all_events();
      m_lcd_vars.ui8_lcd_menu_max_power = 0;
      ui8_target_max_battery_power_state = 0;
      power_number.ui8_refresh_all_digits = 1;

      // save the updated variables on EEPROM
      eeprom_write_variables();

      buttons_clear_all_events();
      return;
    }

    if(buttons_get_up_click_event())
    {
      buttons_clear_all_events();

      if(ui_vars.ui8_target_max_battery_power < 10)
      {
        ui_vars.ui8_target_max_battery_power++;
      }
      else
      {
        ui_vars.ui8_target_max_battery_power += 2;
      }

      // limit to 100 * 25 = 2500 Watts
      if(ui_vars.ui8_target_max_battery_power > 100) { ui_vars.ui8_target_max_battery_power = 100; }
    }

    if(buttons_get_down_click_event ())
    {
      buttons_clear_all_events();

      if(ui_vars.ui8_target_max_battery_power == 0)
      {

      }
      else if(ui_vars.ui8_target_max_battery_power <= 10)
      {
        ui_vars.ui8_target_max_battery_power--;
      }
      else
      {
        ui_vars.ui8_target_max_battery_power -= 2;
      }
    }

    if(ui8_lcd_menu_flash_state)
    {
      if(ui8_target_max_battery_power_state == 1)
      {
        ui8_target_max_battery_power_state = 0;

        // clear area
        power_number.ui8_clean_area_all_digits = 1;
        lcd_print_number(&power_number);
        power_number.ui8_clean_area_all_digits = 0;
      }
    }
    else
    {
      if(ui8_target_max_battery_power_state == 0)
      {
        ui8_target_max_battery_power_state = 1;

        ui16_target_max_power = ui_vars.ui8_target_max_battery_power * 25;

        power_number.ui8_refresh_all_digits = 1;
        power_number.ui32_number = ui16_target_max_power;
        lcd_print_number(&power_number);

        ui_vars.ui8_target_max_battery_power = ui16_target_max_power / 25;
      }
    }
  }
#endif
}

void screen_clock(void) {
  static int counter_time_ms = 0;
  int time_ms = 0;

  // No point to processing less than every 100ms, as the data comming from the motor is only updated every 100ms, not less
  time_ms = get_time_base_counter_1ms();
  if((time_ms - counter_time_ms) >= 100) // not least than evey 100ms
  {
    counter_time_ms = time_ms;

    // exchange data from realtime layer to UI layer
    // do this in atomic way, disabling the real time layer (should be no problem as
    // copy_rt_to_ui_vars() should be fast and take a small piece of the 100ms periodic realtime layer processing
    rt_processing_stop();
    copy_rt_to_ui_vars();
    rt_processing_start();

    lcd_main_screen();
#ifndef SW102
    clock_time();
#endif
    DisplayResetToDefaults();
    batteryTotalWh();
    batteryCurrent();
    motorCurrent();
    batteryPower();
    pedalPower();
#ifndef SW102
    thresholds();
#endif
    screenUpdate();
  }
}

void thresholds(void) {
#ifndef SW102
  if (*wheelSpeedField.rw->editable.number.auto_thresholds == FIELD_THRESHOLD_AUTO) {
    wheelSpeedField.rw->editable.number.error_threshold =
        wheelSpeedFieldGraph.rw->editable.number.error_threshold = ui_vars.wheel_max_speed_x10;
    wheelSpeedField.rw->editable.number.warn_threshold =
        wheelSpeedFieldGraph.rw->editable.number.warn_threshold = ui_vars.wheel_max_speed_x10 - (ui_vars.wheel_max_speed_x10 / 5); // -20%
  } else if (*wheelSpeedField.rw->editable.number.auto_thresholds == FIELD_THRESHOLD_MANUAL) {
    wheelSpeedField.rw->editable.number.error_threshold =
        wheelSpeedFieldGraph.rw->editable.number.error_threshold = *wheelSpeedField.rw->editable.number.config_error_threshold;
    wheelSpeedField.rw->editable.number.warn_threshold =
        wheelSpeedFieldGraph.rw->editable.number.warn_threshold = *wheelSpeedField.rw->editable.number.config_warn_threshold;
  }

  if (*cadenceField.rw->editable.number.auto_thresholds == FIELD_THRESHOLD_AUTO) {
    cadenceField.rw->editable.number.error_threshold =
        cadenceFieldGraph.rw->editable.number.error_threshold = 92;
    cadenceField.rw->editable.number.warn_threshold =
        cadenceFieldGraph.rw->editable.number.warn_threshold = 83; // -10%
  } else if (*cadenceField.rw->editable.number.auto_thresholds == FIELD_THRESHOLD_MANUAL) {
    cadenceField.rw->editable.number.error_threshold =
        cadenceFieldGraph.rw->editable.number.error_threshold = *cadenceField.rw->editable.number.config_error_threshold;
    cadenceField.rw->editable.number.warn_threshold =
        cadenceFieldGraph.rw->editable.number.warn_threshold = *cadenceField.rw->editable.number.config_warn_threshold;
  }

  if (*humanPowerField.rw->editable.number.auto_thresholds == FIELD_THRESHOLD_MANUAL) {
    humanPowerField.rw->editable.number.error_threshold =
        humanPowerFieldGraph.rw->editable.number.error_threshold = *humanPowerField.rw->editable.number.config_error_threshold;
    humanPowerField.rw->editable.number.warn_threshold =
        humanPowerFieldGraph.rw->editable.number.warn_threshold = *humanPowerField.rw->editable.number.config_warn_threshold;
  }

  if (*batteryPowerField.rw->editable.number.auto_thresholds == FIELD_THRESHOLD_AUTO) {
    int32_t temp = (int32_t) (((int32_t) ui_vars.ui8_battery_max_current * (int32_t) ui_vars.ui8_battery_cells_number) * (float) LI_ION_CELL_VOLTS_90);
    batteryPowerField.rw->editable.number.error_threshold =
        batteryPowerFieldGraph.rw->editable.number.error_threshold = temp;
    temp *= 10; // power * 10
    batteryPowerField.rw->editable.number.warn_threshold =
        batteryPowerFieldGraph.rw->editable.number.warn_threshold = (temp - (temp / 10)) / 10; // -10%
  } else if (*batteryPowerField.rw->editable.number.auto_thresholds == FIELD_THRESHOLD_MANUAL) {
    batteryPowerField.rw->editable.number.error_threshold =
        batteryPowerFieldGraph.rw->editable.number.error_threshold = *batteryPowerField.rw->editable.number.config_error_threshold;
    batteryPowerField.rw->editable.number.warn_threshold =
        batteryPowerFieldGraph.rw->editable.number.warn_threshold = *batteryPowerField.rw->editable.number.config_warn_threshold;
  }
  if (*batteryVoltageField.rw->editable.number.auto_thresholds == FIELD_THRESHOLD_AUTO) {
    int32_t temp = (int32_t) ui_vars.ui16_battery_low_voltage_cut_off_x10;
    batteryVoltageField.rw->editable.number.error_threshold =
        batteryVoltageFieldGraph.rw->editable.number.error_threshold = temp;
    temp *= 10;
    batteryVoltageField.rw->editable.number.warn_threshold =
        batteryVoltageFieldGraph.rw->editable.number.warn_threshold = (temp + (temp / 20)) / 10; // -5%
  } else if (*batteryVoltageField.rw->editable.number.auto_thresholds == FIELD_THRESHOLD_MANUAL) {
    batteryVoltageField.rw->editable.number.error_threshold =
        batteryVoltageFieldGraph.rw->editable.number.error_threshold = *batteryVoltageField.rw->editable.number.config_error_threshold;
    batteryVoltageField.rw->editable.number.warn_threshold =
        batteryVoltageFieldGraph.rw->editable.number.warn_threshold = *batteryVoltageField.rw->editable.number.config_warn_threshold;
  }

  if (*batteryCurrentField.rw->editable.number.auto_thresholds == FIELD_THRESHOLD_AUTO) {
    int32_t temp = (int32_t) ui_vars.ui8_battery_max_current * 10;
    batteryCurrentField.rw->editable.number.error_threshold =
        batteryCurrentFieldGraph.rw->editable.number.error_threshold = temp;
    temp *= 10; // current_x10 * 10
    batteryCurrentField.rw->editable.number.warn_threshold =
        batteryCurrentFieldGraph.rw->editable.number.warn_threshold = (temp - (temp / 10)) / 10; // -10%
  } else if (*batteryCurrentField.rw->editable.number.auto_thresholds == FIELD_THRESHOLD_MANUAL) {
    batteryCurrentField.rw->editable.number.error_threshold =
        batteryCurrentFieldGraph.rw->editable.number.error_threshold = *batteryCurrentField.rw->editable.number.config_error_threshold;
    batteryCurrentField.rw->editable.number.warn_threshold =
        batteryCurrentFieldGraph.rw->editable.number.warn_threshold = *batteryCurrentField.rw->editable.number.config_warn_threshold;
  }

  if (*motorCurrentField.rw->editable.number.auto_thresholds == FIELD_THRESHOLD_AUTO) {
    int32_t temp = (int32_t) ui_vars.ui8_motor_max_current * 10;
    motorCurrentField.rw->editable.number.error_threshold =
        motorCurrentFieldGraph.rw->editable.number.error_threshold = temp;
    temp *= 10; // current_x10 * 10
    motorCurrentField.rw->editable.number.warn_threshold =
        motorCurrentFieldGraph.rw->editable.number.warn_threshold = (temp - (temp / 10)) / 10; // -10%
  } else if (*motorCurrentField.rw->editable.number.auto_thresholds == FIELD_THRESHOLD_MANUAL) {
    motorCurrentField.rw->editable.number.error_threshold =
        motorCurrentFieldGraph.rw->editable.number.error_threshold = *motorCurrentField.rw->editable.number.config_error_threshold;
    motorCurrentField.rw->editable.number.warn_threshold =
        motorCurrentFieldGraph.rw->editable.number.warn_threshold = *motorCurrentField.rw->editable.number.config_warn_threshold;
  }

  if (*batterySOCField.rw->editable.number.auto_thresholds == FIELD_THRESHOLD_AUTO) {
    batterySOCField.rw->editable.number.error_threshold =
        batterySOCFieldGraph.rw->editable.number.error_threshold = 10;
    batterySOCField.rw->editable.number.warn_threshold =
        batterySOCFieldGraph.rw->editable.number.warn_threshold = 25;
  } else if (*batterySOCField.rw->editable.number.auto_thresholds == FIELD_THRESHOLD_MANUAL) {
    batterySOCField.rw->editable.number.error_threshold =
        batterySOCFieldGraph.rw->editable.number.error_threshold = *batterySOCField.rw->editable.number.config_error_threshold;
    batterySOCField.rw->editable.number.warn_threshold =
        batterySOCFieldGraph.rw->editable.number.warn_threshold = *batterySOCField.rw->editable.number.config_warn_threshold;
  }

  if (*motorTempField.rw->editable.number.auto_thresholds == FIELD_THRESHOLD_AUTO) {
    motorTempField.rw->editable.number.error_threshold =
        motorTempFieldGraph.rw->editable.number.error_threshold = (int32_t) ui_vars.ui8_motor_temperature_max_value_to_limit;
    motorTempField.rw->editable.number.warn_threshold =
        motorTempFieldGraph.rw->editable.number.warn_threshold = (int32_t) ui_vars.ui8_motor_temperature_min_value_to_limit;
  } else if (*motorTempField.rw->editable.number.auto_thresholds == FIELD_THRESHOLD_MANUAL) {
    motorTempField.rw->editable.number.error_threshold =
        motorTempFieldGraph.rw->editable.number.error_threshold = *motorTempField.rw->editable.number.config_error_threshold;
    motorTempField.rw->editable.number.warn_threshold =
        motorTempFieldGraph.rw->editable.number.error_threshold = *motorTempField.rw->editable.number.config_warn_threshold;
  }

  if (*motorErpsField.rw->editable.number.auto_thresholds == FIELD_THRESHOLD_AUTO) {
    motorErpsField.rw->editable.number.error_threshold =
        motorErpsFieldGraph.rw->editable.number.error_threshold = 525;
    motorErpsField.rw->editable.number.warn_threshold =
        motorErpsFieldGraph.rw->editable.number.warn_threshold = 473; // -10%
  } else if (*motorErpsField.rw->editable.number.auto_thresholds == FIELD_THRESHOLD_MANUAL) {
    motorErpsField.rw->editable.number.error_threshold =
        motorErpsFieldGraph.rw->editable.number.error_threshold = *motorErpsField.rw->editable.number.config_error_threshold;
    motorErpsField.rw->editable.number.warn_threshold =
        motorErpsFieldGraph.rw->editable.number.warn_threshold = *motorErpsField.rw->editable.number.config_warn_threshold;
  }

  if (*pwmDutyField.rw->editable.number.auto_thresholds == FIELD_THRESHOLD_AUTO) {
    pwmDutyField.rw->editable.number.error_threshold =
        pwmDutyFieldGraph.rw->editable.number.error_threshold = 254;
    pwmDutyField.rw->editable.number.warn_threshold =
        pwmDutyFieldGraph.rw->editable.number.warn_threshold = 228; // -10%
  } else if (*pwmDutyField.rw->editable.number.auto_thresholds == FIELD_THRESHOLD_MANUAL) {
    pwmDutyField.rw->editable.number.error_threshold =
        pwmDutyFieldGraph.rw->editable.number.error_threshold = *pwmDutyField.rw->editable.number.config_error_threshold;
    pwmDutyField.rw->editable.number.warn_threshold =
        pwmDutyFieldGraph.rw->editable.number.warn_threshold = *pwmDutyField.rw->editable.number.config_warn_threshold;
  }

  if (*motorFOCField.rw->editable.number.auto_thresholds == FIELD_THRESHOLD_AUTO) {
    motorFOCField.rw->editable.number.error_threshold =
        motorFOCFieldGraph.rw->editable.number.error_threshold = 8;
    motorFOCField.rw->editable.number.warn_threshold =
        motorFOCFieldGraph.rw->editable.number.warn_threshold = 6; // -20%
  } else if (*motorFOCField.rw->editable.number.auto_thresholds == FIELD_THRESHOLD_MANUAL) {
    motorFOCField.rw->editable.number.error_threshold =
        motorFOCFieldGraph.rw->editable.number.error_threshold = *motorFOCField.rw->editable.number.config_error_threshold;
    motorFOCField.rw->editable.number.warn_threshold =
        motorFOCFieldGraph.rw->editable.number.warn_threshold = *motorFOCField.rw->editable.number.config_warn_threshold;
  }
#endif
}

void trip_time(void) {
	rtc_time_t *p_time = rtc_get_time_since_startup();
	static int oldmin = -1; // used to prevent unneeded updates
	char timestr[MAX_TIMESTR_LEN]; // 12:13

	if(p_time->ui8_minutes != oldmin) {
		oldmin = p_time->ui8_minutes;
		sprintf(timestr, "%d:%02d", p_time->ui8_hours, p_time->ui8_minutes);
		updateReadOnlyStr(&tripTimeField, timestr);
	}
}

static ColorOp warnColor = ColorNormal;
static char warningStr[MAX_FIELD_LEN];

// We use a custom callback so we can reuse the standard drawtext code, but with a dynamically changing color
static bool renderWarning(FieldLayout *layout) {
	layout->color = warnColor;
	return renderDrawTextCommon(layout, warningStr);
}

static void setWarning(ColorOp color, const char *str) {
	warnColor = color;
	warnField.rw->blink = (color == ColorError);
	warnField.rw->dirty = (strcmp(str, warningStr) != 0);
	if(warnField.rw->dirty)
		strncpy(warningStr, str, sizeof(warningStr));
}

//static const char *motorErrors[] = { "None", "No config", "Motor Blocked", "Torque Fault", "Brake Fault", "Throttle Fault", "Speed Fault", "Low Volt" };
static const char *motorErrors[] = { "None", "No config", "Motor Blocked", "Torque Fault", "Brake Fault", "Throttle Fault", "Hall Sensors fault", "Over current" };

void warnings(void) {
  uint32_t motor_temp_limit = ui_vars.ui8_temperature_limit_feature_enabled & 1;

	// High priorty faults in red
if(ui_vars.ui8_error_states) {
  const char *str = itoa(ui_vars.ui8_error_states);
  setWarning(ColorError, str);
  return;
}

//	if(ui_vars.ui8_error_states) {
//		const char *str = (ui_vars.ui8_error_states > ERROR_MAX) ? "Unknown Motor" : motorErrors[ui_vars.ui8_error_states];
//		setWarning(ColorError, str);
//		return;
//	}

	if(motor_temp_limit &&
	    ui_vars.ui8_motor_temperature >= ui_vars.ui8_motor_temperature_max_value_to_limit) {
		setWarning(ColorError, _S("Temp Shutdown", "Temp Shut"));
		return;
	}

	// If we had a watchdog failure, show it forever - so user will report a bug
	if(wd_failure_detected) {
    setWarning(ColorError, "Report Bug!");
    return;
	}

	// warn faults in yellow
  if(motor_temp_limit &&
      ui_vars.ui8_motor_temperature >= ui_vars.ui8_motor_temperature_min_value_to_limit) {
		setWarning(ColorWarning, _S("Temp Warning", "Temp Warn"));
		return;
	}

	// All of the following possible 'faults' are low priority

	if(ui_vars.ui8_braking) {
		setWarning(ColorNormal, "BRAKE");
		return;
	}

	if(ui_vars.ui8_walk_assist) {
		setWarning(ColorNormal, "WALK");
		return;
	}

	if(ui_vars.ui8_lights) {
		setWarning(ColorNormal, "LIGHT");
		return;
	}

	setWarning(ColorNormal, "");
}

void battery_soc(void) {
  switch (ui_vars.ui8_battery_soc_enable) {
    default:
    case 0:
      // clear the area
      fieldPrintf(&socField, "");
      break;

    case 1:
      fieldPrintf(&socField, "%3d%%", ui16_g_battery_soc_watts_hour);
      break;

    case 2:
      fieldPrintf(&socField, "%u.%1uV",
          ui_vars.ui16_battery_voltage_soc_x10 / 10,
          ui_vars.ui16_battery_voltage_soc_x10 % 10);
      break;
  }
}


void time(void) {
	rtc_time_t *p_rtc_time = rtc_get_time();

	// force to be [0 - 12]
	if (ui_vars.ui8_units_type) { // FIXME, should be based on a different eeprom config value, just because someone is using mph doesn't mean they want 12 hr time
		if (p_rtc_time->ui8_hours > 12) {
			p_rtc_time->ui8_hours -= 12;
		}
	}

	fieldPrintf(&timeField, "%d:%02d", p_rtc_time->ui8_hours,
			p_rtc_time->ui8_minutes);
}

void walk_assist_state(void) {
	// kevinh - note on the sw102 we show WALK in the box normally used for BRAKE display - the display code is handled there now
	if (ui_vars.ui8_walk_assist_feature_enabled) {
		// if down button is still pressed
		if (ui8_walk_assist_state && buttons_get_down_state()) {
			ui_vars.ui8_walk_assist = 1;
		} else if (buttons_get_down_state() == 0) {
			ui8_walk_assist_state = 0;
			ui_vars.ui8_walk_assist = 0;
		}
	} else {
		ui8_walk_assist_state = 0;
		ui_vars.ui8_walk_assist = 0;
	}
}

// Screens in a loop, shown when the user short presses the power button
extern Screen *screens[];

void showNextScreen() {
	static int nextScreen;

	Screen *next = screens[nextScreen++];

	if (!next) {
		nextScreen = 0;
		next = screens[nextScreen++];
	}

	screenShow(next);
}

static bool appwide_onpress(buttons_events_t events)
{
  // power off only after we release first time the onoff button
  if (events & ONOFF_LONG_CLICK)
  {
    lcd_power_off(1);
    return true;
  }

  if(events & SCREENCLICK_NEXT_SCREEN) {
    showNextScreen();
    return true;
  }
	return false;
}

/// Called every 20ms to check for button events and dispatch to our handlers
static void handle_buttons() {

  static uint8_t firstTime = 1;

  // keep tracking of first time release of onoff button
  if(firstTime && buttons_get_onoff_state() == 0) {
    firstTime = 0;
    buttons_clear_onoff_click_event();
    buttons_clear_onoff_long_click_event();
    buttons_clear_onoff_click_long_click_event();
  }

  if (buttons_events && firstTime == 0)
  {
    bool handled = false;

		if (!handled)
			handled |= screenOnPress(buttons_events);

		// Note: this must be after the screen/menu handlers have had their shot
		if (!handled)
			handled |= appwide_onpress(buttons_events);

		if (handled)
			buttons_clear_all_events();
	}

	buttons_clock(); // Note: this is done _after_ button events is checked to provide a 20ms debounce
}

/// Call every 20ms from the main thread.
void main_idle() {
  static int counter_time_ms = 0;
  int time_ms = 0;

  // no point to processing less than every 100ms, as the data comming from the motor is only updated every 100ms, not less
  time_ms = get_time_base_counter_1ms();
  if((time_ms - counter_time_ms) >= 100) // not least than evey 100ms
  {
    counter_time_ms = time_ms;

    // asking the TSDZ2 firmware version and this will happen only once (at startup)
    if (g_tsdz2_firmware_version.major == 0xff) { // if version is invalid, like at startup
      if (g_communications_state == COMMUNICATIONS_READY)
        g_communications_state = COMMUNICATIONS_GET_MOTOR_FIRMWARE_VERSION;
    // after we get firmware version, set the TSDZ2 configurations
    } else if (g_tsdz2_configurations_set == false) {
      if (g_communications_state == COMMUNICATIONS_READY) {
        prepare_torque_sensor_calibration_table(); // we need to first prepare the table
        g_communications_state = COMMUNICATIONS_SET_CONFIGURATIONS; // set configuration to TSDZ2
      }
    }

    automatic_power_off_management();
  }

	handle_buttons();
	screen_clock(); // This is _after_ handle_buttons so if a button was pressed this tick, we immediately update the GUI
}

void batteryTotalWh(void) {

  ui32_g_configuration_wh_100_percent = ui_vars.ui32_wh_x10_100_percent / 10;
}

void onSetConfigurationBatteryTotalWh(uint32_t v) {

  ui_vars.ui32_wh_x10_100_percent = v * 10;
}

void DisplayResetToDefaults(void) {

  if (ui8_g_configuration_display_reset_to_defaults) {
    ui8_g_configuration_display_reset_to_defaults = 0;
    eeprom_init_defaults();
  }
}

void batteryCurrent(void) {

  ui16_m_battery_current_filtered_x10 = ui_vars.ui16_battery_current_filtered_x5 * 2;
}

void motorCurrent(void) {

  ui16_m_motor_current_filtered_x10 = ui_vars.ui16_motor_current_filtered_x5 * 2;
}

void onSetConfigurationWheelOdometer(uint32_t v) {

  // let's update the main variable used for calculations of odometer
  rt_vars.ui32_odometer_x10 = v;
}

void batteryPower(void) {

  ui16_m_battery_power_filtered = ui_vars.ui16_battery_power;

  // loose resolution under 200W
  if (ui16_m_battery_power_filtered < 200) {
    ui16_m_battery_power_filtered /= 10;
    ui16_m_battery_power_filtered *= 10;
  }
  // loose resolution under 400W
  else if (ui16_m_battery_power_filtered < 500) {
    ui16_m_battery_power_filtered /= 20;
    ui16_m_battery_power_filtered *= 20;
  }
}

void pedalPower(void) {

  ui16_m_pedal_power_filtered = ui_vars.ui16_pedal_power;

  if (ui16_m_pedal_power_filtered > 500) {
    ui16_m_pedal_power_filtered /= 20;
    ui16_m_pedal_power_filtered *= 20;
  } else if (ui16_m_pedal_power_filtered > 200) {
    ui16_m_pedal_power_filtered /= 10;
    ui16_m_pedal_power_filtered *= 10;
  } else if (ui16_m_pedal_power_filtered > 10) {
    ui16_m_pedal_power_filtered /= 5;
    ui16_m_pedal_power_filtered *= 5;
  }
}
