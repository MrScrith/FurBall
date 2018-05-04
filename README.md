# FurBall
Datalogger for cat exercise wheel based on esp8266

This is using the [Wio-Link](https://www.seeedstudio.com/Wio-Link-p-2604.html) board from [Seeedstudio](https://www.seeedstudio.com). This should also be compatible with the [Wio-Node](https://www.seeedstudio.com/Wio-Node-p-2637.html) (untested).

The other components are a [ADS1115](https://www.adafruit.com/product/1085) and a [sensor board](https://www.amazon.com/Tracking-Tracker-Infrared-ITR20001-Detector/dp/B073VH14SP) with multiple [ITR20001/t](http://www.everlight.com/file/ProductFile/ITR20001-T.pdf) infrared sensors, there are 5 on the board but only 4 are in use by this setup. While this may be a waste, this allows a spare in case one dies.

This reads 4-bit circular grey-code on the back of the catwheel giving it a 1/16 positional accuracy for the wheel, this is read every 10ms. Every 5 minutes the total distance is reported to a thingsboard instance running locally, where it is logged and eventually used for tracking the health of the cats.

Note, distance has to be translated from segment counts to actual distance in feet, as the wheel is 4ft in diameter the distance per segment is 0.78 ft.
