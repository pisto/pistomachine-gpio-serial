# pistomachine-gpio-serial

Small project to use joystick and buttons of my pistomachine with the GPIO pins of a Raspberry Pi, then send the input over a serial-to-USB cable to the actual computer that runs the games.

Done _the right way_(?): the gpio-keys driver handles the GPIO with interrupts (see the overlay file), `pistomachine-gpio-serial` reads the dev/input file and encodes events to single bytes to be sent over serial, then again `pistomachine-gpio-serial` creates an uinput device on the receiving host to replay them, and most likely map them to the desired keycodes.

The serial ports are configured by a systemd service, requesting the low_latency mode.
