## Overview

The Boat Environmental Monitor consists of:

- A high efficiency power supply working of 12V
  - Supplier: PiHut
  - Pololu 5V, 1A Step-Down Voltage Regulator D24V10F5
- Nucleo-L432KC Development Board
  - Supplier: Ebay (or RS Components (123-1043), Farnell (2579222), DigiKey UK).
  - Role: Main MCU running core logic.
- Clipper LTE 4G Breakout (SP/CE)
  - Supplier: Pi Hut (or Pimoroni)
  - Role: Handles M2M cellular transmission, offloading TCP/TLS/MQTT via internal 
    AT command engine. Includes SMA antenna (with starter kit) connector and nano-SIM slot.
  - Note: This board can have transient power requirements up to 2A. AI says that 
    the D24V10F5 1A supply with good power reservoir capacitors should cope. If not 
    there is a 2.5A F24V25F5 power supply.
  - Note: Allow 50mm separation between the antenna and the Nucleo development board.
- M2M / IoT Nano-SIM Card
  - Supplier: Things Mobile
  - Role: Provides LTE Cat-M1 network access. Note: Standard consumer phone SIMs 
    often block NB-IoT/Cat-M1 bands; an M2M-specific provider is recommended.
  - The Things Mobile SIM is particularly useful as the data you pay for does not 
    expire (the SIM needs to be connected to the network once every 24 months). 
    Data costs E0.10/Mb as at Sept 2026
- Temperature and humidity sensors
  - 5 × DHT22 / AM2302 Temperature & Humidity Sensors
  - 5 x DHT22 enclosures
  - Supplier: ebay
  - Details: Digital single-wire protocol, reliable up to 10m with 5 × 2.2kΩ pull-up 
    resistors on the data lines.

## Power Control & Isolation Circuit (For True 0 µA Sleep)

Cellular modems draw short current spikes (up to 1-2A) during network transmission; 
powering the modem directly from the Nucleo board's internal 3.3V rail will cause 
brownout resets. Ensure the 5V source feeds the modem rail through a MOSFET. 
This also ensures the modem draws no current when inactive, using a power-gate 
circuit to disconnect its V_CC rail via an STM32 GPIO:

- P-Channel Power MOSFET (High-Side Switch)
  - Part Suggestion: NDP6020P or AO3401A (Logic Level P-FET with low R_DS(on) ).
  - Supplier: RS Components or Farnell.
  - Role: Sits on the positive power rail feeding the SIM7080G to completely 
    cut V_CC during deep sleep.
- NPN Transistor / N-Channel MOSFET (Gate Driver)
  - Part Suggestion: 2N3904 or 2N7000.
  - Supplier: RS Components, Farnell, or Rapid Electronics.
  - Role: Driven directly by the 3.3V GPIO on the Nucleo to pull the P-FET 
    gate down to GND to turn power ON.
- Resistors (Quarter-Watt Through-Hole or 0805 SMD)
  - 1×10 "k"Ω Pull-up resistor (holds P-FET Gate to 5V when inactive).
  - 1×1 "k"Ω Base resistor (limits current from STM32 GPIO pin).
  - Supplier: RS, Farnell, or Rapid Electronics.

## Low power notes

The Nucleo-L432KC board includes an onboard debugger. This consumes significant 
power during sleep mode (it does not sleep). Once development has been completed, 
the debugger can be disabled:

- The Nucleo-L432KC board is physically designed with a perforated breakaway line 
  separating the ST-LINK programmer section from the target MCU section.
- By snapping off the ST-LINK portion along the PCB score line, you isolate the
  bare STM32L432KC target board.
- To program it afterward, you simply connect 4 wires (3V3, GND, SWCLK, SWDIO)
  from the snap-off ST-LINK header (or an external ST-LINK / J-Link probe) to
  the SWD pins on the remaining target board.
- If you don't want to snap the board, you can remove specific solder bridges
  (like SB9 / SB14 depending on board revision) or desolder the power jumper to 
  isolate the ST-LINK rail from the target MCU rail during current measurements.

Power for the SIMCom M2M board also needs to be managed. To keep power consumption 
near zero during sleep, combine modem PSM mode with hardware power-gating:

- Active Transmission: Wake the STM32L432KC, bring the modem UART high, execute
  AT commands to negotiate TLS and publish MQTT payload (~20–60 mA peak during 
  transmission).
- Modem Soft Power-Down: Send AT+CPOWD=1 to allow the modem to cleanly disconnect
  from the tower and enter low-power shutdown (~3.2 µA).
- Hard Cutoff (Optional): Drive a P-Channel MOSFET from an STM32 GPIO pin to
  disconnect the 5V power line feeding the cellular breakout completely, bringing 
  modem rail leakage down to 0 µA.

## Nucleo-L432KV Pin Allocations

TODO: Update from schematic
TODO: check pin usage with AI
TODO: possible fault in schematic - check Nucleo pin out on schematic against documentation (especially end of CN4)
- Analog Input Lines (3 required)
  - PA0 (Arduino A0 / CN4 Pin 5)
  - PA1 (Arduino A1 / CN4 Pin 6)
  - PA3 (Arduino A2 / CN4 Pin 7) - TODO: !DUPLICATED with modem!
- Digital Input Lines (5 required)
  - PB0 (Arduino D3 / CN3 Pin 6)
  - PB1 (Arduino D6 / CN3 Pin 9)
  - PC14 (Arduino D7 / CN3 Pin 10)
  - PC15 (Arduino D8 / CN3 Pin 11)
  - PA8 (Arduino D9 / CN3 Pin 12)
- N-FET Gate Control Output (1 line)
  - PA12 (Arduino D2 / CN3 Pin 5)
- Clipper LTE Modem on USART1 (USART2 is the ST-LINK)
  - PA9 - Modem Rx
  - PA10 - Modem Tx
  - PA8 - PWRKEY power control of the modem board
  - PA11 - RESET the modem board
- Unused Header Pins Leftover for Other Functions
  - I2C1 Bus: PB6 (D5) and PB7 (D4) remain open if you ever need an external
    display, RTC, or sensors.
  - SPI1 Bus: PA11 (D10), PB5 (D11), and PB4 (D12) remain available for external
    SPI flash or display modules.
  - UART Serial Debugging: PA9 (D1) and PA10 (D0) remain free.

Connections from Clipper (on the left) to Nucleo-L432KC (on the right):
Vin -> 5V
VDDIO -> 3.3V
Tx -> PA10
Netlight -> Not connected
RESET -> PA11
Rx -> PA9
PWRKEY -> PA8
Gnd -> Gnd

## Moving from development to production

Power-On Sequence: When debugging via USB, turn on your external 12V supply first
so the +5V rail is active before plugging the USB cable into your PC.

Standalone Operation: When you plan to run the (Nucleo) board from the 5V supply 
without a USB cable connected, remove/open solder bridge SB9 to prevent the 
unpowered ST-LINK from holding the main MCU in reset.
