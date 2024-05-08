#include <Adafruit_NeoPixel.h>

//Libraries needed are:
// 1. Adafruit SSD1306, in Teensy folder if Teensy has been installed on computer. Note: the h file needs to be commented and uncommented to select the correct oled screen resolution 128 x 64
// 2. Adafruit GFX Library (in local documents folder)
// 3. SdFat (in local documents folder)
// Update note: Due to changes to ADC.h library the code must be ammended for multiple instances in the Setup. Where: adc->setAveraging(32, ADC_0) change to: adc->adc0->setAveraging(32) (and make similar changes to next two instances), also adc-> becomes adc->adc0->.
// Changed syringe volume to 7mL to accomodate disparity in pump rates. With new tubing should be reset to 6mL
// Changed Run Water and Run Syringe commands in Prime menu to turn on and off with enter button rather than limiting to 10 sec
// Encoder values made absolute on lines 566 & 574 to avoid issue with motors not stopping (e.g. syringe pump running forever)
char saseVersion [] = "V3d"; //V3d Updated code for use on Teensy 4.1, comments below

//LIBRARIES/Users/fas/Desktop/AOML/SASe/code_/libraries_/Encoder.h
  #include <TimeLib.h>//Time library
  #include <Wire.h>// I2C library
  #include <Snooze.h>// Low power sleep library for Teensy, Needed to edit two files related to snooze, SnoozeDigital.cpp and hal.c, follow guidance on https://github.com/duff2013/Snooze/issues/108 and https://github.com/duff2013/Snooze/issues/114 to fix
  //Previously we've used Hibernate function from the snooze library for sub mA current draw in sleep mode. With the new Teensy 4s and a unmaintained library it isn't working anymore. Sleep function does work.
  //It's not as low current draw but it will still allow for short-term deployments (maybe days, need to check current draw)
  #include <Adafruit_GFX.h>// OLED graphics library
  #include <Adafruit_SSD1306.h>// OLED library
  #include <IRremote.h>// Needed to enable line 51 in IRremote.hpp (#define SUPPRESS_ERROR_MESSAGE_FOR_BEGIN, files on my mac in /Users/.../Library/Arduino15/packages/teensy/hardware/avr/1.58.1/libraries/IRremote/src), also needed to change some variables, mainly "irrecv" to "IRreceiver" (see for guidance: https://github.com/Arduino-IRremote/Arduino-IRremote?tab=readme-ov-file#converting-your-2x-program-to-the-4x-version)
  //Uncomment line 174 and open serial monitor while pressing remote buttons to see what codes each IR button is generating. Need to make sure code IDs are used in IR definitions
  #include <SdFat.h>//sd cart library needed to make some updates to variables, but all done in code here
  #include <SD.h>//sd library
  #include <SPI.h>// Serial library
  #include <ADC.h>// Analog to digital converter library
  #include <ADC_util.h> //Added in following ADC examples
  #include <Encoder.h>
//DEFINITIONS
  //Snooze
    SnoozeAlarm alarm;
    SnoozeDigital digital;
    SnoozeBlock config_teensy40(digital, alarm);
    int hrToSleep;// Number of hours to sleep
    int minToSleep;// Number of remaining minutes to sleep
    int secToSleep;// Number of remaining seconds to sleep
  //OLED
    #define OLED_RESET 8// OLED reset
    Adafruit_SSD1306 display(OLED_RESET);
    int oledPowerPin = 17;// Pin that powers the OLED
  //Voltage Read
    int voltageReadPin = A2;// Pin that reads the voltage
    ADC *adc = new ADC();// Creates adc object
    float voltage = 0;// The voltage of the battery backs
    float rawVoltage = 0;// The raw voltage read across the voltage divider
  //IR
    int irPowerPin = 20;// Pin that powers the IR sensor
    #define IR_RECEIVE_PIN 21
    //Original code
    // #define LEFTIR 16584943// IR signal read by sensor that translates to LEFTIR
    // #define RIGHTIR 16601263// IR signal read by sensor that translates to RIGHTIR
    // #define UPIR 16621663// IR signal read by sensor that translates to UPIR
    // #define DOWNIR 16625743// IR signal read by sensor that translates to DOWNIR
    // #define ENTERIR 16617583// IR signal read by sensor that translates to ENTERIR
    //NEED TO UPDATE THE IR HEX NUMBERS (BELOW) TO PROPERLY BE READ BY REMOTE. Uncomment line 172 for further troubleshooting in serial monitor
    //Normal Remote Below
     #define LEFTIR 0xF708BF00      // IR signal read by sensor that translates to LEFTIR
     #define RIGHTIR 0xF50ABF00     // IR signal read by sensor that translates to RIGHTIR
     #define UPIR 0xFA05BF00        // IR signal read by sensor that translates to UPIR
     #define DOWNIR 0xF20DBF00      // IR signal read by sensor that translates to DOWNIR
     #define ENTERIR 0xF609BF00     // IR signal read by sensor that translates to ENTERIR 
    //Convenient Remote Below
   // #define LEFTIR 3910598400// IR signal read by sensor that translates to LEFTIR
   // #define RIGHTIR 4061003520// IR signal read by sensor that translates to RIGHTIR
   // #define UPIR 3927310080// IR signal read by sensor that translates to UPIR
   // #define DOWNIR 3877175040// IR signal read by sensor that translates to DOWNIR
   // #define ENTERIR 3860463360// IR signal read by sensor that translates to ENTERIR
  //Reed switch
    int REED_INTERRUPT_PIN = 22;// Pin that reed switch is attached to
  //Pumps
    float maxBattVoltage = 10.50;// Max battery voltage, at which the calibration is run
    int pumpAPin =38;// Pin to signal FET to drive motor 1
    int pumpAEncPowerPin = 36;
    int pumpAEncAPin = 37;
    int pumpAEncBPin = 35;        
    Encoder pumpAEnc(pumpAEncAPin, pumpAEncBPin);
    long pumpAGearRatio = 34.0;
    long pumpAVolume = 0.0;

    int syringePin =27;// Pin to signal FET to drive motor 2
    int syringeEncPowerPin = 29;
    int syringeEncAPin = 28;
    int syringeEncBPin = 30;  
    Encoder syringeEnc(syringeEncAPin, syringeEncBPin);
    long syringeGearRatio = 99.0;
    long syringeVolume = 0.0;

    long sampleVolumeChangeMl = 10.0; //Units of change for sampling volume 
    long calibrationVolumeChangeMl = 1.0; //Units of change for calibration volume
    long calibrationCount = 0.0;
    long calibrationVolume = 1.0;
    int waitforit = 0;

  // Menu
    uint8_t menu = 0;// An index value to establish which navigation menu is currently selected
    uint8_t pos = 0;// An index value to establish which navigation position (within a menu) is currently selected
  //sd card
    #define SD_CONFIG SdioConfig(FIFO_SDIO)
    FsFile sampleParam;// Name of sample parameter file saved on sd card
    FsFile dataLog;// Name of data logging file saved on sd card
    #define SAMPLE_PARAM_ROW 1// Number of rows in sampleParam.txt file
    #define SAMPLE_PARAM_COL 9// Number of columns in sampleParam.txt file
    int sampleParamArray[SAMPLE_PARAM_ROW][SAMPLE_PARAM_COL];// Create an sampleParamArray with SAMPLE_PARAM_ROW rows and SAMPLE_PARAM_COL columns
  
  //RTC
    time_t nowSecTime;// Create a time variable called nowSecTime, representing the time now in seconds since 1970
    time_t updateSecTime;// Create a time variable called updateSecTime, representing the time to be set to the RTC in seconds since 1970
    time_t aSecTime;// Create a time variable called aSecTime, representing the time to fire pump A in seconds since 1970
    time_t bSecTime;// Create a time variable called bSecTime, representing the time to fire pump B in seconds since 1970
    int nowHr; int nowMin; int nowSec; int nowDay; int nowMon;  int nowYr;// create integers for the time now in hours, minutes, seconds, days, months, years
    int aHr;int aMin;int aSec;int aDay;int aMon;int aYr;// create integers for the time to fire A in hours, minutes, seconds, days, months, years
    long aEndTime;
    int aAlarmFlag = 0;
  //Sample Parameters. For sampleMode 0 is daily, 1 is once
    int sampleMode;// An integer to hold what sampling mode is initiated, 0 for daily, 1 for once
    long sampleVolume = 5.0;// An integer for the number of mL of sample that will be collected
    long syringeDispenseVolume = 7.0; //Volume of preservative pumped from syringe

//<-------------------------------------------------------------------------------------------------------------------------------------------------------------->
//<----------------------------------------------------------------------SETUP----------------------------------------------------------------------------------->
//<-------------------------------------------------------------------------------------------------------------------------------------------------------------->
void setup(){// Run setup code on initial power up
  //Communication setup     
    //Wire.begin();// NOT SURE THIS IS NECESSARY
    Serial.begin(9600);// Initiate the serial connection, used for debugging
    delay (500);// Pause for a half second
  //Voltage read setup
    pinMode(voltageReadPin, INPUT);// Set the voltage read pin as an input, where we will be reading the voltage of the battery pack
    adc->adc0->setAveraging(32); // set number of averages read in the analog to digital converter we will use to read the voltage of the battery pack
    adc->adc0->setResolution(16); // set bits of resolution of the analaog to digital converter we will use to read the voltage of the battery pack
    adc->adc0->setConversionSpeed(ADC_CONVERSION_SPEED::VERY_LOW_SPEED); // change the conversion speed, added this command in to follow example guidance
    adc->adc0->setSamplingSpeed(ADC_SAMPLING_SPEED::LOW_SPEED); // set the sampling speed of the analaog to digital converter to low
  //RTC setup          
    setSyncProvider(getTeensy3Time);// Sets the time using the getTeensy3Time function
  //Snooze
    pinMode(REED_INTERRUPT_PIN, INPUT_PULLUP);
    delay(200);
    digital.pinMode(REED_INTERRUPT_PIN, INPUT_PULLUP, RISING);// Sets the pin the reed switch is connected to as an interrupt pin that can wake the teensy up from sleep
    alarm.setRtcTimer(0, 1, 0);// Set the alarm to go off at hour, min, sec

  //OLED Setup
    pinMode(oledPowerPin, OUTPUT);// Set the oledPowerPin to an output because this is the pin that we power the OLED from
    digitalWrite(oledPowerPin, HIGH);// Set the oledPowerPin to high to turn on the OLED
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);// Begin and initialize the OLED display
    display.clearDisplay();// Clear the OLED display
  //Infrared Setup
    pinMode(irPowerPin, OUTPUT);// Set the irPowerPin to an output because this is the pin that we power the IR sensor from
    digitalWrite(irPowerPin, HIGH);// Set the irPowerPin to high to turn on the IR sensor   
    IrReceiver.begin(IR_RECEIVE_PIN, DISABLE_LED_FEEDBACK);// Starts the IR receiving object irrecv
  //Pump Setup
    pinMode(pumpAPin, OUTPUT);// Set the pumpAPin to an output because this is the pin that we will use to power pump A
    pinMode(pumpAEncPowerPin, OUTPUT);
    pinMode(syringePin, OUTPUT);// Set the syringePin to an output because this is the pin that we will use to power the syringe pump
    pinMode(syringeEncPowerPin, OUTPUT);  
    pumpAEnc.write(0);
    syringeEnc.write(0);
    
  //sd Setup
    bool ok;
    ok = SD.sdfs.begin(SdioConfig(FIFO_SDIO));
    if (!ok) { display.setTextColor(WHITE,BLACK);  display.setTextSize(1);  display.setCursor(2,8);  display.println("Microsd not");display.setCursor(5,16);  display.println("detected!");display.display();}// If the sd card fails to communicate, display that the card is not detected to the serial debugging
    readSampleParamArray();// Call the function that reads the sample parameters off of the sd card
    Serial.println(sampleVolume);
    calculateInitialAlarmSecondTime();    
}

//<-------------------------------------------------------------------------------------------------------------------------------------------------------------->
//<------------------------------------------------------------------MAIN LOOP----------------------------------------------------------------------------------->
//<-------------------------------------------------------------------------------------------------------------------------------------------------------------->
void loop(){
  switch(menu){//What menu is selected?
    case 0: getNow(); statusMenuNavigation(); statusMenuDisplay(); break;//Loop the Status Menu navigation and display
    case 1: settingsMenuNavigation(); settingsMenuDisplay(); break;//Loop the Settings Menu navigation and display 
    case 2: pumpAMenuNavigation(); pumpAMenuDisplay(); break;//Loop the Pump A Menu navigation and display 
    case 3: initiateMenuNavigation(); initiateMenuDisplay(); break;//Loop the Initiate Menu navigation and display 
    case 4: timeSetMenuNavigation(); timeSetMenuDisplay(); break;//Loop the Time Set Menu navigation and display 
    case 5: calibrationMenuNavigation(); calibrationMenuDisplay(); break;//Loop the Calibration Menu navigation and display
    case 6: primeMenuNavigation(); primeMenuDisplay(); break;//Loop the Priming Menu navigation and display
    case 7: getNow(); samplingMode(); checkSleep(); //Operating code loop for checking time, checking alarms and running samples, and determining next alarm
  }
}

//<-------------------------------------------------------------------------------------------------------------------------------------------------------------->
//<---------------------------------------------------------------MENU NAVIGATION-------------------------------------------------------------------------------->
//<-------------------------------------------------------------------------------------------------------------------------------------------------------------->
void statusMenuNavigation(){
  if(IrReceiver.decode()){// If there is an IR signal, return true and store signal in "results"
//Please use IrReceiver.decode() without a parameter and IrReceiver.decodedIRData.<fieldname> . [-Wdeprecated-declarations]
    //Serial.println(IrReceiver.decodedIRData.decodedRawData); //Uncomment this line to see what codes each IR button is generating to adjust code IDs in IR definitions
    switch(pos){// What is the selected position within this menu?
     case 0:// Position is 0
      switch (IrReceiver.decodedIRData.decodedRawData){// What was the signal received from the IR remote?
        case LEFTIR: menu = 6; break; case RIGHTIR: menu = 1; break;
      };break;
    }  
    IrReceiver.resume();// Continue receiving looking for IR signals
  }
}

void settingsMenuNavigation(){
  if(IrReceiver.decode()){// If there is an IR signal, return true and store signal in "results"
    switch(pos){// What is the selected position within this menu?
      case 0:// Position is 0
        switch (IrReceiver.decodedIRData.decodedRawData){// What was the signal received from the IR remote?
          case LEFTIR: menu = 0; break; case RIGHTIR: menu = 2; break; case UPIR: pos = 0; break; case DOWNIR: pos = 1; break;// Navigate           
        };break;
      case 1:// Position is 1
        switch (IrReceiver.decodedIRData.decodedRawData){// What was the signal received from the IR remote?
          case LEFTIR: pos = 0; break; case RIGHTIR: pos = 11; break; case UPIR: pos = 0; break; case DOWNIR: pos = 2; break;// Navigate            
        };break;
      case 11:// Position is 11
        switch (IrReceiver.decodedIRData.decodedRawData){// What was the signal received from the IR remote?
          case LEFTIR: pos = 1; break; case RIGHTIR: pos = 2; break; case UPIR: sampleMode++; break; case DOWNIR: sampleMode--; break;// Navigate or change sample mode  
        }break;
      case 2:// Position is 2
        switch (IrReceiver.decodedIRData.decodedRawData){// What was the signal received from the IR remote?
          case LEFTIR: pos = 11; break; case RIGHTIR: pos = 21; break; case UPIR: pos = 1; break; case DOWNIR: pos = 3; break;// Navigate 
        }break;
      case 21:// Position is 21
        switch (IrReceiver.decodedIRData.decodedRawData){// What was the signal received from the IR remote?
          case LEFTIR: pos = 2; break; case RIGHTIR: pos = 3; break; case UPIR: sampleVolume=sampleVolume+sampleVolumeChangeMl; break; case DOWNIR: sampleVolume=sampleVolume-sampleVolumeChangeMl; break;// Navigate or change sample volume by sampleVolumeChangeMl in mL
        }break;
      case 3:
        switch (IrReceiver.decodedIRData.decodedRawData){// What was the signal received from the IR remote?
          case LEFTIR: pos = 21; break; case RIGHTIR: pos = 3; break; case UPIR: pos = 2; break; case DOWNIR: pos = 3; break; case ENTERIR: writeSampleParamArray(); break;// Navigate or write sample parameters to the sd card     
        }break;
    }  
    IrReceiver.resume();// Continue receiving looking for IR signals
  }
}

void pumpAMenuNavigation(){
  if(IrReceiver.decode()){// If there is an IR signal, return true and store signal in "results"
    switch(pos){// What is the selected position within this menu?
      case 0:// Position is 0
        switch (IrReceiver.decodedIRData.decodedRawData){// What was the signal received from the IR remote?
          case LEFTIR: menu = 1; break; case RIGHTIR: menu = 3; break; case UPIR: pos = 0; break; case DOWNIR: pos = 1; break;// Navigate    
        }break;
      case 1:// Position is 1
        switch (IrReceiver.decodedIRData.decodedRawData){// What was the signal received from the IR remote?
          case LEFTIR: pos = 0; break; case RIGHTIR: pos = 11; break; case UPIR: pos = 0; break; case DOWNIR: pos = 2; break;// Navigate  
        }break;
      case 11:// Position is 11
        switch (IrReceiver.decodedIRData.decodedRawData){// What was the signal received from the IR remote?
          case LEFTIR: pos = 1; break; case RIGHTIR: pos = 12; break; case UPIR: aHr++; break; case DOWNIR: aHr--; break;// Navigate or change hour Pump A fires  
        }break;
      case 12:// Position is 12
        switch (IrReceiver.decodedIRData.decodedRawData){// What was the signal received from the IR remote?
          case LEFTIR: pos = 11; break; case RIGHTIR: pos = 2; break; case UPIR: aMin++; break; case DOWNIR: aMin--; break;// Navigate or change minute Pump A fires   
        }break;
      case 2:// Position is 2
        switch (IrReceiver.decodedIRData.decodedRawData){// What was the signal received from the IR remote?
          case LEFTIR: pos = 12; break; case RIGHTIR: pos = 21; break; case UPIR: pos = 1; break; case DOWNIR: pos = 3; break;// Navigate
        }break;
      case 21:// Position is 21
        switch (IrReceiver.decodedIRData.decodedRawData){// What was the signal received from the IR remote?
          case LEFTIR: pos = 2; break; case RIGHTIR: pos = 22; break; case UPIR: aDay++; break; case DOWNIR: aDay--; break;// Navigate or change day Pump A fires   
        }break;
      case 22:// Position is 22
        switch (IrReceiver.decodedIRData.decodedRawData){// What was the signal received from the IR remote?
          case LEFTIR: pos = 21; break; case RIGHTIR: pos = 23; break; case UPIR: aMon++; break; case DOWNIR: aMon--; break;// Navigate or change month Pump A fires   
        }break;
      case 23:// Position is 23
        switch (IrReceiver.decodedIRData.decodedRawData){// What was the signal received from the IR remote?
          case LEFTIR: pos = 22; break; case RIGHTIR: pos = 3; break; case UPIR: aYr++; break; case DOWNIR: aYr--; break;// Navigate or change year Pump A fires  
        }break;
      case 3:// Position is 3
        switch (IrReceiver.decodedIRData.decodedRawData){// What was the signal received from the IR remote?
          case LEFTIR: pos = 23; break; case RIGHTIR: pos = 3; break; case UPIR: pos = 2; break; case DOWNIR: pos = 3; break; case ENTERIR: writeSampleParamArray(); calculateInitialAlarmSecondTime(); break;// Navigate or save sample parameters to the sd card
        }break;
    }  
    IrReceiver.resume();// Continue receiving looking for IR signals
  }
}


void initiateMenuNavigation(){
  if(IrReceiver.decode()){// If there is an IR signal, return true and store signal in "results"
    switch(pos){// What is the selected position within this menu?
      case 0:// Position is 0
        switch (IrReceiver.decodedIRData.decodedRawData){// What was the signal received from the IR remote?
          case LEFTIR: menu = 2; break; case RIGHTIR: menu = 4; break; case DOWNIR: pos = 1; break;//Navigate   
        };break;
      case 1:// Position is 1
        switch (IrReceiver.decodedIRData.decodedRawData){// What was the signal received from the IR remote?
          case LEFTIR: pos = 1; break; case RIGHTIR: pos = 1; break; case UPIR: pos = 0; break; case DOWNIR: pos = 1; break; case ENTERIR: initialAlarmFlag(); menu = 7; break;  //check the alarms then start sleeping then calculates the amount of time to sleep
        }break;
    }  
    IrReceiver.resume();// Continue receiving looking for IR signals
  }
}
void timeSetMenuNavigation(){
  if(IrReceiver.decode()){// If there is an IR signal, return true and store signal in "results"
    switch(pos){// What is the selected position within this menu?
      case 0:// Position is 0
        switch (IrReceiver.decodedIRData.decodedRawData){// What was the signal received from the IR remote?
          case LEFTIR: menu = 3; break; case RIGHTIR: menu = 5; break; case UPIR: pos = 0; break; case DOWNIR: pos = 1; break;// Navigate  
        }break;
      case 1:// Position is 1
        switch (IrReceiver.decodedIRData.decodedRawData){// What was the signal received from the IR remote?
          case LEFTIR: pos = 0; break; case RIGHTIR: pos = 11; break; case UPIR: pos = 0; break; case DOWNIR: pos = 2; break;// Navigate  
        }break;
      case 11:// Position is 11
        switch (IrReceiver.decodedIRData.decodedRawData){// What was the signal received from the IR remote?
          case LEFTIR: pos = 1; break; case RIGHTIR: pos = 12; break; case UPIR: nowHr++; break; case DOWNIR: nowHr--; break;// Navigate or set the hour it is now
        }break;
      case 12:// Position is 12
        switch (IrReceiver.decodedIRData.decodedRawData){// What was the signal received from the IR remote?
          case LEFTIR: pos = 11; break; case RIGHTIR: pos = 13; break; case UPIR: nowMin++; break; case DOWNIR: nowMin--; break;// Navigate or set the minute it is now  
        }break;
      case 13:// Position is 13
        switch (IrReceiver.decodedIRData.decodedRawData){// What was the signal received from the IR remote?
          case LEFTIR: pos = 12; break; case RIGHTIR: pos = 2; break; case UPIR: nowSec++; break; case DOWNIR: nowSec--; break;// Navigate or set the second it is now    
        }break;
      case 2:// Position is 2
        switch (IrReceiver.decodedIRData.decodedRawData){// What was the signal received from the IR remote?
          case LEFTIR: pos = 13; break; case RIGHTIR: pos = 21; break; case UPIR: pos = 1; break; case DOWNIR: pos = 3; break;// Navigate  
        }break;
      case 21:// Position is 21
        switch (IrReceiver.decodedIRData.decodedRawData){// What was the signal received from the IR remote?
          case LEFTIR: pos = 2; break; case RIGHTIR: pos = 22; break; case UPIR: nowDay++; break; case DOWNIR: nowDay--; break;// Navigate or set the day it is now    
        }break;
      case 22:// Position is 22
        switch (IrReceiver.decodedIRData.decodedRawData){// What was the signal received from the IR remote?
          case LEFTIR: pos = 21; break; case RIGHTIR: pos = 23; break; case UPIR: nowMon++; break; case DOWNIR: nowMon--; break;// Navigate or set the month it is now    
        }break;
      case 23:// Position is 23
        switch (IrReceiver.decodedIRData.decodedRawData){// What was the signal received from the IR remote?
          case LEFTIR: pos = 22; break; case RIGHTIR: pos = 3; break; case UPIR: nowYr++; break; case DOWNIR: nowYr--; break;// Navigate or set the year it is now    
        }break;
      case 3:// Position is 3
        switch (IrReceiver.decodedIRData.decodedRawData){// What was the signal received from the IR remote?
          case LEFTIR: pos = 23; break; case RIGHTIR: pos = 3; break; case UPIR: pos = 2; break; case DOWNIR: pos = 3; break; case ENTERIR: calcUpdateSecTime(); Teensy3Clock.set(updateSecTime); display.clearDisplay(); display.setTextSize(2); display.setTextColor(WHITE,BLACK); display.setCursor(0,0); display.println(" <SAVED>"); display.display(); delay(1000); break;//Navigate or 1. calculate the updated time in secons, set the clock to that second time, display "<SAVED>"     
        }break;
    }  
    IrReceiver.resume();// Continue receiving looking for IR signals
  }
}
void calibrationMenuNavigation(){
  if(IrReceiver.decode()){// If there is an IR signal, return true and store signal in "results"
    switch(pos){// What is the selected position within this menu?
      case 0:// Position is 0
        switch (IrReceiver.decodedIRData.decodedRawData){// What was the signal received from the IR remote?
          case LEFTIR: menu = 4; break; case RIGHTIR: menu = 6; break; case DOWNIR: pos = 1; break;//Navigate   
        };break;
        case 1:// Position is 1
        switch (IrReceiver.decodedIRData.decodedRawData){// What was the signal received from the IR remote?
          case LEFTIR: pos = 1; break; case RIGHTIR: pos = 2; break; case UPIR: pos = 0; break; case DOWNIR: pos = 2; break; case ENTERIR: runCalibration(); break;  //Run calibration to scale Pump A accurately
        }break;
      case 2:// Position is 2
        switch (IrReceiver.decodedIRData.decodedRawData){// What was the signal received from the IR remote?
          case LEFTIR: pos = 2; break; case RIGHTIR: pos = 21; break; case UPIR: pos = 1; break; case DOWNIR: pos = 3; break;  //navigate
        }break;
      case 21:// Position is 21
        switch (IrReceiver.decodedIRData.decodedRawData){// What was the signal received from the IR remote?
          case LEFTIR: pos = 2; break; case RIGHTIR: pos = 3; break; case UPIR: calibrationVolume=calibrationVolume+calibrationVolumeChangeMl; break; case DOWNIR: calibrationVolume=calibrationVolume-calibrationVolumeChangeMl; break;// Navigate or change calibration volume by calibrationVolumeChangeMl in mL
        }break;
     case 3:
        switch (IrReceiver.decodedIRData.decodedRawData){// What was the signal received from the IR remote?
          case LEFTIR: pos = 21; break; case RIGHTIR: pos = 3; break; case UPIR: pos = 2; break; case DOWNIR: pos = 3; break; case ENTERIR: writeSampleParamArray(); break;// Navigate or write sample parameters to the sd card     
        }break;
    }  
    IrReceiver.resume();// Continue receiving looking for IR signals
  }
}
void primeMenuNavigation(){
  if(IrReceiver.decode()){// If there is an IR signal, return true and store signal in "results"
    switch(pos){// What is the selected position within this menu?
      case 0:// Position is 0
        switch (IrReceiver.decodedIRData.decodedRawData){// What was the signal received from the IR remote?
          case LEFTIR: menu = 5; break; case RIGHTIR: menu = 0; break; case DOWNIR: pos = 1; break;//Navigate   
        };break;
        case 1:// Position is 1
        switch (IrReceiver.decodedIRData.decodedRawData){// What was the signal received from the IR remote?
          case LEFTIR: pos = 1; break; case RIGHTIR: pos = 1; break; case UPIR: pos = 0; break; case DOWNIR: pos = 2; break; case ENTERIR: runPump(); break;  //Run sample pump for 10 sec to pump fluid for cleaning/priming
        }break;
        case 2:// Position is 2
        switch (IrReceiver.decodedIRData.decodedRawData){// What was the signal received from the IR remote?
          case LEFTIR: pos = 2; break; case RIGHTIR: pos = 2; break; case UPIR: pos = 1; break; case DOWNIR: pos = 0; break; case ENTERIR: runSyringe(); break;  //Run syringe pump for 10 sec to pump fluid for cleaning/priming
        }break;
    }  
    IrReceiver.resume();// Continue receiving looking for IR signals
  }
}

//<-------------------------------------------------------------------------------------------------------------------------------------------------------------->
//<--------------------------------------------------------------------MENU DISPLAY------------------------------------------------------------------------------>
//<-------------------------------------------------------------------------------------------------------------------------------------------------------------->
void statusMenuDisplay(){// Text to display in the status menu
  getVoltage();// Read the voltage of the battery pack
  display.clearDisplay();// Clear the display 
  display.setTextSize(1);// Set the text size to 1 
  if (pos==0){display.setTextColor(BLACK,WHITE);} else{display.setTextColor(WHITE,BLACK);}// If position 0 is selected, highlight text in that position
  display.setCursor(0,0); display.println("    <STATUS MENU>    ");// Display "    <STATUS MENU>    " on the first line
  display.setTextColor(WHITE,BLACK);// Set the color of the text to normal
  display.setCursor(0,8); display.println(nowHr); display.setCursor(12,8); display.println(":"); display.setCursor(18,8); display.println(nowMin); display.setCursor(30,8); display.println(":"); display.setCursor(36,8); display.println(nowSec); display.setCursor(48,16); display.println(" D:"); display.setCursor(66,8); display.println(nowDay); display.setCursor(78,8); display.println("/"); display.setCursor(84,8); display.println(nowMon); display.setCursor(96,8); display.println("/"); display.setCursor(102,8); display.println(nowYr);// Display the time and date line
  display.setCursor(0,16); display.println("Sample mode:");// Display "Sample mode:" text
  display.setCursor(72,16);// Set cursor postion to after "Sample mode:" text
  switch(sampleMode){// What is the sample mode?
    case 0: display.println("Daily"); break;// If the sample mode is 0, display "Daily"
    case 1: display.println("Once"); break;// If the sample mode is 1, display "Once"
  }
  //Pump A line
  display.setCursor(0,24); display.println("P:");// Display "P:" on the sixth line
  display.setCursor(12,24);// Set the cursor to immediately following the "P:"
  switch(sampleMode){// What is the sample mode?
    case 0: display.println("Daily@"); display.setCursor(66,24); display.println(aHr); display.setCursor(78,24); display.println(":"); display.setCursor(84,24); display.println(aMin); break;// If the sample mode is 0, display the time that Pump A will fire daily as Hour:Minute
    case 1: display.println("T:"); display.setCursor(24,24); display.println(aHr); display.setCursor(36,24); display.println(":"); display.setCursor(42,24); display.println(aMin); display.setCursor(56,24); display.println("D:"); display.setCursor(68,24); display.println(aDay); display.setCursor(80,24); display.println("/"); display.setCursor(86,24); display.println(aMon); display.setCursor(98,24); display.println("/"); display.setCursor(104,24); display.println(aYr); break;// If the sample mode is 1, display the time that Pump A will fire daily as T: Hour:Minute D: Day/Month/Year
  }
  //Volume and voltage line
  display.setCursor(0,32); display.println("Vol:"); display.setCursor(24,32); display.println(sampleVolume);  display.setCursor(48,32); display.println("Batt:"); display.setCursor(78,32); display.println(voltage); display.setCursor(108,32); display.println("v");// Display sample "Vol:" followed by sample volume, then "Batt:" followed by battery voltage and "v"
  //Version line
  display.setCursor(0,40); display.println("Version: "); display.setCursor(54,40); display.println(saseVersion);  

  
  display.display();// Update the display
}

void settingsMenuDisplay(){// Text to display in the settings menu
  display.clearDisplay();// Clear the display 
  display.setTextSize(2);// Set the text size to 2 
  if (pos==0){display.setTextColor(BLACK,WHITE);} else{display.setTextColor(WHITE,BLACK);}// If position 0 is selected, highlight text in that position
  display.setCursor(0,0); display.println("<SETTINGS>");// Set cursor to first line and display "<SETTINGS>"
  if (pos==1){display.setTextColor(BLACK,WHITE);} else{display.setTextColor(WHITE,BLACK);}// If position 1 is selected, highlight text in that position
  display.setCursor(0,16); display.println("Mode:");// Set cursor to second line and display "Mode:"
  if (pos==11){display.setTextColor(BLACK,WHITE);} else{display.setTextColor(WHITE,BLACK);}// If position 11 is selected, highlight text in that position
  switch(sampleMode){// What is the sample mode?
    case 0: display.setCursor(60,16); display.println("Daily"); break;// If sample mode is 0, set cursor to after "Mode" and display "Daily"
    case 1:   display.setCursor(60,16); display.println("Once"); break;// If sample mode is 1, set cursor to after "Mode" and display "Once"
  }
  if (pos==2){display.setTextColor(BLACK,WHITE);} else{display.setTextColor(WHITE,BLACK);}// If position 2 is selected, highlight text in that position
  display.setCursor(0,32); display.println("VOL:");// Set cursor to third line and display "VOL:"
  if (pos==21){display.setTextColor(BLACK,WHITE);} else{display.setTextColor(WHITE,BLACK);}// If position 21 is selected, highlight text in that position
  display.setCursor(48,32); display.println(sampleVolume);// Set cursor to after "VOL:" and display the sample volume to be collected
  if (pos==3){display.setTextColor(BLACK,WHITE);} else{display.setTextColor(WHITE,BLACK);}// If position 3 is selected, highlight text in that position
  display.setCursor(0,48); display.println("ENTER SET");// Set cursor to fourth line and display "ENTER SET"
  numberCorrect();// Correct all displayed numbers
  display.display();// Update the display
}

void pumpAMenuDisplay(){// Text to display in the Pump A menu
  display.clearDisplay();// Clear the display  
  display.setTextSize(2);// Set the text size to 2 
  if (pos==0){display.setTextColor(BLACK,WHITE);} else{display.setTextColor(WHITE,BLACK);}// If position 0 is selected, highlight text in that position
  display.setCursor(0,0); display.println(" <PUMP> ");// Set cursor to first line and display " <PUMP A> "
  //TIME DISPLAY
  if (pos==1){display.setTextColor(BLACK,WHITE);} else{display.setTextColor(WHITE,BLACK);}// If position 1 is selected, highlight text in that position
  display.setCursor(0,16); display.println("T:");// Set cursor to second line and display "T:"
  if (pos==11){display.setTextColor(BLACK,WHITE);} else{display.setTextColor(WHITE,BLACK);}// If position 11 is selected, highlight text in that position
  display.setCursor(24,16); display.println(aHr);// Set cursor to after "T:" and display the hour pump A will fire
  display.setTextColor(WHITE,BLACK); display.setCursor(48,16); display.println(":");// Set cursor to after the hour pump A will fire and display ":"
  if (pos==12){display.setTextColor(BLACK,WHITE);} else{display.setTextColor(WHITE,BLACK);}// If position 12 is selected, highlight text in that position
  display.setCursor(60,16); display.println(aMin);// Set cursor to after ":" and display the minute pump A will fire
  //DAY DISPLAY
  if (pos==2){display.setTextColor(BLACK,WHITE);} else{display.setTextColor(WHITE,BLACK);}// If position 2 is selected, highlight text in that position
  display.setCursor(0,32);  display.println("D:");// Set cursor to third line and display "D:"
  if (pos==21){display.setTextColor(BLACK,WHITE);} else{display.setTextColor(WHITE,BLACK);}// If position 21 is selected, highlight text in that position
  display.setCursor(24,32); display.println(aDay);// Set cursor to after "D:" and display the day pump A will fire
  display.setTextColor(WHITE,BLACK); display.setCursor(48,32); display.println("/");// Set cursor to after the hour pump A will fire and display "/"
  if (pos==22){display.setTextColor(BLACK,WHITE);} else{display.setTextColor(WHITE,BLACK);}// If position 22 is selected, highlight text in that position
  display.setCursor(60,32); display.println(aMon);// Set cursor to after "/" and display the month pump A will fire
  display.setTextColor(WHITE,BLACK); display.setCursor(84,32); display.println("/");// Set cursor to after the month pump A will fire and display "/"
  if (pos==23){display.setTextColor(BLACK,WHITE);} else{display.setTextColor(WHITE,BLACK);}// If position 23 is selected, highlight text in that position
  display.setCursor(96,32); display.println(aYr);// Set cursor to after "/" and display the year pump A will fire
  if (pos==3){display.setTextColor(BLACK,WHITE);} else{display.setTextColor(WHITE,BLACK);}// If position 3 is selected, highlight text in that position
  display.setCursor(0,48); display.println("ENTER SET");// Set cursor to fourth line and display "ENTER SET"
  numberCorrect();// Correct all displayed numbers  
  display.display();// Update the display
}

void initiateMenuDisplay(){// Text to display in the initiate menu
  display.clearDisplay();// Clear the display  
  display.setTextSize(2);// Set the text size to 2 
  if (pos==0){display.setTextColor(BLACK,WHITE);} else{display.setTextColor(WHITE,BLACK);}// If position 0 is selected, highlight text in that position
  display.setCursor(0,0); display.println("<INITIATE>");// Set cursor to first line and display "INITIATE"
  display.setTextSize(1);// Set the text size to 1
  if (pos==1){display.setTextColor(BLACK,WHITE);} else{display.setTextColor(WHITE,BLACK);}// If position 1 is selected, highlight text in that position
  display.setCursor(0,16); display.println("Press Enter");// Set cursor to second line and display "Press Enter"
  display.display();// Update the display
}

void timeSetMenuDisplay(){// Text to display in the time setting menu
  display.clearDisplay();// Clear the display  
  display.setTextSize(2);// Set the text size to 2 
  if (pos==0){display.setTextColor(BLACK,WHITE);} else{display.setTextColor(WHITE,BLACK);}// If position 0 is selected, highlight text in that position
  display.setCursor(0,0); display.println("<TIME SET>");// Set cursor to first line and display "TIME SET>"
   //TIME DISPLAY
  if (pos==1){display.setTextColor(BLACK,WHITE);} else{display.setTextColor(WHITE,BLACK);}// If position 1 is selected, highlight text in that position
  display.setCursor(0,16); display.println("T:");// Set cursor to second line and display "T:"
  if (pos==11){display.setTextColor(BLACK,WHITE);} else{display.setTextColor(WHITE,BLACK);}// If position 11 is selected, highlight text in that position
  display.setCursor(24,16); display.println(nowHr);// Set cursor to after "T:" and display the hour the clock should be
  display.setTextColor(WHITE,BLACK); display.setCursor(48,16); display.println(":");// Set cursor to after the hour the clock should be and display ":" 
  if (pos==12){display.setTextColor(BLACK,WHITE);} else{display.setTextColor(WHITE,BLACK);}// If position 12 is selected, highlight text in that position
  display.setCursor(60,16); display.println(nowMin);// Set cursor to after ":" and display the minute the clock should be
  display.setTextColor(WHITE,BLACK); display.setCursor(84,16); display.println(":");// Set cursor to after the minute the clock should be and display ":" 
  if (pos==13){display.setTextColor(BLACK,WHITE);} else{display.setTextColor(WHITE,BLACK);}// If position 13 is selected, highlight text in that position
  display.setCursor(96,16); display.println(nowSec);// Set cursor to after ":" and display the second the clock should be
  //DAY DISPLAY
  if (pos==2){display.setTextColor(BLACK,WHITE);} else{display.setTextColor(WHITE,BLACK);}// If position 2 is selected, highlight text in that position
  display.setCursor(0,32);  display.println("D:");// Set cursor to third line and display "D:"
  if (pos==21){display.setTextColor(BLACK,WHITE);} else{display.setTextColor(WHITE,BLACK);}// If position 21 is selected, highlight text in that position
  display.setCursor(24,32); display.println(nowDay);  // Set cursor to after "D:" and display the day the clock should be
  display.setTextColor(WHITE,BLACK); display.setCursor(48,32); display.println("/"); // Set cursor to after the hour the clock should be and display "/"
  if (pos==22){display.setTextColor(BLACK,WHITE);} else{display.setTextColor(WHITE,BLACK);}// If position 22 is selected, highlight text in that position
  display.setCursor(60,32); display.println(nowMon);// Set cursor to after "/" and display the month the clock should be
  display.setTextColor(WHITE,BLACK); display.setCursor(84,32); display.println("/"); // Set cursor to after the month the clock should be and display "/" 
  if (pos==23){display.setTextColor(BLACK,WHITE);} else{display.setTextColor(WHITE,BLACK);}// If position 23 is selected, highlight text in that position
  display.setCursor(96,32); display.println(nowYr);// Set cursor to after "/" and display the year the clock should be
  if (pos==3){display.setTextColor(BLACK,WHITE);} else{display.setTextColor(WHITE,BLACK);}// If position 3 is selected, highlight text in that position
  display.setCursor(0,48); display.println("ENTER SET");// Set cursor to fourth line and display "ENTER SET"
  numberCorrect();// Correct all displayed numbers  
  display.display();// Update the display
}
void calibrationMenuDisplay(){// Text to display in the initiate menu
  display.clearDisplay();// Clear the display  
  display.setTextSize(2);// Set the text size to 2 
  if (pos==0){display.setTextColor(BLACK,WHITE);} else{display.setTextColor(WHITE,BLACK);}// If position 0 is selected, highlight text in that position
  display.setCursor(0,0); display.println("<CALIB>");// Set cursor to first line and display "<CALIB>"
  if (pos==1){display.setTextColor(BLACK,WHITE);} else{display.setTextColor(WHITE,BLACK);}// If position 1 is selected, highlight text in that position
  display.setCursor(0,16); display.println("RUN CAL");// Set cursor to second line and display "RUN CAL"
   if (pos==2){display.setTextColor(BLACK,WHITE);} else{display.setTextColor(WHITE,BLACK);}// If position 2 is selected, highlight text in that position
  display.setCursor(0,32); display.println("VOL:");// Set cursor to third line and display "VOL:"
  if (pos==21){display.setTextColor(BLACK,WHITE);} else{display.setTextColor(WHITE,BLACK);}// If position 21 is selected, highlight text in that position
  display.setCursor(48,32); display.println(calibrationVolume);// Set cursor to after "VOL:" and display the calibration volume collected
  if (pos==3){display.setTextColor(BLACK,WHITE);} else{display.setTextColor(WHITE,BLACK);}// If position 3 is selected, highlight text in that position
  display.setCursor(0,48); display.println("ENTER SET");// Set cursor to fourth line and display "ENTER SET"
  numberCorrect();// Correct all displayed numbers
  display.display();// Update the display
}
void primeMenuDisplay(){// Text to display in the initiate menu
  display.clearDisplay();// Clear the display  
  display.setTextSize(2);// Set the text size to 2 
  if (pos==0){display.setTextColor(BLACK,WHITE);} else{display.setTextColor(WHITE,BLACK);}// If position 0 is selected, highlight text in that position
  display.setCursor(0,0); display.println("<PRIME>");// Set cursor to first line and display "<PRIME>"
  if (pos==1){display.setTextColor(BLACK,WHITE);} else{display.setTextColor(WHITE,BLACK);}// If position 1 is selected, highlight text in that position
  display.setCursor(0,16); display.println("RUN WATER");// Set cursor to second line and display "RUN WATER"
  if (pos==2){display.setTextColor(BLACK,WHITE);} else{display.setTextColor(WHITE,BLACK);}// If position 2 is selected, highlight text in that position
  display.setCursor(0,32); display.println("RUN SYR.");// Set cursor to second line and display "RUN SYR."
  display.display();// Update the display
}

//<-------------------------------------------------------------------------------------------------------------------------------------------------------------->
//<-------------------------------------------------------------------SAMPLING MODE------------------------------------------------------------------------------>
//<-------------------------------------------------------------------------------------------------------------------------------------------------------------->
void samplingMode(){
  getNowSecTime();
  samplingDisplay();
  //PUMP A
  if (aAlarmFlag == 0){
    if ((aSecTime != 0)&&(nowSecTime > aSecTime)){aAlarmFlag = 1;}
    else{aAlarmFlag = 0;}
  }
  if (aAlarmFlag == 1){
    getVoltage();
    logData(); 
    digitalWrite (pumpAEncPowerPin, HIGH);
    pumpAEnc.write(0);
    pumpAVolume = 0.0;
    
    digitalWrite (syringeEncPowerPin, HIGH);
    syringeEnc.write(0);
    syringeVolume = 0.0;
 
    digitalWrite(pumpAPin, HIGH);
    aAlarmFlag = 2; 
  }
  if (aAlarmFlag == 2){
    pumpAVolume = abs(pumpAEnc.read()/pumpAGearRatio*calibrationVolume/calibrationCount); //Equation for determining sample pump run time utilizing the calibration step and related encoder count   
    if (pumpAVolume > sampleVolume){
      digitalWrite(pumpAPin, LOW);
      digitalWrite (syringePin, HIGH);
      aAlarmFlag = 3;}
    else {aAlarmFlag = 2;}
  }      
  if (aAlarmFlag ==3){
    syringeVolume = abs(syringeEnc.read()/syringeGearRatio*calibrationVolume/calibrationCount); //Equation for determining syringe pump run time using the sample pump calibration
    if (syringeVolume > syringeDispenseVolume){
      digitalWrite(syringePin, LOW);
      digitalWrite(pumpAEncPowerPin, LOW);
      digitalWrite(syringeEncPowerPin, LOW);
      if (sampleMode == 1){aAlarmFlag = 4;}
      if (sampleMode == 0){
        aSecTime = (aSecTime + 86400);// Add 24 hrs to the time that A fires (aSecTime)
        tmElements_t updateAtm;//This is a time elements variable named updateAtm
        breakTime(aSecTime, updateAtm);// Break aSecTime into the time elements variable updateTm
        aHr = updateAtm.Hour;// Name the hour that pump A needs to fire aHr 
        aMin = updateAtm.Minute;// Name the minute that pump A needs to fire aMin 
        aSec = updateAtm.Second;// Name the second that pump A needs to fire aSec 
        aDay = updateAtm.Day;// Name the day that pump A needs to fire aDay
        aMon = updateAtm.Month;// Name the month that pump A needs to fire aMon 
        aYr = updateAtm.Year-30;// Name the year that pump A needs to fire aYr. Minus thirty because value is years since 1970, and we are dealing with years since 2000
        aAlarmFlag = 0; 
      }
    }
    else {aAlarmFlag = 3;}
  }
}

void checkSleep(){
  if ((aAlarmFlag == 0)&&(aSecTime>nowSecTime+60)){goToSleep();}
  if (aAlarmFlag == 4){goToSleep();}
}

void goToSleep(){
  long sleepSecTime;
  if (aAlarmFlag != 4){sleepSecTime = aSecTime - nowSecTime;}
  if (aAlarmFlag == 4){sleepSecTime = 2628000;}//sleep one months, will wake and go to sleep again after that one month

  hrToSleep = int(sleepSecTime/3600);// calculates the hours to sleep
  minToSleep = int((sleepSecTime-(3600*hrToSleep))/60);// calculates the remaining minutes to sleep
  secToSleep = int(sleepSecTime-(3600*hrToSleep)-(60*minToSleep));// calculates the remaining seconds to sleep
  
  digitalWrite(oledPowerPin, LOW);// Turn off the power to the OLED
  digitalWrite(irPowerPin, LOW);// Turn off the power to the IR sensor
  alarm.setRtcTimer(hrToSleep, minToSleep, secToSleep);// Sets the RTC TIMER with the appropriate amount of hour, min, sec to sleep
  delay (100);
  int who; 
  who = Snooze.sleep( config_teensy40 ); //Sleep seems to be the only snooze library that works easily with 4.1 or maybe our code
  //need to par down code and see deepsleep and hibernate still dont work.
  //who = Snooze.hibernate( config_teensy40 );// wake up the Teensy and indentify who woke it up
  if (who == REED_INTERRUPT_PIN){
    delay(500);
    if (digitalRead(REED_INTERRUPT_PIN)==HIGH){
      delay(500);
      if (digitalRead(REED_INTERRUPT_PIN)==HIGH){
        digitalWrite(oledPowerPin, HIGH);// Power on the oled    
        digitalWrite(irPowerPin, HIGH);// Power on the IR sensor
        display.begin(SSD1306_SWITCHCAPVCC, 0x3C);// Start the OLED back up again
        display.clearDisplay(); display.setTextSize(2); display.setTextColor(WHITE,BLACK); display.setCursor(0,0); display.println("REED!");// Put "Interrupt" on the first line of the OLED
        display.display();// Update the OLED display
        Serial.begin(9600);//Troubleshoot
        menu = 0; pos = 0;//Sets menu and position to 0
        delay(1000);  
      }
      else{menu = 7;}
    }
    else{menu = 7;}
  }
  else{//alarm pin wake up
    digitalWrite(oledPowerPin, HIGH);// Power on the oled    
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);// Start the OLED back up again
    menu = 7;//finishes out sleep and sends back to loop running sampling mode (case 7)
    delay(1000);    
  }
}

void samplingDisplay(){
  if (aAlarmFlag == 2){display.clearDisplay(); display.setTextSize(2); display.setTextColor(WHITE,BLACK); display.setCursor(0,0); display.println("PUMP A!");}// Display PUMP A
  if (aAlarmFlag == 3){display.clearDisplay(); display.setTextSize(2); display.setTextColor(WHITE,BLACK); display.setCursor(0,0); display.println("SYRINGE!");}// Display SYRINGE!  
  if (aAlarmFlag != 2 && aAlarmFlag != 3){display.clearDisplay(); display.setTextSize(2); display.setTextColor(WHITE,BLACK); display.setCursor(0,0); display.println("WAIT!");}// Display BOTH PUMPS
  display.display();// Update the OLED display
}

void initialAlarmFlag(){
  getNowSecTime();
  if (nowSecTime > aSecTime){aAlarmFlag = 4;}
  if (nowSecTime < aSecTime){aAlarmFlag = 0;}
}


//<-------------------------------------------------------------------------------------------------------------------------------------------------------------->
//<--------------------------------------------------------------------sd CARD CODE------------------------------------------------------------------------------>
//<-------------------------------------------------------------------------------------------------------------------------------------------------------------->
size_t readField(FsFile* sampleParam, char* str, size_t size, const char* delim) {
  char ch;
  size_t n = 0;
  while ((n + 1) < size && sampleParam->read(&ch, 1) == 1) {
    if (ch == '\r') {continue;}// Delete Carriage Return.
    str[n++] = ch;
    if (strchr(delim, ch)) {break;}
  }
  str[n] = '\0';
  return n;
}

void readSampleParamArray(){
  sampleParam = SD.sdfs.open("SAMPLEPARAM.TXT", FILE_READ);// Open SAMPLEPARAM.txt file on sd card
  if (!sampleParam){Serial.println("open failed");}// If opening failed, write "open failed" to the serial port for debugging
  //sampleParam.rewind();// Rewind the file for read.
  // Array for data.
  int i = 0;// Index for array rows
  int j = 0;// Index for array columns
  size_t n;// Length of returned field with delimiter.
  char str[20];// Longest field with delimiter and zero byte.
  char *ptr;// Test for valid field.
  // Read the file and store the data.
  for (i = 0; i < SAMPLE_PARAM_ROW; i++) {// Go through each row of the sampleParam.txt file
    for (j = 0; j < SAMPLE_PARAM_COL; j++) {// Within each row, go through each column of data
      n = readField(&sampleParam, str, sizeof(str), ",\n");// Read each character
      if (n == 0) {Serial.println("Too few lines");}
      sampleParamArray[i][j] = strtol(str, &ptr, 10);
      if (ptr == str) {Serial.println("bad number");}
      while (*ptr == ' ') {ptr++;}
      if (*ptr != ',' && *ptr != '\n' && *ptr != '\0') {Serial.println("extra characters in field");}
      if (j < (SAMPLE_PARAM_COL-1) && str[n-1] != ',') {Serial.println("line with too few fields");}
    }
    if (str[n-1] != '\n' && sampleParam.available()) {Serial.println("missing endl");}    
  }
  sampleParam.sync(); delay(500);
  //Read in data
  sampleMode = sampleParamArray[0][0]; sampleVolume = sampleParamArray[0][1]; calibrationVolume = sampleParamArray[0][2]; calibrationCount = sampleParamArray[0][3]; aHr = sampleParamArray[0][4];aMin = sampleParamArray[0][5];aDay = sampleParamArray[0][6];aMon = sampleParamArray[0][7];aYr = sampleParamArray[0][8];// sampleMode, sampleVolume, calibrationVolume, calibrationCount, aHr, aMin, aDay, aMon, aYr 
}

void writeSampleParamArray(){
  sampleParam = SD.sdfs.open("SAMPLEPARAM.TXT", FILE_WRITE);// Open the SAMPLEPARAM.TXT file on the sd card
  if (!sampleParam) {Serial.println("open failed");}// If opening the file failed, write a open failed to serial debugging
  //CREATE SAMPLE PARAMETER STRINGS (one per line)
  String sampleParamString0 = String(sampleMode) +","+String(sampleVolume) +","+String(calibrationVolume) +","+String(calibrationCount) +","+ String(aHr) +","+String(aMin)+","+String(aDay)+","+String(aMon)+","+String(aYr)+"\r\n";// Create string of the second line of sample parameters
  //WRITE STRINGS TO sd CARD
  //sampleParam.rewind(); delay(50);// Rewind to the beginning of the SAMPLEPARAM.txt file
  sampleParam.print(sampleParamString0); delay(50);// Overwrite the first line of the SAMPLEPARAM.txt file
  sampleParam.sync(); delay(500);// Sync the SAMPLEPARAM.txt file

  Serial.println(sampleParamString0);//Troubleshoot

  display.clearDisplay(); display.setTextSize(2); display.setTextColor(WHITE,BLACK); display.setCursor(0,0); display.println(" <SAVED>");// Put "<SAVED>" on the first line
  display.display();// Update the display
  delay(1000);
}

void logData(){// Called to log  data
  getVoltage();// Read the current voltage
  dataLog = SD.sdfs.open("dataLog.TXT", FILE_WRITE);// Open dataLog.TXT file
  delay(100);
  if (!dataLog){Serial.println("open failed");}// If open dataLog.txt file doesn't open, write an error to the serial for debugging
  String dataLogString = String(nowHr) + ":" + String(nowMin) + ":" + String(nowSec) + "," + String(nowDay) + "/" + String(nowMon) + "/" + String(nowYr+2000) + "," + String(voltage);// Create a data logging string
  dataLog.println(dataLogString); delay(50);// Write the datalog string to the dataLog.txt file
  Serial.print("Data log string is: "); Serial.println(dataLogString);//Troubleshoot
  dataLog.sync();// Sync the dataLog.txt file
  delay(500);
}

void readFiles(){
    // open the file for reading:
  sampleParam = SD.sdfs.open("SAMPLEPARAM.txt");
  if (sampleParam) {
    Serial.println("sampleParam.txt:");
    // read from the file until there's nothing else in it:
    while (sampleParam.available()) {
      Serial.write(sampleParam.read());
    }
    // close the file:
    sampleParam.close();
  } else {
    // if the file didn't open, print an error:
    Serial.println("error opening sampleParam.txt");
  }
 dataLog = SD.sdfs.open("DATALOG.txt");
  if (dataLog) {
    Serial.println("dataLog.txt:");
    // read from the file until there's nothing else in it:
    while (dataLog.available()) {
      Serial.write(dataLog.read());
    }
    // close the file:
    dataLog.close();
  } else {
    // if the file didn't open, print an error:
    Serial.println("error opening dataLog.txt");
  }
}
//<-------------------------------------------------------------------------------------------------------------------------------------------------------------->
//<-------------------------------------------------------------------------OTHER CODE--------------------------------------------------------------------------->
//<-------------------------------------------------------------------------------------------------------------------------------------------------------------->
void getVoltage(){// Called when we need to know the voltage of the battery pack
  int adcVoltage = adc->adc0->analogRead(voltageReadPin);// 
  rawVoltage = adcVoltage*3.3/adc->adc0->getMaxValue();// Raw voltage is equal to that read by the ADC times the max voltage, divided by the max ADC value
  voltage = 4.9*rawVoltage;// Voltage divider. R1 = 390k ohms and R2 = 100k ohms. 4.9 is variable for resistance of whole system
  //the resistance variable used to be 3.9, maybe issue with resistors that it's 4.9 now, or just difference in the ADC library update
}
void numberCorrect(){
//Settings correct
  if (sampleMode>1){sampleMode = 0;}// If sample mode is greater than 1, make it 0
  if (sampleMode<0){sampleMode = 1;}// If sample mode is less than 0, make it 1
  if (sampleVolume<10){sampleVolume = 2000;}// If sample volume is less than 10, make it 2000
  if (sampleVolume>2000){sampleVolume = 10;}// If sample volume is greater than 2000, make it 10
  if (calibrationVolume<1){calibrationVolume = 25;}// If sample volume is less than 1, make it 25
  if (calibrationVolume>25){calibrationVolume = 1;}// If sample volume is greater than 25, make it 1
//Date and Time Correct
  if (((nowYr != 20)&&(nowYr != 24)&&(nowYr != 28)&&(nowYr != 32)&&(nowYr != 36)&&(nowYr != 40)&&(nowYr != 44))&&(nowMon==2)&&(nowDay > 28)){nowDay=1;}// Sets max day in Feb for non leap years up to 2044, and loops to 1 if you exceed
  if (((nowYr == 20)||(nowYr == 24)||(nowYr == 28)||(nowYr == 32)||(nowYr == 36)||(nowYr == 40)||(nowYr == 44))&&(nowMon==2)&&(nowDay > 29)){nowDay=1;}// Sets max day in Feb for leap years up to 2044, and loops to 1 if you exceed
  if (((nowMon == 4)||(nowMon == 6)||(nowMon == 9)||(nowMon == 11))&&(nowDay > 30)){nowDay=1;}// Sets max days for April, June, September, November, and loops to 1 if you exceed
  if (((nowMon == 1)||(nowMon == 3)||(nowMon == 5)||(nowMon == 7)||(nowMon == 8)||(nowMon == 10)||(nowMon == 12))&&(nowDay > 31)){nowDay=1;}// Sets max days for January, March, May, July, August, October, December, and loops to 1 if you exceed
  if (((nowYr != 20)&&(nowYr != 24)&&(nowYr != 28)&&(nowYr != 32)&&(nowYr != 36)&&(nowYr != 40)&&(nowYr != 44))&&(nowMon==2)&&(nowDay < 1)){nowDay=28;}// Sets max day in Feb for non leap years up to 2044, and loops to max if you go lower than 1
  if (((nowYr == 20)||(nowYr == 24)||(nowYr == 28)||(nowYr == 32)||(nowYr == 36)||(nowYr == 40)||(nowYr == 44))&&(nowMon==2)&&(nowDay < 1)){nowDay=29;}// Sets max day in Feb for leap years up to 2044, and loops to max if you go lower than 1
  if (((nowMon == 4)||(nowMon == 6)||(nowMon == 9)||(nowMon == 11))&&(nowDay < 1)){nowDay=30;}// Sets max days for April, June, September, November, and loops to max if you go lower than 1
  if (((nowMon == 1)||(nowMon == 3)||(nowMon == 5)||(nowMon == 7)||(nowMon == 8)||(nowMon == 10)||(nowMon == 12))&&(nowDay < 1)){nowDay=31;}// Sets max days for January, March, May, July, August, October, and loops to max if you go lower than 1
  if (nowYr < 0){nowYr=0;}// Sets minimum year to 0 or 2000
  if (nowMon > 12){nowMon=1;}// If max month (December) is exceeded, loop to January
  if (nowMon < 1){nowMon=12;}// If min month (January) is exceeded, loop to December
  if (nowHr>23){nowHr=0;}// If hr is greater than 23, set to 0
  if (nowHr<0){nowHr=23;}// If hr is less than 0, set to 23
  if (nowMin>59){nowMin=0;}// If min is greater than 59, set to 0
  if (nowMin<0){nowMin=59;}// If min is less than 0, set to 59
  if (nowSec>59){nowSec=0;}// If sec is greater than 59, set to 0
  if (nowSec<0){nowSec=59;}// If sec is less than 0, set to 59
//Pump A Correct
  if (((aYr != 20)&&(aYr != 24)&&(aYr != 28)&&(aYr != 32)&&(aYr != 36)&&(aYr != 40)&&(aYr != 44))&&(aMon==2)&&(aDay > 28)){aDay=1;}// Sets max day in Feb for non leap years up to 2044, and loops to 1 if you exceed
  if (((aYr == 20)||(aYr == 24)||(aYr == 28)||(aYr == 32)||(aYr == 36)||(aYr == 40)||(aYr == 44))&&(aMon==2)&&(aDay > 29)){aDay=1;}// Sets max day in Feb for leap years up to 2044, and loops to 1 if you exceed
  if (((aMon == 4)||(aMon == 6)||(aMon == 9)||(aMon == 11))&&(aDay > 30)){aDay=1;}// Sets max days for April, June, September, November, and loops to 1 if you exceed
  if (((aMon == 1)||(aMon == 3)||(aMon == 5)||(aMon == 7)||(aMon == 8)||(aMon == 10)||(aMon == 12))&&(aDay > 31)){aDay=1;}// Sets max days for January, March, May, July, August, October, December, and loops to 1 if you exceed
  if (((aYr != 20)&&(aYr != 24)&&(aYr != 28)&&(aYr != 32)&&(aYr != 36)&&(aYr != 40)&&(aYr != 44))&&(aMon==2)&&(aDay < 1)){aDay=28;}// Sets max day in Feb for non leap years up to 2044, and loops to max if you go lower than 1
  if (((aYr == 20)||(aYr == 24)||(aYr == 28)||(aYr == 32)||(aYr == 36)||(aYr == 40)||(aYr == 44))&&(aMon==2)&&(aDay < 1)){aDay=29;}// Sets max day in Feb for leap years up to 2044, and loops to max if you go lower than 1
  if (((aMon == 4)||(aMon == 6)||(aMon == 9)||(aMon == 11))&&(aDay < 1)){aDay=30;}// Sets max days for April, June, September, November, and loops to max if you go lower than 1
  if (((aMon == 1)||(aMon == 3)||(aMon == 5)||(aMon == 7)||(aMon == 8)||(aMon == 10)||(aMon == 12))&&(aDay < 1)){aDay=31;}// Sets max days for January, March, May, July, August, October, and loops to max if you go lower than 1
  if (aYr < 0){aYr=0;}// Sets minimum year to 0 or 2000
  if (aMon > 12){aMon=1;}// If max month (December) is exceeded, loop to January
  if (aMon < 1){aMon=12;}// If min month (January) is exceeded, loop to December
  if (aHr>23){aHr=0;}// If hr is greater than 23, set to 0
  if (aHr<0){aHr=23;}// If hr is less than 0, set to 23
  if (aMin>59){aMin=0;}// If min is greater than 59, set to 0
  if (aMin<0){aMin=59;}// If min is less than 0, set to 59
}

void runCalibration(){ //Calibration step uses encoder count and timed volume collection to determine a volume per rotation calibration for sampling volumes
  digitalWrite (pumpAEncPowerPin, HIGH); //Power on encoder pin
  pumpAEnc.write(0); //reset encoder pin
  calibrationCount = 0.0; //reset pump A encoder pin count to 0
  digitalWrite(pumpAPin, HIGH); delay(30000);// Power on Pump A for 30 seconds
  calibrationCount = pumpAEnc.read()/pumpAGearRatio; //Measure the count of the encoder after 30 seconds of running pump
  digitalWrite(pumpAEncPowerPin, LOW); //turn off Pump A encoder pin
  digitalWrite(pumpAPin, LOW);  // turn off Pump A
  writeSampleParamArray(); //Record the encoder count to sd card
}

void runPump(){
  digitalWrite(pumpAPin, HIGH);
  display.clearDisplay(); display.setTextSize(2); display.setTextColor(WHITE,BLACK); display.setCursor(0,0); display.println("PUMP A!");// Display PUMP A
  display.display();// Update the OLED display    
  delay(500);
  waitforit = 0;
  IrReceiver.resume();// Continue receiving looking for IR signals
  while (waitforit==0){
    if(IrReceiver.decode()){// If there is an IR signal, return true and store signal in "results"
           if(IrReceiver.decodedIRData.decodedRawData == ENTERIR){waitforit = 1; digitalWrite(pumpAPin, LOW);}
       IrReceiver.resume();// Continue receiving looking for IR signals
     } 
  }
}

void runSyringe(){
  digitalWrite(syringePin, HIGH);
  display.clearDisplay(); display.setTextSize(2); display.setTextColor(WHITE,BLACK); display.setCursor(0,0); display.println("Syringe!");// Display PUMP A
  display.display();// Update the OLED display    
  delay(500);
  waitforit = 0;
  IrReceiver.resume();// Continue receiving looking for IR signals
  while (waitforit==0){
    if(IrReceiver.decode()){// If there is an IR signal, return true and store signal in "results"
           if(IrReceiver.decodedIRData.decodedRawData == ENTERIR){waitforit = 1; digitalWrite(syringePin, LOW);}
       IrReceiver.resume();// Continue receiving looking for IR signals
     } 
  }
}

//<-------------------------------------------------------------------------------------------------------------------------------------------------------------->
//<--------------------------------------------------------------------TIME CODE--------------------------------------------------------------------------------->
//<-------------------------------------------------------------------------------------------------------------------------------------------------------------->
time_t getTeensy3Time(){// Time variable getTeensy3Time
  return Teensy3Clock.get();// Get the time from the Teensy clock
}

void calcUpdateSecTime(){
  tmElements_t updateNowTm;// This is a time elements variable named updateNowTm with each of the different elements as below
  updateNowTm.Hour = nowHr;// Makes the hour of updateNowTm equal to nowHr
  updateNowTm.Minute = nowMin;// Makes the minute of updateNowTm equal to nowMin
  updateNowTm.Second = nowSec;// Makes the second of updateNowTm equal to nowSec
  updateNowTm.Day = nowDay;// Makes the day of updateNowTm equal to nowDay
  updateNowTm.Month = nowMon;// Makes the month of updateNowTm equal to nowMon
  updateNowTm.Year = nowYr+30;// Makes the year of updateNowTm equal to nowYr + 30 (because time elements deal with seconds since 1970 and nowYr is from 2000)
  updateSecTime = makeTime(updateNowTm);// Compile all time elements into seconds since 1970 and name it updateSecTime
}

void getNow(){
  setSyncProvider(getTeensy3Time);// Get the time from the teensy NOW!
  nowSec = second();// Name seconds nowSec
  nowMin = minute();// Name minutes nowMin
  nowHr = hour();// Name hour nowHr
  nowDay = day();// Name day nowDay
  nowMon = month();// Name month nowMon
  nowYr = (year()-2000);// Name years since 2000 nowYr
}

void getNowSecTime(){
  tmElements_t nowTm;//This is a time elements variable named nowTm with each of the different elements as below
  nowTm.Hour = nowHr;// Makes the hour of nowTm equal to nowHr
  nowTm.Minute = nowMin;// Makes the minute of nowTm equal to nowMin
  nowTm.Second = nowSec;// Makes the second of nowTm equal to nowSec
  nowTm.Day = nowDay;// Makes the day of nowTm equal to nowDay
  nowTm.Month = nowMon;// Makes the month of nowTm equal to nowMon
  nowTm.Year = nowYr+30;// Makes the year of nowTm equal to nowYr + 30 (because time elements deal with seconds since 1970 and nowYr is from 2000)
  nowSecTime = makeTime(nowTm);// Compile all time elements into seconds since 1970 and name it nowSecTime
}

void calculateInitialAlarmSecondTime(){
  tmElements_t aTm;//This is a time elements variable named aTm with each of the different elements as below
  aTm.Hour = aHr;// Makes the hour of aTm equal to aHr
  aTm.Minute = aMin;// Makes the minute of aTm equal to aMin
  aTm.Second = 0;// Makes the second of aTm equal to zero
  aTm.Day = aDay;// Makes the day of aTm equal to aDay
  aTm.Month = aMon;// Makes the month of aTm equal to aMon
  aTm.Year = aYr+30;// Makes the year of aTm equal to aYr + 30 (because time elements deal with seconds since 1970 and aYr is from 2000)
  aSecTime = makeTime(aTm);// Compile all time elements into seconds since 1970 and name it aSecTime
}
