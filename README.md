# ItsyBitsy M0 (SAMD21) 'Sigma-6' Sound Synthesizer
DIY MIDI Sound Module using additive synthesis technique runs on a 32-bit MCU.

![ItsyBitsy_synth_PCB_assy_web](https://github.com/user-attachments/assets/9bf93723-6282-443f-87af-81abd5dccede)

For details of concept, design, construction and operation, please refer to the project web page, here...  
https://www.mjbauer.biz/Sigma6_M0_synth_weblog.htm

This repo contains an Arduino "sketch" (program) which runs on a platform based on the Adafruit "ItsyBitsy M0 Express"
(ATSAMD21) MCU board.  The same firmware should run on other MCU boards based on the ATSAMD21G18 MCU device, 
e.g. Arduino Zero and the Chinese 'RobotDyn' SAMD21 M0-Mini pictured here...

![Robotdyn_SAMD21_M0_Mini_pic](https://github.com/user-attachments/assets/bd78c449-bc02-46e2-9ee0-2412878ac6a5)

NB: The Robotdyn SAMD21 M0-Mini board has digital pins D2 and D4 reversed, compared with
the Adafruit M0 Express and Arduino Zero boards. The Robotdyn M0-mini I/O pin assignments follow the Arduino M0 board,
which is not the same as the Arduino Zero. I'm guessing the M0 design engineer made a mistake and Robotdyn copied it.
(For all the sordid details, see https://github.com/BLavery/SAMD21-M0-Mini) 
Anyhow, a simple work-around for this incompatibility is embodied in the Sigma-6 synth firmware.

Although I'm not a big fan of Arduino IDE - it doesn't support modular software design - 
I am aware it has become the development environment of choice for the majority of hobbyists.
Further, I must admit that Arduino facilitates quick and easy firmware development.
