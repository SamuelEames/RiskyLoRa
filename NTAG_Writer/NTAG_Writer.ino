/*
Writes data to NFC tags

THE PROGRAM
 * Connect serial
 * Get name from serial input ending in #
 * Tap tag to write to
 * Write to tag
 * Check data was written correctly --> output written data to console with tag UID
 * Repeat steps (input name --> write tag --> check tag

*/

#include <SPI.h>						// NFC Reader
#include <MFRC522.h>					// NFC Reader
#include <Keyboard.h>
#include <FastLED.h>					// Pixel LED

// #define debugMSG	1				// Uncomment to get debug / error messages in console


#define BUZZ_PIN		5				// Buzzer

//////////////////////////////////
/////////// PIXEL SETUP //////////
//////////////////////////////////
#define NUM_LEDS 		12
#define LED_DATA 		6				// Pixel LED String
CRGB leds[NUM_LEDS];

// General
#define COL_BLACK			0x000000		// Black
#define COL_WHITE			0x3F3F3F		// White
#define COL_D_WHITE		0x0F0F0F		// Dim White

// Tribes Bright
#define COL_YELLOW		0xFF6F00 	// Yellow
#define COL_ORANGE		0xFF1C00 	// Orange
#define COL_PURPLE		0x8F00FF 	// Purple
#define COL_CYAN			0x00AAAA 	// Cyan
#define COL_GREEN			0x00FF0A 	// Green
#define COL_BLUE			0x0000FF 	// Blue
#define COL_RED			0xFF0000 	// Red
#define COL_MAGENTA		0xFF002F 	// Magenta

//////////////////////////////////
//////// TAG READER SETUP ////////
//////////////////////////////////
#define ZERO 			0x00			// In case we want to change what zero is
#define DATAROWS		4				// Number of user pages to read from NTAG
#define DATACOLS		4				// Fixed to four
const uint8_t TAG_DATA_SIZE = DATAROWS * DATACOLS;

uint8_t validateCode[3] = {'C', '2', '1'};		// This is stored on all tags used within game

uint8_t tagType = 'K';				// Type of tag
											// 'X' = unassigned
											// 'K' = kid
											//	'L' = leader
											// 'P' = Pastor
											// 'T' = Team modifier

uint8_t teamNum = 'X';				// Team that tag is associated to
											// 'Z' = unassigned
											// 'A' --> 'Y' = team

#define NFC_SS			19 			// 'SDA' Pin
#define NFC_RST		20 			// Reset Pin

MFRC522 mfrc522(NFC_SS, NFC_RST);				// Instanciate card reader driver
MFRC522::StatusCode status;

//////////////////////////////////
////////// VARIABLES /////////////
//////////////////////////////////

// // Array copied onto NFC Tags

uint8_t DataBlock[TAG_DATA_SIZE];
// uint8_t *Ptr_DataBlock;

unsigned char InBuffer[TAG_DATA_SIZE]; 			// Array to store text data from console
unsigned char TagBufferTest[TAG_DATA_SIZE]; 	// Used to test tag data matches written data
boolean newData = false;				// False until newline chatacter is received fron console. Cleared once printed.
boolean readyToTag = false;			// True once newData is received. False after data is written to tag


//////////////////////////////////



void setup() 
{
 	// Ptr_DataBlock = &DataBlock[0];		// Initialise pointer to first elelment of DataBlock

 	// Buzzer setup
	pinMode(BUZZ_PIN, OUTPUT);
	digitalWrite(BUZZ_PIN, HIGH);

	// PIXEL SETUP
	FastLED.addLeds<WS2812B, LED_DATA, GRB>(leds, NUM_LEDS); 
	FastLED.setMaxPowerInVoltsAndMilliamps(5,5); 						// Limit total power draw of LEDs to 200mA at 5V
	fill_solid(leds, NUM_LEDS, COL_BLACK);
	leds[0] = CRGB::Red;
	FastLED.show();



 	#ifdef debugMSG
		Serial.begin(115200);				// Initialize serial communications
		while (!Serial);						// Wait for serial port to open
	#endif


	leds[1] = CRGB::Red;
	FastLED.show();



	SPI.begin();							// Init SPI bus
	mfrc522.PCD_Init();					// Init MFRC522 reader

	FillDataBuffer();


	Keyboard.begin();

	#ifdef debugMSG
		Serial.println(F("Writer ready"));
	#endif

	// Make a little noise
	digitalWrite(BUZZ_PIN, LOW);
	delay(100);
	digitalWrite(BUZZ_PIN, HIGH);

	fill_solid(leds, NUM_LEDS, CRGB::Green);
	FastLED.show();



}


void loop() 
{
	// GetData();
	// PrintInBuffer();


		// Handle NFC errors
	if (status != MFRC522::STATUS_OK) 
	{
		#ifdef debugMSG
			Serial.print(F("\nNFC Fault: "));
			Serial.println(mfrc522.GetStatusCodeName(status));
		#endif

		digitalWrite(BUZZ_PIN, LOW);
		delay(50);
		digitalWrite(BUZZ_PIN, HIGH);
		delay(50);

	}

	// Look for new cards
	if ( ! mfrc522.PICC_IsNewCardPresent())
		return;

	// Select one of the cards
	if ( ! mfrc522.PICC_ReadCardSerial())
		return;

	// if (readyToTag)
	// {
		// Print tag details to console
		mfrc522.PICC_DumpDetailsToSerial(&(mfrc522.uid));

		// Write data to tag
		WriteTagData();
		TestWrittenTag();

		TypeUID();

	// }
	// else
	// {
	// 	ReadTagData();
	// }
	
	#ifdef debugMSG
		DumpRawTagData();			// Reveals raw data stored in tag (edit MFRC522.cpp to show more than 12 pages)
	#endif

	CloseTagComms();
	// delay(3000);
}

///////////////////////////////////////////////////////////////////////////////

void ClearData()
{
	// // Clears data arrays - sets all cells to equal ZERO
	for (uint8_t i = 0; i < TAG_DATA_SIZE; ++i)
		DataBlock[i] = ZERO;

	return;
}


void FillDataBuffer()
{
	// Copies input data from console to DataBlock array (to later write to tag)
	// memcpy ( &DataBlock, &InBuffer, TAG_DATA_SIZE);

	// ACTUALLY NO - Only accept tag type letter from console and keep wrting that to the tags

	memcpy ( &DataBlock, &validateCode, sizeof(validateCode));

	DataBlock[sizeof(validateCode)] = tagType;
	DataBlock[sizeof(validateCode)+1] = teamNum;


	return;
}


void WriteTagData()
{
	//Writes DataBlock to TAG from page 4 (user r/w setion)
	#ifdef debugMSG
		Serial.println("------------------------------------------------ Writing Data");
	#endif

	for (uint8_t page = 0; page < DATAROWS; ++page)
		mfrc522.MIFARE_Ultralight_Write(page + 0x04, (DataBlock + (page * DATACOLS)), DATACOLS);
	
	return;
}

void TestWrittenTag()
{
	ReadTagData();

	// Compare data on tag with data just written to it
	// This test can fail if tag was removed during an operation

	for (uint8_t i = 0; i < TAG_DATA_SIZE; ++i)
	{
		if (InBuffer[i] != TagBufferTest[i])
		{
			#ifdef debugMSG
				Serial.print("------------------------------------------------ ERROR writing tag at index ");
				Serial.println(i, DEC);
				Serial.println("------------------------------------------------ Please re-tap card.");
			#endif
			return;
		}
	}

	#ifdef debugMSG
		Serial.println("------------------------------------------------ SUCCESS! Tag written");
	#endif
	readyToTag = false;

	return;
}




void ReadTagData()
{
	// Read buffer from tag
	const uint8_t readlen = 18;				// 16 bytes + 2x CRC bytes
	unsigned char readBuffer[readlen];
	unsigned char TagBuffer[TAG_DATA_SIZE];		// Buffer to write data into
	

	// Read data from user section of tags
	for (uint8_t ReadBlock = 0; ReadBlock < (DATAROWS/4); ++ReadBlock)
	{
		// Get data from tag, starting from page 4 (user read/write section begins here)
		status = mfrc522.MIFARE_Read(4 + (ReadBlock * 4), readBuffer, &readlen);

		// Check reading was successful
		if (status != MFRC522::STATUS_OK) 
		{
			#ifdef debugMSG
				Serial.println("");
				Serial.print(F("Reading failed: "));
				Serial.println(mfrc522.GetStatusCodeName(status));
			#endif

			return;
		}

		// Record data from tag to TagBuffer array 
		for (int i = 0; i < 16; ++i)
			TagBuffer[i + ReadBlock * 16] = readBuffer[i];
	}

	// Record data for checks later
	for (uint8_t i = 0; i < TAG_DATA_SIZE; ++i)
		TagBufferTest[i] = TagBuffer[i];	


/*	// Print read data to console - HEX
	Serial.print("Tag Data (HEX): ");
	for (uint8_t i = 0; i < DATA_SIZE; ++i)
	{
		Serial.print(TagBuffer[i], HEX);
		Serial.print(" ");
	}		
	Serial.println();


	// Print read data to console - CHAR
	Serial.print("Tag Data (CHAR): \"");
	for (uint8_t i = 0; i < DATA_SIZE; ++i)
		Serial.write(TagBuffer[i]);	
		
	Serial.println("\"");*/

	return;
}

#ifdef debugMSG
	void DumpRawTagData()
	{
		//This is a modifier dunp just change the for cicle to < 232 instead of < 16 in order to see all the pages on NTAG216
		mfrc522.PICC_DumpMifareUltralightToSerial();
		return;
	}
#endif

#ifdef debugMSG
	void GetData() 
	{
		static uint8_t ndx = 0;
		char rc;

		while (Serial.available() > 0 && newData == false) 
		{
			rc = Serial.read();					// Read single character

			if (rc != '\n')						// If not newline character
			{
				if (ndx < TAG_DATA_SIZE)
				{
					InBuffer[ndx] = rc;			// Save Char
					ndx++;							// Increment index
					if (ndx > TAG_DATA_SIZE) 		// Don't overflow buffer
						ndx = TAG_DATA_SIZE - 1;
				}
			}
			else 										// Once we get end character
			{
				for (uint8_t i = ndx; i < TAG_DATA_SIZE; i++)
					InBuffer[i] = ' ';			// Pad with spaces

				ndx = 0;								// Reset variables
				newData = true;
			}
		}

		return;
	}
#endif

#ifdef debugMSG
	void PrintInBuffer() 
	{
		if (newData == true) 
		{
			Serial.print("Data to write: \"");
			for (int i = 0; i < TAG_DATA_SIZE; ++i)
				Serial.write(InBuffer[i]);
			Serial.println("\"");
			
			FillDataBuffer();
			newData = false;
			readyToTag = true;
		}
		return;
	}
#endif



void CloseTagComms()
{
	// Puts tags into halt state

	// Halt PICC - call once finished operations on current tag
	mfrc522.PICC_HaltA();				// Instructs a PICC in state ACTIVE(*) to go to state HALT

	// Stop encryption
	if (mfrc522.PICC_GetType(mfrc522.uid.sak) == mfrc522.PICC_TYPE_MIFARE_1K)
		mfrc522.PCD_StopCrypto1();		// Call after communicating with the authenticated PICC

	// if (status == MFRC522::STATUS_OK) 
	// 	NFC_State = READY_FOR_TAG;

	return;
}

void TypeUID()
{

	// Serial.print(F("UID = "));

	for (uint8_t i = 0; i < mfrc522.uid.size; ++i)
	{
		// Add leading '0' for Hex values less than 0x10
		if (mfrc522.uid.uidByte[i]<0x10)
			Keyboard.print("0");

		// Print hex value
		Keyboard.print(mfrc522.uid.uidByte[i], HEX);
	}

	Keyboard.write(0x0A);

	// Make a little noise
	digitalWrite(BUZZ_PIN, LOW);
	delay(100);
	digitalWrite(BUZZ_PIN, HIGH);

	delay(500);

	// Serial.println();
}