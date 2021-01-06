//This example show how you can get Authenticated by the NTAG213,215,216 by default the tags are unprotected in order to protect them we need to write 4 different values:
// Using mfrc522.MIFARE_Ultralight_Write(PageNum, Data, #Datauint8_ts))
//1.- we need to write the 32bit passWord to page 0xE5 !for ntag 213 and 215 page is different refer to nxp documentation!
//2.- Now Write the 16 bits pACK to the page 0xE6 use the 2 high uint8_ts like this: pACKH + pACKL + 00 + 00 after an authentication the tag will return this secret uint8_ts
//3.- Now we need to write the first page we want to protect this is a 1 uint8_t data in page 0xE3 we need to write 00 + 00 + 00 + firstPage  all pages after this one are write protected
// Now WRITE protection is ACTIVATED so we need to get authenticated in order to write the last data
//4.- Finally we need to write an access record in order to READ protect the card this step is optional only if you want to read protect also write 80 + 00 + 00 + 00 to 0xE4
//After completeing all these steps you will nee to authentiate first in order to read or write ant page after the first page you selected to protect
//To disengage proection just write the page (0xE3) to 00 + 00 + 00 + FF that going to remove all protection
//Made by GARGANTUA from RoboCreators.com & paradoxalabs.com


// Adapted by S.EAMES for a game using unprotected NTAG213 NFC Stickers
// WRITER COMPONENT 
/*

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
#define DATAROWS	16
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

// // Arrays copied onto NFC Tags

uint8_t DataBlock[DATA_SIZE];


uint8_t *Ptr_DataBlock;

const uint8_t InBufferLen = 32;		// Data read in from console is stored here
char InBuffer[InBufferLen]; 			// Array to store text data from console
char TagBufferTest[InBufferLen]; 	// Used to test tag data matches written data
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
	for (uint8_t i = 0; i < InBufferLen; ++i)
	{
		*(Ptr_DataBlock + i) = InBuffer[i];

		// if (i<4)
		// {
		// 	Serial.print("Inbuffer  = ");
		// 	Serial.print(InBuffer[i], HEX);
		// 	Serial.print(", \t DataBlock = ");
		// 	Serial.print(DataBlock[0][i], HEX);
		// 	Serial.println();
		// 	Serial.println();
		// }
	}

	// // Print input buffer
	// for (int i = 0; i < InBufferLen; ++i)
	// {
	// 	Serial.print("Inbuffer  = ");
	// 	Serial.println(*(Ptr_DataBlock + i), HEX);
	// }

	return;
}


void WriteTagData()
{
	//Writes DataBlock to TAG from page 4 (user r/w setion)

	Serial.println("------------------------------------------------ Writing Data");
	

	for (uint8_t page = 0; page < DATAROWS; ++page)
	{

		mfrc522.MIFARE_Ultralight_Write(page + 0x04, (Ptr_DataBlock + (page * DATACOLS)), DATACOLS);

		Serial.print("Page = ");
		Serial.print(page + 0x04, HEX);
		Serial.print(", \t DataWritten = ");
		Serial.print(*(Ptr_DataBlock + (page * DATACOLS)), HEX);
		Serial.print("\t");
		Serial.print(*(Ptr_DataBlock + (page * DATACOLS) + 1), HEX);
		Serial.print("\t");
		Serial.print(*(Ptr_DataBlock + (page * DATACOLS) + 2), HEX);
		Serial.print("\t");
		Serial.print(*(Ptr_DataBlock + (page * DATACOLS) + 3), HEX);
		Serial.println();
	}
	
	return;
}

void TestWrittenTag()
{
	ReadTagData();

	// Compare buffers

	for (uint8_t i = 0; i < InBufferLen; ++i)
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
	const uint8_t readlen = 18;			// 16 bytes + 2x CRC bytes
	unsigned char readBuffer[readlen];

	unsigned char TagBuffer[32];		// Buffer to write data into
	


	Serial.print("Tag Data: \"");

	// Read data from user section of tags
	for (uint8_t ReadBlock = 0; ReadBlock < 2; ++ReadBlock)
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


	// Print read data to console
	for (uint8_t i = 0; i < 32; ++i)
	{
		Serial.print(TagBuffer[i], HEX);
		Serial.print(" ");
		TagBufferTest[i] = TagBuffer[i];
	}		
		

	Serial.println("\"");

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
			if (ndx < InBufferLen)
			{
				InBuffer[ndx] = rc;			// Save Char
				ndx++;							// Increment index
				if (ndx > InBufferLen) 		// Don't overflow buffer
					ndx = InBufferLen - 1;
			}
		}
		else 										// Once we get end character
		{
			for (uint8_t i = ndx; i < InBufferLen; i++)
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
		for (int i = 0; i < InBufferLen; ++i)
			Serial.print(InBuffer[i]);
		Serial.println("\"");
		
		FillDataBuffer();
		newData = false;
		readyToTag = true;
	}
	return;
}
