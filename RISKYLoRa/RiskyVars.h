// Extra variables because apparently Arduino doesn't accept enum types in .ino files


// Game State
enum gameStates {ST_DISABLED, ST_UNLOCKED, ST_BATTLE, ST_OUTCOME, ST_LOCKDOWN, ACT_CARDTAP};

enum tribes {AETOS, ARKOUDA, ELAFI, FIDI, KEROS, LYKOS, TARI, TAVROS, NULLTRIBE};




// PIXEL LED COLOURS

// General
#define COL_BLACK			0x000000		// Black
#define COL_WHITE			0x3F3F3F		// White
#define COL_D_WHITE		0x0F0F0F		// Dim White

// Tribes Bright
#define COL_AETOS			0xFF6F00 	// Yellow
#define COL_ARKOUDA		0xFF1C00 	// Orange
#define COL_ELAFI			0x8F00FF 	// Purple
#define COL_FIDI			0x00AAAA 	// Cyan
#define COL_KEROS			0x00FF0A 	// Green
#define COL_LYKOS			0x0000FF 	// Blue
#define COL_TARI			0xFF0000 	// Red
#define COL_TAVROS		0xFF002F 	// Magenta

// Tribes Dim
#define COL_D_AETOS		0x1F0F00 	// Yellow
#define COL_D_ARKOUDA	0x2F0200 	// Orange
#define COL_D_ELAFI		0x0F001F 	// Purple
#define COL_D_FIDI		0x001A1A 	// Cyan
#define COL_D_KEROS		0x001F00 	// Green
#define COL_D_LYKOS		0x00001F 	// Blue
#define COL_D_TARI		0x1F0000 	// Red
#define COL_D_TAVROS		0x1F0002 	// Magenta
