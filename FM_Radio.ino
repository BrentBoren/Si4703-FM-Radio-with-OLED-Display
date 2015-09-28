/****************************************************************
 * FM_Radio.ino
 * FM Radio using SFE_MicroOLED Library, SI4703_Breakout(modified)
 * library, and elapsedMillis library
 * Brent Boren @ 2B Technology
 * Original Creation Date: September 27, 2015
 *
 * To Do:
 *  1. Check in Si4703_Breakout.cpp and Si4703_Breakout.h changes
 *   to the SparkFun GitHub repository with the changes and a working example 
 *   (changes break Si4703 example in readRDS section of code).
 *  2. Integrate rotary encoder and new control state to allow "dial" tuning
 *   similar to a car stereo.
 * 
 * Development environment specifics:
 *  Arduino 1.0.6
 *  Arduino Pro 3.3V, 8 MHz
 *  Micro OLED Breakout v1.0
 *  Si4703 FM Tuner Evaluation Board
 *  Si4703_Breakout.cpp and Si4703_Breakout.h files modified and included
 *
 * Hardware Specifics:
 * Micro OLED Breakout connected via SPI as in the 
 *   SparkFun OLED Breakout Hookup Guide (using same pinout as SparkFun example)
 * Si4703 FM Tuner Evaluation Boards connected via I2C as in the
 *   SparkFun Si4703 FM Radio Receiver Hookup Guide (using same pinout as example)
 * Three pushbutton switches (or one combo switch) connected to pins listed below
 *   controlling up, down, and select functions (up = 4, down = 5, select = 6)
 *   with the proper pullups, etc.
 * 
 * This code is freeware, and includes the SparkFun beerware code; if 
 * you see the SparkFun folks at the local, and you've found their code
 * helpful, please buy them a round!
 * 
 * Distributed as-is; no warranty is given.
 ***************************************************************/
#include <avr/eeprom.h>
#include <elapsedMillis.h>
#include <MemoryFree.h>
#include <Wire.h>
#include <SPI.h>  // Include SPI if you're using SPI
#include <SFE_MicroOLED.h>  // Include the SFE_MicroOLED library
#include <Si4703_Breakout_Modified.h>

///////////////////////////////
// Si4703 Pin Definitions /////
///////////////////////////////
#define resetPin 2
#define SDIO A4
#define SCLK A5

////////////////////////////////
// MicroOLED Pin Definitions ///
////////////////////////////////
#define PIN_RESET 9   // Connect RST to pin 9
#define PIN_DC    8   // Connect DC to pin 8
#define PIN_CS    10  // Connect CS to pin 10
#define DC_JUMPER 0

//////////////////////////////////
// Switch Input Pin Definitions //
//////////////////////////////////
#define upPin 4      // Button for Up function
#define downPin 5    // Button for Down function
#define selectPin 6  // Button for Select function

//////////////////////
//RDS specific stuff//
//////////////////////
char rdsname[9];
char rdsrt[65];
#define RDStimeout 5000        // used to give up trying to display RDS
elapsedMillis RDStimer;        // and clear the display after switching channels

//////////////////////////////////
// Si4703 Object Declaration /////
//////////////////////////////////
Si4703_Breakout radio(resetPin, SDIO, SCLK);
int controlState;

/////////////////////////////////
// Si4703 Radio preset list /////
/////////////////////////////////
// station presets currently loaded with a few Denver stations.
//  to add new (or different) presets, enter the integer value 
//  of the desired frequency in MHz multiplied by 10.
//  As an example: for 88.1 MHZ you would enter 881 as the preset
//  Be sure to adjust the array size [4] to the appropriate number -
//  the default (presetLength = 4) represents the four default presets here.
const int presetLength = 4;
int presetList[presetLength] = 
{
  973,
  1035,
  1051,
  1059,
};

//////////////////////////////////
// MicroOLED Object Declaration //
//////////////////////////////////
MicroOLED oled(PIN_RESET, PIN_DC, PIN_CS); // SPI declaration
//MicroOLED oled(PIN_RESET, DC_JUMPER);    // I2C declaration

//////////////////////////////////
// Switch Variable Declarations //
//////////////////////////////////
int up;                           // Current state of up button
int down;                         // Current state of down button
int select;                       // Current state of select button
bool upPressed = false;           // Up button pressed flag for debounce & repeats
bool downPressed = false;         // Down button pressed flag for debounce & repeats
bool selPressed = false;          // Select pressed flag for debounce & repeats
elapsedMillis upTimer;            // Time between up button presses
elapsedMillis downTimer;          // Time between down button presses
elapsedMillis selTimer;           // Time between select button presses
#define buttonDelay 1000          // Delay between allowable up presses/repeat rate
#define selDelay 500              // Delay between allowable up presses/repeat rate

////////////////////////////////////////
// Display Timer / EEPROM write setup //
////////////////////////////////////////
elapsedMillis displayTimer;       // display time variable
#define displayDelay 1000         // display delay time

//////////////////////////////////////////////////////////////////////////
// Set up EEPROM to store channel & volume setting between power cycles //
//////////////////////////////////////////////////////////////////////////
int addr = 0;                     // starting address to store variables
int channel;                      // variable to hold radio frequency (x10)
int volume;                       // variable to hold volume setting
int preset;                       // variable holding current preset pointer
bool RDSverbose = false;          // RDS single-line (false) or full screen (true)
bool updateEeprom = false;        // update EEPROM flag

void setup()
{
///////////////////////////////////////////////////////////////
// retrieve settings from EEPROM, initializing if necessary  //
///////////////////////////////////////////////////////////////
// eeprom setup to save channel & volume between power cycles
// this initialization needs to be above the si4703 setup so we can
// load the saved channel and volume level on startup.
//  while(!eeprom_is_ready());   // loop here until eeprom is ready
  cli();    // disable interupts before accesing eeprom
  channel = eeprom_read_word((uint16_t*)addr);
  volume = eeprom_read_word((uint16_t*)addr+4);
  preset = eeprom_read_word((uint16_t*)addr+8);
  RDSverbose = eeprom_read_word((uint16_t*)addr+12);
  if(channel < 0){
    //bottom frequency for US mode - change appropriately for other bands
    // as per presets, channel = desired frequency X 10 (should be integer)
    channel = 8750;        
    eeprom_write_word((uint16_t*)addr, channel);
  }
  if(volume < 0){
    // if volume has not been saved yet, put in 6 as a good starting point
    volume = 6;
    eeprom_write_word((uint16_t*)addr+4, volume);
  }
  if(preset < 0){
    // if preset has not been saved yet set it to 0 to start
    preset = 0;
    eeprom_write_word((uint16_t*)addr+8, preset);
  }
  if(RDSverbose < 0){
    // if RDSverbose has not been saved yet set it to 0 to start
    RDSverbose = 0;
    eeprom_write_word((uint16_t*)addr+12, RDSverbose);
  }
  sei();  // reenable interupts

/////////////////
//  oled setup //
/////////////////
  oled.begin();    // Initialize the OLED
  oled.clear(ALL); // Clear the display's internal memory
  oled.clear(PAGE); // Clear the buffer.
///////////////////////////////
// si4703 setup for fm radio //
///////////////////////////////
// uncomment these to force channel and volume settings if EEPROM gets messed up
//  channel = 973;     // or whatever you want to force it to
//  volume = 6;        // same for volume...
  radio.powerOn();
  radio.setVolume(volume);
  radio.setChannel(channel);
//////////////////  
// button setup //
//////////////////
  pinMode(upPin, INPUT);
  pinMode(downPin, INPUT);
  pinMode(selectPin, INPUT);
  controlState = 0;    // always set initial control state to volume mode
  displayTimer = 0;    // reset all of the timers
  RDStimer = 0;
  upTimer = 0;
  downTimer = 0;
  selTimer = 0;
}

void loop()
{
// read pins to determine button presses  
  up = digitalRead(upPin);
  down = digitalRead(downPin);
  select = digitalRead(selectPin);
  if((up == HIGH)&&!(upPressed)){
    upPressed = true;
    downPressed = false;
    selPressed = false;
    upTimer = 0;
    if(controlState == 0){
      // volume mode - adjust volume to next lower value      
      volume ++;
      if (volume == 16) volume = 15;
      radio.setVolume(volume);
      // print title and volume level
      oled.clear(PAGE);
      oled.setFontType(0);
      oled.setCursor(0,0);
      oled.print(F("Volume Up"));
      oled.setCursor(0,20);
      oled.setFontType(2);
      oled.print(volume);
      oled.display();
      updateEeprom = true;
 //     delay(50);
      displayTimer = 0;
    }
    else if(controlState == 1){
      // seek mode - seek up to next "strong" frequency      
      channel = radio.seekUp();
      // print title and volume level
      oled.clear(PAGE);
      oled.setFontType(0);
      oled.setCursor(0,0);
      oled.print(F("Seek Up"));
      oled.setCursor(0,20);
      oled.setFontType(2);
      // prevent rollover issue where channel returned is zero at end of band
      while (channel == 0){
        channel = radio.seekUp();
      }
      printFreq();
      updateEeprom = true;
      oled.display();
 //     delay(50);
      displayTimer = 0;
    }
    else if(controlState == 2){
      // preset mode - choose next higher preset channel
      ++preset;
      if(preset >= presetLength)
        preset = 0;
      channel = presetList[preset];
      radio.setChannel(channel);
      // print title and channel
      oled.clear(PAGE);
      oled.setFontType(0);
      oled.setCursor(0,0);
      oled.print(F("Preset Up"));
      oled.setCursor(0,20);
      oled.setFontType(2);
      printFreq();
      oled.display();
      updateEeprom = true;
 //     delay(50);
      displayTimer = 0;
    }
    else {
      // RDS mode - toggle RDS display mode      
      if(RDSverbose == true)
        RDSverbose = false;
      else if(RDSverbose == false)
        RDSverbose = true;
      oled.clear(PAGE);
      oled.setFontType(0);
      oled.setCursor(0,0);
      oled.print(F("RDS Mode:"));
      oled.setCursor(0,10);
      if(RDSverbose == true)
        oled.print(F("Screen"));
      if(RDSverbose == false)
        oled.print(F("Scroll"));
      printFreq();
      updateEeprom = true;
 //     delay(50);
      displayTimer = 0;
      }
  }

  if((down == HIGH)&&!(downPressed)){
    downPressed = true;
    upPressed = false;
    selPressed = false;
    downTimer = 0;
    if(controlState == 0){
      // volume mode - adjust volume to next lower value      
      volume --;
      if (volume < 0) volume = 0;
      radio.setVolume(volume);
      // print title and volume level
      oled.clear(PAGE);
      oled.setFontType(0);
      oled.setCursor(0,0);
      oled.print(F("VolumeDown"));
      oled.setCursor(0,20);
      oled.setFontType(2);
      oled.print(volume);
      oled.display();
      updateEeprom = true;
//      delay(50);
      displayTimer = 0;
    }
    else if(controlState == 1){
      //seek mode - seek down to next "strong" frequency      
      channel = radio.seekDown();
      // print title and volume level
      oled.clear(PAGE);
      oled.setFontType(0);
      oled.setCursor(0,0);
      oled.print(F("Seek Down"));
      oled.setCursor(0,20);
      oled.setFontType(2);
      // prevent rollover issue where channel returned is zero at end of band
      while (channel == 0){
        channel = radio.seekDown();
      }
      printFreq();
      updateEeprom = true;
      oled.display();
 //     delay(50);
      displayTimer = 0;
    }
    else if(controlState == 2){
      // preset mode - choose next lower preset channel      
      --preset;
      if(preset < 0)
        preset = presetLength - 1;
      channel = presetList[preset];
      radio.setChannel(channel);
      // print title and volume level
      oled.clear(PAGE);
      oled.setFontType(0);
      oled.setCursor(0,0);
      oled.print(F("PresetDown"));
      oled.setCursor(0,20);
      oled.setFontType(2);
      printFreq();
      oled.display();
      updateEeprom = true;
 //     delay(50);
      displayTimer = 0;
    }
    else {
      // RDS mode - toggle RDS display mode      
      if(RDSverbose == true)
        RDSverbose = false;
      else if(RDSverbose == false)
        RDSverbose = true;
      oled.clear(PAGE);
      oled.setFontType(0);
      oled.setCursor(0,0);
      oled.print(F("RDS Mode:"));
      oled.setCursor(0,10);
      if(RDSverbose == true)
        oled.print(F("Screen"));
      if(RDSverbose == false)
        oled.print(F("Scroll"));
      printFreq();
      updateEeprom = true;
//      delay(50);
      displayTimer = 0;
    }
  }
  
  if((select == HIGH)&&!(selPressed)){
      // select pushbutton used to switch to different control states    
      selPressed = true;
      upPressed = false;
      downPressed = false;
      selTimer = 0;
      controlState ++;
      if (controlState > 3) 
        controlState = 0;    // roll over control state > 3
      if(controlState == 0){
        // first control state is for setting volume      
        // print title and volume level
        oled.clear(PAGE);
        oled.setFontType(0);
        oled.setCursor(0,0);
        oled.print(F("Set"));
        oled.setCursor(0,10);
        oled.print(F("Volume"));
        oled.setCursor(0,20);
        oled.setFontType(2);
        oled.print(volume);
        oled.display();
 //       delay(50);
        displayTimer = 0;
      }
      if(controlState == 1){
        // second control state for seeking      
        // print title and channel
        oled.clear(PAGE);
        oled.setFontType(0);
        oled.setCursor(0,0);
        oled.print(F("Seek"));
        oled.setCursor(0,20);
        oled.setFontType(2);
        printFreq();
        oled.display();
 //       delay(50);
        displayTimer = 0;
      }
      if(controlState == 2){
        // third control state for toggling between preset stations      
        // print title and channel
        oled.clear(PAGE);
        oled.setFontType(0);
        oled.setCursor(0,0);
        oled.print(F("Select"));
        oled.setCursor(0,10);
        oled.print(F("Preset"));
        oled.setCursor(0,20);
        oled.setFontType(2);
        printFreq();
        oled.display();
 //       delay(50);
        displayTimer = 0;
      }
      if(controlState == 3){
        // fourth control state toggles between single line and multi line RDS displays      
        // print title and RDS mode
        oled.clear(PAGE);
        oled.setFontType(0);
        oled.setCursor(0,0);
        oled.print(F("RDS Mode"));
        oled.setCursor(0,10);
        oled.setFontType(0);
        if(RDSverbose == true)
          oled.print(F("Screen"));
        else{
          oled.print(F("Scroll"));
        }
        printFreq();
        oled.display();
 //       delay(50);
        displayTimer = 0;
      }
    }
    
    // check and clear button delay flags after buttonDelay time
    if(upTimer > buttonDelay)
      upPressed = false;
    if(downTimer > buttonDelay)
      downPressed = false;
    if(selTimer > selDelay)
      selPressed = false;

    // allow recently displayed modes and settings to remain visible for
    // displayDelay milliseconds before clearing the display and displaying
    // RDS text (if available) or Frequency (if RDS unavailable)
    if (displayTimer > displayDelay){
      if  (updateEeprom == true){
        savePresets();
      }
      // radio.readRDS(rdsname,rdsrt) depends on the modfied readRDS routine in the 
      // modified Si4703_Breakout.h & Si4703_Breakout.cpp files
      int rdsnum = radio.readRDS(rdsname,rdsrt);
      if(rdsnum != 0){
        oled.clear(PAGE);
        oled.flipVertical(HIGH);
        oled.flipHorizontal(HIGH);
        oled.setFontType(0);
        oled.setCursor(0,0);
        if(RDSverbose == true){
          // put routine to print out free SRAM (actually the difference between
          // the heap and the stack, ignoring anything deallocated) located here
          // and tune to a good RDS station to display. The RDS routines should be
          // coincident with the highest consumption of SRAM...
//         oled.print(freeRam());
          oled.print(rdsrt);
        }
        else {
          oled.print(rdsname);
//          oled.print(freeRam());  // print out free SRAM
          printFreq();
        }
        RDStimer = 0;  // increase RDStimeout value above to wait longer...
      }
      else {
        if (RDStimer > RDStimeout){
          oled.clear(PAGE);
          oled.flipVertical(HIGH);
          oled.flipHorizontal(HIGH);
          oled.setFontType(0);
          printFreq();
          RDStimer = 0; //reset RDStimer so we don't clear all of the time
        }
      }
    }
    oled.display();
}

void printTitle(String title, int font)
// modified with setCursor(0,20) to start printing on 3rd(ish) text line
// for use with printFreq() below...
{
  oled.setFontType(font);
  oled.setCursor(0,20);
  // Various OLED print tools
//    oled.invert(HIGH);
//    oled.flipVertical(HIGH);
//    oled.flipHorizontal(HIGH);
//    int numFonts =  oled.getTotalFonts();
  oled.print(title);
  oled.display();
}
 void printFreq()
{
//  gets current channel from 4703 and formats it correctly before displaying
//  NOTE: this depends on modifying Si4703_Breakout.h by moving declaration of
//  int getChannel() from private: to public:
        float floatVal = radio.getChannel();
        char charVal[6];
        String stringVal = "";
        dtostrf(floatVal/10,0,1,charVal);
        for(int i=0;i<sizeof(charVal);i++){
          stringVal += charVal[i];
        }
        printTitle(stringVal,2);
}

void savePresets()
{
// saves the current value of volume, channel, preset,
// and RDS mode to eeprom
//  while(!eeprom_is_ready());   // loop here until eeprom is ready
  cli();    // disable interupts before accesing eeprom
  eeprom_write_word((uint16_t*)addr, channel);
  eeprom_write_word((uint16_t*)addr+4, volume);
  eeprom_write_word((uint16_t*)addr+8, preset);
  eeprom_write_word((uint16_t*)addr+12, RDSverbose);
  sei();  // reenable interupts
  updateEeprom = false;
}
/*  // uncomment this if calling freeRam() to determine free SRAM
int freeRam () 
{
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}*/
