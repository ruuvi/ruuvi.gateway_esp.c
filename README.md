### Ruuvidongle ESP32 firmware

Developed with ESP_IDF version v4.1-dev-1086-g93a8603c5

Needs nrf52 running and connected to UART pins.

IO pins:
```
#define LED_PIN 23
```
Uart:
```
#define TXD_PIN (GPIO_NUM_4)
#define RXD_PIN (GPIO_NUM_5)
```

### Configure the project

```
idf.py menuconfig
```

* Set serial port under Serial Flasher Options.

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.
