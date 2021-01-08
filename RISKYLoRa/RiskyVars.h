// Extra variables because apparently Arduino doesn't accept enum types in .ino files


// Game State
enum gameStates {ST_DISABLED, ST_UNLOCKED, ST_BATTLE, ST_OUTCOME, ST_LOCKDOWN, ACT_CARDTAP};
enum NFC_State {READY_FOR_TAG, TAG_READ, TAG_VALID, TAG_RETAPPED, TAG_NEW, WAIT_FOR_LORA, WRITE_TO_CARD};

// enum tribes {AETOS, ARKOUDA, ELAFI, FIDI, KEROS, LYKOS, TARI, TAVROS, NULLTRIBE};




// PIXEL LED COLOURS

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

// Tribes Dim
#define COL_D_YELLOW		0x1F0F00 	// Yellow
#define COL_D_ORANGE		0x2F0200 	// Orange
#define COL_D_PURPLE		0x0F001F 	// Purple
#define COL_D_CYAN		0x001A1A 	// Cyan
#define COL_D_GREEN		0x001F00 	// Green
#define COL_DBLUES		0x00001F 	// Blue
#define COL_DREDI			0x1F0000 	// Red
#define COL_DMAGENTAS	0x1F0002 	// Magenta
