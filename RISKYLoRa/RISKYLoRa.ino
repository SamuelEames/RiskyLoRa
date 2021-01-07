//////////////////////////////////////////////////////////////////////////////////////////////
//////							 		~ RISKY LORA ~									//////
////// 		Code based off multiple example projects 									//////
//////		Brought to gether by S.Eames												//////
//////		See REAME for more detail info												//////
//////////////////////////////////////////////////////////////////////////////////////////////

// Include the things
#include <RH_RF95.h>					// LoRa Driver
#include <RHEncryptedDriver.h>	// LoRa Encryption 
#include <Speck.h>					// LoRa Encryption 
#include "RiskyVars.h"				// Extra local variables
#include <FastLED.h>					// Pixel LED
#include <SPI.h>						// NFC Reader
#include <MFRC522.h>					// NFC Reader
		


// ARDUINO PIN SETUP
#define RFM95_CS 		18
#define RFM95_RST 	10
#define RFM95_INT 	0

#define BVOLT_PIN		A6				// Analog pin for battery voltage measurement
#define BUZZ_PIN		5				// Buzzer
#define LED_DATA 		6				// Pixel LED String


#define NFC_SS			19 			// 'SDA' Pin
#define NFC_RST		20 			// Reset Pin


// BATTERY PARAMETERS
#define LIPO_MINV  	9600		//(mV) 9.6V - minimum voltage with a little safety factor
#define LIPO_MAXV 	12600 	//(mV) 12.6V - fully charged battery 


 

// LORA SETUP
#define RF95_FREQ 915.0

RH_RF95 rf95(RFM95_CS, RFM95_INT);				// Instanciate a LoRa driver
Speck myCipher;										// Instanciate a Speck block ciphering
RHEncryptedDriver LoRa(rf95, myCipher);		// Instantiate the driver with those two

unsigned char encryptkey[16]={1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16}; // Encryption Key - keep secret ;)

char HWMessage[] = "Hello World ! I'm happy if you can read me";
uint8_t HWMessageLen;


// TAG READER SETUP
MFRC522 mfrc522(NFC_SS, NFC_RST);				// Instanciate card reader driver
MFRC522::StatusCode status;

#define ZERO 		0x00
#define DATAROWS	12
#define DATACOLS	4
const uint8_t DATA_SIZE = DATAROWS * DATACOLS;

// Data to write to tag is stored here
uint8_t DataBlock_W[DATA_SIZE];
uint8_t *Ptr_DataBlock_W;

// Data read from tag is stored here
uint8_t DataBlock_R[DATA_SIZE];
uint8_t *Ptr_DataBlock_R;

MFRC522::MIFARE_Key key;							// Used with 1K cards



// PIXEL SETUP
#define NUM_LEDS 			24
CRGB leds[NUM_LEDS];									// Instanciate pixel driver


void setup()
{
	// PIXEL SETUP
	FastLED.addLeds<WS2812B, LED_DATA, GRB>(leds, NUM_LEDS); 
	FastLED.setMaxPowerInVoltsAndMilliamps(5,5); 						// Limit total power draw of LEDs to 200mA at 5V
	fill_solid(leds, NUM_LEDS, COL_BLACK);
	leds[0] = CRGB::Red;
	FastLED.show();


	// SERIAL SETUP
	Serial.begin(115200);
	while (!Serial) ; 								// Wait for serial port to be available

	Serial.println("LoRa Simple_Encrypted");

	leds[1] = CRGB::Red;
	FastLED.show();

	// LORA SETUP
	HWMessageLen = strlen(HWMessage);

	if (!rf95.init())
		Serial.println("LoRa init failed");
	
	rf95.setTxPower(13); 							// Setup Power,dBm
	myCipher.setKey(encryptkey, sizeof(encryptkey));

	Serial.println("Waiting for radio to setup");
	delay(1000); 	// TODO - Why this delay?

	Serial.println("Setup completed");

	Serial.print("Max message length = ");
	Serial.print(LoRa.maxMessageLength());


	leds[2] = CRGB::Red;
	FastLED.show();

	// LoRa_TX();

	// delay(5000);

	// LoRa_TX();

	// CARD READER SETUP
	mfrc522.PCD_Init();					// Init MFRC522 reader

	Ptr_DataBlock_R = &DataBlock_R[0];	
	Ptr_DataBlock_W = &DataBlock_W[0];	

	// Prepare the key (used both as key A and as key B) for 1K cards
	for (byte i = 0; i < 6; i++) 
		key.keyByte[i] = 0xFF;

	fill_solid(leds, NUM_LEDS, CRGB::Green);
	FastLED.show();

}

void loop()
{
	LoRa_RX();

	// getBattPercent();


	CheckForCard();




	delay(500);
}



void LoRa_RX()
{
	if (LoRa.available())
	{
		// Should be a message for us now   
		uint8_t buf[LoRa.maxMessageLength()];
		uint8_t len = sizeof(buf);

		if (LoRa.recv(buf, &len)) 
		{
			Serial.print("Received: ");
			Serial.println((char *)&buf);
		}
		else
		{
			Serial.println("recv failed");
		}
	}
}

void LoRa_TX()
{
	uint8_t data[HWMessageLen+1] = {0};
	for(uint8_t i = 0; i<= HWMessageLen; i++) data[i] = (uint8_t)HWMessage[i];

	LoRa.send(data, sizeof(data)); // Send out ID + Sensor data to LoRa gateway

	Serial.print("Sent: ");

	Serial.println((char *)&data);
	
	delay(4000);
}




uint8_t getBattPercent()
{
	/* Returns battery percent as a byte
			* 0 = 0%, 255 = 100%
			* Setup to monitor a 3C lipo (0V = 9V, 100% = 12.6V)
			* Voltage sensor scales voltages down by a factor of 5 via voltage divider


			FORMULAS
			R1 = 30000.0;
			R2 = 7500.0;
			vout = (value * 5.0) / 1024.0;
   		vin = vout / (R2/(R1+R2)); 

   		I did some simplifying of the formulas
	*/

	// Calculate voltage in mV
	uint16_t voltRAW_MV = analogRead(BVOLT_PIN) * 24.4140;

	Serial.print("Battery voltage = ");
	Serial.print(voltRAW_MV);
	Serial.println(" mV");


	if (voltRAW_MV <= LIPO_MINV)			// Flat battery
		return 0;
	else if (voltRAW_MV >= LIPO_MAXV)	// Full battery
		return 255;
	else											// Somewhere inbetween battery
		return (uint8_t) ((voltRAW_MV - LIPO_MINV) / (float) (LIPO_MAXV - LIPO_MINV) * 255.0);
}







void Read_NTAG_Data()			// Used with PICC_TYPE_MIFARE_UL type
{
	// Read buffer from tag
	const uint8_t readlen = 18;				// 16 bytes + 2x CRC bytes
	uint8_t readBuffer[readlen];
	

	// Read data from user section of tags
	for (uint8_t ReadBlock = 0; ReadBlock < (DATAROWS/4); ++ReadBlock)
	{
		// Get data from tag, starting from page 4 (user read/write section begins here)
		status = mfrc522.MIFARE_Read(4 + (ReadBlock * 4), readBuffer, &readlen);

		// Check reading was successful
		if (status != MFRC522::STATUS_OK) 
		{
			Serial.println("");
			Serial.print(F("Reading failed: "));
			Serial.println(mfrc522.GetStatusCodeName(status));

			return;
		}

		// Record data from tag to TagBuffer array 
		for (int i = 0; i < 16; ++i)
			DataBlock_R[i + ReadBlock * 16] = readBuffer[i];
	}


	PrintDataBlock(Ptr_DataBlock_R);

	return;
}


void Read_1KTAG_Data()				// Used with PICC_TYPE_MIFARE_1K type
{
	// Read buffer from tag
	const uint8_t readlen = 18;
	uint8_t readBuffer[readlen];
	
/*
	// Read data from user section of tags
	for (uint8_t ReadBlock = 0; ReadBlock < (DATAROWS/4); ++ReadBlock)
	{
		// Get data from tag, starting from page 4 (user read/write section begins here)
		status = mfrc522.MIFARE_Read(4 + (ReadBlock * 4), readBuffer, &readlen);

		// Check reading was successful
		if (status != MFRC522::STATUS_OK) 
		{
			Serial.println("");
			Serial.print(F("Reading failed: "));
			Serial.println(mfrc522.GetStatusCodeName(status));

			return;
		}

		// Record data from tag to TagBuffer array 
		for (int i = 0; i < 16; ++i)
			DataBlock_R[i + ReadBlock * 16] = readBuffer[i];
	}*/


	 // In this sample we use the second sector,
	 // that is: sector #1, covering block #4 up to and including block #7
	// uint8_t sector         = 1;
	uint8_t blockAddr      = 4;
	uint8_t trailerBlock   = 7;
	// MFRC522::StatusCode status;
	// uint8_t buffer[18];
	// uint8_t size = sizeof(buffer);

	// Authenticate using key A
	// Serial.println(F("Authenticating using key A..."));
	status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(mfrc522.uid));
	if (status != MFRC522::STATUS_OK) 
	{
		// cardSuccess = 0;
		// FirstScreenPass = true;
		// onStartMillis = millis();
		return;
	}

	for (uint8_t ReadBlock = 0; ReadBlock < (DATAROWS/8); ++ReadBlock)
	{
		// Get data from tag, starting from block 4 (user read/write section begins here)
		// status = mfrc522.MIFARE_Read(4 + (ReadBlock * 4), readBuffer, &readlen);
		status = mfrc522.MIFARE_Read(blockAddr + (ReadBlock * 4), readBuffer, &readlen);

		// Check reading was successful
		if (status != MFRC522::STATUS_OK) 
		{
			Serial.println("");
			Serial.print(F("Reading failed: "));
			Serial.println(mfrc522.GetStatusCodeName(status));

			return;
		}

		// Record data from tag to TagBuffer array 
		for (int i = 0; i < 16; ++i)
			DataBlock_R[i + ReadBlock * 16] = readBuffer[i];
	}

	// Read data from the block
	
	// if (status != MFRC522::STATUS_OK) 
	// {
	// 	cardSuccess = 0;
	// 	FirstScreenPass = true;
	// 	onStartMillis = millis();

	// 	return;
	// }


	// // TODO - DO I NEED TO DO THIS EXACT SAME THING AGAIN??? - SEE IF IT WORKS WITHOUT IT
	// // Authenticate using key B --> NOTE: HAD TO CHANGE THIS TO 'A' for it to work on the cards I'm using
	// // Serial.println("Authenticating again using key A..."); 
	// status = (MFRC522::StatusCode) mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(mfrc522.uid));
	// if (status != MFRC522::STATUS_OK) 
	// {
	// 	cardSuccess = 0;
	// 	FirstScreenPass = true;
	// 	onStartMillis = millis();
	// 	return;
	// }


	// // Write data to the block
	// copyBuffer(readBuffer, 16);


	// if (StationType == ST_SCORE)
	// 	extractTimes(dataBlock, 16);
	// else
	// 	recordTime();


	// // Serial.print(F("Writing data into block ")); Serial.print(blockAddr);
	// // Serial.println(F(" ..."));
	// // dump_byte_array(dataBlock, 16); Serial.println();
	// status = (MFRC522::StatusCode) mfrc522.MIFARE_Write(blockAddr, dataBlock, 16);
	// if (status != MFRC522::STATUS_OK) 
	// {
	// 	// Serial.print(F("MIFARE_Write() failed: "));
	// 	// Serial.println(mfrc522.GetStatusCodeName(status));
	// 	cardSuccess = 0;
	// 	FirstScreenPass = true;
	// 	onStartMillis = millis();
	// }
	// Serial.println();

	// // Read data from the block (again, should now be what we have written)
	// status = (MFRC522::StatusCode) mfrc522.MIFARE_Read(blockAddr, readBuffer, &readlen);
	// if (status != MFRC522::STATUS_OK) 
	// {
	// 	// Serial.print(F("MIFARE_Read() failed: "));
	// 	Serial.println(mfrc522.GetStatusCodeName(status));
	// 	cardSuccess = 0;
	// 	FirstScreenPass = true;
	// 	onStartMillis = millis();
	// }


	// // Check that data in block is what we have written
	// // by counting the number of bytes that are equal
	// // Serial.println(F("Checking result..."));
	// uint8_t count = 0;
	// for (byte i = 0; i < 16; i++) 
	// {
	// 	// Compare buffer (= what we've read) with dataBlock (= what we've written)
	// 	if (readBuffer[i] == dataBlock[i])
	// 		count++;
	// }
	// // Serial.print(F("Number of bytes that match = ")); Serial.println(count);
	// if (count == 16) 
	// {
	// 	// Serial.println(F("Success :)"));
	// 	cardSuccess = 1;
	// 	FirstScreenPass = true;
	// 	onStartMillis = millis();
	// } 
	// else 
	// {
	// 	// Serial.println(F("Failure, no match :("));
	// 	// Serial.println(F("  perhaps the write didn't work properly..."));
	// 	cardSuccess = 0;
	// 	FirstScreenPass = true;
	// 	onStartMillis = millis();
	// }
	// // Serial.println();


	// Halt PICC
	mfrc522.PICC_HaltA();
	// Stop encryption on PCD
	mfrc522.PCD_StopCrypto1();









	PrintDataBlock(Ptr_DataBlock_R);

	return;
}





void PrintDataBlock(uint8_t *buffer)
{
	// Print read data to console - HEX
	Serial.print("Tag Data (HEX): ");
	for (uint8_t i = 0; i < DATA_SIZE; ++i)
	{
		Serial.print(*(buffer + i), HEX);
		Serial.print(" ");
	}		
	Serial.println();


	// Print read data to console - CHAR
	Serial.print("Tag Data (CHAR): \"");
	for (uint8_t i = 0; i < DATA_SIZE; ++i)
		Serial.write(*(buffer + i));

	Serial.println("\"");

	return;
}



void CheckForCard()
{
	// Reads card if present


	// Look for new cards
	if ( ! mfrc522.PICC_IsNewCardPresent())
		return;
	// Select one of the cards
	if ( ! mfrc522.PICC_ReadCardSerial())
		return;

	// Check card type
	uint8_t TagType = mfrc522.PICC_GetType(mfrc522.uid.sak);


	switch (TagType)
	{
		case mfrc522.PICC_TYPE_MIFARE_UL:			// NTAG sticker
			Read_NTAG_Data();
			break;
		case mfrc522.PICC_TYPE_MIFARE_1K:			// Card/fob type
			Read_1KTAG_Data();
			break;
		default:
			return;
			break;
	}








	// 

	// Respond to tag if it's part of game

}