# SlackLight

An Arduino / ESP8266 based slack alert light. Perfect for when your build breaks.

## Getting started

You'll need the Arduino IDE and a few additional libraries. This used to be a pain, but Arduino now has a board and library manager built in.

* Download the [Arduino IDE](https://www.arduino.cc/en/Main/Software).
* Open the Arduino IDE and add ESP8266 support - see the [Installing with Boards Manager](https://github.com/esp8266/Arduino#installing-with-boards-manager) guide in the ESP Arduino repo.
* Open the Library Manager under Sketch -> Include Library -> Manage Libraries.
* Search for and install the following libraries:
    * NeoPixelBus by Makuna - used to run the RGB LEDs.
    * Websockets by Markus Sattler - used to connect to Slack realtime messaging.
    * ArduinoJson by Benoit Blanchon - used to compose/parse Slack API calls.
* Make sure `Wemos D1 R2 & mini` is selected under Tools -> Board. Once this is selected a whole bunch of other options appear in the Tools menu. The other important one is Tools -> Port. Something like `ttyUSB0` should appear under there when the board is connected to your computer.
* Check everything is working by opening the SlackLight2 sketch and compiling it (ctrl-r).

NOTE: The USB cable attached to SlackLight **cannot be used for programming it** - it's power only. You'll need to unscrew the top and plug in a micro USB cable directly to the Wemos D1 mini microcrontroller. Having both USB cables connected at the same time is probably a _bad idea_.

Once that's working, you can modify and upload the code to the board using Sketch -> Upload (ctrl-u).

## Configuring SlackLight2 sketch

There's a bunch of `#define`s at the start of the sketch that need to be changed to make it work.

* `SLACK_BOT_TOKEN` the bot token used when connecting to Slack.
* `WIFI_SSID` wifi network name.
* `WIFI_PASSWORD` wifi password.
* `ALARM_TEXT` the string that causes the alarm to go off.
* `ALARM_DURATION` how long the alarm goes for in milliseconds.
* `ALARM_PERIOD` the current alarm is a spinning rainbow, this is the amount of time it takes for a full rotation in milliseconds. Try 500 for a more frantic alarm.

There's also the constant `lightness` that sets the brightness of the LEDs. This can be anywhere from `0.0f` (off) to `0.5f` (insane). Running the LEDs at `0.5f` for sustained period of time (>30s) will probably overheat them reducing their lifespan.

## How SlackLight2 sketch works

All Arduino sketches have the `setup()` and `begin()` functions - in SlackLight2, these are at the bottom. `setup()` runs once at startup, and the `loop()` runs continuously until the microcontroller either crashes or is powered off.

In setup, the sketch starts a rainbow animation, connects to the specified wireless network and sets the internal clock using NTP. In the loop function there's a bunch of call outs to updater methods - this is a common microcontroller pattern, because they're single threaded (you basically have to implement multi-processing manually by providing some "time" to each "process" each cycle of the main loop).

The simplest of these is the last one - a check to see if a running alarm should be turned off (note: the `millis()` function returns the number of milliseconds since the microcrontroller booted). The `websocket.loop()`, `animations.UpdateAnimations()` and `strip.Show()` functions are from their corresponding libraries where you're instructed to put them in your main loop. `slackLoop()` is defined in the sketch and is used to maintain the connection to Slack.

## Further reading / documentation

* The docs for Makuna's [NeoPixelBus library](https://github.com/Makuna/NeoPixelBus/wiki) is great.
* The [ESP8266 for Arduino](https://arduino-esp8266.readthedocs.io/en/latest/) docs are ok.
* There's a boat load of example sketches under File -> Examples, with anything from the basics (make an LED blink!) to the full-mental (ESP8266WebServer that hosts files and implements a suprising amount of HTTP/1.1). Most of the NeoPixelBus examples should work straight away - you need only change the pixel count to 8.
* The official [Arduino docs](https://www.arduino.cc/en/Guide/HomePage) are ok for getting started with programming, but it's mad simple. It's just really dumb C.
