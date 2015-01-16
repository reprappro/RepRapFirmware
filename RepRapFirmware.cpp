
/****************************************************************************************************

RepRapFirmware - Main Program

This firmware is intended to be a fully object-oriented highly modular control program for
RepRap self-replicating 3D printers.

It owes a lot to Marlin and to the original RepRap FiveD_GCode.


General design principles:

  * Control by RepRap G Codes.  These are taken to be machine independent, though some may be unsupported.
  * Full use of C++ OO techniques,
  * Make classes hide their data,
  * Make everything except the Platform class (see below) as stateless as possible,
  * No use of conditional compilation except for #include guards - if you need that, you should be
       forking the repository to make a new branch - let the repository take the strain,
  * Concentration of all machine-dependent defintions and code in Platform.h and Platform.cpp,
  * No specials for (X,Y) or (Z) - all movement is 3-dimensional,
  * Except in Platform.h, use real units (mm, seconds etc) throughout the rest of the code wherever possible,
  * Try to be efficient in memory use, but this is not critical,
  * Labour hard to be efficient in time use, and this is  critical,
  * Don't abhor floats - they work fast enough if you're clever,
  * Don't avoid arrays and structs/classes,
  * Don't avoid pointers,
  * Use operator and function overloading where appropriate.


Naming conventions:

  * #defines are all CAPITALS_WITH_OPTIONAL_UNDERSCORES_BETWEEN_WORDS
  * No underscores in other names - MakeReadableWithCapitalisation
  * Class names and functions start with a CapitalLetter
  * Variables start with a lowerCaseLetter
  * Use veryLongDescriptiveNames


Structure:

There are six main classes:

  * RepRap
  * GCodes
  * Heat
  * Move
  * Platform, and
  * Webserver

RepRap:

This is just a container class for the single instances of all the others, and otherwise does very little.

GCodes:

This class is fed GCodes, either from the web interface, or from GCode files, or from a serial interface,
Interprets them, and requests actions from the RepRap machine via the other classes.

Heat:

This class implements all heating and temperature control in the RepRap machine.

Move:

This class controls all movement of the RepRap machine, both along its axes, and in its extruder drives.

Platform:

This is the only class that knows anything about the physical setup of the RepRap machine and its
controlling electronics.  It implements the interface between all the other classes and the RepRap machine.
All the other classes are completely machine-independent (though they may declare arrays dimensioned
to values #defined in Platform.h).

Webserver:

This class talks to the network (via Platform) and implements a simple webserver to give an interactive
interface to the RepRap machine.  It uses the Knockout and Jquery Javascript libraries to achieve this.



When the software is running there is one single instance of each main class, and all the memory allocation is
done on initialization.  new/malloc should not be used in the general running code, and delete is never
used.  Each class has an Init() function that resets it to its boot-up state; the constructors merely handle
that memory allocation on startup.  Calling RepRap.Init() calls all the other Init()s in the right sequence.

There are other ancillary classes that are declared in the .h files for the master classes that use them.  For
example, Move has a DDA class that implements a Bresenham/digital differential analyser.


Timing:

There is a single interrupt chain entered via Platform.Interrupt().  This controls movement step timing, and
this chain of code should be the only place that volatile declarations and structure/variable-locking are
required.  All the rest of the code is called sequentially and repeatedly as follows:

All the main classes have a Spin() function.  These are called in a loop by the RepRap.Spin() function and implement
simple timesharing.  No class does, or ever should, wait inside one of its functions for anything to happen or call
any sort of delay() function.  The general rule is:

  Can I do a thing?
    Yes - do it
    No - set a flag/timer to remind me to do it next-time-I'm-called/at-a-future-time and return.

The restriction this strategy places on almost all the code in the firmware (that it must execute quickly and
never cause waits or delays) is balanced by the fact that none of that code needs to worry about synchronization,
locking, or other areas of code accessing items upon which it is working.  As mentioned, only the interrupt
chain needs to concern itself with such problems.  Unlike movement, heating (including PID controllers) does
not need the fast precision of timing that interrupts alone can offer.  Indeed, most heating code only needs
to execute a couple of times a second.

Most data is transferred bytewise, with classes' Spin() functions typically containing code like this:

  Is a byte available for me?
    Yes
      read it and add it to my buffer
      Is my buffer complete?
         Yes
           Act on the contents of my buffer
         No
           Return
  No
    Return

Note that it is simple to raise the "priority" of any class's activities relative to the others by calling its
Spin() function more than once from RepRap.Spin().

-----------------------------------------------------------------------------------------------------

Version 0.1

18 November 2012

Adrian Bowyer
RepRap Professional Ltd
http://reprappro.com

Licence: GPL

****************************************************************************************************/

// If this goes in the right place (Platform.h) the compile fails. Why? - AB

//#include <SPI.h>
//#include <Ethernet.h>
//#include <SD.h>

#include "RepRapFirmware.h"

// We just need one instance of RepRap; everything else is contained within it and hidden

RepRap reprap;

//*************************************************************************************************

// RepRap member functions.

// Do nothing more in the constructor; put what you want in RepRap:Init()

RepRap::RepRap() : active(false), debug(false)
{
  platform = new Platform();
  webserver = new Webserver(platform);
  gCodes = new GCodes(platform, webserver);
  move = new Move(platform, gCodes);
  heat = new Heat(platform, gCodes);
  toolList = NULL;
}

void RepRap::Init()
{
  debug = false;
  platform->Init();
  gCodes->Init();
  webserver->Init();
  move->Init();
  heat->Init();
  currentTool = NULL;
  active = true;
  coldExtrude = false;

  snprintf(scratchString, STRING_LENGTH, "%s Version %s dated %s\n", NAME, VERSION, DATE);
  platform->Message(HOST_MESSAGE, scratchString);

  FileStore* startup = platform->GetFileStore(platform->GetSysDir(), platform->GetConfigFile(), false);

  platform->Message(HOST_MESSAGE, "\n\nExecuting ");
  if(startup != NULL)
  {
	  startup->Close();
	  platform->Message(HOST_MESSAGE, platform->GetConfigFile());
	  platform->Message(HOST_MESSAGE, "...\n\n");
	  snprintf(scratchString, STRING_LENGTH, "M98 P%s\n", platform->GetConfigFile());
	  // We inject an M98 into the serial input stream to run the start-up macro
	  platform->GetLine()->InjectString(scratchString);
  } else
  {
	  platform->Message(HOST_MESSAGE, "config.g not found in the sys folder.  Did you copy ormerod1/2.g?\n");
//	  platform->Message(HOST_MESSAGE, platform->GetDefaultFile());
//	  platform->Message(HOST_MESSAGE, " (no configuration file found)...\n\n");
//	  snprintf(scratchString, STRING_LENGTH, "M98 P%s\n", platform->GetDefaultFile());
  }

  bool runningTheFile = false;
  bool initialisingInProgress = true;
  while(initialisingInProgress)
  {
	  Spin();
	  if(gCodes->FractionOfFilePrinted() >= 0.0)
		  runningTheFile = true;
	  if(runningTheFile)
	  {
		  if(gCodes->FractionOfFilePrinted() < 0.0)
			  initialisingInProgress = false;
	  }
  }

  if(platform->NetworkEnabled())
  {
	  platform->Message(HOST_MESSAGE, "\nStarting network...\n");
	  platform->StartNetwork(); // Need to do this here, as the configuration GCodes may set IP address etc.
  } else
	  platform->Message(HOST_MESSAGE, "\nNetwork disabled.\n");

  snprintf(scratchString, STRING_LENGTH, "\n%s is up and running.\n", NAME);
  platform->Message(HOST_MESSAGE, scratchString);
  fastLoop = FLT_MAX;
  slowLoop = 0.0;
  lastTime = platform->Time();
}

void RepRap::Exit()
{
  active = false;
  heat->Exit();
  move->Exit();
  gCodes->Exit();
  webserver->Exit();
  platform->Message(HOST_MESSAGE, "RepRap class exited.\n");
  platform->Exit();
}

void RepRap::Spin()
{
  if(!active)
    return;

  platform->Spin();
  webserver->Spin();
  gCodes->Spin();
  move->Spin();
  heat->Spin();

  // Keep track of the loop time

  double t = platform->Time();
  double dt = t - lastTime;
  if(dt < fastLoop)
	  fastLoop = dt;
  if(dt > slowLoop)
	  slowLoop = dt;
  lastTime = t;
}

void RepRap::Timing()
{
	snprintf(scratchString, STRING_LENGTH, "Slowest main loop (seconds): %f; fastest: %f\n", slowLoop, fastLoop);
	platform->AppendMessage(BOTH_MESSAGE, scratchString);
	fastLoop = FLT_MAX;
	slowLoop = 0.0;
}

void RepRap::Diagnostics()
{
  platform->Diagnostics();
  move->Diagnostics();
  heat->Diagnostics();
  gCodes->Diagnostics();
  webserver->Diagnostics();
  Timing();
}

// Turn off the heaters, disable the motors, and
// deactivate the Heat and Move classes.  Leave everything else
// working.

void RepRap::EmergencyStop()
{
	//platform->DisableInterrupts();

	Tool* tool = toolList;
	while(tool)
	{
		tool->Standby();
		tool = tool->Next();
	}

	heat->Exit();
	for(int8_t heater = 0; heater < HEATERS; heater++)
		platform->SetHeater(heater, 0.0);

	// We do this twice, to avoid an interrupt switching
	// a drive back on.  move->Exit() should prevent
	// interrupts doing this.

	for(int8_t i = 0; i < 2; i++)
	{
		move->Exit();
		for(int8_t drive = 0; drive < DRIVES; drive++)
		{
			platform->SetMotorCurrent(drive, 0.0);
			platform->Disable(drive);
		}
	}

	platform->Message(BOTH_MESSAGE, "Emergency Stop! Reset the controller to continue.");
}

/*
 * The first tool added becomes the one selected.  This will not happen in future releases.
 */

void RepRap::AddTool(Tool* tool)
{
	// First one?

	if(toolList == NULL)
	{
		toolList = tool;
		currentTool = tool;
		tool->Activate(currentTool);
		return;
	}

	// Subsequent one...

	Tool* existingTool = GetTool(tool->Number());
	if(existingTool == NULL)
	{
		toolList->AddTool(tool);
		return;
	}

	// Attempting to add a tool with a number that's been taken.

	snprintf(scratchString, STRING_LENGTH, "Tool creation - attempt to create a tool with a number that's in use: %d", tool->Number());
	reprap.GetPlatform()->Message(HOST_MESSAGE, scratchString);
	delete tool; // Harsh?  Protects against a memory leak.
}

void RepRap::PrintTool(int toolNumber, char* reply)
{
	Tool* tool = toolList;

	while(tool)
	{
		if(tool->Number() == toolNumber)
		{
			tool->Print(reply);
			return;
		}
		tool = tool->Next();
	}
	platform->Message(HOST_MESSAGE, "Attempt to print details of non-existent tool.");
}

void RepRap::PrintTools(char* reply)
{
	Tool* tool = toolList;
	int bufferSize = STRING_LENGTH;
	int startByte = 0;

	reply[0] = 0;

	while(tool)
	{
		tool->PrintInternal(&reply[startByte], bufferSize);
		startByte = strlen(reply) + 1;
		bufferSize -= startByte;
		tool = tool->Next();
		if(tool)
			strncat(reply, "\n", bufferSize);
	}
}

void RepRap::SelectTool(int toolNumber)
{
	Tool* tool = toolList;

	while(tool)
	{
		if(tool->Number() == toolNumber)
		{
			tool->Activate(currentTool);
			currentTool = tool;
			return;
		}
		tool = tool->Next();
	}

	// Selecting a non-existent tool is valid.  It sets them all to standby.

	if(currentTool != NULL)
		StandbyTool(currentTool->Number());
	currentTool = NULL;
}

void RepRap::StandbyTool(int toolNumber)
{
	Tool* tool = toolList;

	while(tool)
	{
		if(tool->Number() == toolNumber)
		{
			tool->Standby();
			if(currentTool == tool)
				currentTool = NULL;
			return;
		}
		tool = tool->Next();
	}

	snprintf(scratchString, STRING_LENGTH, "Attempt to standby a non-existent tool: %d.\n", toolNumber);
	platform->Message(HOST_MESSAGE, scratchString);
}

Tool* RepRap::GetTool(int toolNumber)
{
	Tool* tool = toolList;

	while(tool)
	{
		if(tool->Number() == toolNumber)
			return tool;
		tool = tool->Next();
	}

	return NULL; // Not an error
}

//void RepRap::SetToolVariables(int toolNumber, float* standbyTemperatures, float* activeTemperatures, float* offsets)
//{
//	Tool* tool = toolList;
//
//	while(tool)
//	{
//		if(tool->Number() == toolNumber)
//		{
//			tool->SetVariables(standbyTemperatures, activeTemperatures, offsets);
//			return;
//		}
//		tool = tool->Next();
//	}
//
//	snprintf(scratchString, STRING_LENGTH, "Attempt to set variables for a non-existent tool: %d.\n", toolNumber);
//	platform->Message(HOST_MESSAGE, scratchString);
//}





//*************************************************************************************************

// Utilities and storage not part of any class


char scratchString[STRING_LENGTH];

// String testing

bool StringEndsWith(const char* string, const char* ending)
{
  int j = strlen(string);
  int k = strlen(ending);
  if(k > j)
    return false;

  return(StringEquals(&string[j - k], ending));
}

bool StringEquals(const char* s1, const char* s2)
{
  int i = 0;
  while(s1[i] && s2[i])
  {
     if(tolower(s1[i]) != tolower(s2[i]))
       return false;
     i++;
  }

  return !(s1[i] || s2[i]);
}

bool StringStartsWith(const char* string, const char* starting)
{
  int j = strlen(string);
  int k = strlen(starting);
  if(k > j)
    return false;

  for(int i = 0; i < k; i++)
    if(string[i] != starting[i])
      return false;

  return true;
}

int StringContains(const char* string, const char* match)
{
  int i = 0;
  int count = 0;

  while(string[i])
  {
    if(string[i++] == match[count])
    {
      count++;
      if(!match[count])
        return i;
    } else
      count = 0;
  }

  return -1;
}











