//FAILURE MODES//
//-occasionally gets stuck trying to connect, no print out after getting the ip address, eventually times out with wdt
//-occasionally can't get DHCP resets loop and carries on (not full reset failure)
//-for some reason, the CC3000 will often randomly reset about a minute after starting it...
//-sometimes gets stuck mid way through sending the data... so between connection to the gateway and trying to send the json, times out with wdt...

#include <SPI.h>
#include <Adafruit_CC3000.h>
//#include <ccspi.h>
#include <string.h>
#include <avr/wdt.h>
#include <limits.h>
      
      // These are the interrupt and control pins
      #define ADAFRUIT_CC3000_IRQ   3  // MUST be an interrupt pin!
      // These can be any two pins
      #define ADAFRUIT_CC3000_VBAT  5
      #define ADAFRUIT_CC3000_CS    10
      // Use hardware SPI for the remaining pins
      // On an UNO, SCK = 13, MISO = 12, and MOSI = 11
      Adafruit_CC3000 cc3000 = Adafruit_CC3000(ADAFRUIT_CC3000_CS, ADAFRUIT_CC3000_IRQ, ADAFRUIT_CC3000_VBAT,
                               SPI_CLOCK_DIVIDER); // you can change this clock speed
      
      #define WLAN_SSID       "SAFECAST JP"           // cannot be longer than 32 characters!
      #define WLAN_PASS       "bgeigie1"
      // Security can be WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2
      #define WLAN_SECURITY   WLAN_SEC_WPA2
      #define IDLE_TIMEOUT_MS  3000

// Interrupt mode:
// * For most geiger counter modules: FALLING
// * Geiger Counter Twig by Seeed Studio: RISING
const int interruptMode = FALLING;
  // Attach an interrupt to the digital pin and start counting
  //
  // Note:
  // Most Arduino boards have two external interrupts:
  // numbers 0 (on digital pin 2) and 1 (on digital pin 3) 

unsigned long counts_per_sample;


void onPulse()
  {
    counts_per_sample++;
  }


//watchdog timer setup to be 32s
volatile int wdt_counter;      // Count number of times ISR is called.
volatile int wdt_countmax = 4; //4*8s=32s


// Sampling interval (e.g. 60,000ms = 1min)
unsigned long updateIntervalInMillis = 10000;

//Srem:needsbelow const int
float updateIntervalInMinutes = 5;
// The next time to feed
// unsigned long nextExecuteMillis = 0;

// The last connection time to disconnect from the server
// after uploaded feeds
long lastConnectionTime = 0;
//function definition for elapsed time
unsigned long elapsedTime(unsigned long startTime);


//other setup stuff not wifi or wdt
#define MAX_FAILED_CONNS 3
#define SENT_SZ 200

typedef struct
{
    unsigned char state;
    unsigned char conn_fail_cnt;
    unsigned char conn_success_cnt;
    unsigned char DHCP_fail_cnt;
} devctrl_t;     

enum states
{
    NORMAL = 0,
    RESET = 1
};
      
      
static devctrl_t ctrl;
char longitude[16];
char latitude[16];
char user_id[7];
char CPM_string[15];
char alt[15];
char json_buf[200];
char lenstr[10];
uint32_t ip=1805951398;

int DHCP_count;


      
void setup() {
       
        Serial.begin(115200);
        Serial.println(F("\n\nHello Pointcast micro"));
        //Serial.println("Check for connection");
        if (!cc3000.begin()) {
          Serial.println(F("Check connections please"));
          while (1);
        }
        
        // init the control info
        memset(&ctrl, 0, sizeof(devctrl_t));
      
         
        // enable watchdog to allow reset if anything goes wrong      
        watchdogEnable(); // set up watchdog timer in interrupt-only mode if needed to turn off: wdt_disable();
        
        //Serial.println(F("Deleting old connection profiles"));
        if (!cc3000.deleteProfiles()) {
          Serial.println(F("Failed!"));
          while(1);
        }
        
        pinMode(3, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(2), onPulse, interruptMode);                // comment out to disable the GM Tube

        //SremupdateIntervalInMillis = updateIntervalInMinutes * 60000;      // update time in ms,        
        updateIntervalInMinutes = updateIntervalInMillis/60000.0; //Sadd
        
        
        wdt_reset();
      
}
//********************************************************************************************************************************************************************
//********************************************************************************************************************************************************************
//****************************SEND DATA TO SERVER****************************************************************************************************************************************
//********************************************************************************************************************************************************************
//********************************************************************************************************************************************************************
//********************************************************************************************************************************************************************

void SendDataToServer(float CPM){

//this is the code to set up the json
       strcpy(longitude , "134.1441"); //140.364314
       strcpy(latitude , "10.0000"); //37.403075
       strcpy(user_id , "100062");
       dtostrf(CPM, 0, 0, CPM_string);
               
       memset(json_buf, 0, SENT_SZ);
       sprintf_P(json_buf, PSTR("{\"longitude\":\"%s\",\"latitude\":\"%s\",\"value\":\"%s\",\"unit\":\"cpm\",\"device_id\":\"%s\"}"), \
                       longitude, \
                       latitude, \
                       CPM_string, \
                       user_id);
       
        //int len = strlen(json_buf);
        json_buf[strlen(json_buf)] = '\0';
        //Serial.print(json_buf);
        //Serial.print("The length of the json is: ");
        //Serial.println(len);
        //sprintf(lenstr, "%d", len);

//code for connecting to WiFi and sending data
       Serial.print(F("Connect to ")); Serial.println(WLAN_SSID); //Between cc3000 and wifi network
        if (!cc3000.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY)) {
          Serial.println(F("Failed to connect to the WiFi network"));
          return;
        }
       // else Serial.println(F("Connection succesful"));

      DHCP_count=0;
      while (!cc3000.checkDHCP()) //Between cc3000 and beginning of internet (setting up?)
      {
        delay(100); 
       DHCP_count++;
       if (DHCP_count==100){
        Serial.println(F("Couldn't get DHCP, reset..."));
        ctrl.DHCP_fail_cnt++;
        return;
       }
      }
      Serial.println(F("DHCP Acquired"));
      

      
      uint32_t s1 = 107;   //IP is 107.161.164.166
      uint32_t s2 = 161;
      uint32_t s3 = 164;
      uint32_t s4 = 163;
      uint32_t ip = (s1 << 24) | (s2 << 16) | (s3 << 8) | s4;
       
      //use same code outside to calculate and got 1805951398 just in case it doesn't work!
      Serial.println("ip address:");
      cc3000.printIPdotsRev(ip);
      Serial.println();
      
      Adafruit_CC3000_Client client = cc3000.connectTCP(ip, 80);
      if (client.connected()) {
          lastConnectionTime = millis();
          Serial.println(F("Sending data"));
          client.fastrprintln(F("POST /scripts/indextest.php?api_key=AzQLKPwQqkyCTDGZHSdy HTTP/1.1"));
          client.fastrprintln(F("Accept: application/json"));
          client.fastrprintln(F("Host: 107.161.164.166"));
          client.fastrprint(F("Content-Length: "));
          client.println(strlen(json_buf));
          client.fastrprintln(F("Content-Type: application/json"));
          client.println();  
          //Serial.println(F("Attempt to send json"));
          client.fastrprintln(json_buf); 
          Serial.print(F("JSON sent \nresponse: ")); 
      }
      else {
        Serial.println(F("Connection failed"));
        ctrl.conn_fail_cnt++;
        if (ctrl.conn_fail_cnt >= MAX_FAILED_CONNS){
          ctrl.state = RESET;
          ctrl.conn_fail_cnt=0;
        }
      }
      
      while (client.connected()){
        while (client.available()){
          char c = client.read();
          Serial.print(c);                  //to print the response of the gateway 
          if (c=='K'){                      //if prints HTTP/1.1 200 OK stops full printout and increases successfull send count.
            ctrl.conn_success_cnt++;
            Serial.println();
            client.close(); 
          }
        }
      }
      
      if (client.connected()) client.close();
      
      
      cc3000.disconnect(); //not the end of the 'begin' but rather the end of the connection to wifi
      Serial.println(F("Disconnected"));



}

//********************************************************************************************************************************************************************
//********************************************************************************************************************************************************************
//****************************LOOP LOOP LOOP****************************************************************************************************************************************
//********************************************************************************************************************************************************************
//********************************************************************************************************************************************************************


void loop() {

      if (ctrl.state != RESET){
            wdt_reset();  //Stag resets counter for wdt (ie 8s from now)
            wdt_counter=0;
      }
      else return;
      if (elapsedTime(lastConnectionTime) < updateIntervalInMillis)
      {
        return;
      }
      Serial.print(F("\n\tSuccessful sends: ")); Serial.println(ctrl.conn_success_cnt);
      Serial.print(F("\tFailed sends: ")); Serial.println(ctrl.conn_fail_cnt);
      Serial.print(F("\tDHCP fails: ")); Serial.println(ctrl.DHCP_fail_cnt);

      //float counts_per_sample = 169*5;
      
      
      float CPM = (float)counts_per_sample / (float)updateIntervalInMinutes;
      
      //Serial.print("\t\tcount: "); Serial.println(counts_per_sample);
      
      counts_per_sample = 0;
    
      SendDataToServer(CPM);
      
      Serial.print("\t\tCPM = "); Serial.println(CPM);  //Sadd
      
      
       
      
      
      

}

/******************************************************************************************************
//set up the watchdog timer with a 32s delay (long delay needed for time it takes to 
/*****************************************************************************************************/

void watchdogEnable()
{
 wdt_counter=0;
 cli();                              // disable interrupts
 MCUSR = 0;                          // reset status register flags
  WDTCSR |= 0b00011000;              // Put timer in interrupt-only mode:                                        
                                     // Set WDCE (5th from left) and WDE (4th from left) to enter config mode,  WDCE watchdog clear? enable, WDE watchdog enable?
                                     // using bitwise OR assignment (leaves other bits unchanged).
 WDTCSR =  0b01000000 | 0b100001;    // set WDIE (interrupt enable...7th from left, on left side of bar)
                                     // clr WDE (reset enable...4th from left)
                                     // and set delay interval (right side of bar) to 8 seconds,
                                     // using bitwise OR operator.
 sei();                              // re-enable interrupts
 //wdt_reset();                      // this is not needed...timer starts without it
}

// watchdog timer interrupt service routine
ISR(WDT_vect) {     
 wdt_counter+=1;
 if (wdt_counter < wdt_countmax)
 {
   wdt_reset(); // start timer again (still in interrupt-only mode)
 }
 else             // then change timer to reset-only mode with short (16 ms) fuse
 {
  MCUSR = 0;                          // reset flags

                                       // Put timer in reset-only mode:
   WDTCSR |= 0b00011000;               // Enter config mode.
   WDTCSR =  0b00001000 | 0b000000;    // clr WDIE (interrupt enable...7th from left)
                                       // set WDE (reset enable...4th from left), and set delay interval
                                       // reset system in 16 ms...
                                       // unless wdt_disable() in loop() is reached first

   //wdt_reset(); // not needed
 }
}


/**************************************************************************/
// calculate elapsed time, taking into account rollovers
/**************************************************************************/

unsigned long elapsedTime(unsigned long startTime)
{
  unsigned long stopTime = millis();

  if (startTime >= stopTime)
  {
    return startTime - stopTime;
  }
  else
  {
    return (ULONG_MAX - (startTime - stopTime));
  }
}

