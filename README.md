# ItsyBitsy M0 (SAMD21) 'Sigma-6' Sound Synthesizer
DIY MIDI Sound Module using additive synthesis technique runs on a 32-bit MCU.

![ItsyBitsy_synth_PCB_assy_web](https://github.com/user-attachments/assets/9bf93723-6282-443f-87af-81abd5dccede)

For details of concept, design, construction and operation, please refer to the project web page, here...  
https://www.mjbauer.biz/Sigma6_M0_synth_weblog.htm

This repo contains an Arduino "sketch" (program) which runs on a platform based on the Adafruit "ItsyBitsy M0 Express"
(ATSAMD21) MCU board.  The same firmware should run on other MCU boards based on the ATSAMD21G18 MCU device, 
e.g. Arduino Zero and the Chinese 'RobotDyn' SAMD21 M0-Mini pictured here...

![Robotdyn_SAMD21_M0_Mini_pic](https://github.com/user-attachments/assets/bd78c449-bc02-46e2-9ee0-2412878ac6a5)

# Firmware  Installation

Download and install the latest version of Arduino IDE and follow the instructions here:

[Arduino IDE Setup](https://learn.adafruit.com/introducing-itsy-bitsy-m0/setup)

Regardless of the board type connected, Adafruit or Robotdyn, select "Adafruit ItsyBitsy M0 Express".
(Don't choose "Arduino Zero" or any other Arduino board!)

To build the ItsyBitsy M0 synth firmware, you also need to install a "fast timer" library in the Arduino IDE.
Open the Arduino Library Manager. In the box labeled 'Type', choose 'Installed' from the drop-down list. 
Your Arduino IDE window should show this information:
![Screenshot_fast_timer_library_instal](https://github.com/user-attachments/assets/d96e22b1-42b5-49f7-82b0-d8f763630378)

Steps to build and "upload" the firmware:

    > Download the Sigma-6 source files from the repository here.
    > Create a project folder in your computer local drive named "ItsyBitsy_M0_synth".
    > Copy the downloaded source files into the project folder.
    > Double-click on the file "ItsyBitsy_M0_synth.ino" -- this should open Arduino IDE and load
      all source files into the editor window. (Alternatively, open Arduino IDE first, then open the
      source file "ItsyBitsy_M0_synth.ino".)
    > Make any required changes to the source code to suit your preferences.*
    > Compile the code and upload the firmware to your Sigma-6 module(s).^

* If you are using the RobotDyn 'SAMD21-M0-Mini' board, edit the #define on line 13 in the header file "M0-synth-def.h".
  (The Robo M0-Mini board is not fully I/O compatible with the Adafruit M0.)

^ Sometimes it is necessary to retry an upload more than once, or to select (again) the board COM port, 
  and/or to "double click" the MCU reset button to enter the bootloader. This is "normal" for Arduino!
  
