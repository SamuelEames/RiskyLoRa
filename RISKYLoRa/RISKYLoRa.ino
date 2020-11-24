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
		

// Uncomment to enable
#define serialDubug
// #define stationID


// ARDUINO PIN SETUP
#define RFM95_CS 		18
#define RFM95_RST 	10
#define RFM95_INT 	0
 

// LORA SETUP
#define RF95_FREQ 915.0

RH_RF95 rf95(RFM95_CS, RFM95_INT);				// Instanciate a LoRa driver
Speck myCipher;										// Instanciate a Speck block ciphering
RHEncryptedDriver LoRa(rf95, myCipher);	// Instantiate the driver with those two

unsigned char encryptkey[16]={1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16}; // Encryption Key - keep secret ;)

char HWMessage[] = "Hello World ! I'm happy if you can read me";
uint8_t HWMessageLen;


void setup()
{
	HWMessageLen = strlen(HWMessage);

	// SERIAL SETUP
	Serial.begin(115200);
	while (!Serial) ; 							// Wait for serial port to be available

	Serial.println("LoRa Simple_Encrypted");

	// LORA SETUP
	if (!rf95.init())
		Serial.println("LoRa init failed");
	
	rf95.setTxPower(13); 							// Setup Power,dBm
	myCipher.setKey(encryptkey, sizeof(encryptkey));

	Serial.println("Waiting for radio to setup");
	delay(4000); 	// TODO - Why this delay?

	Serial.println("Setup completed");

	Serial.print("Max message length = ");
	Serial.print(LoRa.maxMessageLength());

	// LoRa_TX();

	// delay(5000);

	// LoRa_TX();
}

void loop()
{
	LoRa_RX();
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