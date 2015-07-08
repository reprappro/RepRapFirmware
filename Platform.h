/****************************************************************************************************

RepRapFirmware - Platform: RepRapPro Ormerod with Duet controller

Platform contains all the code and definitions to deal with machine-dependent things such as control
pins, bed area, number of extruders, tolerable accelerations and speeds and so on.

No definitions that are system-independent should go in here.  Put them in Configuration.h.  Note that
the lengths of arrays such as DRIVES (see below) are defined here, so any array initialiser that depends on those
lengths, for example:

#define DRIVES 4
.
.
.
#define DRIVE_RELATIVE_MODES {false, false, false, true}

also needs to go here.

-----------------------------------------------------------------------------------------------------

Version 0.3

28 August 2013

Adrian Bowyer
RepRap Professional Ltd
http://reprappro.com

Licence: GPL

****************************************************************************************************/

#ifndef PLATFORM_H
#define PLATFORM_H

// What are we supposed to be running on

#define ELECTRONICS "Duet (+ Extension)"

// Language-specific includes

#include <cctype>
#include <cstring>
#include <malloc.h>
#include <cstdlib>
#include <climits>

// Platform-specific includes

#include "Arduino.h"
#include "SD_HSMCI.h"
#include "MCP4461.h"

// Firmware-specific includes

#include "Configuration.h"
#include "RepRapFirmware.h"

/**************************************************************************************************/

// Hardware-dependent timing

static const float TIME_TO_REPRAP = 1.0e6;				// Convert seconds to the units used by the machine (usually microseconds)
static const float TIME_FROM_REPRAP = 1.0e-6;			// Convert the units used by the machine (usually microseconds) to seconds

// The physical capabilities of the machine

static const size_t DRIVES = 8;							// The number of drives in the machine, including X, Y, and Z plus extruder drives
static const size_t AXES = 3;							// The number of movement axes in the machine, usually just X, Y and Z. <= DRIVES
static const size_t HEATERS = 6;						// The number of heaters in the machine; 0 is the heated bed even if there isn't one
static const size_t NUM_SERIAL_CHANNELS = 2;			// The number of serial IO channels (usually USB and auxiliary UART)

// The numbers of entries in each array must correspond with the values of DRIVES,
// AXES, or HEATERS. Set values to -1 to flag unavailability.


// DRIVES

static const bool ENABLE_DRIVE = false;					// What to send to enable...
static const bool DISABLE_DRIVE = !ENABLE_DRIVE;		// ... and disable a drive

static const int8_t ENABLE_PINS[DRIVES] = { 29, 27, X1, X0, 37, X8, 50, 47 };
static const int8_t STEP_PINS[DRIVES] = { 14, 25, 5, X2, 41, 39, X4, 49 };
static const int8_t DIRECTION_PINS[DRIVES] = { 15, 26, 4, X3, 35, 53, 51, 48 };

static const float DEFAULT_IDLE_CURRENT_FACTOR = 0.3;	// Proportion of normal motor current that we use for idle hold
static const bool DISABLE_DRIVES[DRIVES] = { false, false, false, true, true, true, true, true };								// Set true to disable a drive when it becomes idle

static const int8_t END_STOP_PINS[DRIVES] = { 11, 28, 60, 31, 24, 46, 45, 44 };
static const int ENDSTOP_HIT = HIGH;

static const bool FORWARDS = true;
static const bool BACKWARDS = !FORWARDS;
static const bool DIRECTIONS[DRIVES] = { BACKWARDS, FORWARDS, FORWARDS, FORWARDS, FORWARDS, FORWARDS, FORWARDS, FORWARDS };		// What each axis needs to make it go forwards - defaults

// Indices for motor current digipots (if any): First 4 are for digipot 1 (on duet), second 4 for digipot 2 (on expansion board)
static const uint8_t POT_WIPES[DRIVES] = { 1, 3, 2, 0, 1, 3, 2, 0 };			// Only define as many entries as DRIVES are defined
static const float SENSE_RESISTOR = 0.1;										// Stepper motor current sense resistor
static const float MAX_STEPPER_DIGIPOT_VOLTAGE = (3.3 * 2.5 / (2.7 + 2.5));		// Stepper motor current reference voltage

static const float MAX_FEEDRATES[DRIVES] = { 100.0, 100.0, 3.0, 20.0, 20.0, 20.0, 20.0, 20.0 };						// mm/sec
static const float ACCELERATIONS[DRIVES] = { 500.0, 500.0, 20.0, 250.0, 250.0, 250.0, 250.0, 250.0 };				// mm/sec^2
static const float DRIVE_STEPS_PER_UNIT[DRIVES] = { 87.4890, 87.4890, 4000.0, 420.0, 420.0, 420.0, 420.0, 420.0 };	// steps/mm
static const float INSTANT_DVS[DRIVES] = { 15.0, 15.0, 0.2, 2.0, 2.0, 2.0, 2.0, 2.0 };								// mm/sec

static const size_t E0_DRIVE = 3;
static const size_t E1_DRIVE = 4;
static const size_t E2_DRIVE = 5;
static const size_t E3_DRIVE = 6;
static const size_t E4_DRIVE = 7;

// AXES

static const size_t X_AXIS = 0, Y_AXIS = 1, Z_AXIS = 2, E0_AXIS = 3;		// The indices of the Cartesian axes in drive arrays
static const size_t A_AXIS = 0, B_AXIS = 1, C_AXIS = 2;						// The indices of the 3 tower motors of a delta printer in drive arrays

static const float AXIS_MINIMA[AXES] = { 0.0, 0.0, 0.0 };					// mm
static const float AXIS_MAXIMA[AXES] = { 230.0, 210.0, 200.0 };				// mm
static const float HOME_FEEDRATES[AXES] = { 50.0, 50.0, 100.0/60.0 };		// mm/sec (dc42 increased Z because we slow down z-homing when approaching the target height)

static const float DEFAULT_PRINT_RADIUS = 50.0;								// mm
static const float DEFAULT_DELTA_HOMED_HEIGHT = 200.0;						// mm

// HEATERS

static const bool HEAT_ON = false;											// false for inverted heater (eg Duet v0.6), true for not (e.g. Duet v0.4)

static const int8_t TEMP_SENSE_PINS[HEATERS] = { 5, 4, 0, 7, 8, 9 };		// Analogue pin numbers
static const int8_t HEAT_ON_PINS[HEATERS] = { 6, X5, X7, 7, 8, 9 };			// Pin D38 is PWM capable but not an Arduino PWM pin

// Bed thermistor: http://uk.farnell.com/epcos/b57863s103f040/sensor-miniature-ntc-10k/dp/1299930?Ntt=129-9930
// Hot end thermistor: http://www.digikey.co.uk/product-search/en?x=20&y=11&KeyWords=480-3137-ND
static const float DEFAULT_THERMISTOR_BETAS[HEATERS] = { 3988.0, 4138.0, 4138.0, 4138.0, 4138.0, 4138.0 };				// Bed thermistor: B57861S104F40; Extruder thermistor: RS 198-961
static const float DEFAULT_THERMISTOR_SERIES_RS[HEATERS] = { 1000.0, 1000.0, 1000.0, 1000.0, 1000.0, 1000.0 };			// Ohms in series with the thermistors
static const float DEFAULT_THERMISTOR_25_RS[HEATERS] = { 10000.0, 100000.0, 100000.0, 100000.0, 100000.0, 100000.0 };	// Thermistor ohms at 25 C = 298.15 K

// Note on hot end PID parameters:
// The system is highly nonlinear because the heater power is limited to a maximum value and cannot go negative.
// If we try to run a traditional PID when there are large temperature errors, this causes the I-accumulator to go out of control,
// which causes a large amount of overshoot at lower temperatures. There are at least two ways of avoiding this:
// 1. Allow the PID to operate even with very large errors, but choose a very small I-term, just the right amount so that when heating up
//    from cold, the I-accumulator is approximately the value needed to maintain the correct power when the target temperature is reached.
//    This works well most of the time. However if the Duet board is reset when the extruder is hot and is then
//    commanded to heat up again before the extruder has cooled, the I-accumulator doesn't grow large enough, so the
//    temperature undershoots. The small value of the I-term then causes it to take a long time to reach the correct temperature.
// 2. Only allow the PID to operate when the temperature error is small enough for the PID to operate in the linear region.
//    So we set FULL_PID_BAND to a small value. It needs to be at least 15C because that is how much the temperature overshoots by
//    when we turn the heater off from full power at about 180C. We set the I-accumulator to zero when the PID is off, and use a
//    much larger I-term. So the I-accumulator grows from zero to the value needed to maintain the required temperature
//    much faster, but not so fast as to cause too much overshoot. This works well most of the time, except when we reduce
//    the temperature by more than FULL_PID_BAND. In this case we turn off the PID and the heater, clear the
//    I-accumulator, and wait for the temperature to drop before we turn the PID on again. The temperature has to undershoot by 10C
//    or more in order for the I-accumulator to build up again. However, dropping the temperature by more then 20C is not a normal
//    operation for a 3D printer, so we don't worry about this case.
// An improvement on method (2) would be to preset the I-accumulator to an estimate of the value needed to maintain the
// target temperature when we start using the PID (instead of clearing it to zero), and then reduce the I-term a little.
//
// Note: a negative P, I or D value means do not use PID for this heater, use bang-bang control instead.
// This allows us to switch between PID and bang-bang using the M301 and M304 commands.

// We use method 2 (see above)
static const float DEFAULT_PID_KIS[HEATERS] = { 5.0, 0.1, 0.1, 0.1, 0.1, 0.1 };				// Integral PID constants
static const float DEFAULT_PID_KDS[HEATERS] = { 500.0, 100.0, 100.0, 100.0, 100.0, 100.0 };	// Derivative PID constants
const float DEFAULT_PID_KPS[HEATERS] = { -1.0, 10.0, 10.0, 10.0, 10.0, 10.0 };				// Proportional PID constants, negative values indicate use bang-bang instead of PID
static const float DEFAULT_PID_KTS[HEATERS] = { 2.7, 0.4, 0.4, 0.4, 0.4, 0.4 };				// Approximate PWM value needed to maintain temperature, per degC above room temperature
static const float DEFAULT_PID_KSS[HEATERS] = { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 };				// PWM scaling factor, to allow for variation in heater power and supply voltage
static const float DEFAULT_PID_FULLBANDS[HEATERS] = { 5.0, 30.0, 30.0, 30.0, 30.0, 30.0 };	// Errors larger than this cause heater to be on or off
static const float DEFAULT_PID_MINS[HEATERS] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };			// Minimum value of I-term
const float DEFAULT_PID_MAXES[HEATERS] = { 255.0, 180.0, 180.0, 180.0, 180.0, 180.0 };		// Maximum value of I-term, must be high enough to reach 245C for ABS printing

static const float STANDBY_TEMPERATURES[HEATERS] = { ABS_ZERO, ABS_ZERO, ABS_ZERO, ABS_ZERO, ABS_ZERO, ABS_ZERO }; // We specify one for the bed, though it's not needed
static const float ACTIVE_TEMPERATURES[HEATERS] = { ABS_ZERO, ABS_ZERO, ABS_ZERO, ABS_ZERO, ABS_ZERO, ABS_ZERO };

static const int8_t HOT_BED = 0;						// Index of the heated bed; set to -1 if there is no heated bed
static const int8_t E0_HEATER = 1;						// Index of the first extruder heater
static const int8_t E1_HEATER = 2;						// Index of the second extruder heater
static const int8_t E2_HEATER = 3;						// Index of the third extruder heater
static const int8_t E3_HEATER = 4;						// Index of the fourth extruder heater
static const int8_t E4_HEATER = 5;						// Index of the fifth extruder heater


// COOLING FAN

static const uint8_t COOLING_FAN_PIN = X6;				// Pin D34 is PWM capable but not an Arduino PWM pin - use X6 instead
static const uint8_t COOLING_FAN_RPM_PIN = 36;			// Pin PC4
static const float COOLING_FAN_RPM_SAMPLE_TIME = 2.0;	// Seconds


// AD CONVERSION

// For the theory behind ADC oversampling, see http://www.atmel.com/Images/doc8003.pdf
static const unsigned int AD_OVERSAMPLE_BITS = 1;		// Number of bits we oversample when reading temperatures

// Define the number of temperature readings we average for each thermistor. This should be a power of 2 and at least 4 ** AD_OVERSAMPLE_BITS.
// Keep THERMISTOR_AVERAGE_READINGS * NUM_HEATERS * 2ms no greater than HEAT_SAMPLE_TIME or the PIDs won't work well.
static const unsigned int THERMISTOR_AVERAGE_READINGS = (HEATERS > 3) ? 32 : 64;
static const unsigned int AD_RANGE_REAL = 4095;													// The ADC that measures temperatures gives an int this big as its max value
static const unsigned int AD_RANGE_VIRTUAL = ((AD_RANGE_REAL + 1) << AD_OVERSAMPLE_BITS) - 1;	// The max value we can get using oversampling
static const unsigned int AD_DISCONNECTED_REAL = AD_RANGE_REAL - 3;								// We consider an ADC reading at/above this value to indicate that the thermistor is disconnected
static const unsigned int AD_DISCONNECTED_VIRTUAL = AD_DISCONNECTED_REAL << AD_OVERSAMPLE_BITS;


// Z PROBE

static const int Z_PROBE_AD_VALUE = 400;						// Default for the Z proboe - should be overwritten by experiment
static const float Z_PROBE_STOP_HEIGHT = 0.7;					// Millimetres
static const int8_t Z_PROBE_PIN = 10;							// Analogue pin number
static const int8_t Z_PROBE_MOD_PIN = 52;						// Digital pin number to turn the IR LED on (high) or off (low)
static const int8_t Z_PROBE_MOD_PIN07 = X25;					// Digital pin number to turn the IR LED on (high) or off (low) on Duet v0.7 onwards
static const bool Z_PROBE_AXES[AXES] = { true, false, true };	// Axes for which the Z-probe is normally used
static const unsigned int Z_PROBE_AVERAGE_READINGS = 8;			// We average this number of readings with IR on, and the same number with IR off

// Inkjet (if any - no inkjet is flagged by INKJET_BITS negative)

static const int8_t INKJET_BITS = -1;							// How many nozzles? (stock Ormerods don't have any)
static const int INKJET_FIRE_MICROSECONDS = 5;					// How long to fire a nozzle
static const int INKJET_DELAY_MICROSECONDS = 800;				// How long to wait before the next bit

// Inkjet control pins

static const int8_t INKJET_SERIAL_OUT = 65;						// Serial bitpattern into the shift register
static const int8_t INKJET_SHIFT_CLOCK = 20;					// Shift the register
static const int8_t INKJET_STORAGE_CLOCK = 67;					// Put the pattern in the output register
static const int8_t INKJET_OUTPUT_ENABLE = 66;					// Make the output visible
static const int8_t INKJET_CLEAR = 36;							// Clear the register to 0

/****************************************************************************************************/

// File handling

static const size_t MAX_FILES = 10;				// Must be large enough to handle the max number of simultaneous web requests + files being printed

static const size_t FILE_BUFFER_LENGTH = 256;
static const size_t FILENAME_LENGTH = 100;

static const char *FS_PREFIX = "0:";
static const char *WEB_DIR = "0:/www/";			// Place to find web files on the SD card
static const char *GCODE_DIR = "0:/gcodes/";	// Ditto - G-Codes
static const char *SYS_DIR = "0:/sys/";			// Ditto - System files
static const char *MACRO_DIR = "0:/macros/";	// Ditto - Macro files

static const uint8_t MAC_ADDRESS[6] = { 0xBE, 0xEF, 0xDE, 0xAD, 0xFE, 0xED };

/****************************************************************************************************/

// Miscellaneous...

const unsigned int USB_BAUD_RATE = 115200;		// Default communication speed of the USB if needed
const unsigned int AUX_BAUD_RATE = 57600;		// Ditto - for auxiliary UART devices

const int8_t atxPowerPin = 12;					// Arduino Due pin number that controls the ATX power on/off

/***************************************************************************************************/

// Enumeration describing the reasons for a software reset.
// The spin state gets or'ed into this, so keep the lower ~4 bits unused.
namespace SoftwareResetReason
{
	enum
	{
		user = 0,								// M999 command
		inAuxOutput = 0x0800,					// This bit is or'ed in if we were in aux output at the time
		stuckInSpin = 0x1000,					// We got stuck in a Spin() function for too long
		inLwipSpin = 0x2000,					// We got stuck in a call to LWIP for too long
		inUsbOutput = 0x4000					// This bit is or'ed in if we were in USB output at the time
	};
}

// Enumeration to describe various tests we do in response to the M111 command
namespace DiagnosticTest
{
	enum
	{
		TestWatchdog = 1001,					// Test that we get a watchdog reset if the tick interrupt stops
		TestSpinLockup = 1002,					// Test that we get a software reset if a Spin() function takes too long
		TestSerialBlock = 1003					// Test what happens when we write a blocking message via debugPrintf()
	};
}

/***************************************************************************************************/

struct FileInfo
{
	bool isDirectory;
	unsigned long size;
	uint8_t day;
	uint8_t month;
	uint16_t year;
	char fileName[FILENAME_LENGTH];
};

class Platform;

class MassStorage
{
	public:
		bool FindFirst(const char *directory, FileInfo &file_info);
		bool FindNext(FileInfo &file_info);
		const char* GetMonthName(const uint8_t month);
		const char* CombineName(const char* directory, const char* fileName);
		bool Delete(const char* directory, const char* fileName);
		bool MakeDirectory(const char *parentDir, const char *dirName);
		bool MakeDirectory(const char *directory);
		bool Rename(const char *oldFilename, const char *newFilename);
		bool FileExists(const char *file) const;
		bool DirectoryExists(const char *path) const;
		bool DirectoryExists(const char* directory, const char* subDirectory);

		friend class Platform;

	protected:

		MassStorage(Platform* p);
		void Init();

	private:

		Platform* platform;
		FATFS fileSystem;

		DIR *findDir;

		char combinedNameBuffer[FILENAME_LENGTH];
		StringRef combinedName;
};

// This class handles input from, and output to, files.

typedef uint32_t FilePosition;
static const FilePosition NO_FILE_POSITION = 0xFFFFFFFF;

enum class IOStatus : uint8_t
{
	nothing = 0,
	byteAvailable = 1,
	atEoF = 2,
	clientLive = 4,
	clientConnected = 8
};

class FileStore
{
	public:
		IOStatus Status() const;						// Returns status of file instance

		bool Read(char& b);								// Read 1 byte
		int Read(char* buf, unsigned int nBytes);		// Read a block of nBytes length
		bool Write(char b);								// Write 1 byte
		bool Write(const char *s, unsigned int len);	// Write a block of len bytes
		bool Write(const char* s);						// Write a string
		bool Close();									// Shut the file and tidy up
		FilePosition Position() const;					// Get the current file position
		bool Seek(FilePosition pos);					// Jump to pos in the file
		bool GoToEnd();									// Position the file at the end (so you can write on the end).
		FilePosition Length() const;					// File size in bytes
		float FractionRead() const;						// How far in we are (in per cent)
		void Duplicate();								// Create a second reference to this file
		bool Flush();									// Write remaining buffer data

		static float GetAndClearLongestWriteTime();		// Return the longest time it took to write a block to a file, in milliseconds

		friend class Platform;

	protected:
		FileStore(Platform* p);
		void Init();
		bool Open(const char* directory, const char* fileName, bool write);
		bool Open(const char* filePath, bool write);

	private:
		bool inUse;
		byte buf[FILE_BUFFER_LENGTH];
		int bufferPointer;
		FilePosition bytesRead;

		bool ReadBuffer();
		bool WriteBuffer();
		bool InternalWriteBlock(const char *s, unsigned int len);

		FIL file;
		Platform* platform;
		bool writing;
		unsigned int lastBufferEntry;
		unsigned int openCount;

		static uint32_t longestWriteTime;
};


/***************************************************************************************************************/

// Struct for holding Z probe parameters

struct ZProbeParameters
{
	int adcValue;							// Target ADC value
	float xOffset, yOffset;					// Offset of the probe relative to the print head
	float height;							// Nozzle height at which the target ADC value is returned
	float calibTemperature;					// Temperature at which we did the calibration
	float temperatureCoefficient;			// Variation of height with bed temperature
	float diveHeight;						// The dive height we use when probing
	float param1, param2;					// Extra parameters used by some types of probe e.g. Delta probe

	void Init(float h)
	{
		adcValue = Z_PROBE_AD_VALUE;
		xOffset = yOffset = 0.0;
		height = h;
		calibTemperature = 20.0;
		temperatureCoefficient = 0.0;		// no default temperature correction
		diveHeight = DEFAULT_Z_DIVE;
		param1 = param2 = 0.0;
	}

	float GetStopHeight(float temperature) const
	{
		return ((temperature - calibTemperature) * temperatureCoefficient) + height;
	}

	bool operator==(const ZProbeParameters& other) const
	{
		return adcValue == other.adcValue
				&& height == other.height
				&& xOffset == other.xOffset
				&& yOffset == other.yOffset
				&& calibTemperature == other.calibTemperature
				&& temperatureCoefficient == other.temperatureCoefficient
				&& diveHeight == other.diveHeight
				&& param1 == other.param1
				&& param2 == other.param2;
	}

	bool operator!=(const ZProbeParameters& other) const
	{
		return !operator==(other);
	}
};

class PidParameters
{
	// If you add any more variables to this class, don't forget to change the definition of operator== in Platform.cpp!
	private:
		float thermistorBeta, thermistorInfR;	// private because these must be changed together

	public:
		float kI, kD, kP, kT, kS;
		float fullBand, pidMin, pidMax;
		float thermistorSeriesR;
		float adcLowOffset, adcHighOffset;

		float GetBeta() const { return thermistorBeta; }
		float GetRInf() const { return thermistorInfR; }

		bool UsePID() const;
		float GetThermistorR25() const;
		void SetThermistorR25AndBeta(float r25, float beta);

		bool operator==(const PidParameters& other) const;
		bool operator!=(const PidParameters& other) const
		{
			return !operator==(other);
		}
};

// Class to perform averaging of values read from the ADC
// numAveraged should be a power of 2 for best efficiency

template<size_t numAveraged> class AveragingFilter
{
	public:
		AveragingFilter()
		{
			Init(0);
		}

		void Init(uint16_t val) volatile
		{
			irqflags_t flags = cpu_irq_save();
			sum = (uint32_t)val * (uint32_t)numAveraged;
			index = 0;
			isValid = false;
			for(size_t i = 0; i < numAveraged; ++i)
			{
				readings[i] = val;
			}
			cpu_irq_restore(flags);
		}

		// Call this to put a new reading into the filter
		// This is only called by the ISR, so it not declared volatile to make it faster
		void ProcessReading(uint16_t r)
		{
			sum = sum - readings[index] + r;
			readings[index] = r;
			++index;
			if (index == numAveraged)
			{
				index = 0;
				isValid = true;
			}
		}

		// Return the raw sum
		uint32_t GetSum() const volatile
		{
			return sum;
		}

		// Return true if we have a valid average
		bool IsValid() const volatile
		{
			return isValid;
		}

	private:
		uint16_t readings[numAveraged];
		size_t index;
		uint32_t sum;
		bool isValid;
		//invariant(sum == + over readings)
		//invariant(index < numAveraged)
};

typedef AveragingFilter<THERMISTOR_AVERAGE_READINGS> ThermistorAveragingFilter;
typedef AveragingFilter<Z_PROBE_AVERAGE_READINGS> ZProbeAveragingFilter;

// Enumeration of error condition bits
enum ErrorCode
{
	ErrorBadTemp = (1 << 0)
};

// Different types of hardware-related input-output
enum class SerialSource
{
	USB,
	AUX
};

// Supported message destinations
enum MessageType
{
	AUX_MESSAGE,						// Type byte of a message that is to be sent to an auxiliary device
	FLASH_LED,							// Type byte of a message that is to flash an LED; the next two bytes define the frequency and M/S ratio
	DISPLAY_MESSAGE,					// Type byte of a message that is to appear on a local display; the L is not displayed; \f and \n should be supported
	HOST_MESSAGE,						// Type byte of a message that is to be sent in non-blocking mode to the host via USB
	DEBUG_MESSAGE,						// Type byte of a debug message to send in blocking mode to USB
	HTTP_MESSAGE,						// Type byte of a message that is to be sent to the web (HTTP)
	TELNET_MESSAGE,						// Type byte of a message that is to be sent to a Telnet client
	GENERIC_MESSAGE,					// Type byte of a message that is to be sent to the web & host
};

// Endstop states
enum class EndStopHit
{
	noStop,								// No endstop hit
	lowHit,								// Low switch hit, or Z-probe in use and above threshold
	highHit,							// High stop hit
	lowNear								// Approaching Z-probe threshold
};

// The values of the following enumeration must tally with the definitions for the M574 command
enum class EndStopType
{
	noEndStop = 0,
	lowEndStop = 1,
	highEndStop = 2
};


// The main class that defines the RepRap machine for the benefit of the other classes

class Platform
{   
	public:

		friend class FileStore;

		Platform();

		//-------------------------------------------------------------------------------------------------------------

		// Enumerations to describe the states of drives and endstops
		enum class DriveStatus : uint8_t { disabled, idle, enabled };

		// These are the functions that form the interface between Platform and the rest of the firmware.

		void Init();										// Set the machine up after a restart.  If called subsequently this should set the machine up as if
															// it has just been restarted; it can do this by executing an actual restart if you like, but beware the loop of death...

		void Spin();										// This gets called in the main loop and should do any housekeeping needed
		void Exit();										// Shut down tidily. Calling Init after calling this should reset to the beginning
		Compatibility Emulating() const;
		void SetEmulating(Compatibility c);
		void Diagnostics();
		void DiagnosticTest(int d);
		void ClassReport(float &lastTime);					// Called on Spin() return to check everything's live.
		void SoftwareReset(uint16_t reason);
		bool AtxPower() const;
		void SetAtxPower(bool on);

		// Timing

		float Time();										// Returns elapsed seconds since some arbitrary time
		static uint32_t GetInterruptClocks();				// Get the interrupt clock count
		static bool ScheduleInterrupt(uint32_t tim);		// Schedule an interrupt at the specified clock count, or return true if it has passed already
		void Tick();

		// Communications

		bool GCodeAvailable(const SerialSource source) const;
		char ReadFromSource(const SerialSource source);

		void SetIPAddress(byte ip[]);
		const unsigned char* IPAddress() const;
		void SetNetMask(byte nm[]);
		const unsigned char* NetMask() const;
		void SetGateWay(byte gw[]);
		const unsigned char* GateWay() const;
		void SetMACAddress(uint8_t mac[]);
		const unsigned char* MACAddress() const;
		void SetBaudRate(size_t chan, uint32_t br);
		uint32_t GetBaudRate(size_t chan) const;
		void SetCommsProperties(size_t chan, uint32_t cp);
		uint32_t GetCommsProperties(size_t chan) const;

		// Files and data storage

		MassStorage* GetMassStorage();
		FileStore* GetFileStore(const char* directory, const char* fileName, bool write);
		FileStore* GetFileStore(const char* filePath, bool write);
		const char* GetWebDir() const;						// Where the htm etc files are
		const char* GetGCodeDir() const;					// Where the gcodes are
		const char* GetSysDir() const;						// Where the system files are
		const char* GetMacroDir() const;					// Where the user-defined macros are
		const char* GetConfigFile() const;					// Where the configuration is stored (in the system dir).
		const char* GetDefaultFile() const;					// Where the default configuration is stored (in the system dir).

		// Message output (see MessageType for further details)

		void Message(const MessageType type, const char *message);
		void Message(const MessageType type, const StringRef& message);
		void Message(const MessageType type, OutputBuffer *buffer);
		void MessageF(const MessageType type, const char *fmt, ...);
		void MessageF(const MessageType type, const char *fmt, va_list vargs);

		// Movement

		void EmergencyStop();
		void SetDirection(size_t drive, bool direction);
		void SetDirectionValue(size_t drive, bool dVal);
		bool GetDirectionValue(size_t drive) const;
		void StepHigh(size_t drive);
		void StepLow(size_t drive);
		void EnableDrive(size_t drive);
		void DisableDrive(size_t drive);
		void SetDriveIdle(size_t drive);
		void SetDrivesIdle();
		void SetMotorCurrent(size_t drive, float current);
		float MotorCurrent(size_t drive) const;
		void SetIdleCurrentFactor(float f);
		float GetIdleCurrentFactor() const { return idleCurrentFactor; }
		float DriveStepsPerUnit(size_t drive) const;
		const float *DriveStepsPerUnit() const { return driveStepsPerUnit; }
		void SetDriveStepsPerUnit(size_t drive, float value);
		float Acceleration(size_t drive) const;
		const float* Accelerations() const;
		void SetAcceleration(size_t drive, float value);
		float MaxFeedrate(size_t drive) const;
		const float* MaxFeedrates() const;
		void SetMaxFeedrate(size_t drive, float value);
		float ConfiguredInstantDv(size_t drive) const;
		float ActualInstantDv(size_t drive) const;
		void SetInstantDv(size_t drive, float value);
		float HomeFeedRate(size_t axis) const;
		void SetHomeFeedRate(size_t axis, float value);
		EndStopHit Stopped(size_t drive) const;
		EndStopHit GetZProbeResult() const;
		float AxisMaximum(size_t axis) const;
		void SetAxisMaximum(size_t axis, float value);
		float AxisMinimum(size_t axis) const;
		void SetAxisMinimum(size_t axis, float value);
		float AxisTotalLength(size_t axis) const;
		float GetElasticComp(size_t drive) const;
		void SetElasticComp(size_t drive, float factor);
		void SetEndStopConfiguration(size_t axis, EndStopType endstopType, bool logicLevel);
		void GetEndStopConfiguration(size_t axis, EndStopType& endstopType, bool& logicLevel) const;

		// Z probe

		float ZProbeStopHeight() const;
		float GetZProbeDiveHeight() const;
		void SetZProbeDiveHeight(float height);
		int ZProbe() const;
		int GetZProbeSecondaryValues(int& v1, int& v2);
		void SetZProbeType(int pt);
		int GetZProbeType() const;
		void SetZProbeChannel(int channel);
		int GetZProbeChannel() const;
		void SetZProbeAxes(const bool axes[AXES]);
		void GetZProbeAxes(bool (&axes)[AXES]);
		const ZProbeParameters& GetZProbeParameters() const;
		bool SetZProbeParameters(const struct ZProbeParameters& params);
		bool MustHomeXYBeforeZ() const;

		void SetExtrusionAncilliaryPWM(float v);
		float GetExtrusionAncilliaryPWM() const;
		void ExtrudeOn();
		void ExtrudeOff();

		size_t SlowestDrive() const;

		// Heat and temperature

		float GetTemperature(size_t heater) const;		// Result is in degrees Celsius
		void SetHeater(size_t heater, float power);		// power is a fraction in [0,1]
		float HeatSampleTime() const;
		void SetHeatSampleTime(float st);
		float GetFanValue() const;						// Result is returned in per cent
		void SetFanValue(float speed);					// Accepts values between 0..1 and 1..255
		float GetFanRPM();
		void SetPidParameters(size_t heater, const PidParameters& params);
		const PidParameters& GetPidParameters(size_t heater) const;
		float TimeToHot() const;
		void SetTimeToHot(float t);
		void SetThermistorNumber(size_t heater, size_t thermistor);
		int GetThermistorNumber(size_t heater) const;

		// Flash operations
		void ResetNvData();
		void ReadNvData();
		void WriteNvData();

		void SetAutoSave(bool enabled);

		// AUX device
		void Beep(int freq, int ms);

		// Hotend configuration
		float GetFilamentWidth() const;
		void SetFilamentWidth(float width);
		float GetNozzleDiameter() const;
		void SetNozzleDiameter(float diameter);

		// Fire the inkjet (if any) in the given pattern
		// If there is no inkjet false is returned; if there is one this returns true
		// So you can test for inkjet presence with if(platform->Inkjet(0))

		bool Inkjet(int bitPattern);

	private:
		void ResetChannel(size_t chan); // re-initialise a serial channel

		// These are the structures used to hold out non-volatile data.
		// The SAM3X doesn't have EEPROM so we save the data to flash. This unfortunately means that it gets cleared
		// every time we reprogram the firmware. So there is no need to cater for writing one version of this
		// struct and reading back another.

		struct SoftwareResetData
		{
			static const uint16_t magicValue = 0x59B2;	// value we use to recognise that all the flash data has been written
			static const uint32_t nvAddress = 0;		// address in flash where we store the nonvolatile data

			uint16_t magic;
			uint16_t resetReason;						// this records why we did a software reset, for diagnostic purposes
			size_t neverUsedRam;						// the amount of never used RAM at the last abnormal software reset
		};

		struct FlashData
		{
			static const uint16_t magicValue = 0x59B2;	// value we use to recognise that the flash data has been written
			static const uint32_t nvAddress = SoftwareResetData::nvAddress + sizeof(struct SoftwareResetData);

			uint16_t magic;

			// The remaining data could alternatively be saved to SD card.
			// Note however that if we save them as G codes, we need to provide a way of saving IR and ultrasonic G31 parameters separately.
			ZProbeParameters switchZProbeParameters;	// Z probe values for the endstop switch
			ZProbeParameters irZProbeParameters;		// Z probe values for the IR sensor
			ZProbeParameters alternateZProbeParameters;	// Z probe values for the alternate sensor
			uint8_t zProbeType;							// the type of Z probe we are currently using
			uint8_t zProbeChannel;						// needed to determine the Z probe modulation pin
			bool zProbeAxes[AXES];						// Z probe is used for these axes
			PidParameters pidParams[HEATERS];
			byte ipAddress[4];
			byte netMask[4];
			byte gateWay[4];
			uint8_t macAddress[6];
			Compatibility compatibility;
		};

		FlashData nvData;
		bool autoSaveEnabled;

		float lastTime;
		float longWait;
		float addToTime;
		unsigned long lastTimeCall;

		bool active;
		Compatibility compatibility;
		uint32_t errorCodeBits;

		void InitialiseInterrupts();
		int GetRawZHeight() const;
		void GetStackUsage(size_t* currentStack, size_t* maxStack, size_t* neverUsed) const;

		// DRIVES

		void SetSlowestDrive();
		void UpdateMotorCurrent(size_t drive);

		int8_t stepPins[DRIVES];
		int8_t directionPins[DRIVES];
		int8_t enablePins[DRIVES];
		volatile DriveStatus driveState[DRIVES];
		bool directions[DRIVES];
		int8_t endStopPins[DRIVES];
		float maxFeedrates[DRIVES];
		float accelerations[DRIVES];
		float driveStepsPerUnit[DRIVES];
		float instantDvs[DRIVES];
		float elasticComp[DRIVES];
		float motorCurrents[DRIVES];
		float idleCurrentFactor;
		MCP4461 mcpDuet;
		MCP4461 mcpExpansion;
		size_t slowestDrive;

		int8_t potWipes[DRIVES];
		float senseResistor;
		float maxStepperDigipotVoltage;

		// Z-Probe

		int8_t zProbePin;
		int8_t zProbeModulationPin;

		volatile ZProbeAveragingFilter zProbeOnFilter;					// Z probe readings we took with the IR turned on
		volatile ZProbeAveragingFilter zProbeOffFilter;					// Z probe readings we took with the IR turned off
		volatile ThermistorAveragingFilter thermistorFilters[HEATERS];	// bed and extruder thermistor readings

		float extrusionAncilliaryPWM;

		// AXES

		void InitZProbe();
		void UpdateNetworkAddress(byte dst[4], const byte src[4]);

		float axisMaxima[AXES];
		float axisMinima[AXES];
		float homeFeedrates[AXES];
		EndStopType endStopType[AXES+1];
		bool endStopLogicLevel[AXES+1];

		// HEATERS - Bed is assumed to be the first

		int GetRawTemperature(size_t heater) const;

		int8_t tempSensePins[HEATERS];
		int8_t heatOnPins[HEATERS];
		float heatSampleTime;
		float standbyTemperatures[HEATERS];
		float activeTemperatures[HEATERS];
		float coolingFanValue;
		int8_t coolingFanPin;
		int8_t coolingFanRpmPin;
		float timeToHot;
		float lastRpmResetTime;

		// Serial/USB

		uint32_t baudRates[NUM_SERIAL_CHANNELS];
		uint8_t commsParams[NUM_SERIAL_CHANNELS];
		OutputBuffer *auxOutputBuffer;
		OutputBuffer *usbOutputBuffer;

		// Files

		MassStorage* massStorage;
		FileStore* files[MAX_FILES];
		bool fileStructureInitialised;
		const char* webDir;
		const char* gcodeDir;
		const char* sysDir;
		const char* macroDir;
		const char* configFile;
		const char* defaultFile;

		// Data used by the tick interrupt handler

		adc_channel_num_t heaterAdcChannels[HEATERS];
		adc_channel_num_t zProbeAdcChannel;
		uint32_t thermistorOverheatSums[HEATERS];
		uint8_t tickState;
		int currentZProbeType;
		uint8_t currentHeater;
		int debugCode;

		static uint16_t GetAdcReading(adc_channel_num_t chan);
		static void StartAdcConversion(adc_channel_num_t chan);
		static adc_channel_num_t PinToAdcChannel(int pin);

		// Hotend configuration

		float filamentWidth;
		float nozzleDiameter;

		// Inkjet

		int8_t inkjetBits;
		int inkjetFireMicroseconds;
		int inkjetDelayMicroseconds;
		int8_t inkjetSerialOut;
		int8_t inkjetShiftClock;
		int8_t inkjetStorageClock;
		int8_t inkjetOutputEnable;
		int8_t inkjetClear;
};

// Small class to hold an open file and data relating to it.
// This is designed so that files are never left open and we never duplicate a file reference.
class FileData
{
	public:
		FileData() : f(nullptr) {}

		// Set this to refer to a newly-opened file
		void Set(FileStore* pfile)
		{
			Close();	// close any existing file
			f = pfile;
		}

		bool IsLive() const { return f != nullptr; }

		bool Close()
		{
			if (f != nullptr)
			{
				bool ok = f->Close();
				f = nullptr;
				return ok;
			}
			return false;
		}

		bool Read(char& b)
		{
			return f->Read(b);
		}

		bool Write(char b)
		{
			return f->Write(b);
		}

		bool Write(const char *s, unsigned int len)
		//pre(len <= 256)
		{
			return f->Write(s, len);
		}

		bool Flush()
		{
			return f->Flush();
		}

		bool Seek(FilePosition position)
		{
			return f->Seek(position);
		}

		float FractionRead() const
		{
			return (f == nullptr ? -1.0 : f->FractionRead());
		}

		FilePosition Position() const
		{
			return (f == nullptr ? 0 : f->Position());
		}

		FilePosition Length() const
		{
			return f->Length();
		}

		// Assignment operator
		void CopyFrom(const FileData& other)
		{
			Close();
			f = other.f;
			if (f != nullptr)
			{
				f->Duplicate();
			}
		}

		// Move operator
		void MoveFrom(FileData& other)
		{
			Close();
			f = other.f;
			other.Init();
		}

		bool operator==(const FileData& other) { return (f == other.f); }
		bool operator!=(const FileData& other) { return (f != other.f); }

	private:
		FileStore *f;

		void Init()
		{
			f = nullptr;
		}

		// Private assignment operator to prevent us assigning these objects
		FileData& operator=(const FileData&);

		// Private copy constructor to prevent us copying these objects
		FileData(const FileData&);
};

// Where the htm etc files are

inline const char* Platform::GetWebDir() const
{
	return webDir;
}

// Where the gcodes are

inline const char* Platform::GetGCodeDir() const
{
	return gcodeDir;
}

// Where the system files are

inline const char* Platform::GetSysDir() const
{
	return sysDir;
}

inline const char* Platform::GetMacroDir() const
{
	return macroDir;
}

inline const char* Platform::GetConfigFile() const
{
	return configFile;
}

inline const char* Platform::GetDefaultFile() const
{
	return defaultFile;
}

inline void Platform::SetExtrusionAncilliaryPWM(float v)
{
	extrusionAncilliaryPWM = v;
}

inline float Platform::GetExtrusionAncilliaryPWM() const
{
	return extrusionAncilliaryPWM;
}

// For the Duet we use the fan output for this
// DC 2015-03-21: To allow users to control the cooling fan via gcodes generated by slic3r etc.,
// only turn the fan on/off if the extruder ancilliary PWM has been set nonzero.
inline void Platform::ExtrudeOn()
{
	if (extrusionAncilliaryPWM > 0.0)
	{
		SetFanValue(extrusionAncilliaryPWM);
	}
}

// DC 2015-03-21: To allow users to control the cooling fan via gcodes generated by slic3r etc.,
// only turn the fan on/off if the extruder ancilliary PWM has been set nonzero.
inline void Platform::ExtrudeOff()
{
	if (extrusionAncilliaryPWM > 0.0)
	{
		SetFanValue(0.0);
	}
}

//*****************************************************************************************************************

// Drive the RepRap machine - Movement

inline float Platform::DriveStepsPerUnit(size_t drive) const
{
	return driveStepsPerUnit[drive];
}

inline void Platform::SetDriveStepsPerUnit(size_t drive, float value)
{
	driveStepsPerUnit[drive] = value;
}

inline float Platform::Acceleration(size_t drive) const
{
	return accelerations[drive];
}

inline const float* Platform::Accelerations() const
{
	return accelerations;
}

inline void Platform::SetAcceleration(size_t drive, float value)
{
	accelerations[drive] = value;
}

inline float Platform::MaxFeedrate(size_t drive) const
{
	return maxFeedrates[drive];
}

inline const float* Platform::MaxFeedrates() const
{
	return maxFeedrates;
}

inline void Platform::SetMaxFeedrate(size_t drive, float value)
{
	maxFeedrates[drive] = value;
}

inline float Platform::ConfiguredInstantDv(size_t drive) const
{
	return instantDvs[drive];
}

inline void Platform::SetInstantDv(size_t drive, float value)
{
	instantDvs[drive] = value;
	SetSlowestDrive();
}

inline size_t Platform::SlowestDrive() const
{
	return slowestDrive;
}

inline void Platform::SetDirectionValue(size_t drive, bool dVal)
{
	directions[drive] = dVal;
}

inline bool Platform::GetDirectionValue(size_t drive) const
{
	return directions[drive];
}

inline float Platform::HomeFeedRate(size_t axis) const
{
	return homeFeedrates[axis];
}

inline void Platform::SetHomeFeedRate(size_t axis, float value)
{
	homeFeedrates[axis] = value;
}

inline float Platform::AxisMaximum(size_t axis) const
{
	return axisMaxima[axis];
}

inline void Platform::SetAxisMaximum(size_t axis, float value)
{
	axisMaxima[axis] = value;
}

inline float Platform::AxisMinimum(size_t axis) const
{
	return axisMinima[axis];
}

inline void Platform::SetAxisMinimum(size_t axis, float value)
{
	axisMinima[axis] = value;
}

inline float Platform::AxisTotalLength(size_t axis) const
{
	return axisMaxima[axis] - axisMinima[axis];
}

// The A4988 requires 1us minimum pulse width, so we make separate StepHigh and StepLow calls so that we don't waste this time
inline void Platform::StepHigh(size_t drive)
{
	const int pin = stepPins[drive];
	if (pin >= 0)
	{
		digitalWrite(pin, 1);
	}
}

inline void Platform::StepLow(size_t drive)
{
	const int pin = stepPins[drive];
	if (pin >= 0)
	{
		digitalWrite(pin, 0);
	}
}

//********************************************************************************************************

// Drive the RepRap machine - Heat and temperature

inline int Platform::GetRawTemperature(size_t heater) const
{
	return (heater < HEATERS)
		? thermistorFilters[heater].GetSum()/(THERMISTOR_AVERAGE_READINGS >> AD_OVERSAMPLE_BITS)
		: 0;
}

inline float Platform::HeatSampleTime() const
{
	return heatSampleTime;
}

inline void Platform::SetHeatSampleTime(float st)
{
	heatSampleTime = st;
}

inline float Platform::TimeToHot() const
{
	return timeToHot;
}

inline void Platform::SetTimeToHot(float t)
{
	timeToHot = t;
}

inline bool Platform::GCodeAvailable(const SerialSource source) const
{
	switch (source)
	{
		case SerialSource::USB:
			return SerialUSB.available() > 0;

		case SerialSource::AUX:
			return Serial.available() > 0;
	}

	return false;
}

inline char Platform::ReadFromSource(const SerialSource source)
{
	switch (source)
	{
		case SerialSource::USB:
			return static_cast<char>(SerialUSB.read());

		case SerialSource::AUX:
			return static_cast<char>(Serial.read());
	}

	return 0;
}

inline const unsigned char* Platform::IPAddress() const
{
	return nvData.ipAddress;
}

inline const unsigned char* Platform::NetMask() const
{
	return nvData.netMask;
}

inline const unsigned char* Platform::GateWay() const
{
	return nvData.gateWay;
}

inline void Platform::SetMACAddress(uint8_t mac[])
{
	bool changed = false;
	for(size_t i = 0; i < 6; i++)
	{
		if (nvData.macAddress[i] != mac[i])
		{
			nvData.macAddress[i] = mac[i];
			changed = true;
		}
	}

	if (changed && autoSaveEnabled)
	{
		WriteNvData();
	}
}

inline const unsigned char* Platform::MACAddress() const
{
	return nvData.macAddress;
}

inline float Platform::GetElasticComp(size_t drive) const
{
	return (drive < DRIVES) ? elasticComp[drive] : 0.0;
}

inline void Platform::SetEndStopConfiguration(size_t axis, EndStopType esType, bool logicLevel)
//pre(axis < AXES)
{
	endStopType[axis] = esType;
	endStopLogicLevel[axis] = logicLevel;
}

inline void Platform::GetEndStopConfiguration(size_t axis, EndStopType& esType, bool& logicLevel) const
//pre(axis < AXES)
{
	esType = endStopType[axis];
	logicLevel = endStopLogicLevel[axis];
}

// Get the interrupt clock count
/*static*/ inline uint32_t Platform::GetInterruptClocks()
{
	//return TC_ReadCV(TC1, 0);
	// sadly, the Arduino IDE does not provide the inlined version of TC_ReadCV, so use the following instead...
	return TC1 ->TC_CHANNEL[0].TC_CV;
}

inline float Platform::GetFilamentWidth() const
{
	return filamentWidth;
}

inline void Platform::SetFilamentWidth(float width)
{
	filamentWidth = width;
}

inline float Platform::GetNozzleDiameter() const
{
	return nozzleDiameter;
}

inline void Platform::SetNozzleDiameter(float diameter)
{
	nozzleDiameter = diameter;
}

//***************************************************************************************

#endif

// vim: ts=4:sw=4
