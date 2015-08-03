/****************************************************************************************************

RepRapFirmware - G Codes

This class interprets G Codes from one or more sources, and calls the functions in Move, Heat etc
that drive the machine to do what the G Codes command.

-----------------------------------------------------------------------------------------------------

Version 0.1

13 February 2013

Adrian Bowyer
RepRap Professional Ltd
http://reprappro.com

Licence: GPL

****************************************************************************************************/

#ifndef GCODES_H
#define GCODES_H

const size_t STACK = 5;								// Maximum depth of the stack

const char AXIS_LETTERS[AXES] = { 'X', 'Y', 'Z' };	// Axes in a G-Code
const char FEEDRATE_LETTER = 'F';					// G-Code feedrate
const char EXTRUDE_LETTER = 'E';					// G-Code extrude

typedef uint16_t EndstopChecks;						// Must be large enough to hold a bitmap of drive numbers
const EndstopChecks ZProbeActive = 1 << 15;			// Must be distinct from 1 << (any drive number)


// Small class to hold an individual GCode and provide functions to allow it to be parsed

class GCodeBuffer
{
	public:
		GCodeBuffer(Platform* p, const char* id);
		//const char *Identity() const { return identity; }
		void Init(); 											// Set it up
		void Clear() { SetFinished(true); }						// Clean it up
		bool Put(char c);										// Add a character to the end
		bool Put(const char *str, size_t len);					// Add an entire string
		bool IsEmpty() const;									// Does this buffer contain any code?
		unsigned int Length() const;							// How many bytes have been fed into this buffer?
		bool Seen(char c);										// Is a character present?
		float GetFValue();										// Get a float after a key letter
		int GetIValue();										// Get an integer after a key letter
		long GetLValue();										// Get a long integer after a key letter
		const char* GetUnprecedentedString(bool optional = false);	// Get a string with no preceding key letter
		const char* GetString();								// Get a string after a key letter
		const void GetFloatArray(float a[], int& length);		// Get a :-separated list of floats after a key letter
		const void GetLongArray(long l[], int& length);			// Get a :-separated list of longs after a key letter
		const char* Buffer() const;								// What G-Code has been fed into this buffer?
		bool Active() const;									// Is this G-Code buffer still being acted upon?
		void SetFinished(bool f);								// Set the G Code executed (or not)
		const char* WritingFileDirectory() const;				// If we are writing the G Code to a file, where that file is
		void SetWritingFileDirectory(const char* wfd);			// Set the directory for the file to write the GCode in
		int GetToolNumberAdjust() const { return toolNumberAdjust; }
		void SetToolNumberAdjust(int arg) { toolNumberAdjust = arg; }
		void SetCommsProperties(uint32_t arg) { checksumRequired = (arg & 1); }

	private:

		enum class GCodeState { idle, executing };
		int CheckSum();											// Compute the checksum (if any) at the end of the G Code
		Platform* platform;										// Pointer to the RepRap's controlling class
		char gcodeBuffer[GCODE_LENGTH];							// The G Code
		const char* identity;									// Where we are from (web, file, serial line etc)
		int gcodePointer;										// Index in the buffer
		int readPointer;										// Where in the buffer to read next
		bool inComment;											// Are we after a ';' character?
		bool checksumRequired;									// True if we only accept commands with a valid checksum
		GCodeState state;										// State of this GCodeBuffer
		const char* writingFileDirectory;						// If the G Code is going into a file, where that is
		int toolNumberAdjust;									// Internal offset for tool numbers
};

//****************************************************************************************************

// The item class for the internal code queue

// We don't want certain codes to be executed too early, so we should queue them and execute
// them just in time while regular moves are being fed into the look-ahead queue.

const size_t CODE_QUEUE_LENGTH = 8;								// Number of storable G-Codes

class CodeQueueItem
{
	public:

		CodeQueueItem(CodeQueueItem *n);
		void Init(GCodeBuffer *gb, unsigned int executeAtMove);
		void SetNext(CodeQueueItem *n);
		CodeQueueItem *Next() const;

		unsigned int ExecuteAtMove() const;
		const char *GetCode() const;
		size_t GetCodeLength() const;
		GCodeBuffer *GetSource() const;

		void Execute();
		bool IsExecuting() const;

	private:

		char code[GCODE_LENGTH];
		size_t codeLength;
		unsigned int moveToExecute;

		GCodeBuffer *source;
		CodeQueueItem *next;
		bool executing;
};

//****************************************************************************************************

// The GCode interpreter

class GCodes
{
	public:

		GCodes(Platform* p, Webserver* w);
		void Spin();												// Called in a tight loop to make this class work
		void Init();												// Set it up
		void Exit();												// Shut it down
		void Reset();												// Reset some parameter to defaults
		bool DoFileMacro(const GCodeBuffer *gb, const char* fileName);	// Run a macro file. gb may be nullptr if called by an external class
		bool ReadMove(float* m, EndstopChecks& ce,
				uint8_t& rMoveType, FilePosition& fPos);			// Called by the Move class to get a movement set by the last G Code
		void ClearMove();
		void QueueFileToPrint(const char* fileName);				// Open a file of G Codes to run
		void DeleteFile(const char* fileName);						// Does what it says
		bool GetProbeCoordinates(int count, float& x,				// Get pre-recorded probe coordinates
				float& y, float& z) const;
		const char* GetCurrentCoordinates() const;					// Get where we are as a string
		float FractionOfFilePrinted() const;						// Returns the current file-based progress or -1.0 if no file is being printed
		bool DoingFileMacro() const;								// Are we busy processing a macro file?
		void Diagnostics();											// Send helpful information out
		bool HaveIncomingData() const;								// Is there something that we have to do?
		size_t GetStackPointer() const;								// Returns the current stack pointer
		bool GetAxisIsHomed(size_t axis) const;						// Is the axis at 0?
		void SetAxisIsHomed(size_t axis);							// Tell us that the axis is now homes
		bool CoolingInverted() const;								// Is the current fan value inverted?
		void MoveQueued();											// Announce a new move to be executed
		void MoveCompleted();										// Indicate that a move has been completed (called by ISR)
		bool HaveAux() const;										// Any device on the AUX line?
		bool IsPausing() const;
		bool IsPaused() const;
		bool IsResuming() const;
		bool IsRunning() const;
		OutputBuffer *GetAuxGCodeReply();							// Returns cached G-Code reply for AUX devices and clears its reference
		uint32_t GetAuxSeq() const;									// Returns the AUX sequence number

	private:

		bool AllMovesAreFinishedAndMoveBufferIsLoaded();			// Wait for move queue to exhaust and the current position is loaded
		bool DoCannedCycleMove(EndstopChecks ce);					// Do a move from an internally programmed canned cycle
		bool FileMacroCyclesReturn();								// End a macro
		bool CanStartMacro(const GCodeBuffer *gb) const;			// Verify if this GCodeBuffer can start another macro file
		bool CanQueueCode(GCodeBuffer *gb) const;					// Can we queue this code for delayed execution?
		bool ActOnCode(GCodeBuffer* gb, bool executeImmediately = true);	// Do a G, M or T Code
		bool HandleGcode(GCodeBuffer* gb);							// Do a G code
		bool HandleMcode(GCodeBuffer* gb);							// Do an M code
		bool HandleTcode(GCodeBuffer* gb);							// Do a T code
		void CancelPrint();											// Cancel the current print
		int SetUpMove(GCodeBuffer* gb, StringRef& reply);			// Pass a move on to the Move module
		bool DoDwell(GCodeBuffer *gb);								// Wait for a bit
		bool DoDwellTime(float dwell);								// Really wait for a bit
		bool DoHome(const GCodeBuffer *gb, StringRef& reply, bool& error);	// Home some axes
		bool DoSingleZProbeAtPoint();								// Probe at a given point
		bool DoSingleZProbe();										// Probe where we are
		int DoZProbe(float distance);								// Do a Z probe cycle up to the maximum specified distance
		bool SetSingleZProbeAtAPosition(GCodeBuffer *gb, StringRef& reply);	// Probes at a given position - see the comment at the head of the function itself
		bool SetBedEquationWithProbe(const GCodeBuffer *gb, StringRef& reply);	// Probes a series of points and sets the bed equation
		bool SetPrintZProbe(GCodeBuffer *gb, StringRef& reply);		// Either return the probe value, or set its threshold
		void SetOrReportOffsets(StringRef& reply, GCodeBuffer *gb);	// Deal with a G10
		bool SetPositions(GCodeBuffer *gb);							// Deal with a G92
		void SetPositions(float positionNow[DRIVES]);				// Set the current position to be this
		bool LoadMoveBufferFromGCode(GCodeBuffer *gb,  				// Set up a move for the Move class
				bool doingG92, bool applyLimits);
		bool NoHome() const;										// Are we homing and not finished?
		bool Push();												// Push feedrate etc on the stack
		bool Pop();													// Pop feedrate etc
		void DisableDrives();										// Turn the motors off
		void SetEthernetAddress(GCodeBuffer *gb, int mCode);		// Does what it says
		void SetMACAddress(GCodeBuffer *gb);						// Deals with an M540
		void HandleReply(GCodeBuffer *gb, bool error, const char *reply);	// Handle G-Code replies
		void HandleReply(GCodeBuffer *gb, bool error, OutputBuffer *reply);
		bool OpenFileToWrite(const char* directory,					// Start saving GCodes in a file
				const char* fileName, GCodeBuffer *gb);
		void WriteGCodeToFile(GCodeBuffer *gb);						// Write this GCode into a file
		void WriteHTMLToFile(char b, GCodeBuffer *gb);				// Save an HTML file (usually to upload a new web interface)
		bool OffsetAxes(GCodeBuffer *gb);							// Set offsets - deprecated, use G10
		void SetPidParameters(GCodeBuffer *gb, int heater, StringRef& reply);	// Set the P/I/D parameters for a heater
		void SetHeaterParameters(GCodeBuffer *gb, StringRef& reply);	// Set the thermistor and ADC parameters for a heater
		void ManageTool(GCodeBuffer *gb, StringRef& reply);			// Create a new tool definition
		void SetToolHeaters(Tool *tool, float temperature);			// Set all a tool's heaters to the temperature.  For M104...
		bool ChangeTool(const GCodeBuffer *gb, int newToolNumber);	// Select a new tool
		bool ToolHeatersAtSetTemperatures(const Tool *tool) const;	// Wait for the heaters associated with the specified tool to reach their set temperatures
		bool AllAxesAreHomed() const;								// Return true if all axes are homed
		void SetAllAxesNotHomed();									// Flag all axes as not homed

		Platform* platform;											// The RepRap machine
		bool active;												// Live and running?
		Webserver* webserver;										// The webserver class
		float dwellTime;											// How long a pause for a dwell (seconds)?
		bool dwellWaiting;											// We are in a dwell
		GCodeBuffer* httpGCode;										// The sources...
		GCodeBuffer* telnetGCode;									// ...
		GCodeBuffer* fileGCode;										// ...
		GCodeBuffer* serialGCode;									// ...
		GCodeBuffer* auxGCode;										// ...
		GCodeBuffer* fileMacroGCode;								// ...
		GCodeBuffer* queuedGCode;									// ... of G Codes
		bool moveAvailable;											// Have we seen a move G Code and set it up?
		float moveBuffer[DRIVES+1]; 								// Move coordinates; last is feed rate
		float savedMoveBuffer[DRIVES+1];							// The position and feedrate when we started the current simulation
		float pauseCoordinates[DRIVES+1];							// Holds the XYZ coordinates of the last move, the amount of skipped extrusion plus feedrate
		EndstopChecks endStopsToCheck;								// Which end stops we check them on the next move
		uint8_t moveType;											// 0 = normal move, 1 = homing move, 2 = direct motor move
		bool drivesRelative; 										// Are movements relative - all except X, Y and Z
		bool axesRelative;   										// Are movements relative - X, Y and Z
		bool drivesRelativeStack[STACK];							// For dealing with Push and Pop
		bool axesRelativeStack[STACK];								// For dealing with Push and Pop
		float feedrateStack[STACK];									// For dealing with Push and Pop
		float extruderPositionStack[STACK][DRIVES-AXES];			// For dealing with Push and Pop
		FileData fileStack[STACK];
		bool doingFileMacroStack[STACK];							// For dealing with Push and Pop
		uint8_t stackPointer;										// Push and Pop stack pointer
		char axisLetters[AXES]; 									// 'X', 'Y', 'Z'
		float lastExtruderPosition[DRIVES - AXES];					// Extruder position of the last move fed into the Move class
		float record[DRIVES+1];										// Temporary store for move positions
		float moveToDo[DRIVES+1];									// Where to go set by G1 etc
		bool activeDrive[DRIVES+1];									// Is this drive involved in a move?
		bool offSetSet;												// Are any axis offsets non-zero?
		float distanceScale;										// MM or inches
		FileData fileBeingPrinted;
		FileData fileToPrint;
		FileStore* fileBeingWritten;								// A file to write G Codes (or sometimes HTML) in
		bool doingFileMacro, returningFromMacro;					// Are we executing a macro file or are we returning from it?
		const GCodeBuffer *macroSourceGCode;						// Which GCodeBuffer is running the macro(s)?
		bool isPausing, isPaused, isResuming;						// What is the state of the current file print?
		bool doPauseMacro;											// Do we need to run pause.g and resume.g?
		float fractionOfFilePrinted;								// Only used to record the main file when a macro is being printed
		uint8_t eofStringCounter;									// Check the EoF string as we read.
		bool homing;												// Are we homing any axes?
		bool homeX;													// True to home the X axis this move
		bool homeY;													// True to home the Y axis this move
		bool homeZ;													// True to home the Z axis this move
		int probeCount;												// Counts multiple probe points
		int8_t cannedCycleMoveCount;								// Counts through internal (i.e. not macro) canned cycle moves
		bool cannedCycleMoveQueued;									// True if a canned cycle move has been set
		bool zProbesSet;											// True if all Z probing is done and we can set the bed equation
		bool settingBedEquationWithProbe;							// True if we're executing G32 without a macro
		float longWait;												// Timer for things that happen occasionally (seconds)
		bool limitAxes;												// Don't think outside the box.
		bool axisIsHomed[AXES];										// These record which of the axes have been homed
		bool waitingForMoveToComplete;
		bool coolingInverted;
		float lastFanValue;
		float lastProbedZ;											// The last height at which the Z probe stopped
		int8_t toolChangeSequence;									// Steps through the tool change procedure
		CodeQueueItem *internalCodeQueue;							// Linked list of all the queued codes
		CodeQueueItem *releasedQueueItems;							// Linked list of all released queue items
		unsigned int totalMoves;									// Total number of moves that have been fed into the look-ahead
		volatile unsigned int movesCompleted;						// Number of moves that have been completed (changed by ISR)
		bool auxDetected;											// Have we processed at least one G-Code from an AUX device?
		OutputBuffer *auxGCodeReply;								// G-Code reply for AUX devices (special one because it is actually encapsulated before sending)
		uint32_t auxSeq;											// Sequence number for AUX devices
		bool simulating;
		float simulationTime;
		FilePosition filePos;										// The position we got up to in the file being printed
		FilePosition moveFilePos;									// Saved version of filePos for the next real move to be processed
};

//*****************************************************************************************************

// Get an Int after a G Code letter

inline int GCodeBuffer::GetIValue()
{
	return static_cast<int>(GetLValue());
}

inline const char* GCodeBuffer::Buffer() const
{
	return gcodeBuffer;
}

inline bool GCodeBuffer::Active() const
{
	return (state == GCodeState::executing);
}

inline void GCodeBuffer::SetFinished(bool f)
{
	if (f)
	{
		state = GCodeState::idle;
		gcodeBuffer[0] = 0;
	}
	else
	{
		state = GCodeState::executing;
	}
}

inline const char* GCodeBuffer::WritingFileDirectory() const
{
	return writingFileDirectory;
}

inline void GCodeBuffer::SetWritingFileDirectory(const char* wfd)
{
	writingFileDirectory = wfd;
}

//*****************************************************************************************************

inline bool GCodes::DoingFileMacro() const
{
	return doingFileMacro || returningFromMacro;
}

inline bool GCodes::CanStartMacro(const GCodeBuffer *gb) const
{
	// Macros may always start another macro file
	if (gb == fileMacroGCode && !returningFromMacro)
		return true;

	// Regular GCodeBuffers may do this only if no macro file is being run,
	// or if they're the source of the currently executing macro file.
	if (!DoingFileMacro() || gb == macroSourceGCode)
		return true;

	return false;
}

inline bool GCodes::HaveIncomingData() const
{
	return fileBeingPrinted.IsLive() ||
				webserver->GCodeAvailable(WebSource::HTTP) ||
				webserver->GCodeAvailable(WebSource::Telnet) ||
				platform->GCodeAvailable(SerialSource::USB) ||
				platform->GCodeAvailable(SerialSource::AUX);
}

inline bool GCodes::GetAxisIsHomed(size_t axis) const
{
	return axisIsHomed[axis];
}

inline void GCodes::SetAxisIsHomed(size_t axis)
{
	axisIsHomed[axis] = true;
}

inline bool GCodes::AllAxesAreHomed() const
{
	return axisIsHomed[X_AXIS] && axisIsHomed[Y_AXIS] && axisIsHomed[Z_AXIS];
}

inline void GCodes::SetAllAxesNotHomed()
{
	axisIsHomed[X_AXIS] = axisIsHomed[Y_AXIS] = axisIsHomed[Z_AXIS] = false;
}

inline bool GCodes::NoHome() const
{
	return !(homeX || homeY || homeZ);
}

inline size_t GCodes::GetStackPointer() const
{
	return stackPointer;
}

inline bool GCodes::CoolingInverted() const
{
	return coolingInverted;
}

inline OutputBuffer *GCodes::GetAuxGCodeReply()
{
	OutputBuffer *temp = auxGCodeReply;
	auxGCodeReply = nullptr;
	return temp;
}

inline uint32_t GCodes::GetAuxSeq() const
{
	return auxSeq;
}

#endif

// vim: ts=4:sw=4
