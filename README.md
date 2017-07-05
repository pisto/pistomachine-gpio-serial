# pistomachine-gpio-serial

Small project to use joysticks and buttons of my pistomachine (a DYI arcade cabinet) with the GPIO pins of a Raspberry Pi wired to an old laptop. This was necessary because the USB-to-GPIO adapter I have is very unstable and crashes very often.

It is done _the right way_(?). The RPi is backfeeded by an USB-to-serial cable attached to the laptop, so it is brought up and down at the same time as the laptop. The installation on the RPi memory card runs on a read-only system, so power loss is not an issue. Input follows this path:

* GPIO pins are configured with a device tree overlay (`pistomachine-keys.dts`) that exposes them as a virtual keyboard through the `gpio-keys` driver
* `pistomachine-gpio-serial` on the RPi reads the event device, translates codes into single-byte commands and sends them over serial
* `pistomachine-gpio-serial` on the laptop reads the serial inputs, decodes the keys, maps them to desired values, and replays them to a virtual uinput device

While harder to setup than an average userland implementation, this solution should be better because `gpio-keys` uses kernel features (interrupts over polling, automatic debouncing) and the terminal is set to `low_latency` mode. The RPi should not drain much power since it is downclocked, and both the HDMI output and the USB bus are powered off at boot.
