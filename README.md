# RiskyLoRa

# TODO LIST


* Setup FastLEDs
	* Make some handy functions
* Setup card reader
	* Read tag (NTAG)
	* Process tag
		* Ignore tags not involved in game
* LoRa
	* Send tag data
	* Send status message - once a min and on request
	* Set node state based on LoRa message
* Add sounds
* Add a screen??
	* Show status (battery %, LoRa signal strength, state)
* Add card reader
	* Read NFC UID data (assuming it won't fit on internal memory)
	* Write logs??
* Add battery voltage monitor
* NFC - enable special cards (milfaire)




* MASTER STATION
	* dump received messages
		* Append signal strength to received status messages
	* Transmit commands to nodes
	* 