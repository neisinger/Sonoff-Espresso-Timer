# SonoffEspressoTimer
This is a Coffee Machine Controller with a Sonoff Basic as a Timer. This project doesn't attempt to create a PID for any Espresso machine, but be the simplest single- & double-shot timer that can be integrated.

## Setup
Connect the Pump of the Coffee Machine to the Output of the Sonoff and the Pushbutton to GPIO 14 and Ground.
Connect TX to SCK and RX to SDA of the Display. (0,96" OLED)

## Elektra Cappuccina
I'm using an old (1996) Elektra Cappuccina R1 as basis for this project, so no guarantees this might work on different machines (though it should).

## Circuit Diagram

This is the original Circuit Diagram of the Elektra coffee machine.

![circuit diagram Elektra Cappuccina original](/images/circuit_elektra_orig.png)

I tried to make my own diagram, so it's easier to understand what is going on.

![circuit diagram of unmodified Elektra](/images/circuit_diag_elektra_orig.png)

The workings of this machine are very easy. Basically the power switch gives power to the machine and it immediately starts heating up until the 95°C breaker opens. If you want to brew Espresso you push the espresso button which gives power to the pump for as long as you have the switch pushed. I will ignore the steam part, as I don't use it.

![circuit diagram with added Sonoff Timer](/images/circuit_diag_elektra_sonoff.png)

Basically I just replace the espresso button with the flashed Sonoff timer and use the original push button as input for the Sonoff timer.

## Usage
The Controller tries to connect to specified WiFi. Add your credentials in this file: /src/main.cpp under Configuration.
You can connect to the Sonoff by typing "SonoffEspresso/" or the IP in your Webbrowser.
The Webinterface is presented to programm the pumping Time for single and double Shot.

There are 3 Operation-Modes:
- Single: Brewing for the Time you set for Singleshot
- Double: Brewing for the Time you set for Doubleshot
- Manual: Brewing as long as the Button is held

If you push the button again while the machine is brewing the machine will stop.

Saving new Times:
- Start Single- or Doubleshot brewing
- Push the Button again and hold as long as you want
- New brewing Time is saved for Single- or Doubleshot

## Thanks
This project is a modificatin of ljans sonoffgrinder https://github.com/ljan/sonoffgrinder
Thanks to timo.helfer @ www.kaffeenetz.de for the inspiration and codebase.
