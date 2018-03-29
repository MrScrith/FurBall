# FurBall
Datalogger for cat exercise wheel based on esp8266

This is using the [Wio-Link](https://www.seeedstudio.com/Wio-Link-p-2604.html) board from [Seeedstudio](https://www.seeedstudio.com). This should also be compatible with the [Wio-Node](https://www.seeedstudio.com/Wio-Node-p-2637.html) (untested).

The other components are a [ADS1115](https://www.adafruit.com/product/1085) and a [sensor board](https://www.amazon.com/Tracking-Tracker-Infrared-ITR20001-Detector/dp/B073VH14SP) with multiple [ITR20001/t](http://www.everlight.com/file/ProductFile/ITR20001-T.pdf) infrared sensors, there are 5 on the board but only 4 are in use by this setup. While this may be a waste, this allows a spare in case one dies.

The eventual setup will be painted segments on the back of the wheel that will give a circular 4-bit binary counter, this will be read by the infrared sensors to track the wheel position and it's movement. This movement will be recorded to the on-chip filesystem (flash) and read via webpage.
