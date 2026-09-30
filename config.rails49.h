/*
 * Build configuration for the rails49 layout command station.
 *
 * This describes the hardware on the layout. It is not config.h: the release
 * workflow copies it to config.h and builds that. Nothing is injected, see
 * rails49/README.md.
 */
#ifndef config_rails49_h
#define config_rails49_h

// EX-CSB1 with an EX8874 stacked on top: four tracks, A and B on the CSB1,
// C and D on the EX8874.
#define MOTOR_SHIELD_TYPE EXCSB1_WITH_EX8874

// Boot limit for every track. Since <JG track mA> exists this is a starting
// value rather than a ceiling: the layout raises or lowers it at power on.
#define MAX_CURRENT 1234

// No wifi. The layout server reaches the station over USB.

#define OLED_DRIVER 132,64
#define SCROLLMODE 1

// The ESP32 build has no EEPROM anyway; saying so keeps the banner honest.
#define DISABLE_EEPROM

#endif
