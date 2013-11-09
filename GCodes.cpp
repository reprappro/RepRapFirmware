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

#include "RepRapFirmware.h"

GCodes::GCodes(Platform* p, Webserver* w)
{
  active = false;
  platform = p;
  webserver = w;
  webGCode = new GCodeBuffer(platform, "web: ");
  fileGCode = new GCodeBuffer(platform, "file: ");
  serialGCode = new GCodeBuffer(platform, "serial: ");
}

void GCodes::Exit()
{
   active = false;
}

void GCodes::Init()
{
  webGCode->Init();
  fileGCode->Init();
  serialGCode->Init();
  webGCode->SetFinished(true);
  fileGCode->SetFinished(true);
  serialGCode->SetFinished(true);
  moveAvailable = false;
 // heatAvailable = false;
  drivesRelative = true;
  axesRelative = false;
  checkEndStops = false;
  gCodeLetters = GCODE_LETTERS;
  distanceScale = 1.0;
  for(int8_t i = 0; i < DRIVES - AXES; i++)
    lastPos[i] = 0.0;
  fileBeingPrinted = NULL;
  fileToPrint = NULL;
  homeX = false;
  homeY = false;
  homeZ = false;
  homeZFinalMove = false;
//  homeXQueued = false;
//  homeYQueued = false;
//  homeZQueued = false;
  dwellWaiting = false;
  stackPointer = 0;
  selectedHead = -1;
  gFeedRate = platform->MaxFeedrate(Z_AXIS); // Typically the slowest
  for(int i = 0; i < NUMBER_OF_PROBE_POINTS; i++)
	  bedZs[i] = 0.0;
  zProbesSet = false;
  probeCount = 0;
  cannedCycleMoveCount = 0;
//  probeMoveQueued = false;
  cannedCycleMoveQueued = false;
  active = true;
  dwellTime = platform->Time();
}

void GCodes::Spin()
{
  if(!active)
    return;
    
  char b;
    
  if(!webGCode->Finished())
  {
    webGCode->SetFinished(ActOnGcode(webGCode));
    return;
  }
  
  if(!serialGCode->Finished())
  {
    serialGCode->SetFinished(ActOnGcode(serialGCode));
    return;
  }  

  if(!fileGCode->Finished())
  {
    fileGCode->SetFinished(ActOnGcode(fileGCode));
    return;
  }  

  if(webserver->GCodeAvailable())
  {
    if(webGCode->Put(webserver->ReadGCode()))
      webGCode->SetFinished(ActOnGcode(webGCode));
    return;
  }
  
  if(platform->GetLine()->Status() & byteAvailable)
  {
	platform->GetLine()->Read(b);
    if(serialGCode->Put(b))
      serialGCode->SetFinished(ActOnGcode(serialGCode));
    return;
  }  
  
  if(fileBeingPrinted != NULL)
  {
     if(fileBeingPrinted->Read(b))
     {
       if(fileGCode->Put(b))
         fileGCode->SetFinished(ActOnGcode(fileGCode));
     } else
     {
        if(fileGCode->Put('\n')) // In case there wasn't one ending the file
         fileGCode->SetFinished(ActOnGcode(fileGCode));
        fileBeingPrinted->Close();
        fileBeingPrinted = NULL;
     }
  }
}

void GCodes::Diagnostics() 
{
  platform->Message(HOST_MESSAGE, "GCodes Diagnostics:\n");
}

bool GCodes::AllMovesAreFinishedAndMoveBufferIsLoaded()
{
  // Last one gone?
  
  if(moveAvailable)
    return false;
  
  // Wait for all the queued moves to stop so we get the actual last position and feedrate
      
  if(!reprap.GetMove()->AllMovesAreFinished())
    return false;
  reprap.GetMove()->ResumeMoving();
    
  // Load the last position; If Move can't accept more, return false - should never happen
  
  if(!reprap.GetMove()->GetCurrentState(moveBuffer))
    return false;
  
  return true;  
}

bool GCodes::Push()
{
  if(stackPointer >= STACK)
  {
    platform->Message(HOST_MESSAGE, "Push(): stack overflow!\n");
    return true;
  }
  
  if(!AllMovesAreFinishedAndMoveBufferIsLoaded())
    return false;
  
  drivesRelativeStack[stackPointer] = drivesRelative;
  axesRelativeStack[stackPointer] = axesRelative;
  feedrateStack[stackPointer] = gFeedRate; 
  stackPointer++;
  
  return true;
}

bool GCodes::Pop()
{
  if(stackPointer <= 0)
  {
    platform->Message(HOST_MESSAGE, "Push(): stack underflow!\n");
    return true;  
  }
  
  if(!AllMovesAreFinishedAndMoveBufferIsLoaded())
    return false;
    
  stackPointer--;
  drivesRelative = drivesRelativeStack[stackPointer];
  axesRelative = axesRelativeStack[stackPointer];
  
  // Remember for next time if we have just been switched
  // to absolute drive moves
  
  for(int8_t i = AXES; i < DRIVES; i++)
    lastPos[i - AXES] = moveBuffer[i];
  
  // Do a null move to set the correct feedrate
  
  gFeedRate = feedrateStack[stackPointer];
  moveBuffer[DRIVES] = gFeedRate;
  
  checkEndStops = false;
  moveAvailable = true;
  return true;
}

// If the Move class can't receive the move (i.e. things have to wait)
// this returns false, otherwise true.

bool GCodes::SetUpMove(GCodeBuffer *gb)
{
  // Last one gone yet?
  
  if(moveAvailable)
    return false;
    
  // Load the last position; If Move can't accept more, return false
  
  if(!reprap.GetMove()->GetCurrentState(moveBuffer))
    return false;
  
  LoadMoveBufferFromGCode(gb);
  
  checkEndStops = false;
  moveAvailable = true;
  return true; 
}

// The Move class calls this function to find what to do next.

bool GCodes::ReadMove(float* m, bool& ce)
{
    if(!moveAvailable)
      return false; 
    for(int8_t i = 0; i <= DRIVES; i++) // 1 more for F
      m[i] = moveBuffer[i];
    ce = checkEndStops;
    moveAvailable = false;
    checkEndStops = false;
    return true;
}


bool GCodes::ReadHeat(float* h)
{

}

// To execute any move, call this until it returns true.
// false entries in action[] will be ignored.

bool GCodes::DoCannedCycleMove(float moveToDo[], bool action[], bool ce)
{
	if(cannedCycleMoveQueued)
	{
		if(!Pop()) // Wait for the move to finish
			return false;
		cannedCycleMoveQueued = false;
		return true;
	} else
	{
		if(!Push()) // Wait for the RepRap to finish whatever it was doing
			return false;
		for(int8_t drive = 0; drive <= DRIVES; drive++)
		{
			if(action[drive])
				moveBuffer[drive] = moveToDo[drive];
		}
		checkEndStops = ce;
		cannedCycleMoveQueued = true;
		moveAvailable = true;
	}
	return false;
}

bool GCodes::DoHome()
{
	// Treat more or less like any other move
	// Do one axis at a time, starting with X.

	float moveToDo[DRIVES+1];
	bool action[DRIVES+1];
	for(int8_t drive = 0; drive < DRIVES; drive++)
		action[drive] = false;
	action[DRIVES] = true;

	if(homeX)
	{
		action[X_AXIS] = true;
		moveToDo[X_AXIS] = platform->HomeDirection(X_AXIS)*2.0*platform->AxisLength(X_AXIS);
		moveToDo[DRIVES] = platform->HomeFeedRate(X_AXIS);
		if(DoCannedCycleMove(moveToDo, action, true))
		{
			homeX = false;
			return NoHome();
		}
		return false;
	}

	if(homeY)
	{
		action[Y_AXIS] = true;
		moveToDo[Y_AXIS] = platform->HomeDirection(Y_AXIS)*2.0*platform->AxisLength(Y_AXIS);
		moveToDo[DRIVES] = platform->HomeFeedRate(Y_AXIS);
		if(DoCannedCycleMove(moveToDo, action, true))
		{
			homeY = false;
			return NoHome();
		}
		return false;
	}

	if(homeZ)
	{
		action[Z_AXIS] = true;
		moveToDo[DRIVES] = platform->HomeFeedRate(Z_AXIS);
		if(platform->IsZProbeEnabled() && homeZFinalMove)
		{
			moveToDo[Z_AXIS] = 0.0;
			if(DoCannedCycleMove(moveToDo, action, false))
			{
				homeZFinalMove = false;
				homeZ = false;
				return NoHome();
			}
			return false;
		}else
		{
			moveToDo[Z_AXIS] = platform->HomeDirection(Z_AXIS)*2.0*platform->AxisLength(Z_AXIS);
			if(DoCannedCycleMove(moveToDo, action, true))
				homeZFinalMove = true;
			if(!platform->IsZProbeEnabled())
			{
				homeZ = false;
				return NoHome();
			}
			return false;
		}
	}

	// Should never get here

	checkEndStops = false;
	moveAvailable = false;

	return true;
}

// This lifts Z a bit, moves to the probe XY coordinates (obtained by a call to GetProbeCoordinates() ),
// probes the bed height, and records the Z coordinate probed.  If you want to program any general
// internal canned cycle, this shows how to do it.

bool GCodes::DoSingleZProbe()
{
	float x, y, z;

	reprap.GetMove()->SetIdentityTransform();  // It doesn't matter if these are called repeatedly

	float moveToDo[DRIVES+1];
	bool action[DRIVES+1];
	for(int8_t drive = 0; drive <= DRIVES; drive++)
		action[drive] = false;

	switch(cannedCycleMoveCount)
	{
	case 0:
		moveToDo[Z_AXIS] = Z_DIVE;
		action[Z_AXIS] = true;
		moveToDo[DRIVES] = platform->HomeFeedRate(Z_AXIS);
		action[DRIVES] = true;
		reprap.GetMove()->SetZProbing(false);
		if(DoCannedCycleMove(moveToDo, action, false))
			cannedCycleMoveCount++;
		return false;

	case 1:
		GetProbeCoordinates(probeCount, moveToDo[X_AXIS], moveToDo[Y_AXIS], moveToDo[Z_AXIS]);
		action[X_AXIS] = true;
		action[Y_AXIS] = true;
		// NB - we don't use the Z value
		moveToDo[DRIVES] = platform->HomeFeedRate(X_AXIS);
		action[DRIVES] = true;
		reprap.GetMove()->SetZProbing(false);
		if(DoCannedCycleMove(moveToDo, action, false))
			cannedCycleMoveCount++;
		return false;

	case 2:
		moveToDo[Z_AXIS] = -2.0*platform->AxisLength(Z_AXIS);
		action[Z_AXIS] = true;
		moveToDo[DRIVES] = platform->HomeFeedRate(Z_AXIS);
		action[DRIVES] = true;
		reprap.GetMove()->SetZProbing(true);
		if(DoCannedCycleMove(moveToDo, action, true))
		{
//			platform->GetLine()->Write(platform->ZProbe());
			cannedCycleMoveCount++;
		}
		return false;

	default:
		cannedCycleMoveCount = 0;
		bedZs[probeCount] = reprap.GetMove()->GetLastProbedZ();
		return true;
	}
}

bool GCodes::DoMultipleZProbe()
{
	if(DoSingleZProbe())
		probeCount++;
	if(probeCount >= NUMBER_OF_PROBE_POINTS)
	{
		probeCount = 0;
		zProbesSet = true;
		reprap.GetMove()->SetZProbing(false);
		reprap.GetMove()->SetProbedBedPlane();
		return true;
	}
	return false;
}

bool GCodes::GetProbeCoordinates(int count, float& x, float& y, float& z)
{
	switch(count)
	{
	case 0:
		x = 0.2*platform->AxisLength(X_AXIS);
		y = 0.2*platform->AxisLength(Y_AXIS);
		break;
	case 1:
		x = 0.8*platform->AxisLength(X_AXIS);
		y = 0.2*platform->AxisLength(Y_AXIS);
		break;
	case 2:
		x = 0.5*platform->AxisLength(X_AXIS);
		y = 0.8*platform->AxisLength(Y_AXIS);
		break;
	default:
		platform->Message(HOST_MESSAGE, "probeCount beyond maximum requested.\n");
		break;
	}
	z = bedZs[count];
	return zProbesSet;
}

// FIXME - needs to deal with multiple extruders

char* GCodes::GetCurrentCoordinates()
{
	float liveCoordinates[DRIVES+1];
	reprap.GetMove()->LiveCoordinates(liveCoordinates);

	sprintf(scratchString, "X:%f Y:%f Z:%f E:%f", liveCoordinates[X_AXIS], liveCoordinates[Y_AXIS], liveCoordinates[Z_AXIS], liveCoordinates[AXES]);
	return scratchString;
}


void GCodes::QueueFileToPrint(char* fileName)
{
  fileToPrint = platform->GetFileStore(platform->GetGCodeDir(), fileName, false);
  if(fileToPrint == NULL)
	  platform->Message(HOST_MESSAGE, "GCode file not found\n");
}

void GCodes::RunConfigurationGCodes()
{
	  fileToPrint = platform->GetFileStore(platform->GetSysDir(), platform->GetConfigFile(), false);
	  if(fileToPrint == NULL)
	  {
		  platform->Message(HOST_MESSAGE, "Configuration file not found\n");
		  return;
	  }
      fileBeingPrinted = fileToPrint;
      fileToPrint = NULL;
}


// Function to handle dwell delays.  Return true for
// Dwell finished, false otherwise.

bool GCodes::DoDwell(GCodeBuffer *gb)
{
  unsigned long dwell;
  
  if(gb->Seen('P'))
    dwell = 0.001*(float)gb->GetLValue(); // P values are in milliseconds; we need seconds
  else
    return true;  // No time given - throw it away
      
  // Wait for all the queued moves to stop
      
  if(!reprap.GetMove()->AllMovesAreFinished())
    return false;
      
  // Are we already in a dwell?
      
  if(dwellWaiting)
  {
    if(platform->Time() - dwellTime >= 0.0)
    {
      dwellWaiting = false;
      reprap.GetMove()->ResumeMoving();
      return true;
    }
    return false;
  }
      
  // New dwell - set it up
      
  dwellWaiting = true;
  dwellTime = platform->Time() + dwell;
  return false;
}

bool GCodes::SetOffsets(GCodeBuffer *gb)
{
  int8_t head;
  if(gb->Seen('P'))
  {
    head = gb->GetIValue() + 1;  // 0 is the Bed
    if(gb->Seen('R'))
      reprap.GetHeat()->SetStandbyTemperature(head, gb->GetFValue());
      
    if(gb->Seen('S'))
      reprap.GetHeat()->SetActiveTemperature(head, gb->GetFValue());
    // FIXME - do X, Y and Z
  }
  return true;  
}


// Move expects all axis movements to be absolute, and all
// extruder drive moves to be relative.  This function serves that.

void GCodes::LoadMoveBufferFromGCode(GCodeBuffer *gb)
{
	float absE;

	for(uint8_t i = 0; i < DRIVES; i++)
	{
		//X,Y or Z
	    if(i < AXES)
	    {
	      if(gb->Seen(gCodeLetters[i]))
	      {
	        if(axesRelative)
	          moveBuffer[i] += gb->GetFValue()*distanceScale;
	        else
	          moveBuffer[i] = gb->GetFValue()*distanceScale;
	      }
	    } else //extruder 0,1,2 etc
	    {
	      if(gb->Seen(gCodeLetters[i]))
	      {
	        if(drivesRelative)
	          moveBuffer[i] = gb->GetFValue()*distanceScale;
	        else
	        {
	          absE = gb->GetFValue()*distanceScale;
	          moveBuffer[i] = absE - lastPos[i - AXES];
	          lastPos[i - AXES] = absE;
	        }
	      }
	    }
	}

	// Deal with feedrate

	if(gb->Seen(gCodeLetters[DRIVES]))
	  gFeedRate = gb->GetFValue()*distanceScale*0.016666667; // G Code feedrates are in mm/minute; we need mm/sec

	moveBuffer[DRIVES] = gFeedRate;  // We always set it, as Move may have modified the last one.
}

bool GCodes::SetPositions(GCodeBuffer *gb)
{
	if(!AllMovesAreFinishedAndMoveBufferIsLoaded())
		return false;

	LoadMoveBufferFromGCode(gb);

	reprap.GetMove()->SetPositions(moveBuffer);

	return true;
}

void GCodes::DisableDrives()
{
	for(int8_t drive = 0; drive < DRIVES; drive++)
		platform->Disable(drive);
}

void GCodes::StandbyHeaters()
{
	for(int8_t heater = 0; heater < DRIVES; heater++)
		reprap.GetHeat()->Standby(heater);
}

// If the GCode to act on is completed, this returns true,
// otherwise false.  It is called repeatedly for a given
// GCode until it returns true for that code.

bool GCodes::ActOnGcode(GCodeBuffer *gb)
{
  int code;
  float value;
  char* str;
  bool result = true;

  if(gb->Seen('G'))
  {
    code = gb->GetIValue();
    switch(code)
    {
    case 0: // There are no rapid moves...
    case 1: // Ordinary move
      result = SetUpMove(gb);
      break;
      
    case 4: // Dwell
      result = DoDwell(gb);
      break;
      
    case 10: // Set offsets
      result = SetOffsets(gb);
      break;
    
    case 20: // Inches (which century are we living in, here?)
      distanceScale = INCH_TO_MM;
      break;
    
    case 21: // mm
      distanceScale = 1.0;
      break;
    
    case 28: // Home
      if(NoHome())
      {
        homeX = gb->Seen(gCodeLetters[X_AXIS]);
        homeY = gb->Seen(gCodeLetters[Y_AXIS]);
        homeZ = gb->Seen(gCodeLetters[Z_AXIS]);
        if(NoHome())
        {
          homeX = true;
          homeY = true;
          homeZ = true;
        }
      }
      result = DoHome();
      break;

    case 31:
      if(!platform->IsZProbeEnabled())
      {
        platform->Message(HOST_MESSAGE, "Z Probe not supported\n");
        break;
      }
    	if(gb->Seen(gCodeLetters[Z_AXIS]))
    	{
    		platform->SetZProbeStopHeight(gb->GetFValue());
    		if(gb->Seen('P'))
    		{
    			platform->SetZProbe(gb->GetIValue());
    		}
    	} else
    	{
    		platform->GetLine()->Write(platform->ZProbe());
    		platform->GetLine()->Write("\n");
    	}
    	break;

    case 32: // Probe Z at multiple positions and generate the bed transform
      if(!platform->IsZProbeEnabled())
      {
        platform->Message(HOST_MESSAGE, "Z Probe not supported\n");
        break;
      }
    	result = DoMultipleZProbe();
    	break;

    case 90: // Absolute coordinates
      drivesRelative = false;
      axesRelative = false;
      break;
      
    case 91: // Relative coordinates
      drivesRelative = true;
      axesRelative = true;
      break;
      
    case 92: // Set position
      result = SetPositions(gb);
      break;
      
    default:
      platform->Message(HOST_MESSAGE, "GCodes - invalid G Code: ");
      platform->Message(HOST_MESSAGE, gb->Buffer());
      platform->Message(HOST_MESSAGE, "\n");
    }
    return result;
  }
  
  if(gb->Seen('M'))
  {
    code = gb->GetIValue();
    switch(code)
    {
    case 0: // Stop
    case 1: // Sleep
      if(!AllMovesAreFinishedAndMoveBufferIsLoaded())
    	  return false;
      DisableDrives();
      StandbyHeaters();
      break;
    
    case 18: // Motors off
      if(!AllMovesAreFinishedAndMoveBufferIsLoaded())
    	   return false;
      DisableDrives();
      break;
      
    case 20:  // Deprecated...
      platform->GetLine()->Write("GCode files:\n");
      platform->GetLine()->Write(platform->GetMassStorage()->FileList(platform->GetGCodeDir()));
      platform->GetLine()->Write("\n");
      break;

    case 23: // Set file to print
      QueueFileToPrint(gb->GetString());
      break;
      
    case 24: // Print/resume-printing the selected file
      fileBeingPrinted = fileToPrint;
      fileToPrint = NULL;
      break;
      
    case 25: // Pause the print
    	fileToPrint = fileBeingPrinted;
    	fileBeingPrinted = NULL;
    	break;

    case 31: //toggle the Z probe enable, M31 with nothing reports the status, M31 S0 = disable, M31 S1 = enable
    	if(gb->Seen('S'))
    		platform->EnableZProbe((bool)gb->GetIValue());
    	if(platform->IsZProbeEnabled())
    		platform->Message(HOST_MESSAGE, "Z Probe enabled \n");
    	else
    		platform->Message(HOST_MESSAGE, "Z Probe disabled \n");
    	break;
    case 82:
    	if(drivesRelative)
    		for(uint8_t i = AXES; i < DRIVES; i++)
    		    lastPos[i - AXES] = 0.0;
    	drivesRelative = false;
    	break;

    case 83:
    	if(!drivesRelative)
    		for(uint8_t i = AXES; i < DRIVES; i++)
    			lastPos[i - AXES] = 0.0;
    	drivesRelative = true;

    	break;

    case 92: // Set steps/mm for each axis
    	if(reprap.debug())
    		platform->GetLine()->Write("Steps/mm: ");
    	for(uint8_t i = 0; i < DRIVES; i++)
    	{
    		if(gb->Seen(gCodeLetters[i]))
    		{
    			value = gb->GetFValue();
    		}else{
    			value = -1;
    		}
    		platform->SetDriveStepsPerUnit(i, value);
    		if(reprap.debug())
    		{
    			platform->GetLine()->Write(gCodeLetters[i]);
    			platform->GetLine()->Write(ftoa(NULL, value, 3));
    			platform->GetLine()->Write(" ");
    		}
    	}
		if(reprap.debug())
			platform->GetLine()->Write("\n");
        break;

    case 104: // Deprecated, optional P to set extruder other wise uses selected head
    	if(gb->Seen('P'))
			{
				value = gb->GetIValue()+1;// 0 = bed
				if(value >= HEATERS) //not a valid heater
				{
					platform->Message(HOST_MESSAGE, "Invalid P number: ");
					platform->Message(HOST_MESSAGE, gb->Buffer());
					platform->Message(HOST_MESSAGE, "\n");
					break;
				}
			}
    	else
    		value = selectedHead >=0 ? selectedHead + 1 : 1 ;
    	if(gb->Seen('S'))
    	{
    	      reprap.GetHeat()->SetActiveTemperature(value, gb->GetFValue());
    		  reprap.GetHeat()->Activate(value);
    	}
    	break;

    case 105: // Deprecated...
    	platform->GetLine()->Write("ok T:");
    	for(int8_t i = HEATERS - 1; i > 0; i--)
    	{
    		platform->GetLine()->Write(ftoa(0, reprap.GetHeat()->GetTemperature(i), 1));
    		platform->GetLine()->Write(" ");
    	}
    	platform->GetLine()->Write("B:");
    	platform->GetLine()->Write(ftoa(0, reprap.GetHeat()->GetTemperature(0), 1));
    	platform->GetLine()->Write('\n');
    	break;
   
    case 106: // Fan on
      platform->Message(HOST_MESSAGE, "Fan on received\n");
      break;
    
    case 107: // Fan off
      platform->Message(HOST_MESSAGE, "Fan off received\n");
      break;

    case 109: //depreciated but useful if you don't want to use M116
    	if(gb->Seen('P'))
    	{
    		value = gb->GetIValue()+1;// 0 = bed
			if(value >= HEATERS) //not a valid heater
			{
			    platform->Message(HOST_MESSAGE, "Invalid P number: ");
			    platform->Message(HOST_MESSAGE, gb->Buffer());
			    platform->Message(HOST_MESSAGE, "\n");
				break;
			}
    	}
    	else
    		value = selectedHead >=0 ? selectedHead + 1 : 1 ;
    	if(gb->Seen('S'))
    	{
    		reprap.GetHeat()->SetActiveTemperature(value, gb->GetFValue());
    		reprap.GetHeat()->Activate(value);
    	}
    	result = reprap.GetHeat()->HeaterAtSetTemperature(value);
    	break;
    
    case 116: // Wait for everything
      if(!AllMovesAreFinishedAndMoveBufferIsLoaded())
    	    return false;
      result = reprap.GetHeat()->AllHeatersAtSetTemperatures();
      break;
      
    case 111: // Debug level
    	if(gb->Seen('S'))
    		reprap.debug(gb->GetIValue());
    	break;

    case 114: // Deprecated
    	str = GetCurrentCoordinates();
    	if(str != 0)
    	{
    		platform->GetLine()->Write(str);
    		platform->GetLine()->Write("\n");
    	} else
    		result = false;
    	break;

    case 120:
    	result = Push();
    	break;
      
    case 121:
      result = Pop();
      break;
    
    case 122:
      reprap.Diagnostics();
      break;
      
    case 126: // Valve open
      platform->Message(HOST_MESSAGE, "M126 - valves not yet implemented\n");
      break;
      
    case 127: // Valve closed
      platform->Message(HOST_MESSAGE, "M127 - valves not yet implemented\n");
      break;
      
    case 140: // Set bed temperature
      if(gb->Seen('S'))
      {
        reprap.GetHeat()->SetActiveTemperature(0, gb->GetFValue());
        reprap.GetHeat()->Activate(0);
      }
      break;
    
    case 141: // Chamber temperature
      platform->Message(HOST_MESSAGE, "M141 - heated chamber not yet implemented\n");
      break;

    case 190: //depreciated but useful if you dont want to use M116
    	if(gb->Seen('S'))
    	{
    		reprap.GetHeat()->SetActiveTemperature(0, gb->GetFValue());
    		reprap.GetHeat()->Activate(0);
    	}
    	result = reprap.GetHeat()->HeaterAtSetTemperature(0);
    	break;

    case 201: // Set axis accelerations
		if(reprap.debug())
			platform->GetLine()->Write("Accelerations: ");
    	for(uint8_t i = 0; i < DRIVES; i++)
    	{
    		if(gb->Seen(gCodeLetters[i]))
    		{
    			value = gb->GetFValue();
    		}else{
    			value = -1;
    		}
    		float accel = platform->Acceleration(i, value);
    		if(reprap.debug())
    		{
    			platform->GetLine()->Write(gCodeLetters[i]);
    			platform->GetLine()->Write(ftoa(NULL,accel,1));
    			platform->GetLine()->Write(" ");
    		}
    	}
		if(reprap.debug())
			platform->GetLine()->Write("\n");
    	break;

    case 906: // Motor currents
    	for(uint8_t i = 0; i < DRIVES; i++)
    	{
    		if(gb->Seen(gCodeLetters[i]))
    		{
    			value = gb->GetFValue();
    			platform->SetMotorCurrent(i, value);
    		}
    	}
    	break;
     
    default:
      platform->Message(HOST_MESSAGE, "GCodes - invalid M Code: ");
      platform->Message(HOST_MESSAGE, gb->Buffer());
      platform->Message(HOST_MESSAGE, "\n");
    }
    return result;
  }
  
  if(gb->Seen('T'))
  {
    code = gb->GetIValue();
    //check to see if the tool is being changed
    if(code == selectedHead)
      return result;
      
    bool ok = false;
    //set the old tool temperature to standby
    for(int8_t i = AXES; i < DRIVES; i++)
    {
      if(selectedHead == i - AXES)
        reprap.GetHeat()->Standby(selectedHead + 1); // 0 is the Bed
    }
    //set the new tool temperature to active
    for(int8_t i = AXES; i < DRIVES; i++)
    {    
      if(code == i - AXES)
      {
        selectedHead = code;
        reprap.GetHeat()->Activate(selectedHead + 1); // 0 is the Bed
        ok = true;
      }
    }

    if(!ok)
    {
      platform->Message(HOST_MESSAGE, "GCodes - invalid T Code: ");
      platform->Message(HOST_MESSAGE, gb->Buffer());
      platform->Message(HOST_MESSAGE, "\n");
    }
    
    return result;
  }
  
  // An empty buffer jumps to here and gets discarded
  
  return result;
}



//*************************************************************************************

GCodeBuffer::GCodeBuffer(Platform* p, char* id)
{ 
  platform = p;
  identity = id; 
}

void GCodeBuffer::Init()
{
  gcodePointer = 0;
  readPointer = -1;
  inComment = false;   
}

bool GCodeBuffer::Put(char c)
{
  bool result = false;
  gcodeBuffer[gcodePointer] = c;
  
  if(c == ';')
    inComment = true;
    
  if(c == '\n' || !c)
  {
    gcodeBuffer[gcodePointer] = 0;
    Init();
    if(reprap.debug() && gcodeBuffer[0]) // Don't bother with blank/comment lines
    {
      platform->Message(HOST_MESSAGE, identity);
      platform->Message(HOST_MESSAGE, gcodeBuffer);
      platform->Message(HOST_MESSAGE, "\n"); 
    }
    result = true;
  } else
  {
    if(!inComment)
      gcodePointer++;
  }
  
  if(gcodePointer >= GCODE_LENGTH)
  {
    platform->Message(HOST_MESSAGE, "G Code buffer length overflow.\n");
    gcodePointer = 0;
    gcodeBuffer[0] = 0;
  }
  
  return result;
}   

// Is 'c' in the G Code string?
// Leave the pointer there for a subsequent read.

bool GCodeBuffer::Seen(char c)
{
  readPointer = 0;
  while(gcodeBuffer[readPointer])
  {
    if(gcodeBuffer[readPointer] == c)
      return true;
    readPointer++;
  }
  readPointer = -1;
  return false;
}

// Get a float after a G Code letter

float GCodeBuffer::GetFValue()
{
  if(readPointer < 0)
  {
     platform->Message(HOST_MESSAGE, "GCodes: Attempt to read a GCode float before a search.\n");
     return 0.0;
  }
  float result = (float)strtod(&gcodeBuffer[readPointer + 1], 0);
  readPointer = -1;
  return result; 
}

// This returns a pointer to the end of the buffer where a
// string starts.  It assumes that an M or G search has
// been done followed by a GetIValue(), so readPointer will
// be -1.  It absorbs "M/Gnnn " (including the space) from the
// start and returns a pointer to the next location.

char* GCodeBuffer::GetString()
{
  readPointer = 0;
  while(gcodeBuffer[readPointer] && gcodeBuffer[readPointer] != ' ')
	  readPointer++;

  if(!gcodeBuffer[readPointer])
  {
     platform->Message(HOST_MESSAGE, "GCodes: String expected but not seen.\n");
     return gcodeBuffer; // Good idea?
  }

  char* result = &gcodeBuffer[readPointer+1];
  readPointer = -1;
  return result;
}


// Get an long after a G Code letter

long GCodeBuffer::GetLValue()
{
  if(readPointer < 0)
  {
    platform->Message(HOST_MESSAGE, "GCodes: Attempt to read a GCode int before a search.\n");
    return 0;
  }
  long result = strtol(&gcodeBuffer[readPointer + 1], 0, 0);
  readPointer = -1;
  return result;  
}




