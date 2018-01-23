#include <Wire.h>  // supports the I2C communications with LCD and shields
#include "RTClib.h"  // support the real time clock
#include "SD.h" // supports the SD read/write functions on Adafruit datalogger shield
#include <SPI.h>  // Enables the SPI bus communication on MISO, MOSI, SCK pins on Adafruit datalogger shield

RTC_PCF8523 rtc;

#define rtdaddress 111  // I2C ID number for EZO RTD Circuit
#define ecaddress 112   // I2C ID number for EZO EC Circuit
#define pHaddress 113   // I2C ID number for EZO pH Circuit
#define doaddress 114   // I2C ID number for EZO DO Circuit

const int I2C_pin = 39;
//const int I2C2_pin = 43;
const int TentSP_pin = 44;
int Sample_incr = 30; // this is the increments of time in seconds to log, eg 300 for 5min, 900 for 15min, etc.
const int chipSelect = 34;  // Corresponds to chipselect pin on Adafruit datalogger (default is 10, unless conflicting)

// additional variables
uint8_t op_mode;  // variable used to direct program to the various logging and calibration subroutines
byte code=0;  // used to hold the I2C response code. 
byte in_char=0; // used as a 1 byte buffer to store in bound bytes from the pH Circuit.   
byte i=0; // counter used for ph_data array. 
char rtd_data[20]; // we make a 20 byte character array to hold incoming RTD data.
char ph_data[20]; // we make a 20 byte character array to hold incoming data from the pH circuit. 
char ec_data[48]; // we make a 48 byte character array to hold incoming EC data.
char do_data[20]; // we make a 20 byte character array to hold incoming data from the ORP circuit. 
char *ec; // char pointer used in string parsing for conductivitiy output
char *tds; // char pointer used in string parsing for total dissolved solids data from conductivity output
char *sal; // char pointer used in string parsing for salinity data from conductivity output
char *sg; // char pointer used in string parsing for specific gravity data from conductivity output
char *do_per; // char pointer used in string parsing for DO percent parameter
char *do_mg;  // char pointer used in string parsing for DO mg/L parameter
char filename[]= "Test00.TXT"; // <-------Name File here!!! (ex. BLANK00.TXT)
DateTime tyme; // a working variable to handle times
DateTime nextlog; //the next time to log 
DateTime end_window; // the time for the end of a view window
File dataFile;



void setup() {
  // initialize serial monitor.
  Serial.begin(9600);

  pinMode(TentSP_pin, OUTPUT);
  pinMode(I2C_pin, OUTPUT);
  //pinMode(I2C2_pin, OUTPUT);

  digitalWrite(TentSP_pin, LOW);
  digitalWrite(I2C_pin, LOW);
  //digitalWrite(I2C2_pin, LOW);
 
  rtc.begin();  // start the real time clock
  if (! rtc.begin()) {
   Serial.println("Couldn't find RTC");
   while (1);
  }
  if (! rtc.initialized()) {
    Serial.println("RTC is NOT running!");
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }

  rtc.begin();  //start the real time clock
  tyme = rtc.now(); //recall time from the real time clock
   Serial.println();
    Serial.print(tyme.year(), DEC);
    Serial.print('/');
    if(tyme.month() < 10){
      Serial.print('0');
      Serial.print(tyme.month(), DEC);}
    else {Serial.print(tyme.month(), DEC);}
    Serial.print('/');
    if (tyme.day() < 10){
      Serial.print('0');
      Serial.print(tyme.day(), DEC);}    
    else {Serial.print(tyme.day(), DEC);}
    Serial.print(',');
    if (tyme.hour() < 10){  
      Serial.print('0');
      Serial.print(tyme.hour(), DEC);}
    else {Serial.print(tyme.hour(), DEC);} 
    Serial.print(':');
    if (tyme.minute() < 10){
      Serial.print('0');
      Serial.print(tyme.minute(), DEC);}
    else {Serial.print(tyme.minute(), DEC);}
    Serial.print(':');
    if (tyme.second() < 10){
      Serial.print('0');
      Serial.print(tyme.second(), DEC);}
    else {Serial.print(tyme.second(), DEC);} // end of current time display}


  Serial.println("Initializing SD card...");
  // make sure that the default chip select pin is set to correct configuration
  // output, even if you don't use it:
 pinMode(SS, OUTPUT);
 
  // see if the card is present and can be initialized:
 if (!SD.begin(chipSelect)) {
  Serial.println("Card failed, or not present");
    // don't do anything more:
    while (1) ;
  }
 
  Serial.println("card initialized.");
  // Open up the file we're going to log to!
  for (uint8_t i = 0; i < 100; i++) {
    filename[4] = i/10 +'0';
    filename[5] = i%10 +'0';
    if (! SD.exists(filename)) {
      // Only open a new file if it doesn't exist
      dataFile = SD.open(filename, FILE_WRITE);
      break; // leave the loop!
    }
  }

 // if (! SD.exists(filename)){
    dataFile = SD.open(filename, FILE_WRITE);
  //}
 
  if (! dataFile) {
    Serial.println("error opening data file");
    // Wait forever since we cant write data
    while (1) ;
  }
 
  Serial.print("Logging to: ");
  Serial.print(filename);
  dataFile.print("Filename: ");
  dataFile.print(filename);
  Serial.println(" ");
  dataFile.println(" ");
  // Add additional parameters and units in following two lines in apporpriate order if used
  Serial.println("Date[y/m/d],Time[h:m:s],Temp[deg.C],pH[S.U.],SpCond[uS/cm],DO[mg/L],DO[%Sat]"); 
  dataFile.println("Date[y/m/d],Time[h:m:s],Temp[deg.C],pH[S.U.],SpCond[uS/cm],DO[mg/L],DO[%Sat]");
    
  Serial.println("Setup complete.");
  delay(200);

}

void loop() {
 Serial.println("Logging now...");
 delay(200);
 logmode();
 Serial.println("Done Logging, Wait 30 Seconds...");
 delay(30000);
}

// Log a measurement
void logmode(){
  
  rtc.begin();  //start the real time clock
  tyme = rtc.now(); //recall time from the real time clock
  delay(200);
  // Turn on power to Tentacle shield
  digitalWrite(TentSP_pin, HIGH);
  digitalWrite(I2C_pin, HIGH);
  //digitalWrite(I2C2_pin, HIGH);
  delay(1000);
  
  // Beginning of Temp Call 
  Wire.beginTransmission(rtdaddress); //call the circuit by its ID number. 
  Wire.write("R");        //transmit the command that was sent through the serial port.
  Wire.endTransmission();          //end the I2C data transmission. 
  Serial.print(".");
  delay(500);
       
  Wire.requestFrom(rtdaddress,20,1); //call the circuit and request 20 bytes (this may be more then we need).
  code=Wire.read();               //the first byte is the response code, we read this separately.  
  Serial.print(".");    
  while(Wire.available()){          //are there bytes to receive.  
    in_char = Wire.read();           //receive a byte.
    rtd_data[i]= in_char;             //load this byte into our array.
    i+=1;                            //incur the counter for the array element. 
      if(in_char==0){                 //if we see that we have been sent a null command. 
          i=0;                        //reset the counter i to 0.
          Wire.endTransmission();     //end the I2C data transmission.
          break;                      //exit the while loop.
      }
    }
      
   int inttemp = rtd_data;
   Wire.beginTransmission(rtdaddress);
   Wire.write("Sleep");
   Wire.endTransmission();
   delay(500);
   Serial.print(".");
   
   // END of Temp Call
      
   // Beginning of PH Call
    
   Wire.beginTransmission(pHaddress); //call the circuit by its ID number. 
   Wire.write("T,"); 
   Wire.write(rtd_data);
   Wire.endTransmission();          //end the I2C data transmission. 
   Serial.print(".");
   delay(500);                    //wait the correct amount of time for the circuit to complete its instruction. 
   
   Wire.requestFrom(pHaddress,20,1); //call the circuit and request 20 bytes (this may be more then we need).
   code=Wire.read();               //the first byte is the response code, we read this separately.  
   Wire.beginTransmission(pHaddress); //call the circuit by its ID number. 
   Wire.write("R");        //transmit the command that was sent through the serial port.
   Wire.endTransmission();          //end the I2C data transmission. 
   Serial.print(".");
   delay(1000);
       
   Wire.requestFrom(pHaddress,20,1); //call the circuit and request 20 bytes (this may be more then we need).
   code=Wire.read();               //the first byte is the response code, we read this separately.  
   Serial.print(".");    
   while(Wire.available()){          //are there bytes to receive.  
    in_char = Wire.read();           //receive a byte.
    ph_data[i]= in_char;             //load this byte into our array.
    i+=1;                            //incur the counter for the array element. 
      if(in_char==0){                 //if we see that we have been sent a null command. 
          i=0;                        //reset the counter i to 0.
          Wire.endTransmission();     //end the I2C data transmission.
          break;                      //exit the while loop.
       }
    }
      
      Wire.beginTransmission(pHaddress);
      Wire.write("Sleep");
      Wire.endTransmission();
      delay(500);
      
      // END of PH Call
      
      delay(500);      
      Serial.print(".");
  
      // Beginning of EC Call
  
      Wire.beginTransmission(ecaddress); //call the circuit by its ID number.  
      Wire.write("T,"); 
      Wire.write(rtd_data);
      Wire.endTransmission();          //end the I2C data transmission. 
      Serial.print(".");
      delay(500);                    //wait the correct amount of time for the circuit to complete its instruction. 
      
      Wire.requestFrom(ecaddress,48,1); //call the circuit and request 20 bytes (this may be more then we need).
      code=Wire.read();               //the first byte is the response code, we read this separately.  
      Serial.print(".");
      Wire.beginTransmission(ecaddress); //call the circuit by its ID number. 
      Wire.write("R");        //transmit the command that was sent through the serial port.
      Wire.endTransmission();          //end the I2C data transmission. 
    
      Serial.print(".");

      delay(1000);
      
      Wire.requestFrom(ecaddress,48,1); //call the circuit and request 20 bytes (this may be more then we need).
      code=Wire.read();               //the first byte is the response code, we read this separately.  
      
      while(Wire.available()){          //are there bytes to receive.  
      in_char = Wire.read();           //receive a byte.
      ec_data[i]= in_char;             //load this byte into our array.
      i+=1;                            //incur the counter for the array element. 
        if(in_char==0){                 //if we see that we have been sent a null command. 
          i=0;                        //reset the counter i to 0.
          Wire.endTransmission();     //end the I2C data transmission.
          break;                      //exit the while loop.
        }
      }
      
      Wire.beginTransmission(ecaddress);
      Wire.write("Sleep");
      Wire.endTransmission();
      delay(500);

      // Parse out EC data from TDS, Sal, and S.G.
      
      ec=strtok(ec_data, ",");            //let's pars the string at each comma.
      tds=strtok(NULL, ",");              //let's pars the string at each comma.
      sal=strtok(NULL, ",");              //let's pars the string at each comma.
      sg=strtok(NULL, ",");               //let's pars the string at each comma.

      int intsal = sal;
      
      // END of EC Call
      
      delay(500);      
      Serial.print(".");
      
      // Beginning of DO Call
      
      Wire.beginTransmission(doaddress); //call the circuit by its ID number.  
      Wire.write("T,");                   // Temperature compensation.
      Wire.write(rtd_data);
      Wire.endTransmission();          //end the I2C data transmission. 
      Serial.print(".");
      delay(500);                    //wait the correct amount of time for the circuit to complete its instruction. 

      Wire.beginTransmission(doaddress); //call the circuit by its ID number.  
      Wire.write("S,");                  // Salinity compensation.
      Wire.write(sal);
      Wire.endTransmission();          //end the I2C data transmission. 
      Serial.print(".");
      delay(500);                    //wait the correct amount of time for the circuit to complete its instruction. 
      
      Wire.requestFrom(doaddress,48,1); //call the circuit and request 20 bytes (this may be more then we need).
      code=Wire.read();               //the first byte is the response code, we read this separately.  
      Wire.beginTransmission(doaddress); //call the circuit by its ID number.  
      Wire.write("R"); 
      Wire.endTransmission();          //end the I2C data transmission. 
      Serial.print(".");
      delay(500);                    //wait the correct amount of time for the circuit to complete its instruction. 
    
      Serial.println(".");

      delay(500);
      
      Wire.requestFrom(doaddress,48,1); //call the circuit and request 20 bytes (this may be more then we need).
      code=Wire.read();               //the first byte is the response code, we read this separately.  
      
      while(Wire.available()){          //are there bytes to receive.  
      in_char = Wire.read();           //receive a byte.
      do_data[i]= in_char;             //load this byte into our array.
      i+=1;                            //incur the counter for the array element. 
        if(in_char==0){                 //if we see that we have been sent a null command. 
          i=0;                        //reset the counter i to 0.
          Wire.endTransmission();     //end the I2C data transmission.
          break;                      //exit the while loop.
        }
      }
      
      Wire.beginTransmission(doaddress);
      Wire.write("Sleep");
      Wire.endTransmission();

      do_mg = strtok(do_data, ",");            //let's pars the string at each comma.
      do_per = strtok(NULL, ",");              //let's pars the string at each comma.
      
      // END of DO Call

  // Print timestamp to serial monitor
    Serial.println();
    Serial.print(tyme.year(), DEC);
    Serial.print('/');
    if(tyme.month() < 10){
      Serial.print('0');
      Serial.print(tyme.month(), DEC);}
    else {Serial.print(tyme.month(), DEC);}
    Serial.print('/');
    if (tyme.day() < 10){
      Serial.print('0');
      Serial.print(tyme.day(), DEC);}    
    else {Serial.print(tyme.day(), DEC);}
    Serial.print(',');
    if (tyme.hour() < 10){  
      Serial.print('0');
      Serial.print(tyme.hour(), DEC);}
    else {Serial.print(tyme.hour(), DEC);} 
    Serial.print(':');
    if (tyme.minute() < 10){
      Serial.print('0');
      Serial.print(tyme.minute(), DEC);}
    else {Serial.print(tyme.minute(), DEC);}
    Serial.print(':');
    if (tyme.second() < 10){
      Serial.print('0');
      Serial.print(tyme.second(), DEC);}
    else {Serial.print(tyme.second(), DEC);} // end of current time display}

  // Log timestamp to data file
  
    dataFile.println();
    dataFile.print(tyme.year(), DEC);
    dataFile.print('/');
    if(tyme.month() < 10){
      dataFile.print('0');
      dataFile.print(tyme.month(), DEC);}
    else {dataFile.print(tyme.month(), DEC);}
    dataFile.print('/');
    if (tyme.day() < 10){
      dataFile.print('0');
      dataFile.print(tyme.day(), DEC);}    
    else {dataFile.print(tyme.day(), DEC);}
    dataFile.print(',');
    if (tyme.hour() < 10){  
      dataFile.print('0');
      dataFile.print(tyme.hour(), DEC);}
    else {dataFile.print(tyme.hour(), DEC);} 
    dataFile.print(':');
    if (tyme.minute() < 10){
      dataFile.print('0');
      dataFile.print(tyme.minute(), DEC);}
    else {dataFile.print(tyme.minute(), DEC);}
    dataFile.print(':');
    if (tyme.second() < 10){
      dataFile.print('0');
      dataFile.print(tyme.second(), DEC);}
    else {dataFile.print(tyme.second(), DEC);} // end of current time display

      // Print to Serial Monitor
  
      Serial.print(", ");
      Serial.print(rtd_data); 
      Serial.print(", ");
      Serial.print(ph_data);     
      Serial.print(", ");
      Serial.print(ec);
      Serial.print(", ");
      Serial.print(do_mg);
      Serial.print(", ");
      Serial.println(do_per);   

      // Log to SD card

      dataFile.print(',');
      dataFile.print(rtd_data); 
      dataFile.print(',');
      dataFile.print(ph_data);     
      dataFile.print(',');
      dataFile.print(ec);
      dataFile.print(',');
      dataFile.print(do_mg);
      dataFile.print(',');
      dataFile.println(do_per);

      dataFile.flush();
      delay(5000);

      digitalWrite(I2C_pin, LOW);
      //digitalWrite(I2C2_pin, LOW);
      digitalWrite(TentSP_pin, LOW);
      delay(200);
}


