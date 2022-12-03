# Nick2-Nixie-Clock
Nixie clocks based on ESP8266 and NTP, easily flashable (NodeMCU style) and super slim

IN-16 version: Schematic, gerber and BOM and 3D model of cover are included.

Wiki
https://github.com/mcer12/Nick2-Nixie-Clock/wiki

Some things to note:

**Some things to note:**
- This design is made with small footprint and ultra low-profile in mind, using only 3mm high components.
- Can be almost completely sourced from LCSC (except for the VFDs of course) and partially assembled using JLCPCB assembly service.
- Colons made out of WS2812 addressable leds
- CH340 with NodeMCU style auto-reset built-in for easy programming / debugging
- Each digit is driven directly, not multiplexed
- ~100hz refresh rate using HW ISR timer, zero flicker and not affected by wifi activity.
- NTP based, no battery / RTC. Connect to wifi and you're done.
- Simple and easy to set up, mobile-friendly web interface
- diyHue and remote control support
- 3 levels of brightness, each with 8 more levels for dimming / crossfade. 48 pwm steps in total for each segment!
- No buttons design. Simple configuration portal is used for settings.
- Daylight saving (summer time) support built in and super simple to set up.
- USB-C connector Below 500mA power consumption on 5V on full brightness.

**License:**  
GPL-3.0 License  
LGPL-2.1 License (modified SPI library)  
Mozilla Public License 2.0 (iro.js, https://github.com/jaames/iro.js)
