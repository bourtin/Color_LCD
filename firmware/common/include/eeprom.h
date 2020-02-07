/*
 * Bafang LCD 850C firmware
 *
 * Copyright (C) Casainho, 2018.
 *
 * Released under the GPL License, Version 3
 */

#ifndef _EEPROM_H_
#define _EEPROM_H_

#include "lcd.h"
#include "state.h"
#include "screen.h"

// For compatible changes, just add new fields at the end of the table (they will be inited to 0xff for old eeprom images).  For incompatible
// changes bump up EEPROM_MIN_COMPAT_VERSION and the user's EEPROM settings will be discarded.
#define EEPROM_MIN_COMPAT_VERSION 0x26
#define EEPROM_VERSION 0x26

typedef struct {
  graph_auto_max_min_t auto_max_min;
  int32_t max;
  int32_t min;
} Graph_eeprom;

typedef struct eeprom_data {
	uint8_t eeprom_version; // Used to detect changes in eeprom encoding, if != EEPROM_VERSION we will not use it

	uint8_t ui8_assist_level;
	uint16_t ui16_wheel_perimeter;
	uint8_t ui8_wheel_max_speed;
	uint8_t ui8_units_type;
	uint32_t ui32_wh_x10_offset;
	uint32_t ui32_wh_x10_100_percent;
	uint8_t ui8_battery_soc_enable;
	uint8_t ui8_battery_soc_symbol;
	uint8_t ui8_battery_max_current;
	uint8_t ui8_motor_max_current;
  uint8_t ui8_motor_current_min_adc;
	uint8_t ui8_ramp_up_amps_per_second_x10;
	uint8_t ui8_ramp_down_amps_per_second_x10;
	uint8_t ui8_battery_cells_number;
	uint16_t ui16_battery_low_voltage_cut_off_x10;
	uint8_t ui8_motor_type;
	uint8_t ui8_motor_assistance_startup_without_pedal_rotation;
	uint16_t ui16_assist_level_factor[9];
	uint8_t ui8_number_of_assist_levels;
	uint8_t ui8_startup_motor_power_boost_feature_enabled;
	uint8_t ui8_startup_motor_power_boost_always;
	uint8_t ui8_startup_motor_power_boost_limit_power;
	uint16_t ui16_startup_motor_power_boost_factor[9];
	uint8_t ui8_startup_motor_power_boost_time;
	uint8_t ui8_startup_motor_power_boost_fade_time;
	uint8_t ui8_temperature_limit_feature_enabled;
	uint8_t ui8_motor_temperature_min_value_to_limit;
	uint8_t ui8_motor_temperature_max_value_to_limit;
	uint16_t ui16_battery_voltage_reset_wh_counter_x10;
	uint8_t ui8_lcd_power_off_time_minutes;
	uint8_t ui8_lcd_backlight_on_brightness;
	uint8_t ui8_lcd_backlight_off_brightness;
	uint16_t ui16_battery_pack_resistance_x1000;
	uint8_t ui8_offroad_feature_enabled;
	uint8_t ui8_offroad_enabled_on_startup;
	uint8_t ui8_offroad_speed_limit;
	uint8_t ui8_offroad_power_limit_enabled;
	uint8_t ui8_offroad_power_limit_div25;
	uint32_t ui32_odometer_x10;
	uint8_t ui8_walk_assist_feature_enabled;
	uint8_t ui8_walk_assist_level_factor[9];

	uint8_t ui8_battery_soc_increment_decrement;
	uint8_t ui8_buttons_up_down_invert;
  uint8_t ui8_torque_sensor_calibration_feature_enabled;
  uint8_t ui8_torque_sensor_calibration_pedal_ground;
  uint16_t ui16_torque_sensor_calibration_table_left[8][2];
  uint16_t ui16_torque_sensor_calibration_table_right[8][2];

	uint8_t field_selectors[NUM_CUSTOMIZABLE_FIELDS]; // this array is opaque to the app, but the screen layer uses it to store which field is being displayed (it is stored to EEPROM)

	uint8_t x_axis_scale; // x axis scale
	uint8_t customizable_choices_selector;
	uint8_t customizableFieldIndex;

#ifndef SW102
	Graph_eeprom graph_eeprom[VARS_SIZE];
  uint8_t tripDistanceField_x_axis_scale_config;
  uint8_t odoField_x_axis_scale_config;
	field_threshold_t wheelSpeedField_auto_thresholds;
	int32_t wheelSpeedField_config_error_threshold;
	int32_t wheelSpeedField_config_warn_threshold;
	uint8_t wheelSpeedField_x_axis_scale_config;
	field_threshold_t cadenceField_auto_thresholds;
  int32_t cadenceField_config_error_threshold;
  int32_t cadenceField_config_warn_threshold;
  uint8_t cadenceField_x_axis_scale_config;
  field_threshold_t humanPowerField_auto_thresholds;
  int32_t humanPowerField_config_error_threshold;
  int32_t humanPowerField_config_warn_threshold;
  uint8_t humanPowerField_x_axis_scale_config;
  field_threshold_t batteryPowerField_auto_thresholds;
  int32_t batteryPowerField_config_error_threshold;
  int32_t batteryPowerField_config_warn_threshold;
  uint8_t batteryPowerField_x_axis_scale_config;
  field_threshold_t batteryVoltageField_auto_thresholds;
  int32_t batteryVoltageField_config_error_threshold;
  int32_t batteryVoltageField_config_warn_threshold;
  uint8_t batteryVoltageField_x_axis_scale_config;
  field_threshold_t batteryCurrentField_auto_thresholds;
  int32_t batteryCurrentField_config_error_threshold;
  int32_t batteryCurrentField_config_warn_threshold;
  uint8_t batteryCurrentField_x_axis_scale_config;
  field_threshold_t motorCurrentField_auto_thresholds;
  int32_t motorCurrentField_config_error_threshold;
  int32_t motorCurrentField_config_warn_threshold;
  uint8_t motorCurrentField_x_axis_scale_config;
  field_threshold_t batterySOCField_auto_thresholds;
  int32_t batterySOCField_config_error_threshold;
  int32_t batterySOCField_config_warn_threshold;
  uint8_t batterySOCField_x_axis_scale_config;
  field_threshold_t motorTempField_auto_thresholds;
  int32_t motorTempField_config_error_threshold;
  int32_t motorTempField_config_warn_threshold;
  uint8_t motorTempField_x_axis_scale_config;
  field_threshold_t motorErpsField_auto_thresholds;
  int32_t motorErpsField_config_error_threshold;
  int32_t motorErpsField_config_warn_threshold;
  uint8_t motorErpsField_x_axis_scale_config;
  field_threshold_t pwmDutyField_auto_thresholds;
  int32_t pwmDutyField_config_error_threshold;
  int32_t pwmDutyField_config_warn_threshold;
  uint8_t pwmDutyField_x_axis_scale_config;
  field_threshold_t motorFOCField_auto_thresholds;
  int32_t motorFOCField_config_error_threshold;
  int32_t motorFOCField_config_warn_threshold;
  uint8_t motorFOCField_x_axis_scale_config;
#endif

// FIXME align to 32 bit value by end of structure and pack other fields
} eeprom_data_t;

void eeprom_init(void);
void eeprom_init_variables(void);
void eeprom_write_variables(void);
void eeprom_init_defaults(void);

// *************************************************************************** //
// EEPROM memory variables default values
#define DEFAULT_VALUE_ASSIST_LEVEL                                  0
#define DEFAULT_VALUE_NUMBER_OF_ASSIST_LEVELS                       9
#define DEFAULT_VALUE_WHEEL_PERIMETER                               2100 // 27.5'' wheel: 2100mm perimeter
#define DEFAULT_VALUE_WHEEL_MAX_SPEED                               50
#define DEFAULT_VALUE_UNITS_TYPE                                    0 // 0 = km/h
#define DEFAULT_VALUE_WH_X10_OFFSET                                 0
#define DEFAULT_VALUE_HW_X10_100_PERCENT                            4000 // default to a battery of 400 Wh
#define DEAFULT_VALUE_SHOW_NUMERIC_BATTERY_SYMBOL                   0 // SOC
#define DEAFULT_VALUE_SHOW_NUMERIC_BATTERY_SOC                      2 // volts
#define DEFAULT_VALUE_BATTERY_MAX_CURRENT                           16 // 16 amps
#define DEFAULT_VALUE_MOTOR_MAX_CURRENT                             16 // 16 amps
#define DEFAULT_VALUE_CURRENT_MIN_ADC                               1 // 1 unit, 0.156 A
#define DEFAULT_VALUE_RAMP_UP_AMPS_PER_SECOND_X10                   50 // 5.0 amps per second ramp up
#define DEFAULT_VALUE_RAMP_DOWN_AMPS_PER_SECOND_X10                   50 // 5.0 amps per second ramp down
#define DEFAULT_VALUE_TARGET_MAX_BATTERY_POWER                      0 // e.g. 20 = 20 * 25 = 500, 0 is disabled
#define DEFAULT_VALUE_BATTERY_CELLS_NUMBER                          14 // 14 --> 52V
#define DEFAULT_VALUE_BATTERY_LOW_VOLTAGE_CUT_OFF_X10               420 // 52v battery, LVC = 42.0 (3.0 * 14)
#define DEFAULT_VALUE_MOTOR_TYPE                                    0 // ui8_motor_type = 0 = 48V
#define DEFAULT_VALUE_MOTOR_ASSISTANCE_WITHOUT_PEDAL_ROTATION       0 // 0 to keep this feature disable
#define DEFAULT_VALUE_ASSIST_LEVEL_FACTOR_1                         10 // 0.01
#define DEFAULT_VALUE_ASSIST_LEVEL_FACTOR_2                         15
#define DEFAULT_VALUE_ASSIST_LEVEL_FACTOR_3                         23
#define DEFAULT_VALUE_ASSIST_LEVEL_FACTOR_4                         35
#define DEFAULT_VALUE_ASSIST_LEVEL_FACTOR_5                         53
#define DEFAULT_VALUE_ASSIST_LEVEL_FACTOR_6                         78
#define DEFAULT_VALUE_ASSIST_LEVEL_FACTOR_7                         118
#define DEFAULT_VALUE_ASSIST_LEVEL_FACTOR_8                         177
#define DEFAULT_VALUE_ASSIST_LEVEL_FACTOR_9                         267
#define DEFAULT_VALUE_WALK_ASSIST_FEATURE_ENABLED                   1
#define DEFAULT_VALUE_WALK_ASSIST_LEVEL_FACTOR_1                    35
#define DEFAULT_VALUE_WALK_ASSIST_LEVEL_FACTOR_2                    40
#define DEFAULT_VALUE_WALK_ASSIST_LEVEL_FACTOR_3                    45
#define DEFAULT_VALUE_WALK_ASSIST_LEVEL_FACTOR_4                    50
#define DEFAULT_VALUE_WALK_ASSIST_LEVEL_FACTOR_5                    55
#define DEFAULT_VALUE_WALK_ASSIST_LEVEL_FACTOR_6                    60
#define DEFAULT_VALUE_WALK_ASSIST_LEVEL_FACTOR_7                    70
#define DEFAULT_VALUE_WALK_ASSIST_LEVEL_FACTOR_8                    80
#define DEFAULT_VALUE_WALK_ASSIST_LEVEL_FACTOR_9                    90
#define DEFAULT_VALUE_STARTUP_MOTOR_POWER_BOOST_FEATURE_ENABLED     0
#define DEFAULT_VALUE_STARTUP_MOTOR_POWER_BOOST_ALWAYS              1
#define DEFAULT_VALUE_STARTUP_MOTOR_POWER_BOOST_ASSIST_LEVEL_1      5
#define DEFAULT_VALUE_STARTUP_MOTOR_POWER_BOOST_ASSIST_LEVEL_2      8
#define DEFAULT_VALUE_STARTUP_MOTOR_POWER_BOOST_ASSIST_LEVEL_3      12
#define DEFAULT_VALUE_STARTUP_MOTOR_POWER_BOOST_ASSIST_LEVEL_4      18
#define DEFAULT_VALUE_STARTUP_MOTOR_POWER_BOOST_ASSIST_LEVEL_5      27
#define DEFAULT_VALUE_STARTUP_MOTOR_POWER_BOOST_ASSIST_LEVEL_6      41
#define DEFAULT_VALUE_STARTUP_MOTOR_POWER_BOOST_ASSIST_LEVEL_7      62
#define DEFAULT_VALUE_STARTUP_MOTOR_POWER_BOOST_ASSIST_LEVEL_8      93
#define DEFAULT_VALUE_STARTUP_MOTOR_POWER_BOOST_ASSIST_LEVEL_9      140
#define DEFAULT_VALUE_STARTUP_MOTOR_POWER_BOOST_TIME                20 // 2.0 seconds
#define DEFAULT_VALUE_STARTUP_MOTOR_POWER_BOOST_FADE_TIME           35 // 3.5 seconds
#define DEFAULT_VALUE_MOTOR_TEMPERATURE_FEATURE_ENABLE              0
#define DEFAULT_VALUE_MOTOR_TEMPERATURE_MIN_VALUE_LIMIT             75 // 75 degrees celsius
#define DEFAULT_VALUE_MOTOR_TEMPERATURE_MAX_VALUE_LIMIT             85 // 85 degrees celsius
#define DEFAULT_VALUE_BATTERY_VOLTAGE_RESET_WH_COUNTER_X10          554 // 52v battery, 55.8 volts at fully charged
#define DEFAULT_VALUE_LCD_POWER_OFF_TIME                            60 // 60 minutes, each unit 1 minute
#ifdef SW102
#define DEFAULT_VALUE_LCD_BACKLIGHT_ON_BRIGHTNESS                   100 // 8 = 40%
#define DEFAULT_VALUE_LCD_BACKLIGHT_OFF_BRIGHTNESS                  20 // 20 = 100%
#else
#define DEFAULT_VALUE_LCD_BACKLIGHT_ON_BRIGHTNESS                   15 // 100 = 100%
#define DEFAULT_VALUE_LCD_BACKLIGHT_OFF_BRIGHTNESS                  100
#endif
#define DEFAULT_VALUE_BATTERY_PACK_RESISTANCE                       300 // 52v battery, 14S3P measured 300 milli ohms
#define DEFAULT_VALUE_OFFROAD_FEATURE_ENABLED                       0
#define DEFAULT_VALUE_OFFROAD_MODE_ENABLED_ON_STARTUP               0
#define DEFAULT_VALUE_OFFROAD_SPEED_LIMIT                           25
#define DEFAULT_VALUE_OFFROAD_POWER_LIMIT_ENABLED                   0
#define DEFAULT_VALUE_OFFROAD_POWER_LIMIT_DIV25                     10 //10 * 25 = 250W
#define DEFAULT_VALUE_ODOMETER_X10                                  0
#define DEFAULT_VALUE_BUTTONS_UP_DOWN_INVERT                        0 // regular state
#define DEFAULT_VALUE_X_AXIS_SCALE                                  0 // 15m
#define DEFAULT_CUSTOMIZABLE_CHOICES_SELECTOR                       0 // the very first one
#define DEFAULT_CUSTOMIZABLE_FIELD_INDEX                            0 // the very first one

#define DEFAULT_TORQUE_SENSOR_CALIBRATION_FEATURE_ENABLE            0 // disabled
#define DEFAULT_TORQUE_SENSOR_CALIBRATION_PEDAL_GROUND              0 // left pedal
#define DEFAULT_TORQUE_SENSOR_CALIBRATION_LEFT_WEIGHT_1             0
#define DEFAULT_TORQUE_SENSOR_CALIBRATION_LEFT_ADC_1                292
#define DEFAULT_TORQUE_SENSOR_CALIBRATION_LEFT_WEIGHT_2             5
#define DEFAULT_TORQUE_SENSOR_CALIBRATION_LEFT_ADC_2                317
#define DEFAULT_TORQUE_SENSOR_CALIBRATION_LEFT_WEIGHT_3             10
#define DEFAULT_TORQUE_SENSOR_CALIBRATION_LEFT_ADC_3                331
#define DEFAULT_TORQUE_SENSOR_CALIBRATION_LEFT_WEIGHT_4             15
#define DEFAULT_TORQUE_SENSOR_CALIBRATION_LEFT_ADC_4                338
#define DEFAULT_TORQUE_SENSOR_CALIBRATION_LEFT_WEIGHT_5             19
#define DEFAULT_TORQUE_SENSOR_CALIBRATION_LEFT_ADC_5                342
#define DEFAULT_TORQUE_SENSOR_CALIBRATION_LEFT_WEIGHT_6             30
#define DEFAULT_TORQUE_SENSOR_CALIBRATION_LEFT_ADC_6                350
#define DEFAULT_TORQUE_SENSOR_CALIBRATION_LEFT_WEIGHT_7             54
#define DEFAULT_TORQUE_SENSOR_CALIBRATION_LEFT_ADC_7                359
#define DEFAULT_TORQUE_SENSOR_CALIBRATION_LEFT_WEIGHT_8             105
#define DEFAULT_TORQUE_SENSOR_CALIBRATION_LEFT_ADC_8                369
#define DEFAULT_TORQUE_SENSOR_CALIBRATION_RIGHT_WEIGHT_1             0
#define DEFAULT_TORQUE_SENSOR_CALIBRATION_RIGHT_ADC_1                292
#define DEFAULT_TORQUE_SENSOR_CALIBRATION_RIGHT_WEIGHT_2             5
#define DEFAULT_TORQUE_SENSOR_CALIBRATION_RIGHT_ADC_2                324
#define DEFAULT_TORQUE_SENSOR_CALIBRATION_RIGHT_WEIGHT_3             10
#define DEFAULT_TORQUE_SENSOR_CALIBRATION_RIGHT_ADC_3                340
#define DEFAULT_TORQUE_SENSOR_CALIBRATION_RIGHT_WEIGHT_4             15
#define DEFAULT_TORQUE_SENSOR_CALIBRATION_RIGHT_ADC_4                354
#define DEFAULT_TORQUE_SENSOR_CALIBRATION_RIGHT_WEIGHT_5             19
#define DEFAULT_TORQUE_SENSOR_CALIBRATION_RIGHT_ADC_5                360
#define DEFAULT_TORQUE_SENSOR_CALIBRATION_RIGHT_WEIGHT_6             30
#define DEFAULT_TORQUE_SENSOR_CALIBRATION_RIGHT_ADC_6                370
#define DEFAULT_TORQUE_SENSOR_CALIBRATION_RIGHT_WEIGHT_7             52
#define DEFAULT_TORQUE_SENSOR_CALIBRATION_RIGHT_ADC_7                378
#define DEFAULT_TORQUE_SENSOR_CALIBRATION_RIGHT_WEIGHT_8             105
#define DEFAULT_TORQUE_SENSOR_CALIBRATION_RIGHT_ADC_8                386

// *************************************************************************** //

// Torque sensor value found experimentaly
// measuring with a cheap digital hook scale, we found that each torque sensor unit is equal to 0.556 Nm
// using the scale, was found that each 1kg was measured as 3 torque sensor units
// Force (Nm) = Kg * 9.18 * 0.17 (arm cranks size)
#define TORQUE_SENSOR_FORCE_SCALE_X1000 556

// *************************************************************************** //
// BATTERY

// ADC Battery voltage
// 0.344 per ADC_8bits step: 17.9V --> ADC_8bits = 52; 40V --> ADC_8bits = 116; this signal atenuated by the opamp 358
#define ADC10BITS_BATTERY_VOLTAGE_PER_ADC_STEP_X512 44
#define ADC10BITS_BATTERY_VOLTAGE_PER_ADC_STEP_X256 (ADC10BITS_BATTERY_VOLTAGE_PER_ADC_STEP_X512 >> 1)
#define ADC8BITS_BATTERY_VOLTAGE_PER_ADC_STEP 0.344

// ADC Battery current
// 1A per 5 steps of ADC_10bits
#define ADC_BATTERY_CURRENT_PER_ADC_STEP_X512 102
// *************************************************************************** //

#endif /* _EEPROM_H_ */
