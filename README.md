# ItsyBitsy M0 (SAMD21) 'Sigma-6' Sound Synthesizer
MIDI Sound Synth app using Additive Synthesis technique runs on a 32-bit MCU.

![ItsyBitsy M0 Synth proto v1 web-pic](https://github.com/user-attachments/assets/1d608cde-76c9-4658-8a4e-76bf738acc02)

NB: This is a preliminary version with minimal functionality for development on the experimaental proto board (pictured). 
A future revision will support a user-interface (1.3 inch OLED display, buttons, 'data entry' pot) and voltage control of
oscillator pitch, modulation, etc, using ADC inputs instead of MIDI... (jumper-selectable option).

For details of concept, design and operation, please refer to the project web page, here...  
https://www.mjbauer.biz/Sigma6_M0_synth_weblog.htm

This repo contains an Arduino "sketch" (program) which runs on a synth module based on the Adafruit "ItsyBitsy M0 Express"
(ATSAMD21) MCU board.  The same firmware should run on compatible MCU boards, e.g. the Chinese 'RobotDyn' SAMD21 M0-Mini.

The synth application was developed originally on a PIC32MX platform using Microchip MPLAB.X IDE.  
Since it is now difficult to obtain a breakout module or proto board based on the PIC32MX family,
I have migrated the application to a better supported, readily available 32-bit MCU device, 
i.e. ATSAMD21, as in Arduino Zero, Adafruit SAMD21 "Itsy-Bitsy M0 Express" boards and compatibles. 

Although I'm not a big fan of Arduino IDE - it doesn't support modular software design - 
I am aware it has become the development environment of choice for the majority of hobbyists.
Further, I must admit that Arduino facilitates quick and easy firmware development.

For anyone still interested in running the Sigma-6 app on a PIC32MX platform, details of a suitable hardware
design can be found on my website: www.mjbauer.biz.  The program code is available on request.
