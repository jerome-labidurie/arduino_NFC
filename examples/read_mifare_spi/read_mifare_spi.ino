
/**************************************************************************/
/*! 
    @file     read_mifare.pde
    @author   odopod thisispete
    @license 
    
    This file shows how to initialize either the SPI or I2C versions of the 
    pn532 development sheilds and read a mifare tag. 

*/
/**************************************************************************/


//compiler complains if you don't include this even if you turn off the I2C.h 
//@TODO: look into how to disable completely
#include <Wire.h>


//////////////////////////////////////////////
// Definitions related to BuddiesJewel project
#define DEBUG
#define BUDDYLENGTH_ULTRA 48
#define BUDDYLENGTH_CLASSIC 43
/* RGB led test */
#define PIN_RED 3
#define PIN_GREEN 5
#define PIN_BLUE 6
uint8_t r,g,b;
char c;


///////////////////////////////////////////

//I2C:
/*
#include <PN532_I2C.h>

#define IRQ   2
#define RESET 3

PN532 * board = new PN532_I2C(IRQ, RESET);
*/
//end I2C -->

//SPI:

#include <PN532_SPI.h>

#define SCK 13
#define MOSI 11
#define SS 10
#define MISO 12

PN532 * board = new PN532_SPI(SCK, MISO, MOSI, SS);

//end SPI -->

#include <Mifare.h>
Mifare mifare;
//init keys for reading classic
uint8_t Mifare::useKey = KEY_A;
uint8_t Mifare::keyA[6] = {0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7 };
uint8_t Mifare::keyB[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
uint32_t Mifare::cardType = 0; //will get overwritten if it finds a different card

#include <NDEF.h>

#define PAYLOAD_SIZE 224
uint8_t payload[PAYLOAD_SIZE] = {};

void setup(void) {
  Serial.begin(115200);

  board->begin();

  uint32_t versiondata = board->getFirmwareVersion();
  if (! versiondata) {
    Serial.println("err");
    while (1); // halt
  }
  else
  {
    Serial.println("Welcome to Buddies World!");  
  }
  
  // Got ok data, print it out!
  #ifdef DEBUG
	  Serial.print("5");Serial.println((versiondata>>24) & 0xFF, HEX); 
	  Serial.print("v: "); Serial.println((versiondata>>16) & 0xFF, DEC); 
	  Serial.println((versiondata>>8) & 0xFF, DEC);
	  Serial.print("Supports "); Serial.println(versiondata & 0xFF, HEX);
  #endif
  


  if(mifare.SAMConfig()){
    Serial.println("SAM mode enabled");  
  }else{
    Serial.println("SAM mode disabled");
  }
  
  

   // ----------- BUDDIESJEWEL specific --------
   // set pins mode
   pinMode(PIN_RED,   OUTPUT);
   pinMode(PIN_GREEN, OUTPUT);
   pinMode(PIN_BLUE,  OUTPUT);
   setColor (0,0,255);  
    // ----------- end BUDDIESJEWEL specific -------- 
  
}



void loop(void) {
 uint8_t * uid = mifare.readTarget();
 uint8_t readlength=0;
 FOUND_MESSAGE m;
 if(uid){
   Serial.println(Mifare::cardType == MIFARE_CLASSIC ?"Classic" : "Ultralight");
    
    memset(payload, 0, PAYLOAD_SIZE);
    
      //read 
    
      mifare.readPayload(payload, PAYLOAD_SIZE,&readlength);
	  

		#ifdef DEBUG
			Serial.println("length: ");
			Serial.print(readlength);
		
			Serial.print("NDEF MESSAGE: ");
			for (uint8_t i=0; i< 80; i++) {
				Serial.print(" 0x");Serial.print(payload[i], HEX);
			}
			Serial.print("\n");
		#endif 
		if (readlength==BUDDYLENGTH_ULTRA && Mifare::cardType == MIFARE_ULTRALIGHT )
		{
			m = NDEF().decode_message(payload);
			
		}
		else
		{
			if (readlength==BUDDYLENGTH_CLASSIC && Mifare::cardType == MIFARE_CLASSIC )
			{
				m = NDEF().decode_message(payload);
				
			}
			else
			{
				Serial.println("This length is not a buddylength!!!: ");
				Serial.print(readlength);
			}

		}
		
      
      switch(m.type){
       case NDEF_TYPE_URI:
         Serial.print("URI: ");
         Serial.println((int)m.format);
         Serial.println((char*) m.payload); 
        break;
       case NDEF_TYPE_TEXT:
         Serial.print("TEXT: "); 
         Serial.println(m.format);
         Serial.println((char*)m.payload);
        break;
       case NDEF_TYPE_MIME:
         Serial.print("MIME: "); 
         Serial.println(m.format);
		 Serial.print("Buddy data:");		 
		for (uint8_t i=0; i< m.payloadLength; i++) {
			Serial.print(" 0x");Serial.print(m.payload[i], HEX);
		}			
		Serial.print("\n");	
		r=m.payload[1];
		g=m.payload[2];
		b=m.payload[3];
		setColor (r, g, b);
        break;
       default:
         Serial.println("unsupported");
        break; 
      }  
	  
 }
 delay(1000);
}



/**  set the RGB LED color
 * @param red the R value (0-255)
 * @param green the G value
 * @param blue the B value
 */
void setColor(uint8_t red, uint8_t green, uint8_t blue)
{
   // anode commune, on inverse les valeurs
   analogWrite (PIN_RED,   255 - red);
   analogWrite (PIN_GREEN, 255 - green);
   analogWrite (PIN_BLUE,  255 - blue);
}
