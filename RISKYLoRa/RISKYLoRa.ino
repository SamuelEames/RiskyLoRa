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


// // #include <SPI.h>
// #include <Wire.h>
// #include <Adafruit_GFX.h>
// #include <Adafruit_SH1106.h>

#define debugMSG	1

// #define OLED_RESET 1
// Adafruit_SH1106 display(OLED_RESET);


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
MFRC522 mfbyteIn522(NFC_SS, NFC_RST);				// Instanciate card reader driver
MFRC522::StatusCode status;

#define ZERO 		0x00					// In case we want to change what zero is
#define DATAROWS	4						// Number of user pages to read from NTAG
#define DATACOLS	4						// Fixed to four
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

uint8_t NFC_State = READY_FOR_TAG;	// Used to track what stage of processing a tag we're in

uint8_t validateCode[3] = {'C', '2', '1'};		// This is stored on all tags used within game

#define MAX_UID_LEN 			10
#define NUM_LOGGED_UIDS 	10
uint8_t UID_Log[MAX_UID_LEN * NUM_LOGGED_UIDS];	// Stores the UIDs of the last NUM_LOGGED_UIDS cards logged
uint8_t lastUIDRow = 0;									// Row into which UID of last valid tag was stored


// PIXEL SETUP
#define NUM_LEDS 			24
CRGB leds[NUM_LEDS];									// Instanciate pixel driver




// LORA SETUP
#define RF95_FREQ 915.0

#define ADDR_MASTER 	21							// Address of master station

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

bool ImTheMaster;
bool newLoRaMessage = false;


// SERIAL SETUP
uint8_t SerialINBuffer[5]; 						// Size indicates number of data bytes we want (ignoring validateCode bytes)
bool newUSBSerial = false;


/*
	[ 1] 	Message type
	[ 1] 	Destination Station
	[ 1] 	Destination state

	[10] DUID
	[ 2] CC
	[ 3] SC
	[]




*/



void setup()
{
	if (EEPROM.read(0) == ADDR_MASTER)
		ImTheMaster = true;
	else
		ImTheMaster = false;


	// PIXEL SETUP
	FastLED.addLeds<WS2812B, LED_DATA, GRB>(leds, NUM_LEDS); 
	FastLED.setMaxPowerInVoltsAndMilliamps(5,5); 						// Limit total power draw of LEDs to 200mA at 5V
	fill_solid(leds, NUM_LEDS, COL_BLACK);
	leds[0] = CRGB::Red;
	FastLED.show();


	// SERIAL SETUP
	Serial.begin(115200);

	#ifdef debugMSG
		while (!Serial) ; 								// Wait for serial port to be available

		if (ImTheMaster)
			Serial.println(F("THE MASTER HAS ARRIVED"));
	#endif




	leds[1] = CRGB::Red;
	FastLED.show();

	// LORA SETUP
	if (!LoRa.init())
	{
		#ifdef debugMSG
			Serial.println("LoRa init failed");
		#endif	
		// Just go to sleep
		// stop();
	}

	rf95.setFrequency(RF95_FREQ);
	rf95.setTxPower(13); 							// Setup Power,dBm

	// Serial.println(F("Waiting for radio to setup"));
	delay(1000); 	// TODO - Why this delay?

	#ifdef debugMSG
		Serial.println(F("Setup completed"));
	#endif




	leds[2] = CRGB::Red;
	FastLED.show();


	// if (!ImTheMaster) 	// Master station doesn't have card reader, so don't bother setting it up
	// {
	// CARD READER SETUP
	mfbyteIn522.PCD_Init();					// Init MFRC522 reader

	Ptr_DataBlock_R = &DataBlock_R[0];	
	Ptr_DataBlock_W = &DataBlock_W[0];	

	// Prepare the key (used both as key A and as key B) for 1K cards
	for (byte i = 0; i < 6; i++) 
		key.keyByte[i] = 0xFF;		
	// }


	fill_solid(leds, NUM_LEDS, CRGB::Green);
	FastLED.show();

	// Buzzer setup
	pinMode(BUZZ_PIN, OUTPUT);

	// Make a little noise
	digitalWrite(BUZZ_PIN, LOW);
	delay(100);
	digitalWrite(BUZZ_PIN, HIGH);


	// display.begin(SH1106_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3D (for the 128x64)

}




void loop()
{
	LoRa_RX();


	if (ImTheMaster)
	{
		// MASTER STATION

		USBSerial_RX();

		if (newLoRaMessage)
		{
			USBSerial_TX();
			newLoRaMessage = false;

			// Get response from USBSerial
		}

		if (newUSBSerial)
		{
			// Copy USB Data to LoRa_TX buffer


			// Send data over LoRa
			LoRa_TX(SerialINBuffer[0]);


			// Reset USBSerial flag
			newUSBSerial = false;
		}

	}
	else
	{
		// SLAVE STATIONS

		// Handle NFC errors
		if (status != MFRC522::STATUS_OK) 
		{
			#ifdef debugMSG
				Serial.print(F("\nNFC Fault: "));
				Serial.println(mfbyteIn522.GetStatusCodeName(status));
			#endif

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


			#ifdef debugMSG
				Serial.print(F("got request from : 0x"));
				Serial.println(LoRa_msgFrom, HEX);

				Serial.print(F("Received: "));
				for (uint8_t i = 0; i < LoRa_RecvBuffLen; ++i)
				{
					Serial.print(F(" "));
					Serial.print(LoRa_RX_Buffer[i], HEX);
				}
				Serial.println();
			#endif


			newLoRaMessage = true;

			// Send a VERY SHORT reply back to the originator client
			if (!LoRa.sendtoWait(&LoRa_msgFrom, 1, LoRa_msgFrom))
			{
				;
				#ifdef debugMSG
					Serial.println(F("sendtoWait failed"));
				#endif
			}
		}
	}

	return;
}


void LoRa_TX(uint8_t toAddr)
{
	// Transmit LoRa message

	// Update station data
	// LoRa_TX_Buffer[0] = GameState;				// Get current game state
	LoRa_TX_Buffer[1] = getBattPercent();		// Get current battery level
	

	if (LoRa.sendtoWait(LoRa_TX_Buffer, LORA_BUFF_LEN, toAddr))
	{
		// Now wait for a reply from the server

		// Print sent data
		#ifdef debugMSG
			Serial.print(F("Sent: "));
			for (uint8_t i = 0; i < LORA_BUFF_LEN; ++i)
			{
				Serial.print(F(" "));
				Serial.print(LoRa_TX_Buffer[i], HEX);
			}
			Serial.println();
		#endif


		LoRa_RecvBuffLen = RH_RF95_MAX_MESSAGE_LEN;  
		if (LoRa.recvfromAckTimeout(LoRa_RX_Buffer, &LoRa_RecvBuffLen, 2000, &LoRa_msgFrom))
		{
			;
			#ifdef debugMSG
				Serial.print(F("got reply from : 0x"));
				Serial.println(LoRa_msgFrom, HEX);


				Serial.print(F("Received: "));
				for (uint8_t i = 0; i < LoRa_RecvBuffLen; ++i)
				{
					Serial.print(F(" "));
					Serial.print(LoRa_RX_Buffer[i], HEX);
				}
				Serial.println();
			#endif
		}
		else
		{
			#ifdef debugMSG
				Serial.println(F("No reply from master station?"));
			#endif
		}
	}
	else
	{
		#ifdef debugMSG
			Serial.println(F("sendtoWait failed"));
		#endif
	}



	return;
}




uint8_t getBattPercent()
{
	/* Returns battery pebyteInent as a byte
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
		status = mfbyteIn522.MIFARE_Read(START_PAGE_ADDR + (BlockNum * 4), readBuffer, &readLen);

		// Check reading was successful
		if (status != MFRC522::STATUS_OK) 
		{
			Serial.print(F("\nReading failed: "));
			Serial.println(mfbyteIn522.GetStatusCodeName(status));
			return;
		}

		// Record data from tag to DataBlock_R array 
		for (int i = 0; i < 16; ++i)
			DataBlock_R[i + BlockNum * 16] = readBuffer[i];
	}

	PrintDataBlock(DataBlock_R);

	return;
}

void Write_NTAG_Data()				// Used with PICC_TYPE_MIFARE_UL type
{
	// Writes DataBlock_W buffer to tag user memory

	for (uint8_t page = 0; page < DATAROWS; ++page)
		mfbyteIn522.MIFARE_Ultralight_Write(page + 0x04, (Ptr_DataBlock_W + (page * DATACOLS)), 16);

	// Check writing was successful
	if (status != MFRC522::STATUS_OK) 
	{
		#ifdef debugMSG
			Serial.print(F("\nMIFARE_Write() failed: "));
			Serial.println(mfbyteIn522.GetStatusCodeName(status));
		#endif
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
	status = mfbyteIn522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, TRAILER_BLOCK + (BlockNum * 4), &key, &(mfbyteIn522.uid));
	if (status != MFRC522::STATUS_OK) 
	{
		#ifdef debugMSG
			Serial.print(F("\nPCD_Authenticate() failed: "));
			Serial.println(mfbyteIn522.GetStatusCodeName(status));
		#endif
		return false;
	}

	return true;
}

void Read_1KTAG_Data()				// Used with PICC_TYPE_MIFARE_1K type
{
	for (uint8_t BlockNum = 0; BlockNum < (DATAROWS/4); ++BlockNum)
	{
		// Authenticate current working block
		if(!Auth_1KTAG(BlockNum))
			return;

		// Reset readlen back to intended size in case it changed after error
		readLen = READLEN; 

		// Get data from tag, starting from block 4 (user read/write section begins here)
		status = mfbyteIn522.MIFARE_Read(START_BLOCK_ADDR + (BlockNum * 4), readBuffer, &readLen);

		// Check reading was successful
		if (status != MFRC522::STATUS_OK) 
		{
			#ifdef debugMSG
				Serial.print(F("\nReading failed: "));
				Serial.println(mfbyteIn522.GetStatusCodeName(status));
			#endif
			return;
		}

		// Record data from tag to DataBlock_R array 
		for (uint8_t i = 0; i < 16; ++i)
			DataBlock_R[i + BlockNum * 16] = readBuffer[i];
	}


	PrintDataBlock(DataBlock_R);

	return;
}

void Write_1KTAG_Data()				// Used with PICC_TYPE_MIFARE_1K type
{
	// Writes DataBlock_W buffer to tag user memory
	// NOTE: ensure sectors used have been authenticated

	for (uint8_t BlockNum = 0; BlockNum < (DATAROWS/8); ++BlockNum)
		status = mfbyteIn522.MIFARE_Write(START_BLOCK_ADDR + (BlockNum * 4), (Ptr_DataBlock_W + (BlockNum * 16)), 16);
	
	// Check writing was successful
	if (status != MFRC522::STATUS_OK) 
	{
		#ifdef debugMSG
			Serial.print(F("\nMIFARE_Write() failed: "));
			Serial.println(mfbyteIn522.GetStatusCodeName(status));
		#endif
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
		#ifdef debugMSG
			Serial.println(F("Error: r/w buffers don't match"));
		#endif
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

#ifdef debugMSG
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
#endif

void ReadTag()
{
	// Look for new cards
	if ( ! mfbyteIn522.PICC_IsNewCardPresent())
		return;
	// Select one of the cards
	if ( ! mfbyteIn522.PICC_ReadCardSerial())
		return;

	// Only read tags we're setup to process
	switch (mfbyteIn522.PICC_GetType(mfbyteIn522.uid.sak))
	{
		case mfbyteIn522.PICC_TYPE_MIFARE_UL:			// NTAG sticker
			Read_NTAG_Data();
			break;

		case mfbyteIn522.PICC_TYPE_MIFARE_1K:			// Card/fob type
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
	switch (mfbyteIn522.PICC_GetType(mfbyteIn522.uid.sak))
	{
		case mfbyteIn522.PICC_TYPE_MIFARE_UL:			// NTAG sticker
			Write_NTAG_Data();
			break;

		case mfbyteIn522.PICC_TYPE_MIFARE_1K:			// Card/fob type
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
	mfbyteIn522.PICC_HaltA();				// Instructs a PICC in state ACTIVE(*) to go to state HALT

	// Stop encryption
	if (mfbyteIn522.PICC_GetType(mfbyteIn522.uid.sak) == mfbyteIn522.PICC_TYPE_MIFARE_1K)
		mfbyteIn522.PCD_StopCrypto1();		// Call after communicating with the authenticated PICC

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
		#ifdef debugMSG
			Serial.println(F("Tag Valid"));
		#endif
		NFC_State = TAG_VALID;
		Copy_R2W_Buffer();
	}
	else
	{
		// Tag is invalid
		CloseTagComms();
		NFC_State = READY_FOR_TAG;

		#ifdef debugMSG
			Serial.println(F("Tag invalid"));
		#endif
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
	if(memcmp((UID_Log + lastUIDRow * MAX_UID_LEN), mfbyteIn522.uid.uidByte, mfbyteIn522.uid.size) == 0)
	{
		// Card was the same as last tapped
		// TODO - add timeout period (e.g. if tag was tapped a min later treat as new card)
		#ifdef debugMSG
			Serial.println(F("RETAPPED"));
		#endif

		NFC_State = TAG_RETAPPED;
		return;
	}
	else
		NFC_State = TAG_NEW;

	
	// Check the rest of the recorded UIDs (if it wasn't last tag)
	for (uint8_t i = 0; i < NUM_LOGGED_UIDS; ++i)
	{
		Similarity = memcmp((UID_Log + i * MAX_UID_LEN), mfbyteIn522.uid.uidByte, mfbyteIn522.uid.size);

		// Serial.print(F("Similarity = "));
		// Serial.println(Similarity, DEC);		

		// TODO - maybe do something if Similarity = 0 for one or more of the logs
	}


	// Record current tag
	if (++lastUIDRow >= NUM_LOGGED_UIDS)		// Update location to store UID to
		lastUIDRow = 0;

	for (uint8_t i = 0; i < MAX_UID_LEN; ++i)
	{
		if (i < mfbyteIn522.uid.size)					// Record UID
			UID_Log[lastUIDRow * MAX_UID_LEN + i] = mfbyteIn522.uid.uidByte[i];
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
	LoRa_TX(ADDR_MASTER);


	// Wait for response


	// Copy response back onto Tag 
	


	NFC_State = WRITE_TO_CARD;


	return;
}


void USBSerial_TX()
{
	// Dumps tag data to USB serial for master computer

	// Start Block
	Serial.print(F("# "));
	
	// Message From
	Serial.print(LoRa_msgFrom, DEC);
	Serial.print(F(", "));

	// Station State, Battery Level, UID
	for (uint8_t i = 0; i < 12; ++i)
	{
		Serial.print(LoRa_RX_Buffer[i], DEC);
		Serial.print(F(", "));		
	}

	// Card Type
	Serial.print(LoRa_RX_Buffer[0], DEC);	// TODO - work out what the ref should be here
	Serial.print(F(", "));		

	// Team
	Serial.print(LoRa_RX_Buffer[0], DEC);// TODO - work out what the ref should be here

	// End block
	Serial.print(F("~\n"));	

	// 

	/*
	// LoRa_RX_Buffer
			* [0] Station state (game ID, mode, etc)
			* [1] Battery Level
			* [2-12] UID[MAX_UID_LEN]
			* TAG_USER_DATA
				* 'C21' (3x char)
				* Card Type (char)
					> 'C' = CC Modifier
					> 'K' = Kid
					> 'L' = Lifegroup Leader
					> 'P' = Pastor
					> 'S' = Other Leader
					> 'T' = Team Form Card
			* Conference Cash value (int16)
			* Store Cash (int16)
			* Time 1 (4x byte)
			* Time 2 (4x byte)
			* Name (16x char)
			* MISC (16x bytes)
	*/

	// for (uint8_t i = 0; i < LORA_BUFF_LEN; ++i)
	// 	Serial.write(LoRa_RX_Buffer[i]);

	// // End Block
	// for (uint8_t i = 0; i < 3; ++i)
	// 	Serial.write('~');

	// // Newline
	// Serial.write('\n');

	return;
}

void USBSerial_RX()
{
	// Gets data from master computer and writes to transmit buffer to send out to stations

	static uint8_t index = 0;
	static uint8_t validCheck = 0;
	static uint8_t loopNum = 0;
	char byteIn;

	// NOTE: newUSBSerial is set to true once a complete message has been received
	// 		It should be set back to false once the message has been handled.

	while (Serial.available() > 0 && newUSBSerial == false) 
	{
		// Serial.println(F("New byte!"));
		byteIn = Serial.read();					// Read single character

		loopNum++;

		
		if (validCheck >= sizeof(validateCode))			// STORE DATA (after doing checks below)
		{
			// Serial.print(F("ValidCheck = "));
			// Serial.print(validCheck, DEC);

			// Serial.print(F("\t index = "));
			// Serial.print(index, DEC);

			// Serial.print(F("\t loopNum = "));
			// Serial.println(loopNum, DEC);
			
			if (index < sizeof(SerialINBuffer))
				SerialINBuffer[index++] = byteIn;
			else													// Once we've read the bytes we expect, reset
			{
				Serial.println(F("Message received!"));
				// Reset
				newUSBSerial = true;
				loopNum = 0;
				validCheck = 0;
				index = 0;
			}
		}
		else if (byteIn == validateCode[loopNum-1])	// Check for validateCode sequence
			validCheck++;
		else 														// Incorrect validateCode - restart checks
		{
			Serial.println(F("Incorrect validateCode"));
			// Reset
			loopNum = 0;
			validCheck = 0;
		}
	}

	return;
}