# ItsyBitsy M0 (SAMD21) 'Sigma-6' Sound Synthesizer
DIY MIDI Sound Synth using additive synthesis technique runs on a 32-bit MCU.
![ItsyBitsy_Synth_Dev_Platform_web](https://github.com/user-attachments/assets/c7aa4c0c-5321-45ef-8a1e-6c454ede4888)
For details of concept, design and operation, please refer to the project web page, here...  
https://www.mjbauer.biz/Sigma6_M0_synth_weblog.htm

This repo contains an Arduino "sketch" (program) which runs on a synth module based on the Adafruit "ItsyBitsy M0 Express"
(ATSAMD21) MCU board.  The same firmware should run on compatible MCU boards, e.g. the Chinese 'RobotDyn' SAMD21 M0-Mini.

The 'Sigma-6'synth application was developed originally on a PIC32MX platform using Microchip MPLAB.X IDE.  
Since it is now difficult to obtain a breakout module or proto board based on the PIC32MX family,
I have migrated the application to a better supported, readily available 32-bit MCU device, 
i.e. ATSAMD21, as in Arduino Zero, Adafruit SAMD21 "Itsy-Bitsy M0 Express" boards and compatibles. 

Although I'm not a big fan of Arduino IDE - it doesn't support modular software design - 
I am aware it has become the development environment of choice for the majority of hobbyists.
Further, I must admit that Arduino facilitates quick and easy firmware development.

For anyone still interested in running the "full-featured" Sigma-6 app on a PIC32MX platform, details of a suitable hardware
design can be found on my website: www.mjbauer.biz.  The PIC32 program code is available on request.
