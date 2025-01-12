# Sigma-6 Sound Synthesizer
MIDI Sound Synth app using Additive Synthesis technique runs on a 32-bit MCU.

For details of concept, design and operation, please refer to the PDF doc. But, please keep in mind
this document applies to a variant of the synth without the user interface (LCD screen and control panel).
Details of the hardware platform and synth operation will be posted shortly.

This repo contains an Arduino "sketch" (program) which runs on a synth module based on the Adafruit "ItsyBitsy M0 Express"
(ATSAMD21) MCU board.  The same firmware runs on compatible MCU boards, e.g. the Chinese 'RobotDyn' SAMD21 M0-mini.

The synth application was developed originally on a PIC32MX platform using Microchip MPLAB.X IDE.  
Since it is now difficult to obtain a breakout module or proto board based on the PIC32MX family,
I have migrated the application to a better supported, readily available 32-bit MCU device, 
i.e. ATSAMD21, as in Arduino Zero, Adafruit SAMD21 "Feather" & "Itsy-Bitsy" M0 boards and compatibles. 

Although I'm not a big fan of Arduino IDE - it doesn't support modular software design - 
I am aware it has become the development environment of choice for the majority of hobbyists.
Further, I must admit that Arduino facilitates quick and easy firmware development.

For anyone still interested in running the Sigma-6 app on a PIC32MX platform, details of a suitable hardware
design can be found on my website: www.mjbauer.biz.  The program code is available on request.
