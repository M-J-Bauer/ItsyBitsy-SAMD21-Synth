# ItsyBitsy M0 (SAMD21) 'Sigma-6' Sound Synthesizer
DIY MIDI Sound Module using additive synthesis technique runs on a 32-bit MCU.

![ItsyBitsy_synth_PCB_assy_web](https://github.com/user-attachments/assets/9bf93723-6282-443f-87af-81abd5dccede)

For details of concept, design, construction and operation, please refer to the project web page, here...  
https://www.mjbauer.biz/Sigma6_M0_synth_weblog.htm

This repo contains an Arduino "sketch" (program) which runs on a platform based on the Adafruit "ItsyBitsy M0 Express"
(ATSAMD21) MCU board.  The same firmware should run on other MCU boards based on the ATSAMD21G18 MCU device, 
e.g. Arduino Zero and the Chinese 'RobotDyn' SAMD21 M0-Mini.

![Robotdyn_SAMD21_M0_Mini_pic](https://github.com/user-attachments/assets/bd78c449-bc02-46e2-9ee0-2412878ac6a5)

NB: One batch (maybe all) of the 'RobotDyn' SAMD21 M0-Mini board has digital pins D2 and D4 reversed. 
(See https://github.com/BLavery/SAMD21-M0-Mini)  The schematic is wrong.
A work-around for this incompatibility is embodied in the firmware (v1.4++).

The 'Sigma-6' synth application was developed originally on a PIC32MX platform using Microchip MPLAB.X IDE.
Since it is now difficult to obtain a breakout module or proto board based on the PIC32MX family,
I have migrated the application to a better supported, readily available 32-bit MCU device, 
i.e. ATSAMD21, as in Arduino Zero, Adafruit SAMD21 M0 Express and compatibles. 

Although I'm not a big fan of Arduino IDE - it doesn't support modular software design - 
I am aware it has become the development environment of choice for the majority of hobbyists.
Further, I must admit that Arduino facilitates quick and easy firmware development.

For anyone still interested in running the Sigma-6 "Mother" application on a PIC32MX platform, details of a suitable hardware
design can be found on my website: www.mjbauer.biz.  The PIC32 program code is available on request.
