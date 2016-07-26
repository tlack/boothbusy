BoothBusy
=========

Booth Busy is a simple ESP8266 application to display the status from an hc-sr501 motion detector.

How to use
----------

Connect HC-SR501 to a GPIO pin (such as D4). Also connect GND and VCC (USB runs at 5v).

Compile/upload to ESP8266 from Arduino.

This code loads static file content from the SPIFFS (built in Flash) system. If you want to use
images or whatever, such as the default background image, you must upload it separately using this
convenient process: https://github.com/esp8266/Arduino/blob/master/doc/filesystem.md#uploading-files-to-file-system

Connect to access point the ESP will present - it should be called "Booth Busy - [Room Name]".

You can then visit http://192.168.42.1 from your device and see the room status.

Uses Ajax to update the status every 250ms.

You can customize the IP address and access point names toward the top of the boothbusy.ino file.

The HTML, CSS, and JS is just slammed in there.

Credits
-------

Built at Building.co

License
-------

BSD 2clause


