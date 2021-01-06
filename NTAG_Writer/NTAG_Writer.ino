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

#include <SPI.h>
#include <MFRC522.h>


#define ZERO 0x00
#define DATAROWS	12
#define DATACOLS	4
const uint8_t DATA_SIZE = DATAROWS * DATACOLS;

//////////////////////////////////
//////// TAG READER SETUP ////////
//////////////////////////////////
#define RST_PIN   20 					// Reset Pin
#define SS_PIN    19 					// Pin labelled "SDA"

MFRC522 mfrc522(SS_PIN, RST_PIN);	// Create MFRC522 instance
MFRC522::StatusCode status;

//////////////////////////////////
////////// VARIABLES /////////////
//////////////////////////////////

// // Array copied onto NFC Tags

uint8_t DataBlock[DATA_SIZE];
uint8_t *Ptr_DataBlock;

unsigned char InBuffer[DATA_SIZE]; 			// Array to store text data from console
unsigned char TagBufferTest[DATA_SIZE]; 	// Used to test tag data matches written data
boolean newData = false;				// False until newline chatacter is received fron console. Cleared once printed.
boolean readyToTag = false;			// True once newData is received. False after data is written to tag


//////////////////////////////////



void setup() 
{

 	Ptr_DataBlock = &DataBlock[0];		// Initialise pointer to first elelment of DataBlock

	Serial.begin(115200);				// Initialize serial communications
	while (!Serial);						// Wait for serial port to open (needed for ATMEGA32U4)
	SPI.begin();							// Init SPI bus
	mfrc522.PCD_Init();					// Init MFRC522 reader
	Serial.println(F("Writer ready"));
}


void loop() 
{
	GetData();
	PrintInBuffer();

	// Look for new cards
	if ( ! mfrc522.PICC_IsNewCardPresent())
		return;

	// Select one of the cards
	if ( ! mfrc522.PICC_ReadCardSerial())
		return;

	if (readyToTag)
	{
		// Print tag details to console
		mfrc522.PICC_DumpDetailsToSerial(&(mfrc522.uid));

		// Write data to tag
		WriteTagData();
		TestWrittenTag();

	}
	else
	{
		ReadTagData();
	}
	
	
	DumpRawTagData();			// Reveals raw data stored in tag (edit MFRC522.cpp to show more than 12 pages)
	delay(3000);
}

///////////////////////////////////////////////////////////////////////////////

void ClearData()
{
	// // Clears data arrays - sets all cells to equal ZERO
	for (uint8_t i = 0; i < DATA_SIZE; ++i)
		DataBlock[i] = ZERO;

	return;
}


void FillDataBuffer()
{
	// Copies input data from console to DataBlock array (to later write to tag)
	for (uint8_t i = 0; i < DATA_SIZE; ++i)
		*(Ptr_DataBlock + i) = InBuffer[i];

	return;
}


void WriteTagData()
{
	//Writes DataBlock to TAG from page 4 (user r/w setion)
	Serial.println("------------------------------------------------ Writing Data");
	
	for (uint8_t page = 0; page < DATAROWS; ++page)
		mfrc522.MIFARE_Ultralight_Write(page + 0x04, (Ptr_DataBlock + (page * DATACOLS)), DATACOLS);
	
	return;
}

void TestWrittenTag()
{
	ReadTagData();

	// Compare data on tag with data just written to it
	// This test can fail if tag was removed during an operation

	for (uint8_t i = 0; i < DATA_SIZE; ++i)
	{
		if (InBuffer[i] != TagBufferTest[i])
		{
			Serial.print("------------------------------------------------ ERROR writing tag at index ");
			Serial.println(i, DEC);
			Serial.println("------------------------------------------------ Please re-tap card.");
			return;
		}
	}

	Serial.println("------------------------------------------------ SUCCESS! Tag written");
	readyToTag = false;

	return;
}




void ReadTagData()
{
	// Read buffer from tag
	const uint8_t readlen = 18;				// 16 bytes + 2x CRC bytes
	unsigned char readBuffer[readlen];
	unsigned char TagBuffer[DATA_SIZE];		// Buffer to write data into
	

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
			TagBuffer[i + ReadBlock * 16] = readBuffer[i];
	}


/*	// Print read data to console - HEX
	Serial.print("Tag Data (HEX): ");
	for (uint8_t i = 0; i < DATA_SIZE; ++i)
	{
		Serial.print(TagBuffer[i], HEX);
		Serial.print(" ");
		TagBufferTest[i] = TagBuffer[i];
	}		
	Serial.println();


	// Print read data to console - CHAR
	Serial.print("Tag Data (CHAR): \"");
	for (uint8_t i = 0; i < DATA_SIZE; ++i)
	{
		Serial.write(TagBuffer[i]);
		TagBufferTest[i] = TagBuffer[i];
	}		
	Serial.println("\"");*/

	return;
}

void DumpRawTagData()
{
	//This is a modifier dunp just change the for cicle to < 232 instead of < 16 in order to see all the pages on NTAG216
	mfrc522.PICC_DumpMifareUltralightToSerial();
	return;
}


void GetData() 
{
	static uint8_t ndx = 0;
	char rc;

	while (Serial.available() > 0 && newData == false) 
	{
		rc = Serial.read();					// Read single character

		if (rc != '\n')						// If not newline character
		{
			if (ndx < DATA_SIZE)
			{
				InBuffer[ndx] = rc;			// Save Char
				ndx++;							// Increment index
				if (ndx > DATA_SIZE) 		// Don't overflow buffer
					ndx = DATA_SIZE - 1;
			}
		}
		else 										// Once we get end character
		{
			for (uint8_t i = ndx; i < DATA_SIZE; i++)
				InBuffer[i] = ' ';			// Pad with spaces

			ndx = 0;								// Reset variables
			newData = true;
		}
	}

	return;
}

void PrintInBuffer() 
{
	if (newData == true) 
	{
		Serial.print("Data to write: \"");
		for (int i = 0; i < DATA_SIZE; ++i)
			Serial.write(InBuffer[i]);
		Serial.println("\"");
		
		FillDataBuffer();
		newData = false;
		readyToTag = true;
	}
	return;
}