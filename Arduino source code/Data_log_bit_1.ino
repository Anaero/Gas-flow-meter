 //
// Gas flow data logging program
//
// Bittern build 
//
// This version is for use with the Mark 2 latch shield, the Adafruit MAX31855K thermocouple amp and type K thermocouple, the BMP280 pressure sensor, the 
// Adafruit micro-SD breakout board and the Adafruit 16x2 RGB LCD display.
// I2C devices: BMP280 pressure sensor addr - 0x77; RGB LCD display - addr 0x20
// micro SD breakout uses hardware SDI (i/o pins 50, 51, 52, 53) with digital output 53 as Card Select
// MAX31855K thermocouple amp uses software SDI running on digital i/o pins 22-CLK, 24-CS and 26-DO (no DI pin needed)
// 
//
// 
//
// Time delay before attempting to clear any latches which have been set ~ 9 seconds
// ml/g output reported for inoc only channels as well as sample channels
// hour log and day log report both incremental and cumulative ml/g
// column headings only written to SD log files at very start of run
// inService channels now identified as inService(= 1), taken out of service by user in setup.csv (= 0), identified as stuck by Arduino (= -1)
// running monitor utility triggers reset attempt for any channels marked with inService = -1 (i.e. any which have been automatically removed from service by arduino)
// LED's on pins 13 (red), 12 (yellow), 11 (green)
// If temperature reported is below zero or below (disconected temp sensor) - last positive temp is used in calculations and alarm is set until problem clears
// Serial port data rate set to 57600 bps
// Serial timeout set to 500 ms
//
// Avocet build (retired 4/5/2017)
// time delay eolDelay introduced at the end of each line transmitted for compatibility with Bluetooth/slower machines
// dummy string added at laptop end to each line of setup data to ensure correct parsing when sample name starts with one or more digits
//
// Bittern build released 4/5/2017
// Build name displayed on LCD panel and sent to console at start up to aid identification/config management
// inoculumOnly variable changed from boolean to int to allow range check as startup data is transferred - helps to detect framing errors/range errors in setup data and/or parsing
// range checking on data transferred from laptop implemented, warnings issued if InService is other than 0 or 1, inoculum only is other than 0 or 1, tumbler volume less than
// 4ml or greater than 14 ml, inoculum only channels have a non zero entry in the sample VS column - provides added protection against user error and framing error in data transfer
// '^' used as end of line delimiter to allow extraneous characters in setup file to be skipped to avoid framing errors
// auto-reset performed at every pingcount cycle on any channels marked as stuck by arduino (i.e. where inService[i] = -1) - only truly stuck channels will remain stuck
// support for fastGrab utility added - reads back eventlog and daily log only (filgrab still works and does full file transfer)
// hangover time can be adjusted from setup.csv file by replacing 'End of data' with an integer between 8 and 35 seconds - default setting is ~ 15 seconds
// 
//

#include <Wire.h>
#include <SPI.h>
#include <SD.h>
//#include "Adafruit_MCP9808.h"
#include <Adafruit_MAX31855.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_RGBLCDShield.h>
#include <utility/Adafruit_MCP23017.h>
#define MAXDO   26
#define MAXCS   24
#define MAXCLK  22
//
// set up variable types using the SD utility libraries
//
Sd2Card card;
SdVolume volume;
SdFile root;
//
// set up sensor objects
// 
Adafruit_MAX31855 thermocouple(MAXCLK, MAXCS, MAXDO); // set up thermocouple object
Adafruit_BMP280 bme;                                  // set up BMP280 pressuere sensor object (using I2C bus option)
Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();  // set up LCD shield object
//
// set up i/o channels and arrays
//
const int chipSelect = 53;
byte Q[17];                   // mapping between between latch outputs (active high)and Arduino inputs
byte R[17];                   // mapping between Arduino outputs and latch resets (active low)
float sampleVS[16];           // mass of volatile solids in each sample under test (user provided)
float inocVS[16];             // mass of volatile solids from inoculum in each sample (user provided)
String alphaName1, alphaName2, alphaName3, alphaName4, alphaName5, alphaName6, alphaName7, alphaName8;           //set up string objects
String alphaName9, alphaName10, alphaName11, alphaName12, alphaName13, alphaName14, alphaName15, alphaName;      //for alphanumeric ch names
String discard;                // used to force parsing of setup data at end of line
long ChannelCount[17];        // used to store cumulative total of number of tumbles recorded per channel
float tumblerVol[16];         // calibrated volume of each tumbler
float totalVol[16];           // used to store cumulative total volume of gas collected on each channel at STP
float volThisTip[16];         // volume of gas at STP from the most recent tip 
float volThisDay[16];         // running total of volume of gas at STP during current day 
float volThisHour[16];        // running total of volume of gas at STP during current hour
int inService[17];            // marks channels as in service or out of service (0 = not in service, 1 = in service, -1 = removed from service following reset fail)
boolean resetFlag[17];        // used to indicate which channels (if any) need to be queued for reset after each latch scan
boolean delayFlag = false;    // binary flag to indicate at least one channel in need of reset
int inoculumOnly[16];         // marks those channels which contain only innoculum - use int rathern than bool to help detect errors in setup data transfer/parsing 
boolean gotTemp = false;      // flag to bypass temp get if just got
boolean gotPress = false;     // flag to bypass press get if just got
float netVol_per_gram[16];    // total volume less average of inoc only volume divided my mass of vol solids (so ml/g)
float netVol_per_gramDay[16]; // netVol_per_gram calculatede for this day
float netVol_per_gramHour[16];// netVol_per_gram calculated for this hour
int tipsThisDay[16];          // running total of tumbler tips during current day
int tipsThisHour[16];         // running total of tumbler tips during current hour
unsigned long time_offset;    // records time at which run commences
unsigned long timex;          // time in ms since start of run
double dtemp;
float temperature;            // temp reading
float baroPress;              // barometer presssure reading
float pressLast;              // stores last recorded valid pressure in case of sensor failure
float tempLast ;              // stores last recorded valid temp in case of sensor failure
float contAve;                // inoculum only output in ml per gram for each inoc only cell averaged over all inoc only channels
int n;                        // counter used to calculate final contAve average
int goFlag = 0;               // used for handshake between Arduino and Phython front end
int startMode = 1000;         // 1000 = normal cold start; 500 = recovery after unwanted reset (not programmed yet)
int alphaIndex;               // used to pass index to get_alphaName function
byte R_LED = 13; byte Y_LED = 12; byte G_LED = 11;  //assignement of Arduino pins to diagnostic LEDs
int ByteReceived = 0;
const int chipSelect1 = 53;
const char comma=(',');
int cardFound;
int cardPresent;
int snapTest;
long days=0;
int newDay=0;
int oldDay=0;
long hours=0;
int newHour=0;
int oldHour=0;
long mins=0;
int newMins=0;
int oldMins=0;
long secs=0;
int oldSecs=0;
int newSecs=0;
int secTest=0;
int incomingByte=0;
int  pingCount = 0;
int dispCount;
int hangTime = 15;                               // length of pause before attempting to clear latches that have been set
int hangTimeReset; 
int fastGrab = 0;                                // used to select full file transfer (filegrab utiltity) or eventlog and daily only (fastgrab utility)
File fSetup;
File fEventLog;
File fDaily;
File fHourly;
File fSnap;
byte sdByte;
int eolDelay=65;                                 // end of line delay to allow bluetooth/laptop to process
String fnameToCheck;
String fnameSetup = "setup.csv";
String fnameEvent = "eventlog.csv";
String fnameDaily = "daily.csv";
String fnameHourly = "hourly.csv";
String fnameSnap = "snapshot.csv";
String fnameError = "errorlog.csv";
//
//
//
void setup() {
//
// setup i/0 channel assignments
//  
   Q[1]=54, Q[2]=55, Q[3]=56, Q[4]=57, Q[5]=58, Q[6]=59, Q[7]=60, Q[8]=61;          // feed to arduino from latches 
   Q[9]=62, Q[10]=63, Q[11]=64, Q[12]=65, Q[13]=66, Q[14]=67, Q[15]=68, Q[16]=69;   // corresponds to A0 through A15
//
//
//
   R[1]=23, R[2]=25, R[3]=27, R[4]=29, R[5]=31, R[6]=33, R[7]=35, R[8]=37;          // latch reset lines
   R[9]=39, R[10]=41, R[11]=43, R[12]=45, R[13]=47, R[14]=49, R[15]=5, R[16]=7; 

//
// Set up pins as inputs and outputs
//
for (int i=1; i<=15; i++){
   pinMode(R[i], OUTPUT);
   pinMode(Q[i], INPUT);
   digitalWrite(R[i], HIGH);   digitalWrite(R[i], LOW);   digitalWrite(R[i], HIGH);
   }
//
//
//
   Serial.begin(57600);
   Serial.setTimeout(500);             // set serial timeout to 0.5 second - increase this if using manual input instead of Python utilities
   lcd.begin(16, 2); 
//
   pinMode(R_LED, OUTPUT);                                                         // set up pins to LEDs as outputs - nut used at the moment
   pinMode(Y_LED, OUTPUT);  
   pinMode(G_LED, OUTPUT); 

   digitalWrite(R_LED, HIGH);                                                       // turn on all LEDs
   digitalWrite(Y_LED, HIGH);
   digitalWrite(G_LED, HIGH);
// 
// program held here until something received on serial port
//
//
// Send reset message every 100ms until incoming data received
//   
   while(goFlag == 0) {
       Serial.println(F("Arduino has been reset"));
       delay(100);
       goFlag = Serial.available();
       }
   lcd.print("hello :-)       ");
   lcd.setCursor(0,1);
   lcd.print("getting ready...");
   delay(2000);
//
//
//
   lcd.setCursor(0,0);                                                          // dispaly bild name
   lcd.print("running         ");
   lcd.setCursor(0,1);
   lcd.print("Bittern build   ");
   delay(5000);
//      
   startMode = Serial.parseInt();                                               // read start mode number supplied by python utility
//       
   Serial.println();
   Serial.println(F("Arduino running - starting self test"));
   Serial.println();
   Serial.println(F("Running Bittern software build"));                         // display software build name
   Serial.println();
// 
   lcd.setCursor(0,0);                                                          // set up the LCD's number of columns and rows: 
   lcd.print("starting self   ");
   lcd.setCursor(0,1);
   lcd.print("test            ");
//
/*    code for MCP9808 I2C temp sensor
      if (!tempsensor.begin()) {                                                   // open temp sensor and check is OK
      Serial.println(F("Temperature sensor not responding"));
      while (1);                                                                // program will loop here continuously if no response from temp sensor
      }
   else {
       Serial.println(F("Temperature sensor OK"));       
       tempsensor.shutdown_wake(0);                                             // wake up temp sensor - required before reading temp
       delay(10);
       temperature = tempsensor.readTempC();
       delay(250);                                                              // give sensor time to respond!
       Serial.print(F("Temp: ")); Serial.print(temperature); Serial.println("C");                                        
       tempsensor.shutdown_wake(1);                                            // shutdown temp sensor to reduce power consumption       
       }
//
   delay(250);  // give time for temp sensor shutdown messaging to complete
*/
//
// Check thermocouple
//
   Serial.print("Internal Temp = ");
   Serial.println(thermocouple.readInternal());
   dtemp = thermocouple.readCelsius();
   if (isnan(dtemp)) {
     Serial.println(F("Thermocouple not registering - please check"));
   } else {
     temperature = (float) dtemp;
     Serial.print("Thermocouple reading is "); 
     Serial.print(temperature);
     Serial.println( "C");
   }
//
//
//
   if (!bme.begin()) {                                                              // open pressure sensor and check is OK
       Serial.println(F("Barometer not responding"));
       digitalWrite(R_LED, HIGH); digitalWrite(Y_LED, LOW);   digitalWrite(G_LED, LOW);   // set red LED to indicate problem
       lcd.setCursor(0,0);                                                                // set up the LCD's number of columns and rows: 
       lcd.print("problem with    ");
       lcd.setCursor(0,1);
       lcd.print("barometer       ");
       while (1);                                                                   // program will loop here in event of barometer failure
       }
   else{     
      Serial.println(F("Barometer OK"));
      bme.begin();                                                                  // re initialise pressure sensor
      delay(15);
      baroPress = bme.readPressure();                                               // take first reading to force BMP internals and discard  
      delay(15);
      baroPress = bme.readPressure();                                               // valid pressure reading
      delay(15);      
      baroPress = baroPress / 100; 
      Serial.print(F("Air pressure is: ")); Serial.print(baroPress); Serial.println(" hPa");
      }

//
//
//
   cardPresent = checkingCard(chipSelect1);              //cardPresent = 1 if card there. 0 if no card
//   Serial.println (cardPresent);
   Serial.println();
   Serial.println(F("Opening SD card for file read/write")); 
   if (!SD.begin(chipSelect1)) {
      Serial.println(F("SD card open failed"));
      return;
      }
   Serial.println();
   Serial.println(F("SD card opened OK"));
//
// Check whether files already exist
//
   Serial.println();
   Serial.println(F("*** This program loads a new setup file and starts a complete new run ***"));
   Serial.println(F("*** if you continue, existing log files on the SD card will be deleted ***"));
   Serial.println();
   Serial.println(F("*** Do you want to save existing log files to your computer before continuing? ***"));
   while(Serial.available() == 0) {  
      }
   incomingByte = Serial.read();   
   if (incomingByte == int('Y') || incomingByte == int('y')) {   // run file writeback if 'Y' or 'y' received
      fastGrab = 0;                       // set fastGrab to zero to ensure all files written back
      file_writeback();
      } 
   Serial.println();
   Serial.println(F("********"));
   Serial.println();
   if(startMode == 1000) {               // if this is a standard cold start now delete log files on SD card
      fnameToCheck = fnameSetup;
      go_check_file();
      fnameToCheck = fnameEvent;
      go_check_file();   
      fnameToCheck = fnameDaily;
      go_check_file();   
      fnameToCheck = fnameHourly;
      go_check_file();
      fnameToCheck = fnameSnap;
      go_check_file();
   }
   Serial.println();
   Serial.println(F("********"));
//
// now open and then close all files - this will create an empty file with the right name with text column headings
// and set up the file handles for use later on
//   
   if(startMode == 1000) {                // if this is a standard cold start, check files amd write column headings
   fSetup = SD.open("setup.csv", FILE_WRITE);
   if (fSetup) {
      Serial.println();
      Serial.println(F("SD card - new setup.csv file opened ok"));       // print message if setup.csv opened OK     
      } 
   fSetup.close();
//   
   fEventLog = SD.open("eventlog.csv", FILE_WRITE);   
   if (fEventLog) {
      Serial.println(F("SD card - new eventlog.csv opened ok"));        // print message if eventlog.csv opened OK 
      fEventLog.print(F("Channel number,Name,Timestamp,Days,Hours,Mins,Temp (C),Press (hPA),Cumulative total tips ,Vol this tip (STP),"));
      fEventLog.println(F("Total Vol (STP),Tips this day,Vol this day (STP),Tips this hour, Vol this hour (STP),net vol per gram (ml/g)"));
      } 
   fEventLog.close();
//
   fDaily = SD.open("daily.csv", FILE_WRITE);  
   if (fDaily) {
      Serial.println(F("SD card - new daily.csv opened ok"));           // print message if daily.csv opened OK 
      } 
//   fDaily.print("Day: "); fDaily.print(days); fDaily.println(comma);
   fDaily.println(F("Channel number,Name,Timestamp,Days,Hours,Mins,In service,Tips this day, Vol this day (STP), Net vol this day (ml/g), Cumulative net vol (ml/g)")); 
   fDaily.close();
//
   fHourly = SD.open("hourly.csv", FILE_WRITE); 
   if (fHourly) {
      Serial.println(F("SD card - new hourly.csv opened ok"));          // print message if hourly.csv opened OK 
      } 
//   fHourly.print("Hour: "); fHourly.print(hours); fHourly.println(comma);
   fHourly.println(F("Channel number,Name,Timestamp,Days,Hours,Mins,In service,Tips this hour, Vol this hour (STP), Net vol this hour (ml/g), Cumulative net vol (ml/g)")); 
   fHourly.close();
//
   fSnap = SD.open("snapshot.csv", FILE_WRITE); 
   if (fSnap) {
      Serial.println(F("SD card - new snapshot.csv opened ok"));          // print message if hourly.csv opened OK 
      } 
   fSnap.close();
   Serial.println(); 
   } 
   Serial.println(F("********"));
   Serial.println();   
//
// initialise variables
//
   for(byte i=1; i <= 16; i++){
      if(i <= 15) {     
         ChannelCount[i] = 0;                                                          // zero out all channel event counters
         totalVol[i] = 0.0;                                                            // zero out total gas volume log
         volThisTip[i] = 0.0;                                                          // zero out vol this tip array
         volThisDay[i] = 0.0;
         volThisHour[i] = 0.0;
         tipsThisDay[i] = 0;
         tipsThisHour[i] = 0;
         netVol_per_gram[i] = 0.0;                                                     // zero out net gas volume per gramV         
         netVol_per_gramDay[i] = 0.0;                                                  // zero out log of net gas volume per hour per gram
         netVol_per_gramHour[i] = 0.0;                                                 // zero out log of net volume per day per gram
         inoculumOnly[i] = 0;                                                       
         }
      inService[i] = 1;                                                                // set all channels as in service for now
      resetFlag[i] = false;                                                            // clear all reset request flags
//
//
//    
      if (digitalRead(Q[i]) == LOW){
         Serial.print(F("Channel ")); Serial.print(i); Serial.println(F(" reset OK"));     // check that channel resets performed earlier have worked
         }
      else {
         digitalWrite(R[i], HIGH); digitalWrite(R[i], LOW); delay(1); digitalWrite(R[i], HIGH);    // if not then have second go at clearing latch
         if (digitalRead(Q[i]) != LOW){                                                            // if second attempt to clear latch didn't work - remove channel from service 
            inService[i] = -1;                                                                     // -1 indicates channel taken out of service by arduino s/w
            Serial.print(F("Problem with channel ")); Serial.print(i); Serial.println(F(" channel removed from service"));
            }
         }
      }
   Serial.println();
   Serial.println(F("Power on self test complete"));
   lcd.setCursor(0, 0);                                                                  // set up the LCD's number of columns and rows: 
   lcd.print("self test       ");
   lcd.setCursor(0,1);
   lcd.print("complete        ");

//
// now populate data arrays
//
   Serial.println(F("Press the enter key to start transfer of setup.csv file to Arduino"));
   Serial.println();
   while (Serial.available() == 0){           // waiting in loop for incoming character
      }   
   lcd.setCursor(0, 0);
   lcd.print("data transfer   ");
   lcd.setCursor(0,1);
   lcd.print("in progress...  ");
   pull_down_setup_data();
   hangTime = hangTime * 1000;                // set hangover time to ms
//
// copying setup data to SD card is included within in pull_down_setup_data() function          
//
   send_Setup_Data();                        // reads setup.csv from SD card and sends back to serial port
   lcd.setCursor(0, 0);
   lcd.print("data transfer   ");
   lcd.setCursor(0,1);
   lcd.print("complete        ");
//
// re-populate inService array to identify any stuck channels, as will have been overwritten
//
   for (int i=1; i<=15; i++){
      if ((digitalRead(Q[i]) != LOW) && (inService[i] == 1)) {                                       // if reset has failed first time around
         digitalWrite(R[i], HIGH); digitalWrite(R[i], LOW); delay(1); digitalWrite(R[i], HIGH);      // have second go at clearing latch
         delay(1);                                                                                   // short wait to allow latch reset to complete
         if (digitalRead(Q[i]) != LOW){                                                              // if attempt to clear didn't work - remove from service 
            inService[i] = -1; 
         } 
      }
   }   
// 
   Serial.println();
   while (Serial.available() > 0) {
      ByteReceived = Serial.read();     // make sure read buffer is empty before sending request    
      }
   Serial.println(F("Press the enter key to start data logging"));
//  
// wait for incoming charater
//  

   while (Serial.available() == 0){           // waiting in loop for incoming character
   }
   while (Serial.available() != 0) {
      ByteReceived = Serial.read();           // make sure read buffer is empty     
      }
   time_offset = millis();                                      //capture time in ms at start of run
   Serial.println(F("--- data log started ---"));
   Serial.println(); Serial.println(F("------------------------------------")); 
   Serial.println();
   lcd.setCursor(0, 0);                                         
   lcd.print("data log running");
   digitalWrite(R_LED, LOW); digitalWrite(Y_LED, LOW);   digitalWrite(G_LED, HIGH);   // turn off red and yellow LEDs - leave green on

}
//
// MAIN LOOP STARTS HERE
// Setup complete - proceed to main loop  
//
void loop() { 
   timex = (millis() - time_offset);                            // capture current time before scanning latches
   uptime();                                                    // takes 'timex' variable and convert elapsed run time to days and hours mins and secs  
   screenTime();                                                // call function to print time since reset to LCD screen
//
// send updated time over serial link every 10 seconds (could do with converting to a function to improve readability)
//
   secTest = (newSecs / 10) * 10;
   if ((newSecs != oldSecs) && (secTest == newSecs)) {
      if (days <=9) {
         Serial.print("0");
         }
      Serial.print(days);
      Serial.print(":");
      if (hours <=9) {
         Serial.print("0");
         }
      Serial.print(hours);
      Serial.print(":");
      if (mins <=9) {
        Serial.print("0");
        }
      Serial.print(mins);
      Serial.print(":");
      if (secs <=9) {
         Serial.print("0");
         }      
      Serial.println(secs);         
      }  
//
// Serial comms bit done
//     
   if (oldDay != newDay) {                                                                  // check whether daylog or hourlog need to be writted to SD card
      write_day_log();  
//
// zero out day counters
//
      for (byte i=1; i<=15; i++) {
         tipsThisDay[i] = 0;
         volThisDay[i] = 0;  
         }
      }
   if (oldHour != newHour) {      
      write_hour_log();  
//
// zero out hour counters
//            
      for (byte i=1; i<=15; i++) {
         tipsThisHour[i] = 0;
         volThisHour[i] = 0.0;  
         }
      snapTest = (newHour/4) * 4;
      if (snapTest == newHour) {
         write_snapshot();
        }   
      }
//
// Now scan all in service latches and update data log accordingly
//   
   for(byte i=1; i <= 15; i++) {                                                            // check each channel in turn to see if any have tripped
      if((inService[i]== 1) && (digitalRead(Q[i])== HIGH)) {                                // identifies in service channels which have been triggered
         lcd.setCursor(0, 1);
         lcd.print("event on ch "); lcd.print(i);
         digitalWrite(Y_LED, HIGH);
         ChannelCount[i] = ChannelCount[i] + 1;                                             // increment relevant channel activity counter
         tipsThisDay[i] = tipsThisDay[i] + 1;
         tipsThisHour[i] = tipsThisHour[i] + 1;
         resetFlag[i] = true;                                                               // mark channel for reset - this can only happen if channel is in service at this point
         delayFlag = true;                                                                  // set delay flag to force delay before reset as at least one ch has triggered - no problem if set several times
//
// Get barometer reading if not already updated
//
         if(gotPress == false) {
            baroPress = bme.readPressure();
            delay(10);
            baroPress = baroPress / 100.0;                                                  // convert frpm pA to HpA
            digitalWrite(R_LED, LOW);                                                       // clear red LED
            lcd.setCursor(0, 0);                                                            //
            lcd.print("data log running");                                                  // reset screen message
            if (baroPress >= 1200.0){                                                       // excessive pressure used to indicate sensor fail/not connected
               digitalWrite(R_LED, HIGH);                                                   // set red LED to indicate problem
               lcd.setCursor(0, 0);
               lcd.print("bmp sensor alert");
               baroPress = pressLast;                                                       // substitute previous pressure reading
               }
            gotPress = true;
            pressLast = baroPress;
            }  
//
// Get temperature 
//         
         if(gotTemp == false) { 
            dtemp = thermocouple.readCelsius();                                      // get temp from thermocouple board
            delay(250);                                                              // allow I2C bus time to clear 
            temperature = (float) dtemp;                                             // convert to float from double
            digitalWrite(R_LED, LOW);                                                // clear red LED
            lcd.setCursor(0, 0);                                                     //
            lcd.print("data log running");                                           // reset screen message
            if (temperature <= 0.0 || isnan(dtemp)){                                                 // -ve or zero tepertature used to indicate sensor fail/not connected
// (isnan(dtemp))
               digitalWrite(R_LED, HIGH);                                            // set red LED to indicate problem
               lcd.setCursor(0, 0);
               lcd.print("tmp sensor alert");
               temperature = tempLast;                                               // substitute last above zero temperture
               }
            gotTemp = true;
            tempLast = temperature;
            }
//
// convert event(s) to stp and update total vol collected
//
         volThisTip[i] = ((baroPress * tumblerVol[i] * 273.0)/(1013.25 * (temperature + 273.0)));  
         totalVol[i] = totalVol[i] +  volThisTip[i];                                  // add vol at stp to previous totalVol
         volThisDay[i] = volThisDay[i] + volThisTip[i];
         volThisHour[i] = volThisHour[i] + volThisTip[i];                     
         }
      }
//
//  Scan of all 15 latches complete
//      
      gotTemp = false;
      gotPress = false;                                                                          
//
// do calculations and update results here to make sure most recent inoculum only events are included
//
   if(delayFlag == true) {                                                        // condition satisfied if one or more channels have tripped
      contAve = 0.0;
      n = 0;
      for(byte i=1; i <= 15; i++) {                                               // scan for inoculum only channels and calculate ml per gram output for each inoc only channel
          if((inService[i]== 1) && (inoculumOnly[i] == 1)) {                   // and average across all inoc only channels
//
             netVol_per_gram[i] = totalVol[i]/ inocVS[i];                         // calculate and update net vol per gram for inoc only channels
//
             contAve = (totalVol[i]/inocVS[i]) + contAve;                         // sum across all channels so average for innoculum only can be found
             n = n + 1; 
             }
          }
      contAve = contAve / (float) n;                                               // average volume per unit mass from inoculum only (control) channels     
//      
//    Now calculate vol per unit mass for ALL in service non-inoc channels -  even if sample channel hasn't tripped, inoc only channel(s) may have
//    so contribution to gas from inoculum may have changed
//      
      for(byte i=1; i<=15; i++){
         if((inService[i]== 1) && (inoculumOnly[i] == 0)) {
            netVol_per_gram[i] = (totalVol[i] - (inocVS[i] * contAve))/ sampleVS[i];
            }
         }
//
// now update log file and remaining results for any channels which have just triggered (i.e. those which are marked for reset)
//
      for (byte i=1; i<=15; i++) {
         if(resetFlag[i] == true){                                 // channel must be in service for resetFlag to have been set so no need to re-test here
            alphaIndex = i;
            get_alphaName();
            Serial.println();
            Serial.println(F("**********"));
            Serial.print(F("Channel: ")); Serial.print(i); Serial.print(", "); Serial.print(alphaName); Serial.print(", ");
            Serial.print(F("timestamp = "));Serial.print(timex); Serial.print(", ");
            Serial.print(days); Serial.print(F(" days ")); Serial.print(hours); Serial.print(F(" hours ")); Serial.print(mins); Serial.println(F(" minutes"));
            delay(70);
            Serial.print(F("temp = ")); Serial.print(temperature, 2); Serial.print(F(" C, pressure = ")); Serial.print(baroPress, 2); Serial.println(F(" hPa"));
            delay(70); 
            Serial.print(F("total tips since start: "));Serial.print(ChannelCount[i]); Serial.print(", ");
            Serial.print(F("volume this tip: "));Serial.print(volThisTip[i], 2); Serial.print(" ml, ");
            Serial.print(F("total vol since start: "));Serial.print(totalVol[i], 2); Serial.println(" ml");
            delay(70);
            Serial.print(F("tips this day: ")); Serial.print(tipsThisDay[i]); Serial.print(", ");
            Serial.print(F("vol this day ")); Serial.print(volThisDay[i], 2); Serial.println(" ml");
            Serial.print(F("tips this hour: ")); Serial.print(tipsThisHour[i]); Serial.print(", ");
            Serial.print(F("vol this hour: ")); Serial.print(volThisHour[i], 2); Serial.println(" ml");
            delay(70);
            Serial.print(F("net gas attributable to test sample since start: ")); Serial.print(netVol_per_gram[i], 2); Serial.println(" ml/g");
            Serial.println();
            delay(70);
//
            Serial.println(F("writing event to SD card"));             // update applied to every channel marked for reset           
            fEventLog = SD.open("eventlog.csv", FILE_WRITE);
            fEventLog.print(i); fEventLog.print(comma);
            fEventLog.print(alphaName); fEventLog.print(comma);
            fEventLog.print(timex); fEventLog.print(comma);
            fEventLog.print(days); fEventLog.print(comma);
            fEventLog.print(hours); fEventLog.print(comma);
            fEventLog.print(mins); fEventLog.print(comma);
            fEventLog.print(temperature,2); fEventLog.print(comma);
            fEventLog.print(baroPress,2); fEventLog.print(comma);
            fEventLog.print(ChannelCount[i]); fEventLog.print(comma);
            fEventLog.print(volThisTip[i],3); fEventLog.print(comma);
            fEventLog.print(totalVol[i],3); fEventLog.print(comma);
            fEventLog.print(tipsThisDay[i]); fEventLog.print(comma);
            fEventLog.print(volThisDay[i],3); fEventLog.print(comma);
            fEventLog.print(tipsThisHour[i]); fEventLog.print(comma);
            fEventLog.print(volThisHour[i],3); fEventLog.print(comma);
            fEventLog.println(netVol_per_gram[i],3);
            fEventLog.close();
            Serial.println(F("event written to SD card"));   
            }  
         }
      }
//
// processing of data for all 15 latches completed - now reset latches for any channels that need it - this section only executed if at least one channel has been triggered
// which results in delayFlag having been set to 'true'
//
// first run delay defined by hangTime to make sure switch not bouncing and/or channel has had time to clear 
//
   if (delayFlag == true) {
      delay(hangTime);
      delayFlag = false;
      digitalWrite(Y_LED, LOW);
//
// now do burst reset on those latches that have been set - this section only executed if at least one latch has been set
//
      for (byte i = 1; i <=15; i++) {
         if((resetFlag[i] == true)) {                                                            // channel must be in service for resetFlag to have been set so no need to re-test
            Serial.print(F("channel ")); Serial.print(i); Serial.println(F(" reset"));
            digitalWrite(R[i], LOW); delay(1); digitalWrite(R[i], HIGH);                                 // reset latch if channel is in service and reset flag is set
            resetFlag[i] = false;                                                                         // clear reset flag            
            delay(70);
            if (digitalRead(Q[i]) != LOW) {                                                                // if reset has failed first time around
               delay(1);                                                                                  // wait 10mS
               digitalWrite(R[i], HIGH); digitalWrite(R[i], LOW); delay(1); digitalWrite(R[i], HIGH);      // have second go at clearing latch
               delay(1);                                                                                   // short wait to allow for any propogation delay
               if (digitalRead(Q[i]) != LOW){                                                              // if second attempt to clear didn't work - remove from service 
                  inService[i] = -1;
                  Serial.println(F("************************************"));
                  Serial.println(F("+++ALERT+++"));
                  Serial.print(F("Channel ")); Serial.print(i); Serial.println(F(" reset problem"));        
                  Serial.print(F("Channel ")); Serial.print(i); Serial.println(F(" queued for auto-reset"));
                  Serial.println(F("************************************"));
                  Serial.println();
                  }
               } 
            }
         }     
      }
//
// latch resets for channels 1 to 15 completed
//      

//
//  check for data transfer request
//
   pingCount = pingCount + 1;
   dispCount = pingCount/10;
   if ((dispCount * 10) == pingCount) {
      lcd.setCursor(14, 1);      
      lcd.print(dispCount);
      }
   if (pingCount >= 150) {
      digitalWrite(Y_LED, HIGH);
      while (Serial.available() > 0) {
         ByteReceived = Serial.read();     // make sure read buffer is empty before sending ping    
         }
      Serial.println(F("ping"));           // send ping
      pingCount = 0;
      delay(900);                          // give sufficient time for response to ping to be received in buffer
      if (Serial.available() > 0) {        // look for response (if any)
         lcd.setCursor(0, 0);
         lcd.print("Request received");
         goFlag = Serial.parseInt();       // decode response to integer value
         if ((goFlag == 9999) || (goFlag == 8888) ||(goFlag == 7777)) {        // check goFlag to ensure it is a valid request, not connection noise   
           lcd.setCursor(0, 0);
           lcd.print("request OK      ");
           delay(3000);                   // allows message to be read on lcd screen
           }
         else {
            lcd.setCursor(0, 0);
            lcd.print("invalid request ");
            goFlag = 0;
            delay(3000);                  // allows message to be read on lcd screen
            } 
         } 
      digitalWrite(Y_LED, LOW);      
//
//  check for any channels marked as stuck and report
//      
      for (int i = 1; i <=15; i++) {
         if(inService[i] == -1) {
//
// first attempt auto-reset on any channel that has been taken out of service by arduino inService[i] = -1
//
            digitalWrite(R[i], HIGH); digitalWrite(R[i], LOW); delay(1); digitalWrite(R[i], HIGH);      
            delay(1);                                                                                   // short wait to allow latch reset to complete
            if (digitalRead(Q[i]) == LOW){                                                              // if attempt to clear has worked, bring back into service
               inService[i] = 1; 
               } 
//
//
//
            if(inService[i] == -1){                                   // if still not cleared display error message
               lcd.setCursor(0, 0);
               lcd.print("                ");
               lcd.setCursor(0, 0);
               lcd.print("reed stuck ch "); lcd.print(i); 
               digitalWrite(R_LED, HIGH);      
               delay(2000); 
               digitalWrite(R_LED, LOW);
               }
            }
         }
      }                
//
// now test goFlag for each individual response
// 9999 - file grab
//       
   if(goFlag !=0){
      if (goFlag == 9999){
         lcd.setCursor(0, 0);
         lcd.print("Upload starting ");
         Serial.println(F("Connection request acknowledged....")); 
         delay(100);                       // allow time for laptop to process response and prepare to receive writeback data
         fastGrab = 0;
         file_writeback();                 // copy all log files from SD to host machine
         }
//
// 8888 - fastgrab - only sends eventlog.csv and daily.csv
//
      if (goFlag == 8888) {
         Serial.println(F("Connection request acknowledged....")); 
         delay(100);
         fastGrab = 1;
         file_writeback();     
         }
//
// 7777 = monitor routine - resets lcd sends snapshot to screen, then tries to reset any stuck channels, simple monitor..
//
      if(goFlag == 7777) {
         lcd.begin(16,2);                                   // reset lcd screen
         lcd.setCursor(0, 0);
         lcd.print("monitor starting");
         Serial.println(F("Connection request acknowledged....")); 
         delay(70);
         Serial.println(F("*****************************************************************************************************************")); 
         delay(70); 
         Serial.println(F("Ch  Name\tTimestamp\tD   H   M   In service   Inoc_only  Tip Count  Total Vol(STP)  Net Vol per gram (ml/g)"));
         delay(70); 
         for (byte i=1; i <=15; i++){
            alphaIndex = i;
            get_alphaName();                                    // call function to return alphanamexx where xx = alphaIndex
            if(i <= 9) {
               Serial.print("0");
               }
            Serial.print(i); Serial.print("  ");
            Serial.print(alphaName); Serial.print("\t");   
            Serial.print(timex); Serial.print("\t\t");
            Serial.print(days); Serial.print("   ");
            Serial.print(hours); Serial.print("   ");
            Serial.print(mins); Serial.print("       ");     
            Serial.print(inService[i]); Serial.print("            ");
            Serial.print(inoculumOnly[i]); Serial.print("          ");
            Serial.print(ChannelCount[i]); Serial.print("           ");
            Serial.print(totalVol[i],2); Serial.print("             ");
            Serial.println(netVol_per_gram[i],2);
            delay(eolDelay); 
            }
         Serial.println(F("*****************************************************************************************************************")); 
         delay(eolDelay);
//
// attempt to reset any channels taken out of service by Arduino(i.e. where inService[i] = -1]
//
         Serial.println("");
         for (int i=1; i<=15; i++){
            if (inService[i] == -1) {                                                                
            Serial.print("Channel "); Serial.print(i); Serial.println(" stuck"); // 
            digitalWrite(R[i], HIGH); digitalWrite(R[i], LOW); delay(1); digitalWrite(R[i], HIGH);      
            delay(1);                                                                                   // short wait to allow latch reset to complete
            if (digitalRead(Q[i]) == LOW){                                                              // if attempt to clear has worked, bring back into service
               inService[i] = 1;
               Serial.print("Channel "); Serial.print(i); Serial.println(" back in service"); // 
               } 
            }
         }
      }
   }   
   goFlag = 0;
   lcd.setCursor(0, 0);
   lcd.print("data log running");           
}
//
// End of main loop
//
//  &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
//  &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
//
//  Here come the funtions
//
//  &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
//  &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
//

int checkingCard(int cardSelect){
   int cardFound;
   Serial.print(F("\nChecking SD card..."));
//
// use the initialization code from the utility libraries to check that SD card is working
//
  if (!card.init(SPI_HALF_SPEED, cardSelect)) {
    Serial.println(F("SD card check failed - is SD card fully secured in holder?"));
    cardFound = 0;
  } else {
    Serial.println(F("SD card present"));
    cardFound = 1;
  }
  Serial.print("\nCard type: ");              // print the type of card
  switch (card.type()) {
    case SD_CARD_TYPE_SD1:
      Serial.println("SD1");
      break;
    case SD_CARD_TYPE_SD2:
      Serial.println("SD2");
      break;
    case SD_CARD_TYPE_SDHC:
      Serial.println("SDHC");
      break;
    default:
      Serial.println("Unknown");
  }
//
// Now we will try to open the 'volume'/'partition' - should be FAT16 or FAT32
//
  if (!volume.init(card)) {
    Serial.println(F("Could not find FAT16/FAT32 partition.\nMake sure card is formatted correctly"));
  }  
  uint32_t volumesize1;                         // print the type and size of the first FAT-type volume
  //Serial.print(F("\nVolume type is FAT"));
  //Serial.println(volume.fatType(), DEC);
  //Serial.println();

  volumesize1 = volume.blocksPerCluster();    // clusters are collections of blocks
  volumesize1 *= volume.clusterCount();       // we'll have a lot of clusters
  volumesize1 *= 512;                         // SD card blocks are always 512 bytes
  //Serial.print(F("Volume size (bytes): "));
  //Serial.println(volumesize1);
  //Serial.print(F("Volume size (Kbytes): "));
  volumesize1 /= 1024;
  //Serial.println(volumesize1);
  //Serial.print(F("Volume size (Mbytes): "));
  volumesize1 /= 1024;
  //Serial.println(volumesize1);
  Serial.println(F("\nFiles found on the card (name and size in bytes): "));
  root.openRoot(volume);
  root.ls(LS_R  | LS_SIZE);                   // list all files in the card with size

  return cardFound;
}
//
//  &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
//
//
// send_Setup_Data() copies contents of file setup.txt on SD card to serial terminal 
//
void send_Setup_Data() {
   fSetup = SD.open("setup.csv");
   delay(100);
   if (fSetup) {
      Serial.println();
      Serial.println(F("starting setup.csv writeback"));
      Serial.println();
      delay(1);                             // allow laptop to process line and open file
      while (fSetup.available()) {
         Serial.write(fSetup.read());       // read from the file and write to serial port until end of file
         delay(1);                          // give laptop time to process data transferred
         }
      fSetup.close();                       // close the file:
      Serial.println();
      Serial.println(F("writeback completed - setup.csv closed"));
      delay(1);                             // allow laptop to process message and close file   
      } 
   else {
      Serial.println(F("problem opening setup.csv on SD card"));// if the file didn't open, print an error:
      }
   }
//
//  &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
//
//
//   uptime() - converts milliseconds to days, hours, minutes and seconds
//
void uptime() {
   secs = long(timex/1000);     //convect milliseconds to seconds (timex is unsigned long)
   mins=secs/60;                //convert seconds to minutes
   hours=mins/60;               //convert minutes to hours
   days=hours/24;               //convert hours to days
   secs=secs-(mins*60);         //subtract the coverted seconds to minutes in order to display 59 secs max
   mins=mins-(hours*60);        //subtract the coverted minutes to hours in order to display 59 minutes max
   hours=hours-(days*24);       //subtract the coverted hours to days in order to display 23 hours max
   oldSecs = newSecs;
   newSecs = secs;
   oldMins = newMins;
   newMins = mins;
   oldHour = newHour;                                                                       // set up variables to check for change of hour
   newHour = hours;
   oldDay = newDay;                                                                         // set up variables to check for change of day
   newDay = days;
   }

//
//  &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
//
//   pull_down_setup_data
//   Collects and parses setup data from serial port - and loads arrays
//   Data is then written to setup.csv on SD card
//
void pull_down_setup_data() {   
   while(Serial.available()== 0){        // wait for incoming data
   }

   for (byte i=1; i<=15; i++){            // start read sequence
//  
      switch(i){
         case 1:
         alphaName1 = Serial.readStringUntil(',');
         break;
//
         case 2:
         alphaName2 = Serial.readStringUntil(',');
         break;
//
         case 3:
         alphaName3 = Serial.readStringUntil(',');
         break;
//
         case 4:
         alphaName4 = Serial.readStringUntil(',');
         break;
//
         case 5:
         alphaName5 = Serial.readStringUntil(',');
         break;
//
         case 6:
         alphaName6 = Serial.readStringUntil(',');
         break;
//
         case 7:
         alphaName7 = Serial.readStringUntil(',');
         break;
//
         case 8:
         alphaName8 = Serial.readStringUntil(',');
         break;
//
         case 9:
         alphaName9 = Serial.readStringUntil(',');
         break;
//
         case 10:
         alphaName10 = Serial.readStringUntil(',');
         break;
//
         case 11:
         alphaName11 = Serial.readStringUntil(',');
         break;
//
         case 12:
         alphaName12 = Serial.readStringUntil(',');
         break;
//
         case 13:
         alphaName13 = Serial.readStringUntil(',');
         break;
//{
         case 14:
         alphaName14 = Serial.readStringUntil(',');
         break;
//
         case 15:
         alphaName15 = Serial.readStringUntil(',');
         break;     
         }
//
      inService[i] = Serial.parseInt();
//
      inoculumOnly[i] = Serial.parseInt();
//
      inocVS[i] = Serial.parseFloat();
//
      sampleVS[i] = Serial.parseFloat();
//
      tumblerVol[i] = Serial.parseFloat();  
//
      discard = Serial.readStringUntil('^');          // forces any extraneous characters at end of line in setup file to be skipped
//
// perform sanity check on data and issue warnings as appropriate
//   
      if(inService[i] != 0 && inService[i] !=1){
         Serial.print(F("+++++ WARNING: in service value for channel ")); Serial.print(i); Serial.println(F(" out of range"));
         Serial.println(F("Please check that setup.csv file is correct - in service value should be 0 or 1"));
         Serial.println();                   
         }
      if(inoculumOnly[i] != 0 && inoculumOnly[i] !=1){
         Serial.print(F("+++++ WARNING: inoculum only value for channel ")); Serial.print(i); Serial.println(F(" out of range"));
         Serial.println(F("Please check that setup.csv file is correct-  inoculum only value should be 0 or 1")); 
         Serial.println();                
         }
      if(tumblerVol[i] < 4.0 || tumblerVol[i] > 14.0){
         Serial.print(F("+++++ WARNING: tumbler volume for channel ")); Serial.print(i); Serial.println(F(" out of range"));
         Serial.println(F("Please check that setup.csv file is correct - maximum volume expected is 6.5 ml"));
         Serial.println();                   
         }
      if(inoculumOnly[i] ==1 && sampleVS[i] > 0.1){
         Serial.print(F("+++++ WARNING: sample volatile solids for inoculum only channel ")); Serial.print(i); Serial.println(F(" not zero"));
         Serial.println(F("Please check that setup.csv file is correct - inoculum only channel not expected to contain sample material"));
         Serial.println();                 
         }  
      }
//
// get 16th line of setup.csv - parse first field as an int - returns zero if no valid number found, otherwise returns int value to use for hangTime
//
//###################
   hangTimeReset = 0;
   hangTimeReset = Serial.parseInt();
   discard = Serial.readStringUntil('^');                // clears serial buffer to end of line
   if(hangTimeReset > 7 && hangTimeReset < 36){          //
      Serial.print(F("hangover time changed to: ")); Serial.print(hangTimeReset); Serial.println(F(" seconds"));
      hangTime = hangTimeReset * 1000;                   // if passed an int field within valid range found then use new value for hangTime - otherwise default will be kept       
      }
//      
//##################      
//         
   delay(250);                                            // allow time for serial port to clear
   Serial.println();
   Serial.println(F("Data tables loaded"));     
//
   fSetup = SD.open("setup.csv", FILE_WRITE);             // open file on SD card to allow setup.csv to be populated
   for (byte i=1; i <=15; i++){
      alphaIndex = i;
      get_alphaName();                                    // call function to return alphanamexx where xx = alphaIndex
//      Serial.print(alphaName); Serial.print(comma);
      fSetup.print(alphaName); fSetup.print(comma); 
//      Serial.print(inService[i]); Serial.print(comma);      
      fSetup.print(inService[i]); fSetup.print(comma);
//      Serial.print(inoculumOnly[i]); Serial.print(comma);  
      fSetup.print(inoculumOnly[i]); fSetup.print(comma);       
//      Serial.print(inocVS[i]); Serial.print(comma);
      fSetup.print(inocVS[i]); fSetup.print(comma);
//      Serial.print(sampleVS[i]); Serial.print(comma);
      fSetup.print(sampleVS[i]); fSetup.print(comma);      
//      Serial.println(tumblerVol[i]);
      fSetup.println(tumblerVol[i]);
      }
   fSetup.close();
   Serial.println();
   Serial.println(F("Setup file written to SD card")); 
   } 
//
//  &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
//
//   get_alphaName()takes global int variable alphaIndex and returns corresponding alphaNamexx as string alphaName
//
void get_alphaName() {
   switch(alphaIndex){
      case 1:
      alphaName = alphaName1;
      break;
//
      case 2:
      alphaName = alphaName2;
      break;
//
      case 3:
      alphaName = alphaName3 ;
      break;
//
      case 4:
      alphaName = alphaName4;
      break;
//
      case 5:
      alphaName = alphaName5;
      break;
//
      case 6:
      alphaName = alphaName6;
      break;
//
      case 7:
      alphaName = alphaName7;
      break;
//
      case 8:
      alphaName = alphaName8;
      break;
//
      case 9:
      alphaName = alphaName9;
      break;
//
      case 10:
      alphaName = alphaName10;
      break;
//
      case 11:
      alphaName = alphaName11;
      break;
//
      case 12:
      alphaName = alphaName12;
      break;
//
      case 13:
      alphaName = alphaName13;
      break;
//{
      case 14:
      alphaName = alphaName14;
      break;
//
      case 15:
      alphaName = alphaName15;
      break;     
      } 
   }  
//
//
//  &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
//
//  go_check_file() - if file exists it is deleted than checked to see that delete has worked
//
void  go_check_file() {   
   if (SD.exists(fnameToCheck)) {
      SD.remove(fnameToCheck);
      }
   if (!SD.exists(fnameToCheck)){
      Serial.print("old ");
      Serial.print(fnameToCheck);
      Serial.println(" deleted");
      } 
   }     
//
//  &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
//
//  write_snapshot() - writes snapshot of present cumulative position to SD card file snapshot.csv
//
void write_snapshot() {
   Serial.println();
   Serial.println(F("Creating snapshot"));
   fSnap = SD.open("snapshot.csv", FILE_WRITE);
   fSnap.println(F("Channel,Name,Timestamp,Days,Hours,Mins,In service,Inoc_only,Tip Count,Total Vol(STP), Net Vol per gram (ml/g)")); 
   for (byte i=1; i <=15; i++){
      alphaIndex = i;
      get_alphaName();                                    // call function to return alphanamexx where xx = alphaIndex
      fSnap.print(i); fSnap.print(comma);
      fSnap.print(alphaName); fSnap.print(comma);   
      fSnap.print(timex); fSnap.print(comma);
      fSnap.print(days); fSnap.print(comma);
      fSnap.print(hours); fSnap.print(comma);
      fSnap.print(mins); fSnap.print(comma);     
      fSnap.print(inService[i]); fSnap.print(comma);
      fSnap.print(inoculumOnly[i]); fSnap.print(comma);
      fSnap.print(ChannelCount[i]); fSnap.print(comma);
      fSnap.print(totalVol[i],2); fSnap.print(comma);
      fSnap.println(netVol_per_gram[i],2); 
      }
   fSnap.println("***************************************************************");  
   fSnap.close();
   }
//
//  &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
//
//   write_day_log() - calculates results for last hour and prints to file daily.csv on SD card
//
// calculate output from inoc only in ml per gram
// average over all inoc only channels
// find contribution inoc has made to other cells - then net vol from sample
// divide by mass of sample to give ml per gram from sample over that day
//
// 
void write_day_log() {
   contAve = 0;
   n = 0;
   for(byte i=1; i <= 15; i++) {                                            // scan for inoculum only channels and calcuolate ml per gram output for each
      if((inService[i]== 1) && (inoculumOnly[i] == 1)) {                 // then take average over all inoc only channels
//
         netVol_per_gramDay[i] = volThisDay[i]/ inocVS[i];                  // calculate and update net vol per gram (icremental) for inoc only channels
         netVol_per_gram[i] = totalVol[i]/ inocVS[i];                       // calculate and update net vol per gram (cumulative) for inoc only channels        
//
         contAve = (volThisDay[i]/inocVS[i]) + contAve;
         n = n + 1; 
         }
      }
   contAve = contAve / (float) n;                                            // average volume per unit mass from inoc only control channels     
//      
//    Now calculate vol per unit mass for remaining (non inoc only) channels in service
//      
   for(byte i=1; i<=15; i++){
      if ((inService[i] == 1) && (inoculumOnly[i] == 0)){
         netVol_per_gramDay[i] = (volThisDay[i] - (inocVS[i] * contAve))/ sampleVS[i];
         }
      }
   fDaily = SD.open("daily.csv", FILE_WRITE); 
   if (fDaily) {
      Serial.println();
      Serial.println(F("SD card - daily.csv opened ok"));          // print message if hourly.csv opened OK
//     fDaily.print("Day: "); fDaily.print(days); fDaily.println(comma);
//     fDaily.println(F("Channel number,Name,Timestamp,Days,Hours,Mins,In service,Tips this day, Vol this day (STP), Net vol this day (ml/g), Cumulative net vol (ml/g)")); 
      for (byte i=1; i<=15; i++) {
         alphaIndex = i;
         get_alphaName();  
         fDaily.print(i); fDaily.print(comma);
         fDaily.print(alphaName); fDaily.print(comma);
         fDaily.print(timex); fDaily.print(comma);
         fDaily.print(days); fDaily.print(comma);
         fDaily.print(hours); fDaily.print(comma);
         fDaily.print(mins); fDaily.print(comma);
         fDaily.print(inService[i]); fDaily.print(comma);
         fDaily.print(tipsThisDay[i]); fDaily.print(comma);
         fDaily.print(volThisDay[i],2); fDaily.print(comma);
         fDaily.print(netVol_per_gramDay[i],2);fDaily.print(comma);
//
         fDaily.println(netVol_per_gram[i],2);
//              
         }
      Serial.println("closing day log on SD card");    
      fDaily.close();        
      }
   }
//
//  &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
//
//  write_hour_log() - calculates results for last hour and prints to file hourly.csv on SD card
//
//
// calculate output from inoc only in ml per gram
// average over all inoc only channels
// find contribution inoc has made to other cells - then net vol from sample
// divide by mass of sample to give ml per gram from sample over that hour
//

void write_hour_log() {
   contAve = 0;
   n = 0;
   for(byte i=1; i <= 15; i++) {                                  // scan for inoculum only channels and calcuolate ml per gram output for each
      if((inService[i]== 1) && (inoculumOnly[i] == 1)) {          // then take average over all inoc only channels
//
         netVol_per_gramHour[i]  = volThisHour[i]/ inocVS[i];     // calculate and update net vol per gram (icremental) for inoc only channels
         netVol_per_gram[i] = totalVol[i]/ inocVS[i];             // calculate and update net vol per gram (cumulative) for inoc only channels       
//
         contAve = (volThisHour[i]/inocVS[i]) + contAve;
         n = n + 1; 
         }
      }
   contAve = contAve / (float) n;                                  // average volume per unit mass from control channels     
//      
//    Now calculate vol per unit mass for all (non inoc only) channels in service
//      
   for(byte i=1; i<=15; i++){
      if ((inService[i] == 1) && (inoculumOnly[i] == 0)){
         netVol_per_gramHour[i] = (volThisHour[i] - (inocVS[i] * contAve))/ sampleVS[i];
         }
      }
   fHourly = SD.open("hourly.csv", FILE_WRITE); 
   if (fHourly) {
      Serial.println();
      Serial.println(F("SD card - hourly.csv opened ok"));          // print message if hourly.csv opened OK
//      fHourly.print("Hour: "); fHourly.print(hours); fHourly.println(comma);
//      fHourly.println(F("Channel number,Name,Timestamp,Days,Hours,Mins,In service,Tips this hour, Vol this hour (STP), Net vol this hour (ml/g), Cumulative net vol (ml/g)")); 
      for (byte i=1; i<=15; i++) {
         alphaIndex = i;
         get_alphaName();  
         fHourly.print(i); fHourly.print(comma);
         fHourly.print(alphaName); fHourly.print(comma);
         fHourly.print(timex); fHourly.print(comma);
         fHourly.print(days); fHourly.print(comma);
         fHourly.print(hours); fHourly.print(comma);
         fHourly.print(mins); fHourly.print(comma);
         fHourly.print(inService[i]); fHourly.print(comma);
         fHourly.print(tipsThisHour[i]); fHourly.print(comma);
         fHourly.print(volThisHour[i],2); fHourly.print(comma);
         fHourly.print(netVol_per_gramHour[i],2);fHourly.print(comma);
//
         fHourly.println(netVol_per_gram[i],2);
//                      
         }
      Serial.println("closing hour log on SD card");    
      fHourly.close();      
      }
   }   
//
//  &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
//
//  file_writeback() - copies files from SD card to serial port 
//
void  file_writeback() {
   Serial.println(F("starting eventlog.csv writeback"));
   delay(1000);                                // allow time for laptop to open file on disk and prepare for data transfer
   if (SD.exists("eventlog.csv")) {
      fEventLog = SD.open("eventlog.csv");
      delay(100);                              // guard time to complete opening of file on SD card
      while (fEventLog.available()) {
         sdByte = fEventLog.read();
         Serial.write(sdByte);                 // read from the file and write to serial port until end of file
         if (sdByte == 10) {                   // if byte represents line feed (\n = 10) then pause to allow Bluetooth/laptop to process
            delay(eolDelay);                   // allow time for laptop to process data transferred   
            }
         }
      fEventLog.close();                       // close the file when all lines sent:
      }
   Serial.println(F("writeback completed - eventlog.csv closed"));
   delay(1000);                                // add delay to make sure Python utility has time to close file 
//   
 
   if (SD.exists("snapshot.csv") && fastGrab == 0) {       // only send snapshot if fastGrab = 0
      Serial.println(F("starting snapshot.csv writeback"));
      delay(1000);                                // allow time for laptop to open file on disk and prepare for data transfer  
      fSnap = SD.open("snapshot.csv");
      delay(100);
      while (fSnap.available()) {         
         sdByte = fSnap.read();
         Serial.write(sdByte);                 // read from the file and write to serial port until end of file
         if (sdByte == 10) {
            delay(eolDelay);
            } 
         }
      fSnap.close();                       // close the file:
      Serial.println(F("writeback completed - snapshot.csv closed")); 
      delay(1000);     
      }
         
//
   Serial.println(F("starting daily.csv writeback"));
   delay(1000);                                // allow time for laptop to open file on disk and prepare for data transfer     
   if (SD.exists("daily.csv")) {
      fDaily = SD.open("daily.csv");
      delay(100);
      while (fDaily.available()) {       
         sdByte = fDaily.read();
         Serial.write(sdByte);                 // read from the file and write to serial port until end of file
         if (sdByte == 10) {
            delay(eolDelay);
            } 
         }
         fDaily.close();                       // close the file:     
      } 
   Serial.println(F("writeback completed - daily.csv closed"));
   delay(1000);                                // add delay to make sure Python utility has time to close file               
//      
   
   if (SD.exists("hourly.csv") && fastGrab == 0) {    // only send hourly log if fastGrab = 0
      Serial.println(F("starting hourly.csv writeback"));
      delay(1000);                                // allow time for laptop to open file on disk and prepare for data transfer 
      fHourly = SD.open("hourly.csv");
      delay(100);
      while (fHourly.available()) {       
         sdByte = fHourly.read();
         Serial.write(sdByte);                 // read from the file and write to serial port until end of file
         if (sdByte == 10) {
            delay(eolDelay);
            }         
         }
      fHourly.close();                       // close the file:
      Serial.println(F("writeback completed - hourly.csv closed"));
      delay(1000);                                // add delay to mke sure Python utility has time to close file    
      }         
   }
//
//
//
void screenTime(){
   lcd.setCursor(0, 1);
   if(days <= 9) {
      lcd.print("0");
      //lcd.setCursor(1, 1);
      }
   lcd.print(days);
   lcd.print(":");

   if(hours <= 9) {
      lcd.print("0");
      }
   lcd.print(hours);
   lcd.print(":"); 

   if(mins <= 9) {
      lcd.print("0");
      }
   lcd.print(mins);
   lcd.print(":");

    if(secs <= 9) {
      lcd.print("0");
      }
   lcd.print(secs);
   lcd.print("     "); 
}
