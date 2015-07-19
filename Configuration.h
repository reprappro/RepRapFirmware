/****************************************************************************************************

RepRapFirmware - Configuration

This is where all machine-independent configuration and other definitions are set up.  Nothing that
depends on any particular RepRap, RepRap component, or RepRap controller  should go in here. Define
machine-dependent things in Platform.h

-----------------------------------------------------------------------------------------------------

Version 0.1

18 November 2012

Adrian Bowyer
RepRap Professional Ltd
http://reprappro.com

Licence: GPL

****************************************************************************************************/

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#define NAME "RepRapFirmware"
#define VERSION "1.09e-zpl"
#define DATE "2015-07-19"
#define AUTHORS "reprappro, dc42, zpl"

// Comment out the following line if you don't want to build the firmware with Flash save support
#define FLASH_SAVE_ENABLED

// Other firmware that we might switch to be compatible with.

enum Compatibility
{
	me = 0,
	reprapFirmware = 1,
	marlin = 2,
	teacup = 3,
	sprinter = 4,
	repetier = 5
};

// Generic constants

static const float ABS_ZERO = -273.15;						// Celsius
static const float NEARLY_ABS_ZERO = -273.0;				// Celsius
static const float ROOM_TEMPERATURE = 21.0;					// Celsius

static const float INCH_TO_MM = 25.4;
static const float MINUTES_TO_SECONDS = 60.0;
static const float SECONDS_TO_MINUTES = 1.0 / MINUTES_TO_SECONDS;

static const float LONG_TIME = 300.0;						// Seconds

// Heater values

static const float HEAT_SAMPLE_TIME = 0.5;					// Seconds
static const float HEAT_PWM_AVERAGE_TIME = 5.0;				// Seconds

static const float TEMPERATURE_CLOSE_ENOUGH = 2.5;			// Celsius
static const float TEMPERATURE_LOW_SO_DONT_CARE = 40.0;		// Celsius
static const float HOT_ENOUGH_TO_EXTRUDE = 160.0;			// Celsius
static const float HOT_ENOUGH_TO_RETRACT = 90.0;			// Celsius
static const float TIME_TO_HOT = 150.0;						// Seconds

static const size_t MAX_BAD_TEMPERATURE_COUNT = 6;			// Number of bad temperature samples before a heater fault is reported
static const float BAD_LOW_TEMPERATURE = -10.0;				// Celsius
static const float BAD_HIGH_TEMPERATURE = 300.0;			// Celsius

// Default Z probe values

static const size_t MAX_PROBE_POINTS = 16;					// Maximum number of probe points
static const size_t MAX_DELTA_PROBE_POINTS = 8;				// Must be <= MaxProbePoints, may be smaller to reduce matrix storage requirements. Preferably a power of 2.

static const float DEFAULT_Z_DIVE = 5.0;					// Millimetres
static const float TRIANGLE_ZERO = -0.001;					// Millimetres
static const float SILLY_Z_VALUE = -9999.0;					// Millimetres

// String lengths

static const size_t LONG_STRING_LENGTH = 1024;
static const size_t FORMAT_STRING_LENGTH = 256;
static const size_t SHORT_STRING_LENGTH = 40;

static const size_t GCODE_LENGTH = 100;
static const size_t GCODE_REPLY_LENGTH = 2048;
static const size_t MESSAGE_LENGTH = 256;

// Output buffer lengths

static const uint16_t OUTPUT_BUFFER_SIZE = 256;				// How many bytes does each OutputBuffer hold?
static const size_t OUTPUT_BUFFER_COUNT = 32;				// How many OutputBuffer instances do we have?

// Move system

static const float DEFAULT_FEEDRATE = 3000.0;				// The initial requested feed rate after resetting the printer
static const float DEFAULT_IDLE_TIMEOUT = 30.0;				// Seconds

// Print estimation defaults

static const float NOZZLE_DIAMETER = 0.5;					// Millimetres
static const float FILAMENT_WIDTH = 1.75;					// Millimetres
static const size_t MAX_LAYER_SAMPLES = 5;					// Number of layer samples for end-time estimation (except for first layer)
static const float ESTIMATION_MIN_FILAMENT_USAGE = 0.025;	// Minimum per cent after which the first layer height is determined
static const float FIRST_LAYER_SPEED_FACTOR = 0.25;			// First layer speed factor compared to other layers (only for layer-based estimation)

// Webserver defaults

static const char *DEFAULT_PASSWORD = "reprap";				// Default machine password
static const char *DEFAULT_NAME = "My Duet";				// Default machine name

static const char *INDEX_PAGE_FILE = "reprap.htm";
static const char *FOUR04_PAGE_FILE = "html404.htm";

// Filesystem and upload defaults

static const char *CONFIG_FILE = "config.g";
static const char *DEFAULT_FILE = "default.g";
static const char *HOME_X_G = "homex.g";
static const char *HOME_Y_G = "homey.g";
static const char *HOME_Z_G = "homez.g";
static const char *HOME_ALL_G = "homeall.g";
static const char *HOME_DELTA_G = "homedelta.g";
static const char *BED_EQUATION_G = "bed.g";
static const char *PAUSE_G = "pause.g";
static const char *RESUME_G = "resume.g";
static const char *STOP_G = "stop.g";
static const char *SLEEP_G = "sleep.g";

static const char *EOF_STRING = "<!-- **EoF** -->";

// List defaults

static const char LIST_SEPARATOR = ':';
static const char FILE_LIST_SEPARATOR = ',';
static const char FILE_LIST_BRACKET = '"';

#endif

// vim: ts=4:sw=4
