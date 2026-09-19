/*
 * Build configuration for the rails49 layout command station.
 *
 * This describes the hardware on the layout. It is not config.h: the release
 * workflow copies it to config.h and builds that. Nothing is injected and there
 * is no secret, see rails49/README.md.
 */
#ifndef config_rails49_h
#define config_rails49_h

// EX-CSB1 with an EX8874 stacked on top: four tracks, A and B on the CSB1,
// C and D on the EX8874.
#define MOTOR_SHIELD_TYPE EXCSB1_WITH_EX8874

// Boot limit for every track. Since <JG track mA> exists this is a starting
// value rather than a ceiling: the layout raises or lowers it at power on.
#define MAX_CURRENT 1234

// The station is its own access point. WifiESP32.cpp treats an SSID beginning
// "Your network " as "no SSID configured" and falls back to AP mode, which is
// what this layout wants: nothing connects the station to a house network, and
// the layout server reaches it over USB.
//
// WIFI_PASSWORD is that access point's password and is deliberately published.
// Left out, the firmware generates PASS_<mac> and shows it on the station's
// display, which is no use to anyone who cannot see the display. Naming it here
// means it can be read from this file instead, and it guards nothing that the
// layout network does not already leave open. It must be 8 characters or more.
// Anyone wanting a password of their own builds their own firmware.
#define ENABLE_WIFI true
#define WIFI_SSID "Your network name"
#define WIFI_PASSWORD "rails49dccex"
#define WIFI_HOSTNAME "dccex"
#define WIFI_CHANNEL 1
#define IP_PORT 2560

#define OLED_DRIVER 132,64
#define SCROLLMODE 1

// The ESP32 build has no EEPROM anyway; saying so keeps the banner honest.
#define DISABLE_EEPROM

#endif
