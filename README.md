# Bullitt-Bike-Lights
light effects for your LvH Bullitt (or any other bike) with ESP and addressable LEDs

- the ESP provides a simple Webservice to switch between effects. You can also connect a button on D7 to switch effects.
- a reed contact is used to measure speed, so effects should be "synchronous" to the street surface (which looks kinda cool...)
- if no speed is measured, "demo"-mode will start after 10 secondes.


## Used Material
- Addressable LED stripe
- ESP8266 (or similar)
- Reed Contact
- Power supply
- Cables, fuses, connectors, housing

### LED stripe
As they are very popular at the moment, I'm using the WS2815 chipsets (as they are 12V and have a backup data line)

### ESP
I've decided to use a USB-C powered developer board with ESP8266.

### Reed Contact
I placed it next to the original reed contact, so I don't need to mount an extra magnet. Works fine!

### Power Supply
Shimano EP801 / EP600 has a dedicated "addon"-power connector, which delivers 13.2V 2A max. I decided to use two DCDC Converters: one 12V converter for the LEDs, one (with 5V USB-C output for the ESP). And it's for sure wise to include a fuse, just to protect the motor unit in case you f*ck up the wiring...
Sidenote: The program tries to estimate the power consumption and limits brightness of the leds to avoid exceeding 1600mA. So there is plenty of reserve for the ESP and loss of the converters...

### Cables & Connector, Housing
you need cables for
- Power 2x0.75mm2
- LED Stripes 3x0.3mm2
- Reed contact 2x0.02mm
I've opted for a robust solution, as I will mount / unmount the system from time to time.
To protect Shimanos power outlet, I added a SP13 Connector (they are IP65).
I packed everything into a IP65 rated housing. You'll have to add some cable outlets, too.

### Used Arduino Libraries
- [FastLED](https://github.com/FastLED/FastLED/wiki/Overview)
- ESP8266WiFi (ESP8266 Core)
- ESP8266WebServer (ESP8266 Core)
- LittleFS (ESP8266 Core)

### Amazon Shopping List:

- [Cable 24awg 2×0,2mm²](https://www.amazon.de/gp/product/B0B7WTT9RK)
- [Cable 18awg 2×0,75mm²](https://www.amazon.de/gp/product/B0B7WSYX8Y)
- [Cable 22awg 3×0,3mm²](https://www.amazon.de/gp/product/B0BFWG2JZY)
- [Connector SP13 2 Pin](https://www.amazon.de/gp/product/B0D5DLMVW6)
- [Reed-Contact](https://www.amazon.de/gp/product/B07Z4RR4QV)
- [IP65 Housing](https://www.amazon.de/gp/product/B0GVH2Y73W)
- [WS2815 5m 300LEDs](https://www.amazon.de/gp/product/B07LG6CK2S)
- [DC 12V to 5V USB-C Step-down](https://www.amazon.de/gp/product/B0D1FVHC1C)
- [DC 12V to 12V Converter](https://www.amazon.de/gp/product/B0B1F2VWLY)
- [D1 ESP8266 Mini Board NodeMCU](https://www.amazon.de/gp/product/B0D66LXTTK)


## Wiring
Check arduiono source code for pin usage:
 *   GPIO 5  - left
 *   GPIO 4  - right
 *   GPIO 14 - crank
 *   GPIO 12 - reed
 *   GPIO 13 - effect button

## Configuration
there are three different LED-Strips defined: cargo area left, cargo area rigth, crank area. 
Length, virtual position and LED-Count are defined in the code:

    constexpr uint16_t NUM_LEFT  = 60;    // 1m = 60 LEDS
    constexpr uint16_t NUM_RIGHT = 60;
    constexpr uint16_t NUM_CRANK = 27;
    ... 
    Segment segmentLeft  = { 0.00f, 1.00f, NUM_LEFT,  true };
    Segment segmentRight = { 0.00f, 1.00f, NUM_RIGHT, true };
    Segment segmentCrank = { 1.00f, 0.45f, NUM_CRANK, false };

    constexpr float VIRTUAL_LENGTH_M = 1.45f; 

If you change the length of your stripes, edit these lines (you may need to shorten segmentLeft and segmentRight to about 0.8m, as I have BullittX)


