// ask_transmitter.pde
// -*- mode: C++ -*-
// Simple example of how to use RadioHead to transmit messages
// with a simple ASK transmitter in a very simple way.
// Implements a simplex (one-way) transmitter with an TX-C1 module
// Tested on Arduino Mega, Duemilanova, Uno, Due, Teensy, ESP-12

#include <RH_ASK.h>
#include <SPI.h> // Not actually used but needed to compile

RH_ASK radio_driver;
// RH_ASK driver(2000, 4, 5, 0); // ESP8266 or ESP32: do not use pin 11 or 2

void setup()
{
    Serial.begin(9600);	  // Debugging only
    if (!radio_driver.init())
         Serial.println("init failed");
}

void loop()
{
    const char *msg = "hello";

    radio_driver.send((uint8_t *)msg, strlen(msg));
    radio_driver.waitPacketSent();
    delay(200);
}
