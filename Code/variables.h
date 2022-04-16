#include <PxMatrix.h>
#include <Preferences.h>

// This defines the 'on' time of the display is us. The larger this number,
// the brighter the display. If too large the ESP will crash
uint8_t display_draw_time = 25; //10-50 is usually fine

#define P_LAT 22
#define P_A 19
#define P_B 23
#define P_C 18
#define P_D 5
#define P_E 15
#define P_OE 16
hw_timer_t* timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

#define O						-1	//Nothing
#define M_WIDTH					64	//Matrix width
#define M_HEIGHT				64	//Matrix height
#define PATTERN_NR				4   //Number of patterns. If you will add some additional patterns you have to increase this number
#define ESP_PREFS				"esp_prefs" //Namespace name is limited to 15 characters
#define PREFS_BRIGHTNESS		"0" //key
#define PREFS_GAIN				"1" //key
#define PREFS_SQUELCH			"2" //key
#define PREFS_PATTERN			"3" //key
#define SMOOTH_SPECTRUM			"4" //key
#define SPACE_GAME_HIGH_SCORE	"5" //key

// Pins for LED MATRIX
//PxMATRIX display(M_WIDTH, M_HEIGHT, P_LAT, P_OE, P_A, P_B, P_C);
//PxMATRIX display(M_WIDTH, M_HEIGHT, P_LAT, P_OE, P_A, P_B, P_C, P_D);
PxMATRIX display(M_WIDTH, M_HEIGHT, P_LAT, P_OE, P_A, P_B, P_C, P_D, P_E);

uint8_t peak[M_WIDTH];
uint8_t prevFFTValue[M_WIDTH];
uint8_t barHeights[M_WIDTH];
uint8_t pattern;
uint8_t brightness;
bool isDeepSlepp = false, isGame = false, isSpectrum = true;

uint16_t white = display.color565(255, 255, 255);
uint16_t ltGray = display.color565(100, 100, 100);
uint16_t black = display.color565(0, 0, 0);
uint16_t stars = display.color565(25, 25, 25);
uint16_t red = display.color565(255, 0, 0);
uint16_t blue = display.color565(0, 0, 254);
uint16_t yellow = display.color565(254, 204, 0);
uint16_t cyan = display.color565(0, 255, 255);
uint16_t darkGreen = display.color565(0, 100, 0);

//EEPROM library is deprecated in favor of the Preferences library, so we will use the Preferences library
Preferences preferences;