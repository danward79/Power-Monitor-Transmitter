/*
 EmonTx CT123 example
 
 An example sketch for the emontx module for
 CT only electricity monitoring.
 
 Part of the openenergymonitor.org project
 Licence: GNU GPL V3
 
 Authors: Glyn Hudson, Trystan Lea
 Builds upon JeeLabs RF12 library and Arduino
 
 emonTx documentation: 	http://openenergymonitor.org/emon/modules/emontx/
 emonTx firmware code explination: http://openenergymonitor.org/emon/modules/emontx/firmware
 emonTx calibration instructions: http://openenergymonitor.org/emon/modules/emontx/firmware/calibration

 THIS SKETCH REQUIRES:

 Libraries in the standard arduino libraries folder:
	- JeeLib		https://github.com/jcw/jeelib
	- EmonLib		https://github.com/openenergymonitor/EmonLib.git

 Other files in project directory (should appear in the arduino tabs above)
	- emontx_lib.ino
 
*/

/*
Code 4825
Ram 229
10uA - Sleep
4mA - Awake
430mS - Awake
		3	nSEL
		5	nIRQ
		7	SDO
		8	SDI
		9	SCK
		13	CT Input

                     +-\/-+
               VCC  1|    |14  GND
          (D0) PB0  2|    |13  AREF (D10)   
          (D1) PB1  3|    |12  PA1 (D9)
             RESET  4|    |11  PA2 (D8)
INT0  PWM (D2) PB2  5|    |10  PA3 (D7)
      PWM (D3) PA7  6|    |9   PA4 (D6)
      PWM (D4) PA6  7|    |8   PA5 (D5) PWM
                     +----+

							+---------------+
				SDO 	1	|  -        	|8	nSEL
				nIRQ  	2	| | |       	|9	SCK   
		FSK/DATA/nFFS	3	| | |       	|10	SDI
		DCLK/CFIL/FFIT	4	| | |       	|11 nINT/VDI
				CLK		5	|  -        	|12	GND
				nRES	6	|           	|13	VDD
				GND		7	|           	|14	Ant
							+---------------+
*/
#define FILTERSETTLETIME 4000   //  Time (ms) to allow the filters to settle before sending data
#define FREQ RF12_433MHZ        // Frequency of RF12B module can be RF12_433MHZ, RF12_868MHZ or RF12_915MHZ. You should use the one matching the module you have.
#define NODEID 5           		// emonTx RFM12B node ID
#define GROUP 212   			// emonTx RFM12B wireless network group - needs to be same as emonBase and emonGLCD

#include <avr/wdt.h>                                                     
#include <JeeLib.h>
ISR(WDT_vect) { Sleepy::watchdogEvent(); }  // Attached JeeLib sleep function to Atmega328 watchdog -enables MCU to be put into sleep mode inbetween readings to reduce power consumption 

#include "EmonLib.h"
EnergyMonitor ct1;                                              		// Create  instances for each CT channel

typedef struct { double current; int battery; } PayloadTX;      				// create structure - a neat way of packaging data for RF comms
PayloadTX emontx;                                                       

boolean settled = false;

void setup() 
{  
	ct1.currentTX(0, 60.61);                                     		// Setup CT channel (ADC input, calibration)
	// Calibration factor = CT ratio / burden resistance = (100A / 0.05A) / 33 Ohms = 60.606 +/-1.5%
  
	rf12_initialize(NODEID, FREQ, GROUP);                    
	rf12_sleep(RF12_SLEEP);
}

void loop() 
{ 
    emontx.current = ct1.calcIrms(1480);                         //ct.calcIrms(number of wavelengths sample)
	emontx.battery = ct1.readVcc();                                   

	if (!settled && millis() > FILTERSETTLETIME) settled = true;

	if (settled)                                                            
	{ 
		send_rf_data();    		
		Sleepy::loseSomeTime(5000);                                                   
	}
}

void send_rf_data()
{
  rf12_sleep(RF12_WAKEUP);
  // if ready to send + exit loop if it gets stuck as it seems too
  int i = 0; while (!rf12_canSend() && i<10) {rf12_recvDone(); i++;}
  rf12_sendStart(0, &emontx, sizeof emontx);//RF12_HDR_ACK
  // set the sync mode to 2 if the fuses are still the Arduino default
  // mode 3 (full powerdown) can only be used with 258 CK startup fuses
  rf12_sendWait(2);

  rf12_sleep(RF12_SLEEP);  
}