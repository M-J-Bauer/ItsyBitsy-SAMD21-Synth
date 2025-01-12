# Sigma-6 Sound Synthesizer
MIDI Sound Synth app using Additive Synthesis technique runs on a 32-bit MCU.

For details of concept, design and operation, please refer to the PDF doc.

Note: This repo contains an Arduino "sketch" (program) which runs on a synth module based on the "ItsyBitsy M0 Express"
(ATSAMD21) MCU board from Adafruit.  It also runs on compatible MCU boards, e.g. the Chinese 'RobotDyn' SAMD21 M0-MINI.

The application was developed originally on a PIC32MX platform using Microchip MPLAB.X IDE.  
Since it is now difficult to obtain a breakout module or proto board based on the PIC32MX family,
I plan to migrate the Sigma-6 software to a better supported, readily available 32-bit MCU device, 
e.g. ATSAMD21 (as in Arduino Zero, Adafruit SAMD21 "Feather" & "Itsy-Bitsy" M0 boards), or STM32 (as in the "Blue Pill" board). 

Although I'm not a big fan of Arduino IDE - it doesn't support modular software design - 
I am aware it has become the development environment of choice for the majority of hobbyists, etc,
so I plan to develop future variants of the Sigma-6 under Arduino.

For anyone still interested in running the Sigma-6 app on a PIC32MX platform, details of a suitable hardware
design can be found on my website: www.mjbauer.biz.  The program code is available on request.
