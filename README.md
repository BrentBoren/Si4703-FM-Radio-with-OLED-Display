# Si4703-FM-Radio-with-OLED-Display 
 FM Radio using SFE_MicroOLED Library, [Si4703_Breakout_Modified](https://github.com/2BTechnolgy/Si4703_Breakout_Modified.git)
  library, and elapsedMillis library
 
  Brent Boren
 
  Original Creation Date: September 27, 2015
 
 
  Development environment specifics:
 
   Arduino 1.0.6
 
   Arduino Pro 3.3V, 8 MHz
 
   Micro OLED Breakout v1.0
 
   Si4703 FM Tuner Evaluation Board
 
   Si4703_Breakout.cpp and Si4703_Breakout.h files modified and included
 
 
  Hardware Specifics:
 
  Micro OLED Breakout connected via SPI as in the 
 
    SparkFun OLED Breakout Hookup Guide (using same pinout as SparkFun example)
 
  Si4703 FM Tuner Evaluation Boards connected via I2C as in the
 
    SparkFun Si4703 FM Radio Receiver Hookup Guide (using same pinout as example)
 
  Three pushbutton switches (or one combo switch) connected to pins listed below
 
    controlling up, down, and select functions (up = 4, down = 5, select = 6)
 
    with the proper pullups, etc.
  
 
  This code is freeware, and includes the SparkFun beerware code; if 
 
  you see the SparkFun folks at the local, and you've found their code
 
  helpful, please buy them a round!
  
 
  Distributed as-is; no warranty is given.
 
  To Do
   1. Check in Si4703_Breakout.cpp and Si4703_Breakout.h changes
    to the SparkFun GitHub repository with the changes and a working example 
    (changes break Si4703 example in readRDS section of code).
   2. Integrate rotary encoder and new control state to allow "dial" tuning
    similar to a car stereo.
  
