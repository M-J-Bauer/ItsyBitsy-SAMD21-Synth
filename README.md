# ItsyBitsy M0 (SAMD21) 'Sigma-6' Sound Synthesizer
DIY MIDI Sound Module using additive synthesis technique runs on a 32-bit MCU.

![ItsyBitsy_synth_PCB_assy_web](https://github.com/user-attachments/assets/9bf93723-6282-443f-87af-81abd5dccede)

For details of concept, design, construction and operation, please refer to the project web page, here...  
https://www.mjbauer.biz/Sigma6_M0_synth_weblog.htm

# Firmware  Installation

Download and install the latest version of Arduino IDE on your PC and follow the instructions provided by Adafruit, here:

[Arduino IDE Setup](https://learn.adafruit.com/introducing-itsy-bitsy-m0/setup)

Regardless of the board type you're using, Adafruit or Robotdyn, install the Adafruit SAMDxx Boards Manager. When you connect
the board to your computer, Arduino may determine the board type automatically, but in any case be sure to select board
type 'Adafruit ItsyBitsy M0 Express (SAMD21)' to ensure that the firmware will compile with the correct library functions.

To build the ItsyBitsy M0 synth firmware, you also need to install a "fast timer" library in the Arduino IDE.
Open the Arduino Library Manager. From the 'Type' drop-down list, choose 'All'. In the 'Filter' box, write "fast_samd21_tc"
and click 'INSTALL'. Then choose Type = "Installed". The library should then appear in your Arduino IDE as in this screen-shot:

![Screenshot_SAMD21_fast_timer_library](https://github.com/user-attachments/assets/398ecf9a-11e7-4b22-b53f-e896f9cf998e)

Steps to compile and "upload" the firmware:

    > Download the Sigma-6 source files from the repository here.
    > Create a project folder in your computer local drive named "ItsyBitsy_M0_synth".
    > Copy the downloaded source files into the project folder.
    > Double-click on the file "ItsyBitsy_M0_synth.ino" -- this should open Arduino IDE and load
      all source files into the editor window. (Alternatively, open Arduino IDE first, then open the
      source file "ItsyBitsy_M0_synth.ino".)
    > Make any required changes to the source code to suit your configuration.*
    > Compile the code and upload the firmware to your Sigma-6 module(s).^

* Edit any applicable #define lines in the header file "M0-synth-def.h" (see comments therein).
  (The 'Robotdyn' M0-Mini board is not fully I/O compatible with the Adafruit M0 or Arduino Zero.)

^ Sometimes it is necessary to retry an upload more than once, or to select (again) the board COM port, 
  and/or to "double click" the MCU reset button to enter the bootloader. This is "normal" for Arduino!

![Coded-by-a-Human-no-AI](https://github.com/user-attachments/assets/e2479440-66a7-4c50-b6dd-358b75b23dfc)


