# ItsyBitsy M0 (SAMD21) 'Sigma-6' Sound Synthesizer
DIY MIDI Sound Module using additive synthesis technique runs on a 32-bit MCU.

![ItsyBitsy_synth_PCB_assy_web](https://github.com/user-attachments/assets/9bf93723-6282-443f-87af-81abd5dccede)

For details of concept, design, construction and operation, please refer to the project web page, here...  
https://www.mjbauer.biz/Sigma6_M0_synth_weblog.htm

# Firmware  Installation

Download and install the latest version of Arduino IDE on your PC and follow the instructions provided by Adafruit, here:

[Arduino IDE Setup](https://learn.adafruit.com/introducing-itsy-bitsy-m0/setup)

Install the Adafruit SAMDxx Boards Manager. When you connect the board to your computer, Arduino IDE usually determines the board type
automatically, but in any case be sure to select the correct board type.

If your synth module uses the Adafruit ItsyBitsy M0 Express board, select board type: 'Adafruit ItsyBitsy M0 Express' (of course).

If your synth module uses the RobotDyn SAMD21 M0 Mini board, select board type: 'Arduino Zero (Native USB)' for the firmware build.
This is preferable to the 'Adafruit ItsyBitsy M0 Express' board package, because the Arduino Zero startup code will enable
the 32.768kHz crystal oscillator for the MCU system clock. The Adafruit ItsyBitsy M0 board uses the MCU internal 8MHz RC oscillator 
which is not as precise or stable.

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
    > Edit any applicable #define lines in the file "M0-synth-def.h" (see comments therein).
    > Connect the MCU board to a USB port on your computer.
    > Select the applicable board type (as noted above), if not already selected.
    > Compile the code and upload the firmware to your Sigma-6 synth module(s).

NB: It may be necessary to reset the MCU and/or unplug and reconnect the USB cable to get the bootloader to start, 
after which it may also be necessary to re-select the board type and/or USB-serial port in the drop-down box in Arduino IDE.
This is "normal" for Arduino!

![Coded-by-a-Human-no-AI](https://github.com/user-attachments/assets/e2479440-66a7-4c50-b6dd-358b75b23dfc)


