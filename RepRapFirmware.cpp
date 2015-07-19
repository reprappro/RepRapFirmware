
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
  * Concentration of all machine-dependent definitions and code in Platform.h and Platform.cpp,
  * No specials for (X,Y) or (Z) - all movement is 3-dimensional,
  * Except in Platform.h, use real units (mm, seconds etc) throughout the rest of the code wherever possible,
  * Try to be efficient in memory use, but this is not critical,
  * Labour hard to be efficient in time use, and this is critical,
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

There are eight main classes:

  * RepRap
  * GCodes
  * Heat
  * Move
  * Platform
  * Network
  * Webserver, and
  * PrintMonitor

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

Network:

This class implements a basic TCP interface for the Webserver classes using lwip.

Webserver:

This class talks to the network (via Platform) and implements a simple webserver to give an interactive
interface to the RepRap machine.  It uses the Knockout and Jquery Javascript libraries to achieve this.
In addition, FTP and Telnet servers are provided for easier SD card file management and G-Code handling.

PrintMonitor:

This class provides methods to obtain statistics (height, filament usage etc.) from generated G-Code
files and to calculate estimated print end-times for a live print.


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

#include "RepRapFirmware.h"

// We just need one instance of RepRap; everything else is contained within it and hidden

RepRap reprap;

//*************************************************************************************************

void watchdogSetup(void)
{
    // RepRapFirmware enables the watchdog by default
    watchdogEnable(1000);
}

//*************************************************************************************************

// RepRap member functions.

// Do nothing more in the constructor; put what you want in RepRap:Init()

RepRap::RepRap() : active(false), debug(0), stopped(false), spinningModule(noModule), ticksInSpinState(0),
	resetting(false)
{
	platform = new Platform();
	network = new Network(platform);
	webserver = new Webserver(platform, network);
	gCodes = new GCodes(platform, webserver);
	move = new Move(platform, gCodes);
	heat = new Heat(platform, gCodes);
	printMonitor = new PrintMonitor(platform, gCodes);

	toolList = nullptr;

	freeOutputBuffers = nullptr;
	for(size_t i=0; i<OUTPUT_BUFFER_COUNT; i++)
	{
		freeOutputBuffers = new OutputBuffer(freeOutputBuffers);
	}
}

void RepRap::Init()
{
	debug = 0;

	// zpl thinks it's a bad idea to count the bed as an active heater...
	activeExtruders = activeHeaters = 0;

	SetPassword(DEFAULT_PASSWORD);
	SetName(DEFAULT_NAME);

	beepFrequency = beepDuration = 0;
	message[0] = 0;

	processingConfig = true;

	// All of the following init functions must execute reasonably quickly before the watchdog times us out
	platform->Init();
	gCodes->Init();
	network->Init();
	webserver->Init();
	move->Init();
	heat->Init();
	printMonitor->Init();
	currentTool = nullptr;

	active = true;		// must do this before we start the network, else the watchdog may time out

	platform->MessageF(HOST_MESSAGE, "%s Version %s dated %s\n", NAME, VERSION, DATE);

	const char *configFile = platform->GetConfigFile();
	FileStore *startup = platform->GetFileStore(platform->GetSysDir(), configFile, false);

	platform->Message(HOST_MESSAGE, "\nExecuting ");
	if (startup != nullptr)
	{
		startup->Close();
		platform->MessageF(HOST_MESSAGE, "%s... ", platform->GetConfigFile());
	}
	else
	{
		platform->MessageF(HOST_MESSAGE, "%s (no configuration file found)... ", platform->GetDefaultFile());
		configFile = platform->GetDefaultFile();
	}

	while (!gCodes->DoFileMacro(nullptr, configFile))
	{
		// GCodes::Spin will read the macro and ensure DoFileMacro returns true when it's done
		Spin();
	}
	processingConfig = false;
	platform->Message(HOST_MESSAGE, "Done!\n");

	if (network->IsEnabled())
	{
		// EMAC driver will report when it's starting up; no need to do this twice
		network->Enable();	// Need to do this here, as the configuration GCodes may set IP address etc.
	}
	else
	{
		platform->Message(HOST_MESSAGE, "Network disabled.\n");
	}

	platform->MessageF(HOST_MESSAGE, "%s is up and running.\n\n", NAME);
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
	platform->Message(GENERIC_MESSAGE, "RepRap class exited.\n");
	platform->Exit();
}

void RepRap::Spin()
{
	if (!active)
		return;

	spinningModule = modulePlatform;
	ticksInSpinState = 0;
	platform->Spin();

	spinningModule = moduleNetwork;
	ticksInSpinState = 0;
	network->Spin();

	spinningModule = moduleWebserver;
	ticksInSpinState = 0;
	webserver->Spin();

	spinningModule = moduleGcodes;
	ticksInSpinState = 0;
	gCodes->Spin();

	spinningModule = moduleMove;
	ticksInSpinState = 0;
	move->Spin();

	spinningModule = moduleHeat;
	ticksInSpinState = 0;
	heat->Spin();

	spinningModule = modulePrintMonitor;
	ticksInSpinState = 0;
	printMonitor->Spin();

	spinningModule = noModule;
	ticksInSpinState = 0;

	// Check if we need to display a cold extrusion warning

	for(Tool *t = toolList; t != nullptr; t = t->Next())
	{
		if (t->DisplayColdExtrudeWarning())
		{
			platform->MessageF(GENERIC_MESSAGE, "Warning: Tool %d was not driven because its heater temperatures were not high enough\n", t->Number());
			break;
		}
	}

	// Keep track of the loop time

	float t = platform->Time();
	float dt = t - lastTime;
	if (dt < fastLoop)
	{
		fastLoop = dt;
	}
	if (dt > slowLoop)
	{
		slowLoop = dt;
	}
	lastTime = t;
}

void RepRap::Timing()
{
	platform->MessageF(GENERIC_MESSAGE, "Slowest main loop (seconds): %f; fastest: %f\n", slowLoop, fastLoop);
	fastLoop = FLT_MAX;
	slowLoop = 0.0;
}

void RepRap::Diagnostics()
{
	platform->Message(GENERIC_MESSAGE, "Diagnostics\n");

	OutputBuffer *buff = freeOutputBuffers;
	size_t freeOutputBuffers = 0;
	while (buff != nullptr)
	{
		freeOutputBuffers++;
		buff = buff->next;
	}
	platform->MessageF(GENERIC_MESSAGE, "Free output buffers: %d of %d\n", freeOutputBuffers, OUTPUT_BUFFER_COUNT);

	platform->Diagnostics();				// this includes a call to our Timing() function
	move->Diagnostics();
	heat->Diagnostics();
	gCodes->Diagnostics();
	network->Diagnostics();
	webserver->Diagnostics();
}

// Turn off the heaters, disable the motors, and
// deactivate the Heat and Move classes.  Leave everything else
// working.

void RepRap::EmergencyStop()
{
	stopped = true;
	platform->SetAtxPower(false);		// turn off the ATX power if we can

	//platform->DisableInterrupts();

	Tool* tool = toolList;
	while(tool)
	{
		tool->Standby();
		tool = tool->Next();
	}

	heat->Exit();
	for(size_t heater = 0; heater < HEATERS; heater++)
	{
		platform->SetHeater(heater, 0.0);
	}

	// We do this twice, to avoid an interrupt switching
	// a drive back on.  move->Exit() should prevent
	// interrupts doing this.

	for(size_t i = 0; i < 2; i++)
	{
		move->Exit();
		for(size_t drive = 0; drive < DRIVES; drive++)
		{
			platform->SetMotorCurrent(drive, 0.0);
			platform->DisableDrive(drive);
		}
	}
}

void RepRap::SetDebug(Module m, bool enable)
{
	if (enable)
	{
		debug |= (1 << m);
	}
	else
	{
		debug &= ~(1 << m);
	}
	PrintDebug();
}

void RepRap::SetDebug(bool enable)
{
	debug = (enable) ? 0xFFFF : 0;
}

void RepRap::PrintDebug()
{
	if (debug != 0)
	{
		platform->Message(GENERIC_MESSAGE, "Debugging enabled for modules:");
		for(size_t i=0; i<16;i++)
		{
			if (debug & (1 << i))
			{
				platform->MessageF(GENERIC_MESSAGE, " %s", moduleName[i]);
			}
		}
		platform->Message(GENERIC_MESSAGE, "\n");
	}
	else
	{
		platform->Message(GENERIC_MESSAGE, "Debugging disabled\n");
	}
}

void RepRap::AddTool(Tool* tool)
{
	if (toolList == nullptr)
	{
		toolList = tool;
	}
	else
	{
		toolList->AddTool(tool);
	}
	tool->UpdateExtruderAndHeaterCount(activeExtruders, activeHeaters);
}

void RepRap::DeleteTool(Tool* tool)
{
	// Must have a valid tool...
	if (tool == nullptr)
	{
		return;
	}

	// Deselect it if necessary
	if (GetCurrentTool() == tool)
	{
		SelectTool(-1);
	}

	// Switch off any associated heater
	for(size_t i=0; i<tool->HeaterCount(); i++)
	{
		reprap.GetHeat()->SwitchOff(tool->Heater(i));
	}

	// Purge any references to this tool
	Tool *parent = nullptr;
	for(Tool *t = toolList; t != nullptr; t = t->Next())
	{
		if (t->Next() == tool)
		{
			parent = t;
			break;
		}
	}

	if (parent == nullptr)
	{
		toolList = tool->Next();
	}
	else
	{
		parent->next = tool->next;
	}

	// Delete it
	delete tool;

	// Update the number of active heaters and extruder drives
	activeExtruders = activeHeaters = 0;
	for(Tool *t = toolList; t != nullptr; t = t->Next())
	{
		t->UpdateExtruderAndHeaterCount(activeExtruders, activeHeaters);
	}
}

void RepRap::SelectTool(int toolNumber)
{
	Tool* tool = toolList;

	while (tool)
	{
		if (tool->Number() == toolNumber)
		{
			tool->Activate(currentTool);
			currentTool = tool;
			return;
		}
		tool = tool->Next();
	}

	// Selecting a non-existent tool is valid.  It sets them all to standby.

	if (currentTool != nullptr)
	{
		StandbyTool(currentTool->Number());
	}
	currentTool = nullptr;
}

void RepRap::PrintTool(int toolNumber, StringRef& reply)
{
	for(Tool *tool = toolList; tool != nullptr; tool = tool->next)
	{
		if (tool->Number() == toolNumber)
		{
			tool->Print(reply);
			return;
		}
	}

	reply.copy("Error: Attempt to print details of non-existent tool.\n");
}

void RepRap::StandbyTool(int toolNumber)
{
	Tool* tool = toolList;

	while (tool)
	{
		if (tool->Number() == toolNumber)
		{
			tool->Standby();
			if (currentTool == tool)
			{
				currentTool = nullptr;
			}
			return;
		}
		tool = tool->Next();
	}

	platform->MessageF(GENERIC_MESSAGE, "Error: Attempt to standby a non-existent tool: %d.\n", toolNumber);
}

Tool* RepRap::GetTool(int toolNumber)
{
	Tool* tool = toolList;

	while (tool)
	{
		if(tool->Number() == toolNumber)
		{
			return tool;
		}

		tool = tool->Next();
	}
	return nullptr; // Not an error
}

/*Tool* RepRap::GetToolByDrive(int driveNumber)
{
	Tool* tool = toolList;

	while (tool)
	{
		for(size_t drive = 0; drive < tool->DriveCount(); drive++)
		{
			if (tool->Drive(drive) + AXES == driveNumber)
			{
				return tool;
			}
		}

		tool = tool->Next();
	}
	return nullptr;
}*/

void RepRap::SetToolVariables(int toolNumber, float* standbyTemperatures, float* activeTemperatures)
{
	Tool* tool = toolList;

	while (tool)
	{
		if (tool->Number() == toolNumber)
		{
			tool->SetVariables(standbyTemperatures, activeTemperatures);
			return;
		}
		tool = tool->Next();
	}

	platform->MessageF(GENERIC_MESSAGE, "Error: Attempt to set variables for a non-existent tool: %d.\n", toolNumber);
}


void RepRap::Tick()
{
	if (active && !resetting)
	{
		platform->Tick();
		++ticksInSpinState;
		if (ticksInSpinState >= 20000)	// if we stall for 20 seconds, save diagnostic data and reset
		{
			resetting = true;
			for(size_t i = 0; i < HEATERS; i++)
			{
				platform->SetHeater(i, 0.0);
			}
			for(size_t i = 0; i < DRIVES; i++)
			{
				platform->DisableDrive(i);
				// We can't set motor currents to 0 here because that requires interrupts to be working, and we are in an ISR
			}

			platform->SoftwareReset(SoftwareResetReason::stuckInSpin);
		}
	}
}

// Get the JSON status response for the web server (or later for the M105 command).
// Type 1 is the ordinary JSON status response.
// Type 2 is the same except that static parameters are also included.
// Type 3 is the same but instead of static parameters we report print estimation values.
OutputBuffer *RepRap::GetStatusResponse(uint8_t type, bool forWebserver)
{
	// Need something to write to...
	OutputBuffer *response;
	if (!AllocateOutput(response))
	{
		// Should never happen
		return nullptr;
	}

	// Machine status
	char ch = GetStatusCharacter();
	response->printf("{\"status\":\"%c\",\"coords\":{", ch);

	/* Coordinates */
	{
		float liveCoordinates[DRIVES + 1];
		if (currentTool != nullptr)
		{
			const float *offset = currentTool->GetOffset();
			for (size_t i = 0; i < AXES; ++i)
			{
				liveCoordinates[i] += offset[i];
			}
		}
		move->LiveCoordinates(liveCoordinates);

		// Homed axes
		response->catf("\"axesHomed\":[%d,%d,%d]",
				(gCodes->GetAxisIsHomed(0)) ? 1 : 0,
				(gCodes->GetAxisIsHomed(1)) ? 1 : 0,
				(gCodes->GetAxisIsHomed(2)) ? 1 : 0);

		// Actual and theoretical extruder positions since power up, last G92 or last M23
		response->catf(",\"extr\":");		// announce actual extruder positions
		ch = '[';
		for (size_t extruder = 0; extruder < GetExtrudersInUse(); extruder++)
		{
			response->catf("%c%.1f", ch, liveCoordinates[AXES + extruder]);
			ch = ',';
		}
		if (ch == '[')
		{
			response->cat("[");
		}

		// XYZ positions
		response->cat("],\"xyz\":");
		ch = '[';
		for (size_t axis = 0; axis < AXES; axis++)
		{
			response->catf("%c%.2f", ch, liveCoordinates[axis]);
			ch = ',';
		}
	}

	// Current tool number
	int toolNumber = (currentTool == nullptr) ? -1 : currentTool->Number();
	response->catf("]},\"currentTool\":%d", toolNumber);

	/* Output - only reported once */
	{
		bool sendBeep = (beepDuration != 0 && beepFrequency != 0);
		bool sendMessage = (message[0]) && ((gCodes->HaveAux() && !forWebserver) || (!gCodes->HaveAux() && forWebserver));
		if (sendBeep || sendMessage)
		{
			response->cat(",\"output\":{");

			// Report beep values
			if (sendBeep)
			{
				response->catf("\"beepDuration\":%d,\"beepFrequency\":%d", beepDuration, beepFrequency);
				if (sendMessage)
				{
					response->cat(",");
				}

				beepFrequency = beepDuration = 0;
			}

			// Report message
			if (sendMessage)
			{
				response->cat("\"message\":");
				response->EncodeString(message, ARRAY_SIZE(message), false);
				message[0] = 0;
			}
			response->cat("}");
		}
	}

	/* Parameters */
	{
		// ATX power
		response->catf(",\"params\":{\"atxPower\":%d", platform->AtxPower() ? 1 : 0);

		// Cooling fan value
		float fanValue = (gCodes->CoolingInverted() ? 1.0 - platform->GetFanValue() : platform->GetFanValue());
		response->catf(",\"fanPercent\":%.2f", fanValue * 100.0);

		// Speed and Extrusion factors
		response->catf(",\"speedFactor\":%.2f,\"extrFactors\":", move->GetSpeedFactor() * 100.0);
		ch = '[';
		for (size_t extruder = 0; extruder < GetExtrudersInUse(); extruder++)
		{
			response->catf("%c%.2f", ch, move->GetExtrusionFactor(extruder) * 100.0);
			ch = ',';
		}
		response->cat((ch == '[') ? "[]}" : "]}");
	}

	// G-code reply sequence for webserver
	if (forWebserver)
	{
		response->catf(",\"seq\":%d", webserver->GetReplySeq());

		// There currently appears to be no need for this one, so skip it
		//response->catf(",\"buff\":%u", webserver->GetGCodeBufferSpace(WebSource::HTTP));
	}

	/* Sensors */
	{
		response->cat(",\"sensors\":{");

		// Probe
		int v0 = platform->ZProbe();
		int v1, v2;
		switch (platform->GetZProbeSecondaryValues(v1, v2))
		{
			case 1:
				response->catf("\"probeValue\":\%d,\"probeSecondary\":[%d]", v0, v1);
				break;
			case 2:
				response->catf("\"probeValue\":\%d,\"probeSecondary\":[%d,%d]", v0, v1, v2);
				break;
			default:
				response->catf("\"probeValue\":%d", v0);
				break;
		}

		// Fan RPM
		response->catf(",\"fanRPM\":%d}", static_cast<unsigned int>(platform->GetFanRPM()));
	}

	/* Temperatures */
	{
		response->cat(",\"temps\":{");

		/* Bed */
		if (HOT_BED != -1)
		{
			response->catf("\"bed\":{\"current\":%.1f,\"active\":%.1f,\"state\":%d},",
					heat->GetTemperature(HOT_BED), heat->GetActiveTemperature(HOT_BED),
					heat->GetStatus(HOT_BED));
		}

		/* Chamber */
		if (heat->GetChamberHeater() != -1)
		{
			response->catf("\"chamber\":{\"current\":%.1f,", heat->GetTemperature(heat->GetChamberHeater()));
			response->catf("\"active\":%.1f,", heat->GetActiveTemperature(heat->GetChamberHeater()));
			response->catf("\"state\":%d},", static_cast<int>(heat->GetStatus(heat->GetChamberHeater())));
		}

		/* Heads */
		{
			response->cat("\"heads\":{\"current\":");

			// Current temperatures
			ch = '[';
			for (size_t heater = E0_HEATER; heater < GetHeatersInUse(); heater++)
			{
				response->catf("%c%.1f", ch, heat->GetTemperature(heater));
				ch = ',';
			}
			response->cat((ch == '[') ? "[]" : "]");

			// Active temperatures
			response->catf(",\"active\":");
			ch = '[';
			for (size_t heater = E0_HEATER; heater < GetHeatersInUse(); heater++)
			{
				response->catf("%c%.1f", ch, heat->GetActiveTemperature(heater));
				ch = ',';
			}
			response->cat((ch == '[') ? "[]" : "]");

			// Standby temperatures
			response->catf(",\"standby\":");
			ch = '[';
			for (size_t heater = E0_HEATER; heater < GetHeatersInUse(); heater++)
			{
				response->catf("%c%.1f", ch, heat->GetStandbyTemperature(heater));
				ch = ',';
			}
			response->cat((ch == '[') ? "[]" : "]");

			// Heater statuses (0=off, 1=standby, 2=active, 3=fault)
			response->cat(",\"state\":");
			ch = '[';
			for (size_t heater = E0_HEATER; heater < GetHeatersInUse(); heater++)
			{
				response->catf("%c%d", ch, static_cast<int>(heat->GetStatus(heater)));
				ch = ',';
			}
			response->cat((ch == '[') ? "[]" : "]");
		}
		response->cat("}}");
	}

	// Time since last reset
	response->catf(",\"time\":%.1f", platform->Time());

	/* Extended Status Response */
	if (type == 2)
	{
		// Cold Extrude/Retract
		response->catf(",\"coldExtrudeTemp\":%1.f", heat->ColdExtrude() ? 0 : HOT_ENOUGH_TO_EXTRUDE);
		response->catf(",\"coldRetractTemp\":%1.f", heat->ColdExtrude() ? 0 : HOT_ENOUGH_TO_RETRACT);

		// Delta configuration
		response->catf(",\"geometry\":\"%s\"", move->GetGeometryString());

		// Machine name
		response->cat(",\"name\":");
		response->EncodeString(myName, ARRAY_SIZE(myName), false);

		/* Probe */
		{
			const ZProbeParameters probeParams = platform->GetZProbeParameters();

			// Trigger threshold
			response->catf(",\"probe\":{\"threshold\":%d", probeParams.adcValue);

			// Trigger height
			response->catf(",\"height\":%.2f", probeParams.height);

			// Type
			response->catf(",\"type\":%d}", platform->GetZProbeType());
		}

		/* Tool Mapping */
		{
			response->cat(",\"tools\":[");
			for(Tool *tool=toolList; tool != nullptr; tool = tool->Next())
			{
				// Heaters
				response->catf("{\"number\":%d,\"heaters\":[", tool->Number());
				for(size_t heater=0; heater<tool->HeaterCount(); heater++)
				{
					response->catf("%d", tool->Heater(heater));
					if (heater < tool->HeaterCount() - 1)
					{
						response->cat(",");
					}
				}

				// Extruder drives
				response->cat("],\"drives\":[");
				for(size_t drive=0; drive<tool->DriveCount(); drive++)
				{
					response->catf("%d", tool->Drive(drive));
					if (drive < tool->DriveCount() - 1)
					{
						response->cat(",");
					}
				}

				// Do we have any more tools?
				if (tool->Next() != nullptr)
				{
					response->cat("]},");
				}
				else
				{
					response->cat("]}");
				}
			}
			response->cat("]");
		}
	}
	else if (type == 3)
	{
		// Current Layer
		response->catf(",\"currentLayer\":%d", printMonitor->GetCurrentLayer());

		// Current Layer Time
		response->catf(",\"currentLayerTime\":%.1f", printMonitor->GetCurrentLayerTime());

		// Raw Extruder Positions
		float rawExtruderTotals[DRIVES - AXES];
		move->RawExtruderTotals(rawExtruderTotals);
		response->cat(",\"extrRaw\":");
		ch = '[';
		for (size_t extruder = 0; extruder < GetExtrudersInUse(); extruder++)		// loop through extruders
		{
			response->catf("%c%.1f", ch, rawExtruderTotals[extruder]);
			ch = ',';
		}
		if (ch == '[')
		{
			response->cat("]");
		}

		// Fraction of file printed
		response->catf("],\"fractionPrinted\":%.1f", (printMonitor->IsPrinting()) ? (gCodes->FractionOfFilePrinted() * 100.0) : 0.0);

		// First Layer Duration
		response->catf(",\"firstLayerDuration\":%.1f", printMonitor->GetFirstLayerDuration());

		// First Layer Height
		response->catf(",\"firstLayerHeight\":%.2f", printMonitor->GetFirstLayerHeight());

		// Print Duration
		response->catf(",\"printDuration\":%.1f", printMonitor->GetPrintDuration());

		// Warm-Up Time
		response->catf(",\"warmUpDuration\":%.1f", printMonitor->GetWarmUpDuration());

		/* Print Time Estimations */
		{
			// Based on file progress
			response->catf(",\"timesLeft\":{\"file\":%.1f", printMonitor->EstimateTimeLeft(fileBased));

			// Based on filament usage
			response->catf(",\"filament\":%.1f", printMonitor->EstimateTimeLeft(filamentBased));

			// Based on layers
			response->catf(",\"layer\":%.1f}", printMonitor->EstimateTimeLeft(layerBased));
		}
	}
	response->cat("}");

	return response;
}

OutputBuffer *RepRap::GetConfigResponse()
{
	// We need some resources to return a valid config response...
	OutputBuffer *response;
	if (!AllocateOutput(response))
	{
		return nullptr;
	}

	// Axis minima
	response->copy("{\"axisMins\":");
	char ch = '[';
	for (size_t axis = 0; axis < AXES; axis++)
	{
		response->catf("%c%.2f", ch, platform->AxisMinimum(axis));
		ch = ',';
	}

	// Axis maxima
	response->cat("],\"axisMaxes\":");
	ch = '[';
	for (size_t axis = 0; axis < AXES; axis++)
	{
		response->catf("%c%.2f", ch, platform->AxisMaximum(axis));
		ch = ',';
	}

	// Accelerations
	response->cat("],\"accelerations\":");
	ch = '[';
	for (size_t drive = 0; drive < DRIVES; drive++)
	{
		response->catf("%c%.2f", ch, platform->Acceleration(drive));
		ch = ',';
	}

	// Firmware details
	response->catf("],\"firmwareElectronics\":\"%s\"", ELECTRONICS);
	response->catf(",\"firmwareName\":\"%s\"", NAME);
	response->catf(",\"firmwareVersion\":\"%s\"", VERSION);
	response->catf(",\"firmwareDate\":\"%s\"", DATE);

	// Minimum feedrates
	response->cat(",\"minFeedrates\":");
	ch = '[';
	for (size_t drive = 0; drive < DRIVES; drive++)
	{
		response->catf("%c%.2f", ch, platform->ConfiguredInstantDv(drive));
		ch = ',';
	}

	// Maximum feedrates
	response->cat("],\"maxFeedrates\":");
	ch = '[';
	for (size_t drive = 0; drive < DRIVES; drive++)
	{
		response->catf("%c%.2f", ch, platform->MaxFeedrate(drive));
		ch = ',';
	}

	// Configuration File (whitespaces are skipped, otherwise we risk overflowing the response->buffer)
	response->cat("],\"configFile\":\"");
	FileStore *configFile = platform->GetFileStore(platform->GetSysDir(), platform->GetConfigFile(), false);
	if (configFile == nullptr)
	{
		response->cat("not found");
	}
	else
	{
		char c, esc;
		bool readingWhitespace = false;
		while (configFile->Read(c))
		{
			if (!readingWhitespace || (c != ' ' && c != '\t'))
			{
				switch (c)
				{
					case '\r':
						esc = 'r';
						break;
					case '\n':
						esc = 'n';
						break;
					case '\t':
						esc = 't';
						break;
					case '"':
						esc = '"';
						break;
					case '\\':
						esc = '\\';
						break;
					default:
						esc = 0;
						break;
				}

				if (esc)
				{
					response->catf("\\%c", esc);
				}
				else
				{
					response->cat(c);
				}
			}
			readingWhitespace = (c == ' ' || c == '\t');
		}
		configFile->Close();
	}
	response->cat("\"}");

	return response;
}

// Get the legacy JSON status response for the web server or M105 command.
// Type 0 is the old-style webserver status response (zpl fork doesn't support it any more).
// Type 1 is the new-style webserver status response.
// Type 2 is the M105 S2 response, which is like the new-style status response but some fields are omitted.
// Type 3 is the M105 S3 response, which is like the M105 S2 response except that static values are also included.
// 'seq' is the response sequence number, if it is not -1 and we have a different sequence number then we send the gcode response
OutputBuffer *RepRap::GetLegacyStatusResponse(uint8_t type, int seq)
{
	// Need something to write to...
	OutputBuffer *response;
	if (!AllocateOutput(response))
	{
		// Should never happen
		return nullptr;
	}

	const GCodes *gc = reprap.GetGCodes();
	if (type != 0)
	{
		// Send the status. Note that 'S' has always meant that the machine is halted in this version of the status response-> so we use A for pAused.
		char ch = GetStatusCharacter();
		if (ch == 'S')			// if paused then send 'A'
		{
			ch = 'A';
		}
		else if (ch == 'H')		// if halted then send 'S'
		{
			ch = 'S';
		}
		response->printf("{\"status\":\"%c\",\"heaters\":", ch);

		// Send the heater actual temperatures
		const Heat *heat = reprap.GetHeat();
		if (HOT_BED != -1)
		{
			ch = ',';
			response->catf("[%.1f", heat->GetTemperature(HOT_BED));
		}
		else
		{
			ch = '[';
		}
		for (size_t heater = E0_HEATER; heater < reprap.GetHeatersInUse(); heater++)
		{
			response->catf("%c%.1f", ch, heat->GetTemperature(heater));
			ch = ',';
		}
		response->cat((ch == '[') ? "[]" : "]");

		// Send the heater active temperatures
		response->catf(",\"active\":");
		if (HOT_BED != -1)
		{
			ch = ',';
			response->catf("[%.1f", heat->GetActiveTemperature(HOT_BED));
		}
		else
		{
			ch = '[';
		}
		for (size_t heater = E0_HEATER; heater < reprap.GetHeatersInUse(); heater++)
		{
			response->catf("%c%.1f", ch, heat->GetActiveTemperature(heater));
			ch = ',';
		}
		response->cat((ch == '[') ? "[]" : "]");

		// Send the heater standby temperatures
		response->catf(",\"standby\":");
		if (HOT_BED != -1)
		{
			ch = ',';
			response->catf("[%.1f", heat->GetStandbyTemperature(HOT_BED));
		}
		else
		{
			ch = '[';
		}
		for (size_t heater = E0_HEATER; heater < reprap.GetHeatersInUse(); heater++)
		{
			response->catf("%c%.1f", ch, heat->GetStandbyTemperature(heater));
			ch = ',';
		}
		response->cat((ch == '[') ? "[]" : "]");

		// Send the heater statuses (0=off, 1=standby, 2=active)
		response->cat(",\"hstat\":");
		if (HOT_BED != -1)
		{
			ch = ',';
			response->catf("[%d", static_cast<int>(heat->GetStatus(HOT_BED)));
		}
		else
		{
			ch = '[';
		}
		for (size_t heater = E0_HEATER; heater < reprap.GetHeatersInUse(); heater++)
		{
			response->catf("%c%d", ch, static_cast<int>(heat->GetStatus(heater)));
			ch = ',';
		}
		response->cat((ch == '[') ? "[]" : "]");

		// Send XYZ positions
		float liveCoordinates[DRIVES + 1];
		reprap.GetMove()->LiveCoordinates(liveCoordinates);
		const Tool* currentTool = reprap.GetCurrentTool();
		if (currentTool != nullptr)
		{
			const float *offset = currentTool->GetOffset();
			for (size_t i = 0; i < AXES; ++i)
			{
				liveCoordinates[i] += offset[i];
			}
		}
		response->catf(",\"pos\":");		// announce the XYZ position
		ch = '[';
		for (size_t drive = 0; drive < AXES; drive++)
		{
			response->catf("%c%.2f", ch, liveCoordinates[drive]);
			ch = ',';
		}

		// Send extruder total extrusion since power up, last G92 or last M23
		response->cat("],\"extr\":");		// announce the extruder positions
		ch = '[';
		for (size_t drive = 0; drive < reprap.GetExtrudersInUse(); drive++)		// loop through extruders
		{
			response->catf("%c%.1f", ch, liveCoordinates[drive + AXES]);
			ch = ',';
		}
		response->cat((ch == ']') ? "[]" : "]");

		// Send the speed and extruder override factors
		response->catf(",\"sfactor\":%.2f,\"efactor\":", move->GetSpeedFactor() * 100.0);
		ch = '[';
		for (size_t i = 0; i < reprap.GetExtrudersInUse(); ++i)
		{
			response->catf("%c%.2f", ch, move->GetExtrusionFactor(i) * 100.0);
			ch = ',';
		}
		response->cat((ch == '[') ? "[]" : "]");

		// Send the current tool number
		int toolNumber = (currentTool == nullptr) ? 0 : currentTool->Number();
		response->catf(",\"tool\":%d", toolNumber);
	}
	else
	{
		// The old (deprecated) poll response->lists the status, then all the heater temperatures, then the XYZ positions, then all the extruder positions.
		// These are all returned in a single vector called "poll".
		// This is a poor choice of format because we can't easily tell which is which unless we already know the number of heaters and extruders.
		// RRP reversed the order at version 0.65 to send the positions before the heaters, but we haven't yet done that.
		char c = (printMonitor->IsPrinting()) ? 'P' : 'I';
		response->printf("{\"poll\":[\"%c\",", c); // Printing
		for (size_t heater = 0; heater < HEATERS; heater++)
		{
			response->catf("\"%.1f\",", reprap.GetHeat()->GetTemperature(heater));
		}
		// Send XYZ and extruder positions
		float liveCoordinates[DRIVES + 1];
		reprap.GetMove()->LiveCoordinates(liveCoordinates);
		for (size_t drive = 0; drive < DRIVES; drive++)	// loop through extruders
		{
			char ch = (drive == DRIVES - 1) ? ']' : ',';	// append ] to the last one but , to the others
			response->catf("\"%.2f\"%c", liveCoordinates[drive], ch);
		}
	}

	// Send the Z probe value
	int v0 = platform->ZProbe();
	int v1, v2;
	switch (platform->GetZProbeSecondaryValues(v1, v2))
	{
		case 1:
			response->catf(",\"probe\":\"%d (%d)\"", v0, v1);
			break;
		case 2:
			response->catf(",\"probe\":\"%d (%d, %d)\"", v0, v1, v2);
			break;
		default:
			response->catf(",\"probe\":\"%d\"", v0);
			break;
	}

	// Send fan RPM value
	response->catf(",\"fanRPM\":%u", static_cast<unsigned int>(platform->GetFanRPM()));

	// Send the home state. To keep the messages short, we send 1 for homed and 0 for not homed, instead of true and false.
	if (type != 0)
	{
		response->catf(",\"homed\":[%d,%d,%d]",
				(gc->GetAxisIsHomed(0)) ? 1 : 0,
				(gc->GetAxisIsHomed(1)) ? 1 : 0,
				(gc->GetAxisIsHomed(2)) ? 1 : 0);
	}
	else
	{
		response->catf(",\"hx\":%d,\"hy\":%d,\"hz\":%d",
				(gc->GetAxisIsHomed(0)) ? 1 : 0,
				(gc->GetAxisIsHomed(1)) ? 1 : 0,
				(gc->GetAxisIsHomed(2)) ? 1 : 0);
	}

	if (printMonitor->IsPrinting())
	{
		// Send the fraction printed
		response->catf(",\"fraction_printed\":%.4f", max<float>(0.0, gc->FractionOfFilePrinted()));
	}

	response->cat(",\"message\":");
	response->EncodeString(message, ARRAY_SIZE(message), false);

	if (type < 2)
	{
		response->catf(",\"buff\":%u", webserver->GetGCodeBufferSpace(WebSource::HTTP));	// send the amount of buffer space available for gcodes
	}
	else if (type == 2)
	{
		if (printMonitor->IsPrinting())
		{
			// Send estimated times left based on file progress, filament usage, and layers
			response->catf(",\"timesLeft\":[%.1f,%.1f,%.1f]",
					printMonitor->EstimateTimeLeft(fileBased),
					printMonitor->EstimateTimeLeft(filamentBased),
					printMonitor->EstimateTimeLeft(layerBased));
		}
	}
	else if (type == 3)
	{
		// Add the static fields. For now this is just the machine name, but other fields could be added e.g. axis lengths.
		response->cat(",\"myName\":");
		response->EncodeString(myName, ARRAY_SIZE(myName), false);
	}

	uint32_t newSeq = gCodes->GetAuxSeq();
	if (type < 2 || (seq != -1 && seq != newSeq))
	{
		// Send the response to the last command. Do this last
		response->catf(",\"seq\":%u,\"resp\":", newSeq);						// send the response sequence number

		// Send the JSON response
		response->EncodeReply(gCodes->GetAuxGCodeReply(), true);
	}
	response->cat("}");

	return response;
}

// Copy some parameter text, stopping at the first control character or when the destination buffer is full, and removing trailing spaces
void RepRap::CopyParameterText(const char* src, char *dst, size_t length)
{
	size_t i;
	for (i = 0; i + 1 < length && src[i] >= ' '; ++i)
	{
		dst[i] = src[i];
	}
	// Remove any trailing spaces
	while (i > 0 && dst[i - 1] == ' ')
	{
		--i;
	}
	dst[i] = 0;
}

// Get just the machine name in JSON format
OutputBuffer *RepRap::GetNameResponse()
{
	// Need something to write to...
	OutputBuffer *response;
	if (!AllocateOutput(response))
	{
		return nullptr;
	}

	response->copy("{\"myName\":");
	response->EncodeString(myName, ARRAY_SIZE(myName), false);
	response->cat("}");

	return response;
}

// Get the list of files in the specified directory in JSON format
OutputBuffer *RepRap::GetFilesResponse(const char *dir, bool flagsDirs)
{
	// Need something to write to...
	OutputBuffer *response;
	if (!AllocateOutput(response))
	{
		return nullptr;
	}

	response->copy("{\"dir\":");
	response->EncodeString(dir, strlen(dir), false);
	response->cat(",\"files\":[");

	FileInfo fileInfo;
	bool firstFile = true;
	bool gotFile = platform->GetMassStorage()->FindFirst(dir, fileInfo);

	char filename[FILENAME_LENGTH];
	filename[0] = '*';
	const char *fname;

	while (gotFile)
	{
		if (!firstFile)
		{
			response->cat(',');
		}
		if (flagsDirs && fileInfo.isDirectory)
		{
			strncpy(filename + sizeof(char), fileInfo.fileName, FILENAME_LENGTH - 1);
			filename[FILENAME_LENGTH - 1] = 0;
			fname = filename;
		}
		else
		{
			fname = fileInfo.fileName;
		}

		response->EncodeString(fname, FILENAME_LENGTH, false);

		firstFile = false;
		gotFile = platform->GetMassStorage()->FindNext(fileInfo);
	}
	response->cat("]}");

	return response;
}

// Allocates an output buffer instance which can be used for (large) string outputs
bool RepRap::AllocateOutput(OutputBuffer *&buf)
{
	const irqflags_t flags = cpu_irq_save();

	if (freeOutputBuffers == nullptr)
	{
		cpu_irq_restore(flags);

		buf = nullptr;
		return false;
	}

	buf = freeOutputBuffers;
	freeOutputBuffers = buf->next;

	buf->next = nullptr;
	buf->dataLength = buf->bytesLeft = 0;
	buf->referenceCounter = 1; // Assume it's only used once by default

	cpu_irq_restore(flags);

	return true;
}

// Releases an output buffer instance and returns the next entry from the chain
OutputBuffer *RepRap::ReleaseOutput(OutputBuffer *buf)
{
	const irqflags_t flags = cpu_irq_save();

	OutputBuffer *nextBuffer = buf->next;

	// If this one is reused by another piece of code, don't free it up
	if (buf->referenceCounter > 1)
	{
		buf->referenceCounter--;
		cpu_irq_restore(flags);
		return nextBuffer;
	}

	// Otherwise prepend it to the list of free output buffers again
	buf->next = freeOutputBuffers;
	freeOutputBuffers = buf;

	cpu_irq_restore(flags);

	return nextBuffer;
}

void RepRap::Beep(int freq, int ms)
{
	if (gCodes->HaveAux())
	{
		// If there is an LCD device present, make it beep
		platform->Beep(freq, ms);
	}
	else
	{
		// Otherwise queue it until the webserver can process it
		beepFrequency = freq;
		beepDuration = ms;
	}
}

void RepRap::SetMessage(const char *msg)
{
	strncpy(message, msg, SHORT_STRING_LENGTH);
	message[SHORT_STRING_LENGTH - 1] = 0;
}

char RepRap::GetStatusCharacter() const
{
	if (processingConfig)
	{
		// Reading the configuration file
		return 'C';
	}
	if (IsStopped())
	{
		// Halted
		return 'H';
	}
	if (gCodes->IsPausing())
	{
		// Pausing / Decelerating
		return 'D';
	}
	if (gCodes->IsResuming())
	{
		// Resuming
		return 'R';
	}
	if (gCodes->IsPaused())
	{
		// Paused / Stopped
		return 'S';
	}
	if (printMonitor->IsPrinting())
	{
		// Printing
		return 'P';
	}
	if (gCodes->DoingFileMacro() || !move->NoLiveMovement())
	{
		// Busy
		return 'B';
	}
	// Idle
	return 'I';
}

bool RepRap::NoPasswordSet() const
{
	return (!password[0] || StringEquals(password, DEFAULT_PASSWORD));
}

bool RepRap::CheckPassword(const char *pw) const
{
	return StringEquals(pw, password);
}

void RepRap::SetPassword(const char* pw)
{
	// Users sometimes put a tab character between the password and the comment, so allow for this
	CopyParameterText(pw, password, ARRAY_SIZE(password));
}

const char *RepRap::GetName() const
{
	return myName;
}

void RepRap::SetName(const char* nm)
{
	// Users sometimes put a tab character between the machine name and the comment, so allow for this
	CopyParameterText(nm, myName, ARRAY_SIZE(myName));

	// Set new DHCP hostname
	network->SetHostname(myName);
}

// Given that we want to extrude/etract the specified extruder drives, check if they are allowed.
// For each disallowed one, log an error to report later and return a bit in the bitmap.
// This may be called by an ISR!
unsigned int RepRap::GetProhibitedExtruderMovements(unsigned int extrusions, unsigned int retractions)
{
	unsigned int result = 0;
	Tool *tool = toolList;
	while (tool != nullptr)
	{
		for (int driveNum = 0; driveNum < tool->DriveCount(); driveNum++)
		{
			const int extruderDrive = tool->Drive(driveNum);
			unsigned int mask = 1 << extruderDrive;
			if (extrusions & mask)
			{
				if (!tool->ToolCanDrive(true))
				{
					result |= mask;
				}
			}
			else if (retractions & (1 << extruderDrive))
			{
				if (!tool->ToolCanDrive(false))
				{
					result |= mask;
				}
			}
		}

		tool = tool->Next();
	}
	return result;
}

//*************************************************************************************************
// StringRef class member implementations

size_t StringRef::strlen() const
{
	return strnlen(p, len - 1);
}

int StringRef::printf(const char *fmt, ...)
{
	va_list vargs;
	va_start(vargs, fmt);
	int ret = vsnprintf(p, len, fmt, vargs);
	va_end(vargs);
	return ret;
}

int StringRef::vprintf(const char *fmt, va_list vargs)
{
	return vsnprintf(p, len, fmt, vargs);
}

int StringRef::catf(const char *fmt, ...)
{
	size_t n = strlen();
	if (n + 1 < len)		// if room for at least 1 more character and a null
	{
		va_list vargs;
		va_start(vargs, fmt);
		int ret = vsnprintf(p + n, len - n, fmt, vargs);
		va_end(vargs);
		return ret + n;
	}
	return 0;
}

// This is quicker than printf for printing constant strings
size_t StringRef::copy(const char* src)
{
	size_t length = strnlen(src, len - 1);
	memcpy(p, src, length);
	p[length] = 0;
	return length;
}

// This is quicker than catf for printing constant strings
size_t StringRef::cat(const char* src)
{
	size_t length = strlen();
	size_t toCopy = strnlen(src, len - length - 1);
	memcpy(p + length, src, toCopy);
	length += toCopy;
	p[length] = 0;
	return length;
}

//*************************************************************************************************
// OutputBuffer class implementation

void OutputBuffer::Append(OutputBuffer *other)
{
	if (other != nullptr)
	{
		OutputBuffer *lastBuffer = this;
		while (lastBuffer->next != nullptr)
		{
			lastBuffer = lastBuffer->next;
		}
		lastBuffer->next = other;
	}
}

void OutputBuffer::SetReferences(size_t refs)
{
	referenceCounter = refs;
	if (next != nullptr)
	{
		next->SetReferences(refs);
	}
}

uint32_t OutputBuffer::Length() const
{
	uint32_t totalLength = 0;
	const OutputBuffer *current = this;
	do {
		totalLength += current->DataLength();
		current = current->next;
	} while (current != nullptr);
	return totalLength;
}

char &OutputBuffer::operator[](size_t index)
{
	// Get the right buffer to access
	OutputBuffer *itemToIndex = this;
	while (index > itemToIndex->DataLength())
	{
		index -= itemToIndex->DataLength();
		itemToIndex = itemToIndex->Next();
	}

	// Return the char reference
	return itemToIndex->data[index];
}

char OutputBuffer::operator[](size_t index) const
{
	// Get the right buffer to access
	const OutputBuffer *itemToIndex = this;
	while (index > itemToIndex->DataLength())
	{
		index -= itemToIndex->DataLength();
		itemToIndex = itemToIndex->Next();
	}

	// Return the char reference
	return itemToIndex->data[index];
}

const char *OutputBuffer::Read(uint16_t len)
{
	size_t offset = dataLength - bytesLeft;
	bytesLeft -= len;
	return data + offset;
}

int OutputBuffer::printf(const char *fmt, ...)
{
	char formatBuffer[FORMAT_STRING_LENGTH];

	va_list vargs;
	va_start(vargs, fmt);
	int ret = vsnprintf(formatBuffer, ARRAY_SIZE(formatBuffer), fmt, vargs);
	va_end(vargs);

	copy(formatBuffer);

	return ret;
}

int OutputBuffer::vprintf(const char *fmt, va_list vargs)
{
	char formatBuffer[FORMAT_STRING_LENGTH];
	int res = vsnprintf(formatBuffer, ARRAY_SIZE(formatBuffer), fmt, vargs);
	cat(formatBuffer);
	return res;
}

int OutputBuffer::catf(const char *fmt, ...)
{
	char formatBuffer[FORMAT_STRING_LENGTH];

	va_list vargs;
	va_start(vargs, fmt);
	int ret = vsnprintf(formatBuffer, ARRAY_SIZE(formatBuffer), fmt, vargs);
	va_end(vargs);

	cat(formatBuffer);

	return ret;
}

size_t OutputBuffer::copy(const char c)
{
	data[0] = c;
	dataLength = bytesLeft = 1;
	return 1;
}

size_t OutputBuffer::copy(const char *src)
{
	return copy(src, strlen(src));
}

size_t OutputBuffer::copy(const char *src, size_t len)
{
	// Unlink other entries before starting the copy process
	OutputBuffer *nextBuffer = next;
	while (nextBuffer != nullptr)
	{
		nextBuffer = reprap.ReleaseOutput(nextBuffer);
	}

	// Does the whole string fit into this instance?
	if (len > OUTPUT_BUFFER_SIZE)
	{
		// No - copy what we can't write into a new chain
		OutputBuffer *currentBuffer, *lastBuffer;
		size_t bytesCopied = OUTPUT_BUFFER_SIZE;
		do {
			if (!reprap.AllocateOutput(currentBuffer))
			{
				// We cannot store the whole string. Should never happen
				break;
			}

			const size_t copyLength = min<size_t>(OUTPUT_BUFFER_SIZE, len - bytesCopied);
			memcpy(currentBuffer->data, src + bytesCopied, copyLength);
			currentBuffer->dataLength = currentBuffer->bytesLeft = copyLength;
			bytesCopied += copyLength;

			if (next == nullptr)
			{
				next = lastBuffer = currentBuffer;
			}
			else
			{
				lastBuffer->next = currentBuffer;
				lastBuffer = currentBuffer;
			}
		} while (bytesCopied < len);

		// Then copy the rest into this instance
		memcpy(data, src, OUTPUT_BUFFER_SIZE);
		dataLength = bytesLeft = OUTPUT_BUFFER_SIZE;
		next = nextBuffer;
		return bytesCopied;
	}

	// Yes - no need to use a new item
	memcpy(data, src, len);
	dataLength = bytesLeft = len;
	return len;
}

size_t OutputBuffer::cat(const char c)
{
	// Get the last entry from the chain
	OutputBuffer *lastBuffer = this;
	while (lastBuffer->next != nullptr)
	{
		lastBuffer = lastBuffer->next;
	}

	// See if we can append a char
	if (lastBuffer->dataLength == OUTPUT_BUFFER_SIZE)
	{
		// No - allocate a new item and link it
		OutputBuffer *nextBuffer;
		if (!reprap.AllocateOutput(nextBuffer))
		{
			// We cannot store any more data. Should never happen
			return 0;
		}
		nextBuffer->copy(c);

		lastBuffer->next = nextBuffer;
	}
	else
	{
		// Yes - we have enough space left
		lastBuffer->data[lastBuffer->dataLength++] = c;
		lastBuffer->bytesLeft++;
	}
	return 1;
}

size_t OutputBuffer::cat(const char *src)
{
	return cat(src, strlen(src));
}

size_t OutputBuffer::cat(const char *src, size_t len)
{
	// Get the last entry from the chain
	OutputBuffer *lastBuffer = this;
	while (lastBuffer->next != nullptr)
	{
		lastBuffer = lastBuffer->next;
	}

	// Do we need to use an extra buffer?
	if (lastBuffer->dataLength + len > OUTPUT_BUFFER_SIZE)
	{
		size_t copyLength = OUTPUT_BUFFER_SIZE - lastBuffer->dataLength;
		size_t bytesCopied = copyLength;
		bytesCopied = copyLength;

		// Yes - copy what we can't write into a new chain
		OutputBuffer *nextBuffer;
		if (!reprap.AllocateOutput(nextBuffer))
		{
			// We cannot store any more data. Should never happen
			return 0;
		}
		bytesCopied += nextBuffer->copy(src + copyLength, len - copyLength);
		lastBuffer->next = nextBuffer;

		// Then copy the rest into this one
		memcpy(lastBuffer->data + lastBuffer->dataLength, src, copyLength);
		lastBuffer->dataLength += copyLength;
		lastBuffer->bytesLeft += copyLength;
		return bytesCopied;
	}

	// No - reuse this one instead
	memcpy(lastBuffer->data + lastBuffer->dataLength, src, len);
	lastBuffer->dataLength += len;
	lastBuffer->bytesLeft += len;
	return len;
}

size_t OutputBuffer::cat(StringRef &str)
{
	return cat(str.Pointer(), str.Length());
}

// Encode a string in JSON format and append it to a string buffer, truncating it if necessary to leave the specified amount of room
void OutputBuffer::EncodeString(const char *src, uint16_t srcLength, bool allowControlChars, bool encapsulateString)
{
	if (encapsulateString)
	{
		cat('"');
	}

	size_t srcPointer = 1;
	char c = *src++;
	while (srcPointer < srcLength && c != 0 && (c >= ' ' || allowControlChars))
	{
		char esc;
		switch (c)
		{
			case '\r':
				esc = 'r';
				break;
			case '\n':
				esc = 'n';
				break;
			case '\t':
				esc = 't';
				break;
			case '"':
				esc = '"';
				break;
			case '\\':
				esc = '\\';
				break;
			default:
				esc = 0;
				break;
		}

		if (esc)
		{
			cat('\\');
			cat(esc);
		}
		else
		{
			cat(c);
		}

		c = *src++;
		srcPointer++;
	}

	if (encapsulateString)
	{
		cat('"');
	}
}

void OutputBuffer::EncodeReply(OutputBuffer *src, bool allowControlChars)
{
	cat('"');

	while (src != nullptr)
	{
		EncodeString(src->Data(), src->DataLength(), allowControlChars, false);
		src = reprap.ReleaseOutput(src);
	}

	cat('"');
}

//*************************************************************************************************
// Utilities and storage not part of any class

static char scratchStringBuffer[FORMAT_STRING_LENGTH];		// this is now used only for short messages and file names
StringRef scratchString(scratchStringBuffer, ARRAY_SIZE(scratchStringBuffer));

void debugPrintf(const char* fmt, ...)
{
	va_list vargs;
	va_start(vargs, fmt);
	reprap.GetPlatform()->MessageF(DEBUG_MESSAGE, fmt, vargs);
	va_end(vargs);
}

// String testing

bool StringEndsWith(const char* string, const char* ending)
{
	int j = strlen(string);
	int k = strlen(ending);
	if(k > j)
	{
		return false;
	}

	return (StringEquals(&string[j - k], ending));
}

bool StringEquals(const char* s1, const char* s2)
{
	int i = 0;
	while(s1[i] && s2[i])
	{
		if(tolower(s1[i]) != tolower(s2[i]))
		{
			return false;
		}
		i++;
	}

	return !(s1[i] || s2[i]);
}

bool StringStartsWith(const char* string, const char* starting)
{
	int j = strlen(string);
	int k = strlen(starting);
	if(k > j)
	{
		return false;
	}

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
			{
				return i;
			}
		}
		else
		{
			count = 0;
		}
	}

	return -1;
}

// vim: ts=4:sw=4
