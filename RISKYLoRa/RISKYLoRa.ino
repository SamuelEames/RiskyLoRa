//////////////////////////////////////////////////////////////////////////////////////////////
//////							 		~ RISKY LORA ~									//////
////// 		Code based off multiple example projects 									//////
//////		Brought to gether by S.Eames												//////
//////		See REAME for more detail info												//////
//////////////////////////////////////////////////////////////////////////////////////////////

// Include the things
#include <RH_RF95.h>					// LoRa Driver
#include <RHReliableDatagram.h>
#include "RiskyVars.h"				// Extra local variables
#include <FastLED.h>					// Pixel LED
#include <SPI.h>						// NFC Reader
#include <MFRC522.h>					// NFC Reader
#include <EEPROM.h>					// EEPROM used to store (this) station ID
#include <avr/sleep.h>				// Just go to sleep if there's an error
		


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


 



// TAG READER SETUP
MFRC522 mfrc522(NFC_SS, NFC_RST);				// Instanciate card reader driver
MFRC522::StatusCode status;

#define ZERO 		0x00
#define DATAROWS	12
#define DATACOLS	4
const uint8_t TAG_DATA_SIZE = DATAROWS * DATACOLS;


uint8_t DataBlock_W[TAG_DATA_SIZE];		// Data to write to tag is stored here
uint8_t *Ptr_DataBlock_W;

uint8_t DataBlock_R[TAG_DATA_SIZE];		// Data read from tag is stored here
uint8_t *Ptr_DataBlock_R;

MFRC522::MIFARE_Key key;				// Used with 1K cards

#define READLEN 				18			// 16 bytes + 2x CRC bytes
uint8_t readLen = READLEN;				// readlen var passed to NFC read functions - it is sometimes modified by these functions so can't be a constant
uint8_t readBuffer[READLEN];			// Temporary buffer tag data is stored to during reading

#define START_PAGE_ADDR  	0x04		// Initial page to read from (NTAG Stickers)
#define START_BLOCK_ADDR 	0x04		// Initial block to read from (1K Cards)
#define TRAILER_BLOCK   	0x07		// Used with 1K cards

uint8_t NFC_State = READY_FOR_TAG;

uint8_t validateCode[3] = {'C', '2', '1'};

#define MAX_UID_LEN 			10
#define NUM_LOGGED_UIDS 	10
uint8_t UID_Log[MAX_UID_LEN * NUM_LOGGED_UIDS];	// Stores the UIDs of the last NUM_LOGGED_UIDS cards logged
uint8_t lastUIDRow = 0;									// Row into which UID of last valid tag was stored


// PIXEL SETUP
#define NUM_LEDS 			24
CRGB leds[NUM_LEDS];									// Instanciate pixel driver




// LORA SETUP
#define RF95_FREQ 915.0

#define ADDR_MASTER 	0x01							// Address of master station

RH_RF95 rf95(RFM95_CS, RFM95_INT);				// Instanciate a LoRa driver
RHReliableDatagram LoRa(rf95, EEPROM.read(0));


// Holds destination Station ID, [this] Station ID, battery %, other?
#define STATION_DATA_LEN 2
// uint8_t StationData[STATION_DATA_LEN]; // don't even need this because data is saved straight into LoRa_TX_Buffer
// STATION DATA MAP
//		[0] Current State / Message type
// 	[1] Batt %	(set in TX function)


const uint8_t LORA_BUFF_LEN = STATION_DATA_LEN + MAX_UID_LEN + TAG_DATA_SIZE; // NOTE: ensure this isn't greater than LoRa.maxMessageLength() = 239 bytes
uint8_t LoRa_RecvBuffLen; 							// Set to RH_RF95_MAX_MESSAGE_LEN before each recv

uint8_t LoRa_TX_Buffer[LORA_BUFF_LEN]; 		// Buffer to transmit over LoRa
uint8_t LoRa_RX_Buffer[RH_RF95_MAX_MESSAGE_LEN];			// Data recieved from LoRa stored here
uint8_t LoRa_msgFrom;								// Holds ID of station last message was received from




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

	leds[1] = CRGB::Red;
	FastLED.show();

	// LORA SETUP
	if (!LoRa.init())
	{
		Serial.println("LoRa init failed");
		// Just go to sleep
		stop();
	}

	rf95.setFrequency(RF95_FREQ);
	rf95.setTxPower(13); 							// Setup Power,dBm

	Serial.println(F("Waiting for radio to setup"));
	delay(1000); 	// TODO - Why this delay?

	Serial.println(F("Setup completed"));


	leds[2] = CRGB::Red;
	FastLED.show();


	// CARD READER SETUP
	mfrc522.PCD_Init();					// Init MFRC522 reader

	Ptr_DataBlock_R = &DataBlock_R[0];	
	Ptr_DataBlock_W = &DataBlock_W[0];	

	// Prepare the key (used both as key A and as key B) for 1K cards
	for (byte i = 0; i < 6; i++) 
		key.keyByte[i] = 0xFF;

	fill_solid(leds, NUM_LEDS, CRGB::Green);
	FastLED.show();

	// Buzzer setup
	pinMode(BUZZ_PIN, OUTPUT);

	// Make a little noise
	digitalWrite(BUZZ_PIN, LOW);
	delay(100);
	digitalWrite(BUZZ_PIN, HIGH);

}




void loop()
{
	LoRa_RX();

	// Handle NFC errors
	if (status != MFRC522::STATUS_OK) 
	{
		Serial.print(F("\nNFC Fault: "));
		Serial.println(mfrc522.GetStatusCodeName(status));

		// Restart card sequence
		NFC_State = READY_FOR_TAG;

		// Retap card

		// If we failed during writing, retap card
			// Ensure UID is still the same
			// Go straight back to writing what we last intended (don't recopy data from tag, in case it's wrong)
	}



	switch (NFC_State)
	{
		case READY_FOR_TAG:		// Step 1 - wait for tag to be tapped
			ReadTag();
			break;

		case TAG_READ:				// Step 2 - Check tag is part of game
			ValidateTag();
			return;
			break;

		case TAG_VALID:			// Step 3 - Check if new tag, or re-tapped card
			CheckUID();
			return;
			break;

		case TAG_RETAPPED:		// Step 3.5 - If retapped card, complete write operation (TODO only if reading & processing was completed)
			WriteTag();
			return;
			break;

		case TAG_NEW:				// Step 4 - Process data & transmit over LoRa to game computer
			ProcessTag();
			return;
			break;

		case WAIT_FOR_LORA:		// Step 5 - Wait for LoRa response from game computer
			// Animate LEDs with 'loading' sequence
			break;

		case WRITE_TO_CARD:		// Step 6 - Write updated data to tag 🥳
			WriteTag();
			return;
			break;

		default:
			break;
	}
	

}


void stop()
{
	// Stop code execution if we run into an error we can't recover from
	set_sleep_mode (SLEEP_MODE_PWR_DOWN);  
	sleep_enable();
	sleep_cpu ();  

	return;
}



void LoRa_RX()
{

	if (LoRa.available())
	{
		// Wait for a message addressed to us from the client
		LoRa_RecvBuffLen = RH_RF95_MAX_MESSAGE_LEN;
		if (LoRa.recvfromAck(LoRa_RX_Buffer, LoRa_RecvBuffLen, &LoRa_msgFrom))
		{
			Serial.print(F("got request from : 0x"));
			Serial.print(LoRa_msgFrom, HEX);

			Serial.print(F("Received: "));
			for (uint8_t i = 0; i < LoRa_RecvBuffLen; ++i)
			{
				Serial.print(F(" "));
				Serial.print(LoRa_RX_Buffer[i], HEX);
			}
			Serial.println();


			// Send a VERY SHORT reply back to the originator client
			if (!LoRa.sendtoWait(&LoRa_msgFrom, 1, LoRa_msgFrom))
				Serial.println(F("sendtoWait failed"));
		}
	}

	return;
}


void LoRa_TX()
{
	// Transmit LoRa message

	// Update station data
	// LoRa_TX_Buffer[0] = GameState;				// Get current game state
	LoRa_TX_Buffer[1] = getBattPercent();		// Get current battery level
	

	if (LoRa.sendtoWait(LoRa_TX_Buffer, LORA_BUFF_LEN, ADDR_MASTER))
	{
		// Now wait for a reply from the server

		// Print sent data
		Serial.print(F("Sent: "));
		for (uint8_t i = 0; i < LORA_BUFF_LEN; ++i)
		{
			Serial.print(F(" "));
			Serial.print(LoRa_TX_Buffer[i], HEX);
		}
		Serial.println();


		LoRa_RecvBuffLen = RH_RF95_MAX_MESSAGE_LEN;  
		if (LoRa.recvfromAckTimeout(LoRa_RX_Buffer, &LoRa_RecvBuffLen, 2000, &LoRa_msgFrom))
		{
			Serial.print(F("got reply from : 0x"));
			Serial.println(LoRa_msgFrom, HEX);


			Serial.print(F("Received: "));
			for (uint8_t i = 0; i < LoRa_RecvBuffLen; ++i)
			{
				Serial.print(F(" "));
				Serial.print(LoRa_RX_Buffer[i], HEX);
			}
			Serial.println();
		}
		else
			Serial.println(F("No reply from master station?"));
	}
	else
		Serial.println(F("sendtoWait failed"));



	return;
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

	// Serial.print(F("Battery voltage = "));
	// Serial.print(voltRAW_MV);
	// Serial.println(F(" mV"));


	if (voltRAW_MV <= LIPO_MINV)			// Flat battery
		return 0;
	else if (voltRAW_MV >= LIPO_MAXV)	// Full battery
		return 255;
	else											// Somewhere inbetween battery
		return (uint8_t) ((voltRAW_MV - LIPO_MINV) / (float) (LIPO_MAXV - LIPO_MINV) * 255.0);
}

void Read_NTAG_Data()			// Used with PICC_TYPE_MIFARE_UL type
{
	// Read data from user section of tags
	for (uint8_t BlockNum = 0; BlockNum < (DATAROWS/4); ++BlockNum)
	{
		// Reset readlen back to intended size in case it changed after error
		readLen = READLEN; 

		// Get data from tag, starting from page 4 (user read/write section begins here)
		status = mfrc522.MIFARE_Read(START_PAGE_ADDR + (BlockNum * 4), readBuffer, &readLen);

		// Check reading was successful
		if (status != MFRC522::STATUS_OK) 
		{
			Serial.print(F("\nReading failed: "));
			Serial.println(mfrc522.GetStatusCodeName(status));
			return;
		}

		// Record data from tag to DataBlock_R array 
		for (int i = 0; i < 16; ++i)
			DataBlock_R[i + BlockNum * 16] = readBuffer[i];
	}

	return;
}

void Write_NTAG_Data()				// Used with PICC_TYPE_MIFARE_UL type
{
	// Writes DataBlock_W buffer to tag user memory

	for (uint8_t page = 0; page < DATAROWS; ++page)
		mfrc522.MIFARE_Ultralight_Write(page + 0x04, (Ptr_DataBlock_W + (page * DATACOLS)), 16);

	// Check writing was successful
	if (status != MFRC522::STATUS_OK) 
	{
		Serial.print(F("\nMIFARE_Write() failed: "));
		Serial.println(mfrc522.GetStatusCodeName(status));
	}

	// Read data from the block (again, should now be what we have written)
	Read_NTAG_Data();

	// Check written data is correct (e.g. may be wrong if card was removed during r/w operation)
	Compare_RW_Buffers();


	return;
}

bool Auth_1KTAG(uint8_t BlockNum)
{
	// Authenticates BlockNum of current tag - used with 1K cards
	// Authenticate using key A. Return 'true' for success and 'false' for fail
	status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, TRAILER_BLOCK + (BlockNum * 4), &key, &(mfrc522.uid));
	if (status != MFRC522::STATUS_OK) 
	{
		Serial.print(F("\nPCD_Authenticate() failed: "));
		Serial.println(mfrc522.GetStatusCodeName(status));
		return false;
	}

	return true;
}

void Read_1KTAG_Data()				// Used with PICC_TYPE_MIFARE_1K type
{
	for (uint8_t BlockNum = 0; BlockNum < (DATAROWS/8); ++BlockNum)
	{
		// Authenticate current working block
		if(!Auth_1KTAG(BlockNum))
			return;

		// Reset readlen back to intended size in case it changed after error
		readLen = READLEN; 

		// Get data from tag, starting from block 4 (user read/write section begins here)
		status = mfrc522.MIFARE_Read(START_BLOCK_ADDR + (BlockNum * 4), readBuffer, &readLen);

		// Check reading was successful
		if (status != MFRC522::STATUS_OK) 
		{
			Serial.print(F("\nReading failed: "));
			Serial.println(mfrc522.GetStatusCodeName(status));
			return;
		}

		// Record data from tag to DataBlock_R array 
		for (uint8_t i = 0; i < 16; ++i)
			DataBlock_R[i + BlockNum * 16] = readBuffer[i];
	}

	return;
}

void Write_1KTAG_Data()				// Used with PICC_TYPE_MIFARE_1K type
{
	// Writes DataBlock_W buffer to tag user memory
	// NOTE: ensure sectors used have been authenticated

	for (uint8_t BlockNum = 0; BlockNum < (DATAROWS/8); ++BlockNum)
		status = mfrc522.MIFARE_Write(START_BLOCK_ADDR + (BlockNum * 4), (Ptr_DataBlock_W + (BlockNum * 16)), 16);
	
	// Check writing was successful
	if (status != MFRC522::STATUS_OK) 
	{
		Serial.print(F("\nMIFARE_Write() failed: "));
		Serial.println(mfrc522.GetStatusCodeName(status));
	}

	// Read data from the block (again, should now be what we have written)
	Read_1KTAG_Data();

	// Check written data is correct (e.g. may be wrong if card was removed during r/w operation)
	Compare_RW_Buffers();


	return;
}

bool Compare_RW_Buffers()
{
	// Check that data in block is what we have written
	// by counting the number of bytes that are equal
	// Serial.println(F("Checking result..."));
	uint8_t count = 0;
	for (uint8_t i = 0; i < TAG_DATA_SIZE; i++) 
	{
		// Compare buffer (= what we've read) with dataBlock (= what we've written)
		if (DataBlock_W[i] == DataBlock_R[i])
			count++;
	}
	
	// Serial.print(F("Number of bytes that match = ")); 
	// Serial.println(count, DEC);

	if (count != TAG_DATA_SIZE) 
	{
		Serial.println(F("Error: r/w buffers don't match"));
		return false;
	}

	return true;
}

void Copy_R2W_Buffer()
{
	// Copies DataBlock_R into DataBlock_W buffer
	memcpy ( &DataBlock_W, &DataBlock_R, TAG_DATA_SIZE);

	return;
}

void PrintDataBlock(uint8_t *buffer)
{
	// Print read data to console - HEX
	Serial.print(F("Tag Data (HEX): "));
	for (uint8_t i = 0; i < TAG_DATA_SIZE; ++i)
	{
		Serial.print(*(buffer + i), HEX);
		Serial.print(F(" "));
	}		
	Serial.println();


	// Print read data to console - CHAR
	Serial.print(F("Tag Data (CHAR): \""));
	for (uint8_t i = 0; i < TAG_DATA_SIZE; ++i)
		Serial.write(*(buffer + i));

	Serial.println(F("\""));

	return;
}

void ReadTag()
{
	// Look for new cards
	if ( ! mfrc522.PICC_IsNewCardPresent())
		return;
	// Select one of the cards
	if ( ! mfrc522.PICC_ReadCardSerial())
		return;

	// Only read tags we're setup to process
	switch (mfrc522.PICC_GetType(mfrc522.uid.sak))
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


	// Indicate if read was successful
	if (status == MFRC522::STATUS_OK) 
		NFC_State = TAG_READ;

	return;
}

void WriteTag()
{
	// Write DataBlock_W buffer to user memory on tags
	// Only write tags we're setup to process
	switch (mfrc522.PICC_GetType(mfrc522.uid.sak))
	{
		case mfrc522.PICC_TYPE_MIFARE_UL:			// NTAG sticker
			Write_NTAG_Data();
			break;

		case mfrc522.PICC_TYPE_MIFARE_1K:			// Card/fob type
			Write_1KTAG_Data();
			break;

		default:
			break;
	}

	CloseTagComms();

	// TODO - enable re-write option if same tag is re-tapped after interrupted write (assuming it was read & validated)

	return;
}

void CloseTagComms()
{
	// Puts tags into halt state

	// Halt PICC - call once finished operations on current tag
	mfrc522.PICC_HaltA();				// Instructs a PICC in state ACTIVE(*) to go to state HALT

	// Stop encryption
	if (mfrc522.PICC_GetType(mfrc522.uid.sak) == mfrc522.PICC_TYPE_MIFARE_1K)
		mfrc522.PCD_StopCrypto1();		// Call after communicating with the authenticated PICC

	if (status == MFRC522::STATUS_OK) 
		NFC_State = READY_FOR_TAG;

	return;
}

void ValidateTag()
{
	// Checks tag is part of ecosystem
	// Included tags have "C21" in first three bytes of user data

	if (memcmp(Ptr_DataBlock_R, validateCode, sizeof(validateCode)) == 0)
	{
		// Tag is valid
		Serial.println(F("Tag Valid"));
		NFC_State = TAG_VALID;
		Copy_R2W_Buffer();
	}
	else
	{
		// Tag is invalid
		CloseTagComms();
		NFC_State = READY_FOR_TAG;
		Serial.println(F("Tag invalid"));
	}

	return;
}

void CheckUID()
{
	// Checks if current tag was tapped within last couple of tags
	// NOTE: these checks don't account for case where first uid.size bytes are the same across different tag types (uid lengths)
	// But I vaguely recall tag types are stored within UID bytes, so this may not be an issue


	int Similarity = 0;			// 0 = identical, other +/- numbers indicate mis-match 

	
	// Check last recorded UID first
	if(memcmp((UID_Log + lastUIDRow * MAX_UID_LEN), mfrc522.uid.uidByte, mfrc522.uid.size) == 0)
	{
		// Card was the same as last tapped
		// TODO - add timeout period (e.g. if tag was tapped a min later treat as new card)
		Serial.println(F("RETAPPED"));
		NFC_State = TAG_RETAPPED;
		return;
	}
	else
		NFC_State = TAG_NEW;

	
	// Check the rest of the recorded UIDs (if it wasn't last tag)
	for (uint8_t i = 0; i < NUM_LOGGED_UIDS; ++i)
	{
		Similarity = memcmp((UID_Log + i * MAX_UID_LEN), mfrc522.uid.uidByte, mfrc522.uid.size);

		// Serial.print(F("Similarity = "));
		// Serial.println(Similarity, DEC);		

		// TODO - maybe do something if Similarity = 0 for one or more of the logs
	}


	// Record current tag
	if (++lastUIDRow >= NUM_LOGGED_UIDS)		// Update location to store UID to
		lastUIDRow = 0;

	for (uint8_t i = 0; i < MAX_UID_LEN; ++i)
	{
		if (i < mfrc522.uid.size)					// Record UID
			UID_Log[lastUIDRow * MAX_UID_LEN + i] = mfrc522.uid.uidByte[i];
		else
			UID_Log[lastUIDRow * MAX_UID_LEN + i] = ZERO; 		// fill the rest with zeroes
	}


	// // Print logged tag UIDs to console
	// for (uint8_t i = 0; i < NUM_LOGGED_UIDS; ++i)
	// {
	// 	Serial.print(F("\nRow "));
	// 	Serial.print(i, HEX);

	// 	for (uint8_t j = 0; j < MAX_UID_LEN; ++j)
	// 	{
	// 		Serial.print(F("\t"));
	// 		Serial.print(UID_Log[i * MAX_UID_LEN + j], HEX);
	// 	}
	// }
	// Serial.println();


	return;
}

void ProcessTag()
{

	// Transmit tag UID & data to main computer via LoRa



	// Copy UID into LoRa_TX_Buffer
	memcpy( (LoRa_TX_Buffer + STATION_DATA_LEN), (UID_Log + lastUIDRow * MAX_UID_LEN), MAX_UID_LEN);

	// Copy tag Data into LoRa_TX_Buffer
	memcpy( (LoRa_TX_Buffer + STATION_DATA_LEN + MAX_UID_LEN), DataBlock_R, TAG_DATA_SIZE);

	// Transmit the things
	LoRa_TX();


	// Wait for response


	// Copy response back onto Tag 
	


	NFC_State = WRITE_TO_CARD;


	return;
}




// TAG UID = 04 A1 83 7A A7 5E 80	


// 2	1	0	FF	0	0	0	0	0	0	0	0	0	0	0	0	0	0	0	0	0	0	0	0	0	0	0	0	0	0	0	0	0	0	0	0	0	0	0	0	0	0	0	0	0	0	0	0	4	A1	83	7A	A7	5E	80	0	0	0	43	32	31	D	20

