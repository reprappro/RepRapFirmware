/****************************************************************************************************

 RepRapFirmware - G Codes

 This class interprets G Codes from one or more sources, and calls the functions in Move, Heat etc
 that drive the machine to do what the G Codes command.

 Most of the functions in here are designed not to wait, and they return a boolean.  When you want them to do
 something, you call them.  If they return false, the machine can't do what you want yet.  So you go away
 and do something else.  Then you try again.  If they return true, the thing you wanted done has been done.

 -----------------------------------------------------------------------------------------------------

 Version 0.1

 13 February 2013

 Adrian Bowyer
 RepRap Professional Ltd
 http://reprappro.com

 Licence: GPL

 ****************************************************************************************************/

#include "RepRapFirmware.h"

const float minutesToSeconds = 60.0;
const float secondsToMinutes = 1.0 / minutesToSeconds;

GCodes::GCodes(Platform* p, Webserver* w)
{
	active = false;
	platform = p;
	webserver = w;
	webGCode = new GCodeBuffer(platform, "web: ");
	fileGCode = new GCodeBuffer(platform, "file: ");
	serialGCode = new GCodeBuffer(platform, "serial: ");
	auxGCode = new GCodeBuffer(platform, "aux: ");
	fileMacroGCode = new GCodeBuffer(platform, "macro: ");
	queuedGCode = new GCodeBuffer(platform, "queued: ");
}

void GCodes::Exit()
{
	platform->Message(BOTH_MESSAGE, "GCodes class exited.\n");
	active = false;
}

void GCodes::Init()
{
	Reset();
	drivesRelative = true;
	axesRelative = false;
	ARRAY_INIT(axisLetters, AXIS_LETTERS);
	distanceScale = 1.0;
	for (int8_t extruder = 0; extruder < DRIVES - AXES; extruder++)
	{
		lastExtruderPosition[extruder] = 0.0;
	}
	configFile = NULL;
	eofString = EOF_STRING;
	eofStringCounter = 0;
	eofStringLength = strlen(eofString);
	homing = false;
	homeX = false;
	homeY = false;
	homeZ = false;
	offSetSet = false;
	zProbesSet = false;
	active = true;
	longWait = platform->Time();
	dwellTime = longWait;
	limitAxes = true;
	axisIsHomed[X_AXIS] = axisIsHomed[Y_AXIS] = axisIsHomed[Z_AXIS] = false;
	toolChangeSequence = 0;
	coolingInverted = false;
	lastFanValue = 0.0;
	internalCodeQueue = NULL;
	releasedQueueItems = NULL;
	for(uint8_t i=0; i<codeQueueLength; i++)
	{
		releasedQueueItems = new CodeQueueItem(releasedQueueItems);
	}
}

// This is called from Init and when doing an emergency stop
void GCodes::Reset()
{
	webGCode->Init();
	fileGCode->Init();
	serialGCode->Init();
	auxGCode->Init();
	fileMacroGCode->Init();
	queuedGCode->Init();
	moveAvailable = false;
	totalMoves = 0;
	movesCompleted = 0;
	fileBeingPrinted.Close();
	fileToPrint.Close();
	fileBeingWritten = NULL;
	endStopsToCheck = 0;
	doingFileMacro = returningFromMacro = false;
	doPauseMacro = isPausing = isResuming = false;
	fractionOfFilePrinted = -1.0;
	dwellWaiting = false;
	stackPointer = 0;
	waitingForMoveToComplete = false;
	probeCount = 0;
	cannedCycleMoveCount = 0;
	cannedCycleMoveQueued = false;
	auxDetected = false;
}

void GCodes::Spin()
{
	if (!active)
		return;

	// Macro files are the most important. We must finish them in one go before we proceed
	// with the other G-Codes, because at least one of them will call DoFileMacro() again.

	if (doingFileMacro && !returningFromMacro)
	{
		if (fileMacroGCode->Active())
		{
			// We must NOT enqueue macro G-Codes, because the code queue doesn't work for these
			fileMacroGCode->SetFinished(ActOnCode(fileMacroGCode, true));
		}
		else
		{
			// Process more of the macro file
			size_t i = 0;
			do
			{
				char b;
				if (fileBeingPrinted.Read(b))
				{
					if (fileMacroGCode->Put(b))
					{
						fileMacroGCode->SetFinished(ActOnCode(fileMacroGCode, true));
						break;
					}
				}
				else
				{
					if (!fileMacroGCode->IsEmpty())
					{
						if (fileMacroGCode->Put('\n')) // In case there wasn't one ending the file
						{
							fileMacroGCode->SetFinished(ActOnCode(fileMacroGCode, true));
							break;
						}
					}

					if (!fileMacroGCode->Active())
					{
						fileBeingPrinted.Close();
						returningFromMacro = true;
					}

					break;
				}
				++i;
			} while (i < GCODE_LENGTH);
		}

		platform->ClassReport(longWait);
		return;
	}

	// Check each of the sources of G Codes (web, aux, serial, queued and file) to
	// see if they are finished in order to feed them new codes.
	//
	// Note the order establishes a priority: web, aux, serial, queued, file.
	// If file weren't last, then the others would never get a look in when
	// a file was being printed.

	if (!webGCode->Active() && webserver->GCodeAvailable())
	{
		uint8_t i = 0;
		do
		{
			char b = webserver->ReadGCode();
			if (webGCode->Put(b))
			{
				// we have a complete gcode
				if (webGCode->WritingFileDirectory() != NULL)
				{
					WriteGCodeToFile(webGCode);
					webGCode->SetFinished(true);
				}
				else
				{
					webGCode->SetFinished(ActOnCode(webGCode, true));
				}
				break;	// stop after receiving a complete gcode in case we haven't finished processing it
			}
			++i;
		} while (i < GCODE_LENGTH && webserver->GCodeAvailable());
		platform->ClassReport(longWait);
		return;
	}

	// Now the serial interfaces.

	if (!auxGCode->Active() && (platform->GetAux()->Status() & byteAvailable))
	{
		uint8_t i = 0;
		do
		{
			char b;
			platform->GetAux()->Read(b);
			if (auxGCode->Put(b))	// add char to buffer and test whether the gcode is complete
			{
				auxDetected = true;
				auxGCode->SetFinished(ActOnCode(auxGCode, true));
				break;	// stop after receiving a complete gcode in case we haven't finished processing it
			}
			++i;
		} while (i < GCODE_LENGTH && (platform->GetAux()->Status() & byteAvailable));
		platform->ClassReport(longWait);
		return;
	}

	if (platform->GetLine()->Status() & byteAvailable)
	{
		// First check the special case of uploading the reprap.htm file
		if (serialGCode->WritingFileDirectory() == platform->GetWebDir())
		{
			char b;
			platform->GetLine()->Read(b);
			WriteHTMLToFile(b, serialGCode);

			platform->ClassReport(longWait);
			return;
		}
		// Otherwise just deal in general with incoming bytes from the serial interface
		else if (!serialGCode->Active())
		{
			// Read several bytes instead of just one. This approximately doubles the speed of file uploading.
			uint8_t i = 0;
			do
			{
				char b;
				platform->GetLine()->Read(b);
				if (serialGCode->Put(b))	// add char to buffer and test whether the gcode is complete
				{
					// we have a complete gcode
					if (serialGCode->WritingFileDirectory() != NULL)
					{
						WriteGCodeToFile(serialGCode);
						serialGCode->SetFinished(true);
					}
					else
					{
						serialGCode->SetFinished(ActOnCode(serialGCode, reprap.GetMove()->IsPaused()));
					}
					break;	// stop after receiving a complete gcode in case we haven't finished processing it
				}
				++i;
			} while (i < GCODE_LENGTH && (platform->GetLine()->Status() & byteAvailable));

			platform->ClassReport(longWait);
			return;
		}
	}

	// Then check if there are any queued codes left to be executed in-time

	if (internalCodeQueue != NULL)
	{
		if (!queuedGCode->Active() && reprap.GetMove()->IsRunning())
		{
			// Check if the last queued code is complete and remove its entry
			if (internalCodeQueue->IsExecuting())
			{
				CodeQueueItem *temp = internalCodeQueue;
				internalCodeQueue = internalCodeQueue->Next();
				temp->SetNext(releasedQueueItems);
				releasedQueueItems = temp;

				platform->ClassReport(longWait);
				return;
			}

			// Check if a new code can be executed
			else if (internalCodeQueue->ExecuteAtMove() <= movesCompleted)
			{
				internalCodeQueue->Execute();
				if (queuedGCode->Put(internalCodeQueue->GetCommand(), internalCodeQueue->GetCommandLength()))
				{
					queuedGCode->SetFinished(ActOnCode(queuedGCode, true));
				}

				platform->ClassReport(longWait);
				return;
			}
		}
	}
	else if (totalMoves == movesCompleted != 0)
	{
		// If we don't have any queued codes left and all moves are complete, we can safely reset our counters here
		totalMoves = 0;
		movesCompleted = 0;
	}

	// At last, see if we can read some more bytes from the the file being printed

	if (!fileGCode->Active() && reprap.GetMove()->IsRunning() && fileBeingPrinted.IsLive())
	{
		uint8_t i = 0;
		do
		{
			char b;
			if (fileBeingPrinted.Read(b))
			{
				if (fileGCode->Put(b))
				{
					fileGCode->SetFinished(ActOnCode(fileGCode));
					break;
				}
			}
			else
			{
				if (fileGCode->Put('\n')) // In case there wasn't one ending the file
				{
					fileGCode->SetFinished(ActOnCode(fileGCode));
				}
				if (!fileGCode->Active() && AllMovesAreFinishedAndMoveBufferIsLoaded())
				{
					fileBeingPrinted.Close();
					reprap.GetPrintMonitor()->StoppedPrint();
				}
				break;
			}
			++i;
		} while (i < GCODE_LENGTH);

		platform->ClassReport(longWait);
		return;
	}


	// Now run the G-Code buffers. It's important to fill up the G-Code buffers before we do this,
	// otherwise we wouldn't have a chance to pause/cancel running prints.

	if (webGCode->Active())
	{
		// Note: Direct web-printing has been dropped, so it's safe to execute web codes immediately
		webGCode->SetFinished(ActOnCode(webGCode, true));
	}
	else if (auxGCode->Active())
	{
		// Don't use code-queuing for codes received from AUX
		auxGCode->SetFinished(ActOnCode(auxGCode, true));
	}
	else if (serialGCode->Active())
	{
		// We want codes from the serial interface to be queued unless the print has been paused
		serialGCode->SetFinished(ActOnCode(serialGCode, reprap.GetMove()->IsPaused()));
	}
	else if (queuedGCode->Active())
	{
		queuedGCode->SetFinished(ActOnCode(queuedGCode, true));
	}
	else if (fileGCode->Active())
	{
		fileGCode->SetFinished(ActOnCode(fileGCode));
	}

	platform->ClassReport(longWait);
}

void GCodes::Diagnostics()
{
	platform->AppendMessage(BOTH_MESSAGE, "GCodes Diagnostics:\n");
	platform->AppendMessage(BOTH_MESSAGE, "Move available? %s\n", moveAvailable ? "yes" : "no");
	platform->AppendMessage(BOTH_MESSAGE, "Internal code queue is %s\n", (internalCodeQueue == NULL) ? "empty." :"not empty:");
	if (internalCodeQueue != NULL)
	{
		platform->AppendMessage(BOTH_MESSAGE, "Total moves: %d, moves completed: %d\n", totalMoves, movesCompleted);
		unsigned int count = 0;
		CodeQueueItem *item = internalCodeQueue;
		do {
			count++;
			platform->AppendMessage(BOTH_MESSAGE, "Queued '%s' for move %d\n", item->GetCommand(), item->ExecuteAtMove());
		} while ((item = item->Next()) != NULL);
		platform->AppendMessage(BOTH_MESSAGE, "%d codes have been queued.\n", count);
	}
}

// The wait till everything's done function.  If you need the machine to
// be idle before you do something (for example homing an axis, or shutting down) call this
// until it returns true.  As a side-effect it loads moveBuffer with the last
// position and feedrate for you.

bool GCodes::AllMovesAreFinishedAndMoveBufferIsLoaded()
{
	// Last one gone?
	if (moveAvailable)
		return false;

	// Wait for all the queued moves to stop so we get the actual last position and feedrate
	if (!reprap.GetMove()->AllMovesAreFinished())
		return false;
	reprap.GetMove()->AddMoreMoves();

	// Load the last position; If Move can't accept more, return false - should never happen
	if (!reprap.GetMove()->GetCurrentUserPosition(moveBuffer))
		return false;

	return true;
}

// Save (some of) the state of the machine for recovery in the future.
// Call repeatedly till it returns true.

bool GCodes::Push()
{
	if (stackPointer >= STACK)
	{
		platform->Message(BOTH_ERROR_MESSAGE, "Push(): stack overflow!\n");
		return true;
	}

	if (!AllMovesAreFinishedAndMoveBufferIsLoaded())
		return false;

	drivesRelativeStack[stackPointer] = drivesRelative;
	axesRelativeStack[stackPointer] = axesRelative;
	feedrateStack[stackPointer] = moveBuffer[DRIVES];
	for(size_t extruder=0; extruder<DRIVES-AXES; extruder++)
	{
		extruderPositionStack[stackPointer][extruder] = lastExtruderPosition[extruder];
	}
	doingFileMacroStack[stackPointer] = doingFileMacro;
	fileStack[stackPointer].CopyFrom(fileBeingPrinted);
	if (stackPointer == 0)
	{
		fractionOfFilePrinted = fileBeingPrinted.FractionRead();	// save this so that we don't return the fraction of the macro file read
	}
	stackPointer++;
	platform->PushMessageIndent();

	return true;
}

// Recover a saved state.  Call repeatedly till it returns true.

bool GCodes::Pop()
{
	if (stackPointer < 1)
	{
		platform->Message(BOTH_ERROR_MESSAGE, "Pop(): stack underflow!\n");
		return true;
	}

	if (!AllMovesAreFinishedAndMoveBufferIsLoaded())
		return false;

	stackPointer--;
	if (stackPointer == 0)
	{
		fractionOfFilePrinted = -1.0;			// restore live updates of fraction read from the file being printed
	}
	drivesRelative = drivesRelativeStack[stackPointer];
	axesRelative = axesRelativeStack[stackPointer];
	moveBuffer[DRIVES] = feedrateStack[stackPointer];
	doingFileMacro = doingFileMacroStack[stackPointer];
	for(size_t extruder=0; extruder<DRIVES-AXES; extruder++)
	{
		lastExtruderPosition[extruder] = extruderPositionStack[stackPointer][extruder];
	}
	fileBeingPrinted.MoveFrom(fileStack[stackPointer]);
	endStopsToCheck = 0;
	platform->PopMessageIndent();

	return true;
}

// Move expects all axis movements to be absolute, and all
// extruder drive moves to be relative.  This function serves that.
// If applyLimits is true and we have homed the relevant axes, then we don't allow movement beyond the bed.

bool GCodes::LoadMoveBufferFromGCode(GCodeBuffer *gb, bool doingG92, bool applyLimits)
{
	// Zero every extruder drive as some drives may not be changed

	for(size_t drive = AXES; drive < DRIVES; drive++)
	{
		moveBuffer[drive] = 0.0;
	}

	// Deal with feed rate

	if (gb->Seen(FEEDRATE_LETTER))
	{
		moveBuffer[DRIVES] = gb->GetFValue() * distanceScale * secondsToMinutes; // G Code feedrates are in mm/minute; we need mm/sec
	}

	// First do extrusion, and check, if we are extruding, that we have a tool to extrude with

	Tool* tool = reprap.GetCurrentTool();

	if (gb->Seen(EXTRUDE_LETTER))
	{
		if (tool == NULL)
		{
			platform->Message(BOTH_ERROR_MESSAGE, "Attempting to extrude with no tool selected.\n");
			return false;
		}

		int eMoveCount = tool->DriveCount();
		if (eMoveCount > 0)
		{
			float eMovement[DRIVES-AXES];
			if(tool->Mixing())
			{
				float length = gb->GetFValue();
				for(size_t drive = 0; drive < tool->DriveCount(); drive++)
				{
					eMovement[drive] = length * tool->GetMix()[drive];
				}
			}
			else
			{
				gb->GetFloatArray(eMovement, eMoveCount);
				if (tool->DriveCount() != eMoveCount)
				{
					platform->Message(BOTH_ERROR_MESSAGE, "Wrong number of extruder drives for the selected tool: %s\n", gb->Buffer());
					return false;
				}
			}

			// Set the drive values for this tool.

			for(size_t eDrive = 0; eDrive < eMoveCount; eDrive++)
			{
				size_t drive = tool->Drive(eDrive);
				float moveArg = eMovement[eDrive] * distanceScale;
				if (doingG92)
				{
					moveBuffer[drive + AXES] = moveArg;
					lastExtruderPosition[drive] = moveArg;
				}
				else
				{
					if (drivesRelative)
					{
						moveBuffer[drive + AXES] = moveArg;
						lastExtruderPosition[drive] += moveArg;
					}
					else
					{
						moveBuffer[drive + AXES] = moveArg - lastExtruderPosition[drive];
						lastExtruderPosition[drive] = moveArg;
					}
				}
			}
		}
	}

	// Now the movement axes

	const Tool *currentTool = reprap.GetCurrentTool();
	for(size_t axis = 0; axis < AXES; axis++)
	{
		if(gb->Seen(axisLetters[axis]))
		{
			float moveArg = gb->GetFValue() * distanceScale;
			if (doingG92)
			{
				axisIsHomed[axis] = true;		// doing a G92 defines the absolute axis position
			}
			else
			{
				if (axesRelative)
				{
					moveArg += moveBuffer[axis];
				}
				else if (currentTool != NULL)
				{
					moveArg -= currentTool->GetOffset()[axis];		// adjust requested position to compensate for tool offset
				}
				if (applyLimits && axis < 2 && axisIsHomed[axis])	// limit X & Y moves unless doing G92
				{
					if (moveArg < platform->AxisMinimum(axis))
					{
						moveArg = platform->AxisMinimum(axis);
					}
					else if (moveArg > platform->AxisMaximum(axis))
					{
						moveArg = platform->AxisMaximum(axis);
					}
				}
			}
			moveBuffer[axis] = moveArg;
		}
	}

	return true;
}



// This function is called for a G Code that makes a move.
// If the Move class can't receive the move (i.e. things have to wait), return 0.
// If we have queued the move and the caller doesn't need to wait for it to complete, return 1.
// If we need to wait for the move to complete before doing another one (because endstops are checked in this move), return 2.

int GCodes::SetUpMove(GCodeBuffer *gb)
{
	// Last one gone yet?
	if (moveAvailable)
		return 0;

	// Load the last position and feed rate into moveBuffer; If Move can't accept more, return false
	if (!reprap.GetMove()->GetCurrentUserPosition(moveBuffer))
		return 0;

	// Check to see if the move is a 'homing' move that endstops are checked on.
	endStopsToCheck = 0;
	if (gb->Seen('S'))
	{
		if (gb->GetIValue() == 1)
		{
			for (size_t axis = 0; axis < AXES; axis++)
			{
				if (gb->Seen(axisLetters[axis]))
				{
					endStopsToCheck |= (1 << axis);
				}
			}
		}
	}

	// Check for 'R' parameter here to go back to the coordinates at which the print was paused
	if (gb->Seen('R') && gb->GetIValue() > 0)
	{
		moveAvailable = reprap.GetMove()->GetPauseCoordinates(moveBuffer);
		if (moveAvailable)
		{
			// Allow specification of axis offsets as seen in dc42's firmware fork
			for(size_t axis=0; axis<AXES; axis++)
			{
				if (gb->Seen(axisLetters[axis]))
				{
					moveBuffer[axis] += gb->GetFValue();
				}
			}

			for(size_t drive=AXES; drive<DRIVES; drive++)
			{
				moveBuffer[drive] = 0.0;
			}

			if (gb->Seen(FEEDRATE_LETTER))
			{
				moveBuffer[DRIVES] = gb->GetFValue();
			}
			return 2;
		}
		else
		{
			platform->Message(BOTH_ERROR_MESSAGE, "Could not obtain pause coordinates!\n");
			return 1;
		}
	}

	// Load the move buffer with either the absolute movement required or the relative movement required
	moveAvailable = LoadMoveBufferFromGCode(gb, false, (endStopsToCheck == 0) && limitAxes);
	return (endStopsToCheck != 0 || reprap.GetMove()->IsPaused()) ? 2 : 1;
}

// The Move class calls this function to find what to do next.

bool GCodes::ReadMove(float m[], EndstopChecks& ce)
{
	if (!moveAvailable)
		return false;

	for (size_t i = 0; i <= DRIVES; i++) // 1 more for feedrate
	{
		m[i] = moveBuffer[i];
	}
	ce = endStopsToCheck;
	moveAvailable = false;
	endStopsToCheck = 0;
	return true;
}

bool GCodes::DoFileMacro(const char* fileName)
{
	// Are we returning from a macro?

	if (returningFromMacro)
	{
		if (!Pop())
		{
			return false;
		}

		returningFromMacro = false;
		return true;
	}

	// No, see if we can push some values on the stack

	if (!Push())
	{
		return false;
	}

	// Then see if we can open the file

	FileStore *f = platform->GetFileStore((fileName[0] == '/') ? "0:" : platform->GetSysDir(), fileName, false);
	if (f == NULL)
	{
		platform->Message(BOTH_ERROR_MESSAGE, "Macro file %s not found.\n", fileName);
		if(!Pop())
		{
			platform->Message(BOTH_ERROR_MESSAGE, "Cannot pop the stack.\n");
		}
		return true;
	}
	fileBeingPrinted.Set(f);

	// Deal with nested macros (rewind back to the position before the last code so it is called again later)

	unsigned int lastStackPointer = stackPointer - 1;
	if (doingFileMacroStack[lastStackPointer])
	{
		fileStack[lastStackPointer].Seek(fileStack[lastStackPointer].Position() - fileMacroGCode->Length());
	}

	// Set some values so the macro file gets processed properly

	doingFileMacro = true;
	fileMacroGCode->Init();

	return false;
}

bool GCodes::FileMacroCyclesReturn()
{
	if (!doingFileMacro)
		return true;

	if (!AllMovesAreFinishedAndMoveBufferIsLoaded())
		return false;

	fileBeingPrinted.Close();
	fileMacroGCode->Init();
	returningFromMacro = true;

	return Pop();
}

// To execute any move, call this until it returns true.
// moveToDo[] entries corresponding with false entries in action[] will
// be ignored.  Recall that moveToDo[DRIVES] should contain the feed rate
// you want (if action[DRIVES] is true).

bool GCodes::DoCannedCycleMove(EndstopChecks ce)
{
	// Is the move already running?

	if (cannedCycleMoveQueued)
	{ // Yes.
		if (!Pop()) // Wait for the move to finish then restore the state
		{
			return false;
		}
		cannedCycleMoveQueued = false;
		return true;
	}
	else
	{ // No.
		if (!Push()) // Wait for the RepRap to finish whatever it was doing, save it's state, and load moveBuffer[] with the current position.
		{
			return false;
		}

		for (size_t drive = 0; drive <= DRIVES; drive++)
		{
			if (activeDrive[drive])
			{
				moveBuffer[drive] = moveToDo[drive];
			}
		}
		endStopsToCheck = ce;
		cannedCycleMoveQueued = true;
		moveAvailable = true;
	}
	return false;
}

// This sets positions.  I.e. it handles G92.

bool GCodes::SetPositions(GCodeBuffer *gb)
{
	if (!AllMovesAreFinishedAndMoveBufferIsLoaded())
		return false;

	if(LoadMoveBufferFromGCode(gb, true, false))
	{
		SetPositions(moveBuffer);
	}

	return true;
}

void GCodes::SetPositions(float positionNow[DRIVES])
{
	// Transform the position so that e.g. if the user does G92 Z0,
	// the position we report (which gets inverse-transformed) really is Z=0 afterwards
	reprap.GetMove()->Transform(moveBuffer);
	reprap.GetMove()->SetLiveCoordinates(moveBuffer);
	reprap.GetMove()->SetPositions(moveBuffer);
}

// Offset the axes by the X, Y, and Z amounts in the M code in gb.  Say the machine is at [10, 20, 30] and
// the offsets specified are [8, 2, -5].  The machine will move to [18, 22, 25] and henceforth consider that point
// to be [10, 20, 30].

bool GCodes::OffsetAxes(GCodeBuffer* gb)
{
	if (!offSetSet)
	{
		if (!AllMovesAreFinishedAndMoveBufferIsLoaded())
			return false;

		for(size_t drive = 0; drive <= DRIVES; drive++)
		{
			if (drive < AXES || drive == DRIVES)
			{
				record[drive] = moveBuffer[drive];
				moveToDo[drive] = moveBuffer[drive];
			}
			else
			{
				record[drive] = 0.0;
				moveToDo[drive] = 0.0;
			}
			activeDrive[drive] = false;
		}

		for(size_t axis = 0; axis < AXES; axis++)
		{
			if (gb->Seen(axisLetters[axis]))
			{
				moveToDo[axis] += gb->GetFValue();
				activeDrive[axis] = true;
			}
		}

		if (gb->Seen(FEEDRATE_LETTER)) // Has the user specified a feedrate?
		{
			moveToDo[DRIVES] = gb->GetFValue();
			activeDrive[DRIVES] = true;
		}

		offSetSet = true;
	}

	if (DoCannedCycleMove(0))
	{
		for (size_t drive = 0; drive <= DRIVES; drive++)
		{
			moveBuffer[drive] = record[drive];
		}
		reprap.GetMove()->SetLiveCoordinates(record); // This doesn't transform record
		reprap.GetMove()->SetPositions(record);       // This does
		offSetSet = false;
		return true;
	}

	return false;
}

// Home one or more of the axes.  Which ones are decided by the
// booleans homeX, homeY and homeZ.
// Returns true if completed, false if needs to be called again.
// 'reply' is only written if there is an error.
// 'error' is false on entry, gets changed to true if there is an error.
bool GCodes::DoHome(StringRef& reply, bool& error)
//pre(reply.upb == STRING_LENGTH)
{
	if (homeX && homeY && homeZ)
	{
		if (!homing)
		{
			homing = true;
			axisIsHomed[X_AXIS] = false;
			axisIsHomed[Y_AXIS] = false;
			axisIsHomed[Z_AXIS] = false;
		}
		if (DoFileMacro(HOME_ALL_G))
		{
			homing = false;
			homeX = false;
			homeY = false;
			homeZ = false;
			return true;
		}
		return false;
	}

	if (homeX)
	{
		if (!homing)
		{
			homing = true;
			axisIsHomed[X_AXIS] = false;
		}
		if (DoFileMacro(HOME_X_G))
		{
			homing = false;
			homeX = false;
			return NoHome();
		}
		return false;
	}

	if (homeY)
	{
		if (!homing)
		{
			homing = true;
			axisIsHomed[Y_AXIS] = false;
		}
		if (DoFileMacro(HOME_Y_G))
		{
			homing = false;
			homeY = false;
			return NoHome();
		}
		return false;
	}

	if (homeZ)
	{
		if (platform->MustHomeXYBeforeZ() && (!axisIsHomed[X_AXIS] || !axisIsHomed[Y_AXIS]))
		{
			// We can only home Z if X and Y have already been homed
			reply.copy("Must home X and Y before homing Z\n");
			error = true;
			homing = false;
			homeZ = false;
			return true;
		}
		if (!homing)
		{
			homing = true;
			axisIsHomed[Z_AXIS] = false;
		}
		if (DoFileMacro(HOME_Z_G))
		{
			homing = false;
			homeZ = false;
			return NoHome();
		}
		return false;
	}

	// Should never get here

	endStopsToCheck = 0;
	moveAvailable = false;

	return true;
}

// This lifts Z a bit, moves to the probe XY coordinates (obtained by a call to GetProbeCoordinates() ),
// probes the bed height, and records the Z coordinate probed.  If you want to program any general
// internal canned cycle, this shows how to do it.

bool GCodes::DoSingleZProbeAtPoint()
{
	reprap.GetMove()->SetIdentityTransform(); // It doesn't matter if these are called repeatedly

	for (size_t drive = 0; drive <= DRIVES; drive++)
	{
		activeDrive[drive] = false;
	}

	switch (cannedCycleMoveCount)
	{
	case 0: // Raise Z to 5mm. This only does anything on the first move; on all the others Z is already there
		moveToDo[Z_AXIS] = Z_DIVE;
		activeDrive[Z_AXIS] = true;
		moveToDo[DRIVES] = platform->MaxFeedrate(Z_AXIS);
		activeDrive[DRIVES] = true;
		reprap.GetMove()->SetZProbing(false);
		if (DoCannedCycleMove(0))
		{
			cannedCycleMoveCount++;
		}
		return false;

	case 1:	// Move to the correct XY coordinates
		GetProbeCoordinates(probeCount, moveToDo[X_AXIS], moveToDo[Y_AXIS], moveToDo[Z_AXIS]);
		activeDrive[X_AXIS] = true;
		activeDrive[Y_AXIS] = true;
		// NB - we don't use the Z value
		moveToDo[DRIVES] = platform->MaxFeedrate(X_AXIS);
		activeDrive[DRIVES] = true;
		reprap.GetMove()->SetZProbing(false);
		if (DoCannedCycleMove(0))
		{
			cannedCycleMoveCount++;
		}
		return false;

	case 2:	// Probe the bed
		moveToDo[Z_AXIS] = -2.0 * platform->AxisMaximum(Z_AXIS);
		activeDrive[Z_AXIS] = true;
		moveToDo[DRIVES] = platform->HomeFeedRate(Z_AXIS);
		activeDrive[DRIVES] = true;
		reprap.GetMove()->SetZProbing(true);
		if (DoCannedCycleMove(1 << Z_AXIS))
		{
			cannedCycleMoveCount++;
		}
		return false;

	case 3:	// Raise the head 5mm
		moveToDo[Z_AXIS] = Z_DIVE;
		activeDrive[Z_AXIS] = true;
		moveToDo[DRIVES] = platform->MaxFeedrate(Z_AXIS);
		activeDrive[DRIVES] = true;
		reprap.GetMove()->SetZProbing(false);
		if (DoCannedCycleMove(0))
		{
			cannedCycleMoveCount++;
		}
		return false;

	default:
		cannedCycleMoveCount = 0;
		reprap.GetMove()->SetZBedProbePoint(probeCount, reprap.GetMove()->GetLastProbedZ());
		return true;
	}
}

// This simply moves down till the Z probe/switch is triggered.

bool GCodes::DoSingleZProbe()
{
	for (size_t drive = 0; drive <= DRIVES; drive++)
	{
		activeDrive[drive] = false;
	}

	switch (cannedCycleMoveCount)
	{
	case 0:
		++cannedCycleMoveCount;
		return false;

	case 1:
		moveToDo[Z_AXIS] = -1.1 * platform->AxisTotalLength(Z_AXIS);
		activeDrive[Z_AXIS] = true;
		moveToDo[DRIVES] = platform->HomeFeedRate(Z_AXIS);
		activeDrive[DRIVES] = true;
		if (DoCannedCycleMove(1 << Z_AXIS))
		{
			cannedCycleMoveCount++;
			probeCount = 0;
		}
		return false;

	default:
		cannedCycleMoveCount = 0;
		return true;
	}
}

// This sets wherever we are as the probe point P (probePointIndex)
// then probes the bed, or gets all its parameters from the arguments.
// If X or Y are specified, use those; otherwise use the machine's
// coordinates.  If no Z is specified use the machine's coordinates.  If it
// is specified and is greater than SILLY_Z_VALUE (i.e. greater than -9999.0)
// then that value is used.  If it's less than SILLY_Z_VALUE the bed is
// probed and that value is used.

bool GCodes::SetSingleZProbeAtAPosition(GCodeBuffer *gb, StringRef& reply)
{
	if (!AllMovesAreFinishedAndMoveBufferIsLoaded())
		return false;

	if (!gb->Seen('P'))
		return DoSingleZProbe();

	int probePointIndex = gb->GetIValue();

	float x = (gb->Seen(axisLetters[X_AXIS])) ? gb->GetFValue() : moveBuffer[X_AXIS];
	float y = (gb->Seen(axisLetters[Y_AXIS])) ? gb->GetFValue() : moveBuffer[Y_AXIS];
	float z = (gb->Seen(axisLetters[Z_AXIS])) ? gb->GetFValue() : moveBuffer[Z_AXIS];

	probeCount = probePointIndex;
	reprap.GetMove()->SetXBedProbePoint(probeCount, x);
	reprap.GetMove()->SetYBedProbePoint(probeCount, y);

	if (z > SILLY_Z_VALUE)
	{
		reprap.GetMove()->SetZBedProbePoint(probeCount, z);
		reprap.GetMove()->SetZProbing(false); // Not really needed, but let's be safe
		probeCount = 0;
		if (gb->Seen('S'))
		{
			zProbesSet = true;
			reprap.GetMove()->SetProbedBedEquation(reply);
		}
		return true;
	}
	else if (DoSingleZProbeAtPoint())
	{
		probeCount = 0;
		reprap.GetMove()->SetZProbing(false);
		if (gb->Seen('S'))
		{
			zProbesSet = true;
			reprap.GetMove()->SetProbedBedEquation(reply);
		}
		return true;
	}

	return false;
}

// This probes multiple points on the bed (three in a
// triangle or four in the corners), then sets the bed transformation to compensate
// for the bed not quite being the plane Z = 0.

bool GCodes::SetBedEquationWithProbe(StringRef& reply)
{
	// zpl-2014-10-09: In order to stay compatible with older firmware versions,
	// only execute bed.g if it is actually present in the /sys directory.
	if (platform->GetMassStorage()->FileExists(SYS_DIR SET_BED_EQUATION))
	{
		return DoFileMacro(SET_BED_EQUATION);
	}

	if (reprap.GetMove()->NumberOfXYProbePoints() < 3)
	{
		reply.copy("Bed probing: there needs to be 3 or more points set.\n");
		return true;
	}

	// zpl 2014-11-02: When calling G32, ensure bed compensation parameters are initially reset
	if (!settingBedEquationWithProbe)
	{
		reprap.GetMove()->SetIdentityTransform();
		settingBedEquationWithProbe = true;
	}

	if (DoSingleZProbeAtPoint())
	{
		probeCount++;
	}

	if (probeCount >= reprap.GetMove()->NumberOfXYProbePoints())
	{
		probeCount = 0;
		zProbesSet = true;
		reprap.GetMove()->SetZProbing(false);
		reprap.GetMove()->SetProbedBedEquation(reply);
		settingBedEquationWithProbe = false;
		return true;
	}
	return false;
}

// This returns the (X, Y) points to probe the bed at probe point count.  When probing,
// it returns false.  If called after probing has ended it returns true, and the Z coordinate
// probed is also returned.

bool GCodes::GetProbeCoordinates(int count, float& x, float& y, float& z) const
{
	x = reprap.GetMove()->XBedProbePoint(count);
	y = reprap.GetMove()->YBedProbePoint(count);
	z = reprap.GetMove()->ZBedProbePoint(count);
	return zProbesSet;
}

bool GCodes::SetPrintZProbe(GCodeBuffer* gb, StringRef& reply)
{
	if (!AllMovesAreFinishedAndMoveBufferIsLoaded())
		return false;

	if (gb->Seen(axisLetters[Z_AXIS]))
	{
		ZProbeParameters params;
		platform->GetZProbeParameters(params);
		params.height = gb->GetFValue();
		if (gb->Seen('P'))
		{
			params.adcValue = gb->GetIValue();
		}
		if (gb->Seen('S'))
		{
			params.calibTemperature = gb->GetFValue();
		}
		else
		{
#if HOT_BED != -1
			// Use the current bed temperature as the calibration temperature if no value was provided
			params.calibTemperature = platform->GetTemperature(HOT_BED);
#else
			platform->Message(BOTH_ERROR_MESSAGE, "Warning: Cannot use hot bed temperature!\n");
#endif
		}
		if (gb->Seen('C'))
		{
			params.temperatureCoefficient = gb->GetFValue();
		}
		else
		{
			params.temperatureCoefficient = 0.0;
		}
		platform->SetZProbeParameters(params);
	}
	else
	{
		int v0 = platform->ZProbe();
		int v1, v2;
		switch(platform->GetZProbeSecondaryValues(v1, v2))
		{
		case 1:
			reply.printf("%d (%d)", v0, v1);
			break;
		case 2:
			reply.printf("%d (%d, %d)", v0, v1, v2);
			break;
		default:
			reply.printf("%d", v0);
			break;
		}
	}
	return true;
}

// Return the current coordinates as a printable string.  Coordinates
// are updated at the end of each movement, so this won't tell you
// where you are mid-movement.

//Fixed to deal with multiple extruders

const char* GCodes::GetCurrentCoordinates() const
{
	float liveCoordinates[DRIVES + 1];
	reprap.GetMove()->LiveCoordinates(liveCoordinates);
	const Tool *currentTool = reprap.GetCurrentTool();
	if (currentTool != NULL)
	{
		const float *offset = currentTool->GetOffset();
		for (size_t i = 0; i < AXES; ++i)
		{
			liveCoordinates[i] += offset[i];
		}
	}

	scratchString.printf("X:%.2f Y:%.2f Z:%.2f ", liveCoordinates[X_AXIS], liveCoordinates[Y_AXIS], liveCoordinates[Z_AXIS]);
	for(unsigned int i = AXES; i< DRIVES; i++)
	{
		scratchString.catf("E%u:%.1f ", i-AXES, liveCoordinates[i]);
	}
	return scratchString.Pointer();
}

float GCodes::FractionOfFilePrinted() const
{
	if (fractionOfFilePrinted >= 0.0)
	{
		return fractionOfFilePrinted;
	}

	if (doingFileMacro && !fileToPrint.IsLive())
	{
		return -1.0;
	}

	if (reprap.GetMove()->IsPaused() && fileToPrint.IsLive())
	{
		return fileToPrint.FractionRead();
	}

	return fileBeingPrinted.FractionRead();
}

bool GCodes::OpenFileToWrite(const char* directory, const char* fileName, GCodeBuffer *gb)
{
	fileBeingWritten = platform->GetFileStore(directory, fileName, true);
	eofStringCounter = 0;
	if (fileBeingWritten == NULL)
	{
		platform->Message(BOTH_ERROR_MESSAGE, "Can't open GCode file \"%s\" for writing.\n", fileName);
		return false;
	}
	else
	{
		gb->SetWritingFileDirectory(directory);
		return true;
	}
}

void GCodes::WriteHTMLToFile(char b, GCodeBuffer *gb)
{
	if (fileBeingWritten == NULL)
	{
		platform->Message(BOTH_ERROR_MESSAGE, "Attempt to write to a null file.\n");
		return;
	}

	if (eofStringCounter != 0 && b != eofString[eofStringCounter])
	{
		for (size_t i = 0; i < eofStringCounter; ++i)
		{
			fileBeingWritten->Write(eofString[i]);
		}
		eofStringCounter = 0;
	}

	if (b == eofString[eofStringCounter])
	{
		eofStringCounter++;
		if (eofStringCounter >= eofStringLength)
		{
			fileBeingWritten->Close();
			fileBeingWritten = NULL;
			gb->SetWritingFileDirectory(NULL);
			const char* r = (platform->Emulating() == marlin) ? "Done saving file." : "";
			HandleReply(false, r, 'M', 560, false);
			return;
		}
	}
	else
	{
		fileBeingWritten->Write(b);
	}
}

void GCodes::WriteGCodeToFile(GCodeBuffer *gb)
{
	if (fileBeingWritten == NULL)
	{
		platform->Message(BOTH_ERROR_MESSAGE, "Attempt to write to a null file.\n");
		return;
	}

	// End of file?

	if (gb->Seen('M'))
	{
		if (gb->GetIValue() == 29)
		{
			fileBeingWritten->Close();
			fileBeingWritten = NULL;
			gb->SetWritingFileDirectory(NULL);
			const char* r = (platform->Emulating() == marlin) ? "Done saving file." : "";
			HandleReply(false, r, 'M', 29, false);
			return;
		}
	}

	// Resend request?

	if (gb->Seen('G'))
	{
		if (gb->GetIValue() == 998)
		{
			if (gb->Seen('P'))
			{
				scratchString.printf("%d\n", gb->GetIValue());
				HandleReply(false, scratchString.Pointer(), 'G', 998, true);
				return;
			}
		}
	}

	fileBeingWritten->Write(gb->Buffer());
	fileBeingWritten->Write('\n');
	HandleReply(false, "", 'G', 1, false);
}

// Set up a file to print, but don't print it yet.

void GCodes::QueueFileToPrint(const char* fileName)
{
	FileStore *f = platform->GetFileStore(platform->GetGCodeDir(), fileName, false);
	if (f != NULL)
	{
		// Cancel current print if there is any
		if (PrintingAFile())
		{
			CancelPrint();
		}

		fileGCode->SetToolNumberAdjust(0);
		queuedGCode->SetToolNumberAdjust(0);

		for (size_t extruder = AXES; extruder < DRIVES; extruder++)
		{
			lastExtruderPosition[extruder - AXES] = 0.0;
		}
		reprap.GetMove()->ResetExtruderPositions();

		fileToPrint.Set(f);
	}
	else
	{
		platform->Message(BOTH_ERROR_MESSAGE, "GCode file \"%s\" not found\n", fileName);
	}
}

void GCodes::DeleteFile(const char* fileName)
{
	if(!platform->GetMassStorage()->Delete(platform->GetGCodeDir(), fileName))
	{
		platform->Message(BOTH_ERROR_MESSAGE, "Could not delete file \"%s\"\n", fileName);
	}
}

// Send the config file to USB in response to an M503 command.
// This is not used for processing M503 requests received via the webserver.
bool GCodes::SendConfigToLine()
{
	if (configFile == NULL)
	{
		configFile = platform->GetFileStore(platform->GetSysDir(), platform->GetConfigFile(), false);
		if (configFile == NULL)
		{
			platform->Message(BOTH_ERROR_MESSAGE, "Configuration file not found\n");
			return true;
		}
		platform->GetLine()->Write('\n', true);
	}

	char b;
	while (configFile->Read(b))
	{
		platform->GetLine()->Write(b, true);
		if (b == '\n')
			return false;
	}

	platform->GetLine()->Write('\n', true);
	configFile->Close();
	configFile = NULL;
	return true;
}

// Function to handle dwell delays.  Return true for
// dwell finished, false otherwise.

bool GCodes::DoDwell(GCodeBuffer *gb)
{
	if(!gb->Seen('P'))
		return true;  // No time given - throw it away

	float dwell = 0.001 * (float) gb->GetLValue(); // P values are in milliseconds; we need seconds

	return DoDwellTime(dwell);
}

bool GCodes::DoDwellTime(float dwell)
{
	// Are we already in the dwell?

	if (dwellWaiting)
	{
		if (platform->Time() - dwellTime >= 0.0)
		{
			dwellWaiting = false;
			return true;
		}
		return false;
	}

	// New dwell - set it up

	dwellWaiting = true;
	dwellTime = platform->Time() + dwell;
	return false;
}

// Set offset, working and standby temperatures for a tool. I.e. handle a G10.

void GCodes::SetOrReportOffsets(StringRef& reply, GCodeBuffer *gb)
{
	if (gb->Seen('P'))
	{
		int8_t toolNumber = gb->GetIValue();
		toolNumber += gb->GetToolNumberAdjust();
		Tool* tool = reprap.GetTool(toolNumber);
		if (tool == NULL)
		{
			reply.printf("Attempt to set/report offsets and temperatures for non-existent tool: %d\n", toolNumber);
			return;
		}

		// Deal with setting offsets
		float offset[AXES];
		for (size_t i = 0; i < AXES; ++i)
		{
			offset[i] = tool->GetOffset()[i];
		}

		bool settingOffset = false;
		if (gb->Seen('X'))
		{
			offset[X_AXIS] = gb->GetFValue();
			settingOffset = true;
		}
		if (gb->Seen('Y'))
		{
			offset[Y_AXIS] = gb->GetFValue();
			settingOffset = true;
		}
		if (gb->Seen('Z'))
		{
			offset[Z_AXIS] = gb->GetFValue();
			settingOffset = true;
		}
		if (settingOffset)
		{
			tool->SetOffset(offset);
		}

		// Deal with setting temperatures
		bool settingTemps = false;
		int hCount = tool->HeaterCount();
		float standby[HEATERS];
		float active[HEATERS];
		if (hCount > 0)
		{
			tool->GetVariables(standby, active);
			if (gb->Seen('R'))
			{
				gb->GetFloatArray(standby, hCount);
				settingTemps = true;
			}
			if (gb->Seen('S'))
			{
				gb->GetFloatArray(active, hCount);
				settingTemps = true;
			}

			if (settingTemps)
			{
				tool->SetVariables(standby, active);
			}
		}

		if (!settingOffset && !settingTemps)
		{
			// Print offsets and temperatures
			reply.printf("Tool %d offsets: X%.1f Y%.1f Z%.1f", toolNumber, offset[X_AXIS], offset[Y_AXIS], offset[Z_AXIS]);
			if (hCount != 0)
			{
				reply.cat(", active/standby temperature(s):");
				for(size_t heater = 0; heater < hCount; heater++)
				{
					reply.catf(" %.1f/%.1f", active[heater], standby[heater]);
				}
			}
		}
	}
}

void GCodes::ManageTool(GCodeBuffer *gb, StringRef& reply)
{
	if(!gb->Seen('P'))
	{
		// DC temporary code to allow tool numbers to be adjusted so that we don't need to edit multi-media files generated by slic3r
		if (gb->Seen('S'))
		{
			int adjust = gb->GetIValue();
			gb->SetToolNumberAdjust(adjust);
			queuedGCode->SetToolNumberAdjust(adjust);
		}
		return;
	}

	// Check tool number
	bool seen = false;
	int toolNumber = gb->GetLValue();
	if (toolNumber < 0)
	{
		platform->Message(BOTH_ERROR_MESSAGE, "Tool number must be positive!\n");
		return;
	}
	if (reprap.GetTool(toolNumber) != NULL)
	{
		reprap.GetPlatform()->Message(BOTH_ERROR_MESSAGE, "Tool number %d already in use!\n", toolNumber);
		return;
	}

	// Check drives
	long drives[DRIVES - AXES];  // There can never be more than we have...
	int dCount = DRIVES - AXES;  // Sets the limit and returns the count
	if(gb->Seen('D'))
	{
		gb->GetLongArray(drives, dCount);
		seen = true;
	}
	else
	{
		dCount = 0;
	}

	// Check heaters
	long heaters[HEATERS];
	int hCount = HEATERS;
	if(gb->Seen('H'))
	{
		gb->GetLongArray(heaters, hCount);
		seen = true;
	}
	else
	{
		hCount = 0;
	}

	// Add or delete tool
	if (seen)
	{
		// M563 P# D-1 H-1 removes an existing tool
		if (dCount == 1 && hCount == 1 && drives[0] == -1 && heaters[0] == -1)
		{
			Tool *tool = reprap.GetTool(toolNumber);
			reprap.DeleteTool(tool);
		}
		else
		{
			Tool* tool = new Tool(toolNumber, drives, dCount, heaters, hCount);
			reprap.AddTool(tool);
		}
	}
	else
	{
		reprap.PrintTool(toolNumber, reply);
	}
}

// Does what it says.

void GCodes::DisableDrives()
{
	for (int8_t drive = 0; drive < DRIVES; drive++)
	{
		platform->DisableDrive(drive);
	}

	axisIsHomed[X_AXIS] = false;
	axisIsHomed[Y_AXIS] = false;
	axisIsHomed[Z_AXIS] = false;
}

// Does what it says.

void GCodes::SetEthernetAddress(GCodeBuffer *gb, int mCode)
{
	byte eth[4];
	const char* ipString = gb->GetString();
	uint8_t sp = 0;
	uint8_t spp = 0;
	uint8_t ipp = 0;
	while (ipString[sp])
	{
		if (ipString[sp] == '.')
		{
			eth[ipp] = atoi(&ipString[spp]);
			ipp++;
			if (ipp > 3)
			{
				platform->Message(BOTH_ERROR_MESSAGE, "Dud IP address: %s\n", gb->Buffer());
				return;
			}
			sp++;
			spp = sp;
		}
		else
		{
			sp++;
		}
	}
	eth[ipp] = atoi(&ipString[spp]);
	if (ipp == 3)
	{
		switch (mCode)
		{
		case 552:
			platform->SetIPAddress(eth);
			break;
		case 553:
			platform->SetNetMask(eth);
			break;
		case 554:
			platform->SetGateWay(eth);
			break;

		default:
			platform->Message(BOTH_ERROR_MESSAGE, "Setting ether parameter - dud code.\n");
		}
	}
	else
	{
		platform->Message(BOTH_ERROR_MESSAGE, "Dud IP address: %s\n", gb->Buffer());
	}
}


void GCodes::SetMACAddress(GCodeBuffer *gb)
{
	uint8_t mac[6];
	const char* ipString = gb->GetString();
	uint8_t sp = 0;
	uint8_t spp = 0;
	uint8_t ipp = 0;
	while(ipString[sp])
	{
		if(ipString[sp] == ':')
		{
			mac[ipp] = strtoul(&ipString[spp], NULL, 16);
			ipp++;
			if(ipp > 5)
			{
				platform->Message(BOTH_ERROR_MESSAGE, "Dud MAC address: %s\n", gb->Buffer());
				return;
			}
			sp++;
			spp = sp;
		}
		else
		{
			sp++;
		}
	}
	mac[ipp] = strtoul(&ipString[spp], NULL, 16);
	if(ipp == 5)
	{
		platform->SetMACAddress(mac);
	}
	else
	{
		platform->Message(BOTH_ERROR_MESSAGE, "Dud MAC address: %s\n", gb->Buffer());
	}
}

void GCodes::HandleReply(bool error, const char* reply, char gMOrT, int code, bool resend)
{
	// Check to which destination this reply should be sent to.
	// Note the same order as in the Spin() method.
	GCodeBuffer *gb;
	if (webGCode->Active())
	{
		gb = webGCode;
	}
	else if (auxGCode->Active())
	{
		gb = auxGCode;
	}
	else if (serialGCode->Active())
	{
		gb = serialGCode;
	}
	else if (internalCodeQueue->IsExecuting())
	{
		gb = internalCodeQueue->GetSource();
	}
	else
	{
		gb = fileGCode;
	}

	// Don't report "ok" responses if a macro file is being processed
	if (doingFileMacro && !reply[0])
	{
		return;
	}

	// Second UART device, may be used for auxiliary purposes
	if (gb == auxGCode)
	{
		// Command was received from the aux interface (LCD display), so send the response only to the aux interface.
		if (reply[0] != 0)
		{
			platform->GetAux()->Write(reply);
		}
		return;
	}

	// Deal with sending a reply to the web interface.
	// The browser only fetches replies once a second or so. Therefore, when we send a web command in the middle of an SD card print,
	// in order to be sure that we see the reply in the web interface, we must not send empty responses to the web interface unless
	// the corresponding command also came from the web interface.
	if (   (gMOrT != 'M' || (code != 111 && code != 122))	// web server reply for M111 and M122 is handled before we get here
		&& (gb == webGCode || error)
	   )
	{
		platform->Message((error) ? WEB_ERROR_MESSAGE : WEB_MESSAGE, reply);
	}

	const Compatibility c = (gb == serialGCode) ? platform->Emulating() : me;
	const char* response = (resend) ? "rs " : "ok";
	const char* emulationType = 0;

	switch (c)
	{
	case me:
	case reprapFirmware:
		if (error)
		{
			platform->GetLine()->Write("Error: ");
		}
		platform->GetLine()->Write(reply);
		return;

	case marlin:
		if (gMOrT == 'M' && code == 20)
		{
			platform->GetLine()->Write("Begin file list\n");
			platform->GetLine()->Write(reply);
			platform->GetLine()->Write("End file list\n");
			platform->GetLine()->Write(response);
			platform->GetLine()->Write('\n');
			return;
		}

		if (gMOrT == 'M' && code == 28)
		{
			platform->GetLine()->Write(response);
			platform->GetLine()->Write('\n');
			platform->GetLine()->Write(reply);
			return;
		}

		if ((gMOrT == 'M' && code == 105) || (gMOrT == 'G' && code == 998))
		{
			platform->GetLine()->Write(response);
			platform->GetLine()->Write(' ');
			platform->GetLine()->Write(reply);
			return;
		}

		if (reply[0])
		{
			platform->GetLine()->Write(reply);
		}
		if (!doingFileMacro)
		{
			platform->GetLine()->Write(response);
			platform->GetLine()->Write('\n');
		}
		return;

	case teacup:
		emulationType = "teacup";
		break;
	case sprinter:
		emulationType = "sprinter";
		break;
	case repetier:
		emulationType = "repetier";
		break;
	default:
		emulationType = "unknown";
	}

	if (emulationType != 0)
	{
		platform->Message(HOST_MESSAGE, "Emulation of %s is not yet supported.\n", emulationType);	// don't send this one to the web as well, it concerns only the USB interface
	}
}

// Set PID parameters (M301 or M304 command). 'heater' is the default heater number to use.
void GCodes::SetPidParameters(GCodeBuffer *gb, int heater, StringRef& reply)
{
	if (gb->Seen('H'))
	{
		heater = gb->GetIValue();
	}

	if (heater >= 0 && heater < HEATERS)
	{
		PidParameters pp = platform->GetPidParameters(heater);
		bool seen = false;
		if (gb->Seen('P'))
		{
			pp.kP = gb->GetFValue();
			seen = true;
		}
		if (gb->Seen('I'))
		{
			pp.kI = gb->GetFValue() / platform->HeatSampleTime();
			seen = true;
		}
		if (gb->Seen('D'))
		{
			pp.kD = gb->GetFValue() * platform->HeatSampleTime();
			seen = true;
		}
		if (gb->Seen('T'))
		{
			pp.kT = gb->GetFValue();
			seen = true;
		}
		if (gb->Seen('S'))
		{
			pp.kS = gb->GetFValue();
			seen = true;
		}
		if (gb->Seen('W'))
		{
			pp.pidMax = gb->GetFValue();
			seen = true;
		}
		if (gb->Seen('B'))
		{
			pp.fullBand = gb->GetFValue();
			seen = true;
		}

		if (seen)
		{
			platform->SetPidParameters(heater, pp);
		}
		else
		{
			reply.printf("Heater %d P:%.2f I:%.3f D:%.2f T:%.2f S:%.2f W:%.1f B:%.1f\n",
					heater, pp.kP, pp.kI, pp.kD, pp.kT, pp.kS, pp.pidMax, pp.fullBand);
		}
	}
}

void GCodes::SetHeaterParameters(GCodeBuffer *gb, StringRef& reply)
{
	if (gb->Seen('P'))
	{
		int heater = gb->GetIValue();
		if (heater >= 0 && heater < HEATERS)
		{
			PidParameters pp = platform->GetPidParameters(heater);
			bool seen = false;

			// We must set the 25C resistance and beta together in order to calculate Rinf. Check for these first.
			float r25, beta;
			if (gb->Seen('T'))
			{
				r25 = gb->GetFValue();
				seen = true;
			}
			else
			{
				r25 = pp.GetThermistorR25();
			}
			if (gb->Seen('B'))
			{
				beta = gb->GetFValue();
				seen = true;
			}
			else
			{
				beta = pp.GetBeta();
			}

			if (seen)	// if see R25 or Beta or both
			{
				pp.SetThermistorR25AndBeta(r25, beta);					// recalculate Rinf
			}

			// Now do the other parameters
			if (gb->Seen('R'))
			{
				pp.thermistorSeriesR = gb->GetFValue();
				seen = true;
			}
			if (gb->Seen('L'))
			{
				pp.adcLowOffset = gb->GetFValue();
				seen = true;
			}
			if (gb->Seen('H'))
			{
				pp.adcHighOffset = gb->GetFValue();
				seen = true;
			}
			if (gb->Seen('X'))
			{
				int thermistor = gb->GetIValue();
				if (thermistor >= 0 && thermistor < HEATERS)
				{
					platform->SetThermistorNumber(heater, thermistor);
				}
				else
				{
					platform->Message(BOTH_ERROR_MESSAGE, "Thermistor number %d is out of range\n", thermistor);
				}
				seen = true;
			}

			if (seen)
			{
				platform->SetPidParameters(heater, pp);
			}
			else
			{
				reply.printf("T:%.1f B:%.1f R:%.1f L:%.1f H:%.1f X:%d\n",
						r25, beta, pp.thermistorSeriesR, pp.adcLowOffset, pp.adcHighOffset, platform->GetThermistorNumber(heater));
			}
		}
		else
		{
			platform->Message(BOTH_ERROR_MESSAGE, "Heater number %d is out of range\n", heater);
		}
	}
}

void GCodes::SetToolHeaters(Tool *tool, float temperature)
{
	if(tool == NULL)
	{
		platform->Message(BOTH_ERROR_MESSAGE, "Setting temperature: no tool selected.\n");
		return;
	}

	float standby[HEATERS];
	float active[HEATERS];
	tool->GetVariables(standby, active);
	for(size_t h = 0; h < tool->HeaterCount(); h++)
	{
		active[h] = temperature;
	}
	tool->SetVariables(standby, active);
}

// Here we must check for codes that shall be executed as soon as all currently queued moves have finished.

bool GCodes::CanQueueCode(GCodeBuffer *gb) const
{
	// Check for G-Codes
	if (gb->Seen('G'))
	{
		const int code = gb->GetIValue();

		// Set active/standby temperatures
		if (code == 10 && (gb->Seen('R') || gb->Seen('S')))
			return true;
	}
	// Check for M-Codes
	else if (gb->Seen('M'))
	{
		const int code = gb->GetIValue();

		// Fan control
		if (code == 106 || code == 107)
			return true;

		// Set temperatures and return immediately
		if (code == 104 || code == 140 || code == 141 || code == 144)
			return true;

		// Display Message (LCD), Beep, RGB colour, Set servo position
		if (code == 117 || code == 300 || code == 280 || code == 420)
			return true;

		// Valve control
		if (code == 126 || code == 127)
			return true;

		// Set networking parameters, Emulation, Compensation, Z-Probe changes
		// File Uploads, Tool management
		if (code == 540 || (code >= 550 && code <= 563))
			return true;

		// Move, heater and auxiliary PWM control
		if (code >= 566 && code <= 573)
			return true;

		// Motor currents
		if (code == 906)
			return true;
	}

	return false;
}

// If the code to act on is completed, this returns true,
// otherwise false.  It is called repeatedly for a given
// code until it returns true for that code.

bool GCodes::ActOnCode(GCodeBuffer *gb, bool executeImmediately)
{
	// Discard empty buffers right away
	if (gb->IsEmpty())
	{
		return true;
	}

	// Check if we can execute this code immediately
	if (executeImmediately || totalMoves == movesCompleted || !CanQueueCode(gb))
	{
		// M-code parameters might contain letters T and G, e.g. in filenames.
		// dc42 assumes that G-and T-code parameters never contain the letter M.
		// Therefore we must check for an M-code first.
		if (gb->Seen('M'))
		{
			return HandleMcode(gb);
		}
		// dc42 doesn't think a G-code parameter ever contains letter T, or a T-code ever contains letter G.
		// So it doesn't matter in which order we look for them.
		if (gb->Seen('G'))
		{
			return HandleGcode(gb);
		}
		if (gb->Seen('T'))
		{
			return HandleTcode(gb);
		}
	}
	else
	{
		// Run the next code immediately and wait for it if there is no free item available
		if (releasedQueueItems == NULL)
		{
			internalCodeQueue->Execute();
			if (queuedGCode->Put(internalCodeQueue->GetCommand(), internalCodeQueue->GetCommandLength()))
			{
				queuedGCode->SetFinished(ActOnCode(queuedGCode, true));
			}

			return false;
		}

		// Set up a new queue item
		CodeQueueItem *newItem = releasedQueueItems;
		releasedQueueItems = releasedQueueItems->Next();
		newItem->Init(gb, totalMoves);

		// And append it to the queue so it's executed once all moves have finished
		if (internalCodeQueue == NULL)
		{
			internalCodeQueue = newItem;
		}
		else
		{
			CodeQueueItem *lastItem = internalCodeQueue;
			while (lastItem->Next() != NULL)
			{
				lastItem = lastItem->Next();
			}
			lastItem->SetNext(newItem);
		}

		// Queued G-Codes will send their own replies later
		return true;
	}

	// An invalid buffer gets discarded
	HandleReply(false, "", 'X', 0, false);
	return true;
}

bool GCodes::HandleGcode(GCodeBuffer* gb)
{
	bool result = true;
	bool error = false;
	bool resend = false;
	char replyBuffer[STRING_LENGTH];
	StringRef reply(replyBuffer, ARRAY_SIZE(replyBuffer));
	reply.Clear();

	int code = gb->GetIValue();
	switch (code)
	{
	case 0: // There are no rapid moves...
	case 1: // Ordinary move
		if (waitingForMoveToComplete)
		{
			// We have already set up this move, but it does endstop checks, so wait for it to complete.
			// Otherwise, if the next move uses relative coordinates, it will be incorrectly calculated.
			// Or we're just performing an isolated move while the Move class is paused.
			result = AllMovesAreFinishedAndMoveBufferIsLoaded();
			if (result)
			{
				waitingForMoveToComplete = false;
			}
		}
		else
		{
			int res = SetUpMove(gb);
			if (res == 2)
			{
				waitingForMoveToComplete = true;
			}
			result = (res == 1);
		}
		break;

	case 4: // Dwell
		if (!AllMovesAreFinishedAndMoveBufferIsLoaded())
			return false;

		result = DoDwell(gb);
		break;

	case 10: // Set/report offsets and temperatures
		SetOrReportOffsets(reply, gb);
		break;

	case 20: // Inches (which century are we living in, here?)
		distanceScale = INCH_TO_MM;
		break;

	case 21: // mm
		distanceScale = 1.0;
		break;

	case 28: // Home
		if (NoHome())
		{
			homeX = gb->Seen(axisLetters[X_AXIS]);
			homeY = gb->Seen(axisLetters[Y_AXIS]);
			homeZ = gb->Seen(axisLetters[Z_AXIS]);
			if (NoHome())
			{
				homeX = true;
				homeY = true;
				homeZ = true;
			}
		}
		result = DoHome(reply, error);
		break;

	case 30: // Z probe/manually set at a position and set that as point P
		result = SetSingleZProbeAtAPosition(gb, reply);
		break;

	case 31: // Return the probe value, or set probe variables
		result = SetPrintZProbe(gb, reply);
		break;

	case 32: // Probe Z at multiple positions and generate the bed transform
		if (!(axisIsHomed[X_AXIS] && axisIsHomed[Y_AXIS]))
		{
			// We can only do bed levelling if X and Y have already been homed
			reply.copy("Must home X and Y before bed probing\n");
			error = true;
		}
		else
		{
			result = SetBedEquationWithProbe(reply);
		}
		break;

	case 90: // Absolute coordinates
		// DC 2014-07-21 we no longer change the extruder settings in response to G90/G91 commands
		//drivesRelative = false;
		axesRelative = false;
		break;

	case 91: // Relative coordinates
		// DC 2014-07-21 we no longer change the extruder settings in response to G90/G91 commands
		//drivesRelative = true; // Non-axis movements (i.e. extruders)
		axesRelative = true;   // Axis movements (i.e. X, Y and Z)
		break;

	case 92: // Set position  // TODO: Make this report position for no arguments?  Or leave that to M114?
		result = SetPositions(gb);
		break;

	default:
		error = true;
		reply.printf("invalid G Code: %s\n", gb->Buffer());
	}
	if (result)
	{
		HandleReply(error, reply.Pointer(), 'G', code, resend);
	}
	return result;
}

bool GCodes::HandleMcode(GCodeBuffer* gb)
{
	bool result = true;
	bool error = false;
	bool resend = false;
	char replyBuffer[STRING_LENGTH];
	StringRef reply(replyBuffer, ARRAY_SIZE(replyBuffer));
	reply.Clear();

	int code = gb->GetIValue();
	switch (code)
	{
	case 0: // Stop
	case 1: // Sleep
		// If Marlin is emulated and M1 is called during a live print, run M25 instead
		if (code == 1 && gb == serialGCode && platform->Emulating() == marlin)
		{
			if (PrintingAFile() && reprap.GetMove()->IsRunning())
			{
				gb->Put("M25", 3);
				return false;
			}
		}

		// Call stop.g or sleep.g to allow users to execute custom actions before everything stops
		if (!DoFileMacro((code == 0) ? STOP_G : SLEEP_G))
			return false;

		{
			if (code == 0)
			{
				// M0 puts each drive into idle state
				for (size_t drive = 0; drive < DRIVES; drive++)
				{
					platform->SetDriveIdle(drive);
				}
			}
			else
			{
				// M1 disables them entirely
				DisableDrives();
			}

			// M1 switches off all heaters, M0 does this only if the print is not paused
			const bool isPaused = reprap.GetMove()->IsPaused();
			if (code == 1 || !isPaused)
			{
				Tool* tool = reprap.GetCurrentTool();
				if(tool != NULL)
				{
					reprap.StandbyTool(tool->Number());
				}

				reprap.GetHeat()->SwitchOffAll();
			}

			// In case a print is paused, send a response
			if (isPaused)
			{
				reply.copy("Print cancelled\n");
			}

			// Reset everything
			CancelPrint();
		}
		break;

	case 18: // Motors off
	case 84:
		if (!AllMovesAreFinishedAndMoveBufferIsLoaded())
			return false;

		{
			bool seen = false;
			for(size_t axis=0; axis<AXES; axis++)
			{
				if (gb->Seen(axisLetters[axis]))
				{
					axisIsHomed[axis] = false;
					platform->DisableDrive(axis);
					seen = true;
				}
			}

			if (gb->Seen(EXTRUDE_LETTER))
			{
				long int eDrive[DRIVES-AXES];
				int eCount = DRIVES-AXES;
				gb->GetLongArray(eDrive, eCount);
				for(size_t i=0; i<eCount; i++)
				{
					seen = true;
					if (eDrive[i] < 0 || eDrive[i] >= DRIVES-AXES)
					{
						reply.printf("Invalid extruder number specified: %ld\n", eDrive[i]);
						error = true;
						break;
					}
					platform->DisableDrive(AXES + eDrive[i]);
				}
			}

			if (!seen)
			{
				DisableDrives();
			}
			break;
		}

	case 20: // List files on SD card
		{
			int sparam = (gb->Seen('S')) ? gb->GetIValue() : 0;
			const char* dir = (gb->Seen('P')) ? gb->GetString() : platform->GetGCodeDir();

			if (sparam == 2)
			{
				reprap.GetFilesResponse(reply, dir, true);		// Send the file list in JSON format
			}
			else
			{
				// To mimic the behaviour of the official RepRapPro firmware:
				// If we are emulating RepRap then we print "GCode files:\n" at the start, otherwise we don't.
				// If we are emulating Marlin and the code came via the serial/USB interface, then we don't put quotes around the names and we separate them with newline;
				// otherwise we put quotes around them and separate them with comma.
				if (platform->Emulating() == me || platform->Emulating() == reprapFirmware)
				{
					reply.copy("GCode files:\n");
				}
				else
				{
					reply.Clear();
				}

				bool encapsulateList = (gb != serialGCode || platform->Emulating() != marlin);
				FileInfo file_info;
				if (platform->GetMassStorage()->FindFirst(dir, file_info))
				{
					// iterate through all entries and append each file name
					do {
						if (encapsulateList)
						{
							reply.catf("%c%s%c%c", FILE_LIST_BRACKET, file_info.fileName, FILE_LIST_BRACKET, FILE_LIST_SEPARATOR);
						}
						else
						{
							reply.catf("%s\n", file_info.fileName);
						}
					} while (platform->GetMassStorage()->FindNext(file_info));

					if (encapsulateList)
					{
						// remove the last separator
						reply[reply.strlen() - 1] = '\n';
					}
				}
				else
				{
					reply.cat("NONE\n");
				}
			}

			break;
		}

	case 21: // Initialise SD - ignore
		break;

	case 23: // Set file to print
	case 32: // Select file and start SD print
		if (doingFileMacro)
		{
			reply.copy("Cannot use M32/M23 in file macros!\n");
			error = true;
		}
		else
		{
			const char* filename = gb->GetUnprecedentedString();
			QueueFileToPrint(filename);
			if (fileToPrint.IsLive())
			{
				reprap.GetPrintMonitor()->StartingPrint(filename);
				if (platform->Emulating() == marlin)
				{
					reply.copy("File opened\nFile selected\n");
				}
			}
			else
			{
				reply.copy("Could not open file for printing!\n");
				error = true;
			}
		}

		if (error || code == 23)
		{
			break;
		}
		// no break otherwise

	case 24: // Print/resume-printing the selected file
		if (!fileToPrint.IsLive() && internalCodeQueue == NULL)
		{
			reply.copy("Cannot resume print, because no print is in progress!\n");
			error = true;
			break;
		}

		if (reprap.GetMove()->IsPaused())
		{
			isResuming = true;
			if (doPauseMacro && !DoFileMacro(RESUME_G))
			{
				result = false;
				break;
			}
		}

		result = reprap.GetMove()->Resume();
		if (result)
		{
			if (!isResuming)
			{
				reprap.GetPrintMonitor()->StartedPrint();
			}
			isResuming = false;

			fileBeingPrinted.MoveFrom(fileToPrint);
			fractionOfFilePrinted = -1.0;
			fileGCode->Resume();
			queuedGCode->Resume();
		}
		break;

	case 226: // Gcode Initiated Pause
		if (!AllMovesAreFinishedAndMoveBufferIsLoaded())
			return false;
		// no break

	case 25: // Pause the print
		if (!isPausing)
		{
			if (!PrintingAFile())
			{
				reply.copy("Cannot pause print, because no file is being printed!\n");
				error = true;
			}
			else if (doingFileMacro)
			{
				reply.copy("Cannot pause macro files, wait for it to complete first!\n");
				error = true;
			}
			else
			{
				isPausing = true;
				doPauseMacro = !reprap.GetMove()->NoLiveMovement();
				reprap.GetMove()->Pause();				// tell Move we wish to pause the current print
				fractionOfFilePrinted = fileBeingPrinted.FractionRead();
				fileToPrint.MoveFrom(fileBeingPrinted);
				fileGCode->Pause();
				queuedGCode->Pause();
				result = false;
			}
		}
		else
		{
			// Wait until Move has finished the last move(s) and until the pause macro has been processed
			result = (reprap.GetMove()->Pause() && (!doPauseMacro || DoFileMacro(PAUSE_G)));
			isPausing = !result;
		}
		break;

	case 26: // Set SD position
		if (gb->Seen('S'))
		{
			long value = gb->GetLValue();
			if (value < 0)
			{
				reply.copy("SD positions can't be negative!\n");
				error = true;
			}
			else if (fileBeingPrinted.IsLive())
			{
				if (!fileBeingPrinted.Seek(value))
				{
					reply.copy("The specified SD position is invalid!\n");
					error = true;
				}
			}
			else if (fileToPrint.IsLive())
			{
				if (!fileToPrint.Seek(value))
				{
					reply.copy("The specified SD position is invalid!\n");
					error = true;
				}
			}
			else
			{
				reply.copy("Cannot set SD file position, because no print is in progress!\n");
				error = true;
			}
		}
		else
		{
			reply.copy("You must specify the SD position in bytes using the S parameter.\n");
			error = true;
		}
		break;

	case 27: // Report print status - Deprecated
		if (PrintingAFile())
		{
			reply.copy("SD printing.\n");
		}
		else
		{
			reply.copy("Not SD printing.\n");
		}
		break;

	case 28: // Write to file
		{
			const char* str = gb->GetUnprecedentedString();
			bool ok = OpenFileToWrite(platform->GetGCodeDir(), str, gb);
			if (ok)
			{
				reply.printf("Writing to file: %s\n", str);
			}
			else
			{
				reply.printf("Can't open file %s for writing.\n", str);
				error = true;
			}
		}
		break;

	case 29: // End of file being written; should be intercepted before getting here
		reply.copy("GCode end-of-file being interpreted.\n");
		break;

	case 30: // Delete file
		DeleteFile(gb->GetUnprecedentedString());
		break;

	// For case 32, see case 24

	case 36: // Return file information
		{
			const char* filename = gb->GetUnprecedentedString(true);	// get filename, or NULL if none provided
			reprap.GetPrintMonitor()->GetFileInfoResponse(reply, filename);
			reply.cat("\n");
		}
		break;

	case 80: // ATX power on
	case 81: // ATX power off
		if (!AllMovesAreFinishedAndMoveBufferIsLoaded())
			return false;

		if (code == 81)
		{
			DisableDrives();
		}
		platform->SetAtxPower(code == 80);
		break;

	case 82: // Use absolute extruder positioning
		if (drivesRelative)	// don't reset the absolute extruder position if it was already absolute
		{
			for (int8_t extruder = AXES; extruder < DRIVES; extruder++)
			{
				lastExtruderPosition[extruder - AXES] = 0.0;
			}
			drivesRelative = false;
		}
		break;

	case 83: // Use relative extruder positioning
		if (!drivesRelative)	// don't reset the absolute extruder position if it was already relative
		{
			for (int8_t extruder = AXES; extruder < DRIVES; extruder++)
			{
				lastExtruderPosition[extruder - AXES] = 0.0;
			}
			drivesRelative = true;
		}
		break;

	// For case 84, see case 18

	case 85: // Set inactive time
		// TODO: put some code in here...
		break;

	case 92: // Set/report steps/mm for some axes
		{
			bool seen = false;
			for(int8_t axis = 0; axis < AXES; axis++)
			{
				if(gb->Seen(axisLetters[axis]))
				{
					platform->SetDriveStepsPerUnit(axis, gb->GetFValue());
					seen = true;
				}
			}

			if(gb->Seen(EXTRUDE_LETTER))
			{
				seen = true;
				float eVals[DRIVES-AXES];
				int eCount = DRIVES-AXES;
				gb->GetFloatArray(eVals, eCount);
				for(size_t e = 0; e < eCount; e++)
				{
					platform->SetDriveStepsPerUnit(AXES + e, eVals[e]);
				}
			}

			if(!seen)
			{
				reply.printf("Steps/mm: X: %.3f, Y: %.3f, Z: %.3f, E: ",
						platform->DriveStepsPerUnit(X_AXIS), platform->DriveStepsPerUnit(Y_AXIS),
						platform->DriveStepsPerUnit(Z_AXIS));
				for(size_t drive = AXES; drive < DRIVES; drive++)
				{
					reply.catf("%.3f", platform->DriveStepsPerUnit(drive));
					if(drive < DRIVES-1)
					{
						reply.cat(":");
					}
				}
				reply.cat("\n");
			}
		}
		break;

	case 98: // Call Macro/Subprogram
		if (gb->Seen('P'))
		{
			result = DoFileMacro(gb->GetString());
		}
		break;

	case 99: // Return from Macro/Subprogram
		result = FileMacroCyclesReturn();
		break;

	case 104: // Deprecated.  This sets the active temperature of every heater of the active tool
		if(gb->Seen('S'))
		{
			float temperature = gb->GetFValue();
			Tool* tool;
			if (gb->Seen('T'))
			{
				int toolNumber = gb->GetIValue();
				toolNumber += gb->GetToolNumberAdjust();
				tool = reprap.GetTool(toolNumber);
			}
			else
			{
				tool = reprap.GetCurrentTool();
			}
			SetToolHeaters(tool, temperature);
		}
		break;

	case 105: // Get Extruder Temperature / Get Status Message
		{
			int param = (gb->Seen('S')) ? gb->GetIValue() : 0;
			int seq = (gb->Seen('R')) ? gb->GetIValue() : -1;
			switch (param)
			{
				// case 1 is reserved for future Pronterface versions, see
				// http://reprap.org/wiki/G-code#M105:_Get_Extruder_Temperature

				/* NOTE: The following responses are subject to deprecation */
				case 2:
					reprap.GetLegacyStatusResponse(reply, 2, seq);	// send JSON-formatted status response
					reply.cat("\n");
					break;

				case 3:
					reprap.GetLegacyStatusResponse(reply, 3, seq);	// send extended JSON-formatted response
					reply.cat("\n");
					break;

				/* This one isn't */
				case 4:
					reprap.GetStatusResponse(reply, 3, false);		// send print status JSON-formatted response
					break;

				default:
					// Report first active tool heater
					reply.copy("T:");
					Tool *currentTool = reprap.GetCurrentTool();
					if (currentTool != NULL && currentTool->HeaterCount() > 0)
					{
						reply.catf("%.1f", reprap.GetHeat()->GetTemperature(currentTool->Heater(0)));
					}

					// Report bed temperature and temperatures for all heaters in use
					char ch = ' ';
					float targetTemperature;
#if HOT_BED != -1
					reply.catf(" B:%.1f", reprap.GetHeat()->GetTemperature(HOT_BED));
					for(size_t heater = HOT_BED; heater < reprap.GetHeatersInUse(); heater++)
#else
					for(size_t heater = E0_HEATER; heater < reprap.GetHeatersInUse(); heater++)
#endif
					{
						if (reprap.GetHeat()->GetStatus(heater) == Heat::HeaterStatus::HS_active)
						{
							targetTemperature = reprap.GetHeat()->GetActiveTemperature(heater);
						}
						else
						{
							targetTemperature = reprap.GetHeat()->GetStandbyTemperature(heater);
						}
						reply.catf(" H%d:%.1f/%.1f", heater, reprap.GetHeat()->GetTemperature(heater), targetTemperature);
					}
					reply.cat("\n");
					break;
			}
		}
		break;
   
	case 106: // Set/report fan values
		{
			bool seen = false;

			if (gb->Seen('I'))		// Invert cooling
			{
				coolingInverted = (gb->GetIValue() > 0);
				seen = true;
			}

			float f = lastFanValue;
			if (gb->Seen('S'))		// Set new fan value
			{
				f = gb->GetFValue();
				f = min<float>(f, 255.0);
				f = max<float>(f, 0.0);
				seen = true;
			}

			if (gb->Seen('R'))		// Restore last-known fan value
			{
				seen = true;
			}
			else
			{
				lastFanValue = f;
			}

			if (seen)
			{
				if (coolingInverted)
				{
					// Check if 1.0 or 255.0 may be used as the maximum value
					platform->SetFanValue((f <= 1.0 ? 1.0 : 255.0) - f);
				}
				else
				{
					platform->SetFanValue(f);
				}
			}
			else
			{
				f = coolingInverted ? (1.0 - platform->GetFanValue()) : platform->GetFanValue();
				reply.printf("Fan value: %d%%, Cooling inverted: %s\n", (byte)(f * 100.0),
						coolingInverted ? "yes" : "no");
			}
		}
		break;

	case 107: // Fan off - deprecated
		platform->SetFanValue(coolingInverted ? 255.0 : 0.0);
		break;

	case 109: // Set Extruder Temperature and Wait - deprecated
		if (!AllMovesAreFinishedAndMoveBufferIsLoaded())
			return false;

		if(gb->Seen('S'))
		{
			float temperature = gb->GetFValue();
			Tool *tool;
			if (gb->Seen('T'))
			{
				int toolNumber = gb->GetIValue();
				toolNumber += gb->GetToolNumberAdjust();
				tool = reprap.GetTool(toolNumber);
			}
			else
			{
				tool = reprap.GetCurrentTool();
			}
			SetToolHeaters(tool, temperature);
			result = ToolHeatersAtSetTemperatures(tool);
		}
		break;

	case 110: // Set line numbers - line numbers are dealt with in the GCodeBuffer class
		break;

	case 111: // Debug level
    	if(gb->Seen('S'))
    	{
    		bool dbv = (gb->GetIValue() != 0);
    		if (gb->Seen('P'))
    		{
    			reprap.SetDebug(static_cast<Module>(gb->GetIValue()), dbv);
    		}
    		else
    		{
    			reprap.SetDebug(dbv);
    		}
    	}
      	else
		{
			reprap.PrintDebug();
		}
		break;

	case 112: // Emergency stop - acted upon in Webserver, but also here in case it comes from USB etc.
		reprap.EmergencyStop();
		Reset();
		reply.copy("Emergency Stop! Reset the controller to continue.\n");
		break;

	case 114: // Deprecated
		{
			const char* str = GetCurrentCoordinates();
			if (str != 0)
			{
				reply.copy(str);
			}
			else
			{
				result = false;
			}
		}
		break;

	case 115: // Print firmware version
		reply.printf("FIRMWARE_NAME:%s FIRMWARE_VERSION:%s ELECTRONICS:%s DATE:%s\n", NAME, VERSION, ELECTRONICS, DATE);
		break;

	case 116: // Wait for everything, especially set temperatures
		if (!AllMovesAreFinishedAndMoveBufferIsLoaded())
			return false;

		{
			bool seen = false;

			if (gb->Seen('P'))
			{
				// Wait for the heaters associated with the specified tool to be ready
				int toolNumber = gb->GetIValue();
				toolNumber += gb->GetToolNumberAdjust();
				if (!ToolHeatersAtSetTemperatures(reprap.GetTool(toolNumber)))
				{
					return false;
				}
				seen = true;
			}

			if (gb->Seen('H'))
			{
				// Wait for specified heaters to be ready
				long heaters[HEATERS];
				int heaterCount = HEATERS;
				gb->GetLongArray(heaters, heaterCount);
				for(int i=0; i<heaterCount; i++)
				{
					if (!reprap.GetHeat()->HeaterAtSetTemperature(heaters[i]))
					{
						return false;
					}
				}
				seen = true;
			}

			if (gb->Seen('C'))
			{
				// Wait for chamber heater to be ready
				const int8_t chamberHeater = reprap.GetChamberHeater();
				if (chamberHeater != -1)
				{
					if (!reprap.GetHeat()->HeaterAtSetTemperature(chamberHeater))
					{
						return false;
					}
				}
				seen = true;
			}

			if (!seen)
			{
				// Wait for all heaters to be ready
				result = reprap.GetHeat()->AllHeatersAtSetTemperatures(true);
			}
		}
		break;

	case 117: // Display message
		reprap.SetMessage(gb->GetUnprecedentedString());
		break;

	case 119: // Get Endstop Status
		{
			reply.copy("Endstops - ");
			char* es;
			char comma = ',';
			for(size_t axis = 0; axis < AXES; axis++)
			{
				switch(platform->Stopped(axis))
				{
					case lowHit:
						es = "at min stop";
						break;

					case highHit:
						es = "at max stop";
						break;

					case noStop:
					default:
						es = "not stopped";
				}

				if(axis == AXES - 1)
				{
					comma = ' ';
				}

				reply.catf("%c: %s%c ", axisLetters[axis], es, comma);
			}
			reply.cat("\n");
		}
		break;

	case 120: // Push
		result = Push();
		break;

	case 121: // Pop
		result = Pop();
		break;

	case 122: // Diagnostics
		{
			int val = (gb->Seen('P') ? gb->GetIValue() : 0);
			if (val == 0)
			{
				reprap.Diagnostics();
			}
			else
			{
				platform->DiagnosticTest(val);
			}
		}
		break;

	case 126: // Valve open
		reply.copy("M126 - valves not yet implemented\n");
		break;

	case 127: // Valve closed
		reply.copy("M127 - valves not yet implemented\n");
		break;

	case 135: // Set PID sample interval
		if(gb->Seen('S'))
		{
			platform->SetHeatSampleTime(gb->GetFValue() * 0.001);  // Value is in milliseconds; we want seconds
		}
		else
		{
			reply.printf("Heat sample time is %.3f seconds.\n", platform->HeatSampleTime());
		}
		break;

	case 140: // Set bed temperature
#if HOT_BED != -1
		if(gb->Seen('S'))
		{
			float temperature = gb->GetFValue();
			if (temperature < NEARLY_ABS_ZERO)
			{
				reprap.GetHeat()->SwitchOff(HOT_BED);
			}
			else
			{
				reprap.GetHeat()->SetActiveTemperature(HOT_BED, temperature);
				reprap.GetHeat()->Activate(HOT_BED);
			}
		}
		if(gb->Seen('R'))
		{
			reprap.GetHeat()->SetStandbyTemperature(HOT_BED, gb->GetFValue());
		}
#else
		reply.copy("Hot bed is not present!\n");
		error = true;
#endif
		break;

	case 141: // Chamber temperature
		{
			bool seen = false;
			if (gb->Seen('H'))
			{
				seen = true;

				int heater = gb->GetIValue();
				if (heater < 0)
				{
					const int8_t currentHeater = reprap.GetChamberHeater();
					if (currentHeater != -1)
					{
						reprap.GetHeat()->SwitchOff(currentHeater);
					}

					reprap.SetChamberHeater(-1);
				}
				else if (heater < HEATERS)
				{
					reprap.SetChamberHeater(heater);
				}
				else
				{
					reply.copy("Bad heater number specified!\n");
					error = true;
				}
			}

			if (gb->Seen('S'))
			{
				seen = true;

				const int8_t currentHeater = reprap.GetChamberHeater();
				if (currentHeater != -1)
				{
					float temperature = gb->GetFValue();

					if (temperature < NEARLY_ABS_ZERO)
					{
						reprap.GetHeat()->SwitchOff(currentHeater);
					}
					else
					{
						reprap.GetHeat()->SetActiveTemperature(currentHeater, temperature);
						reprap.GetHeat()->Activate(currentHeater);
					}
				}
				else
				{
					reply.copy("No chamber heater has been set up yet!\n");
					error = true;
				}
			}

			if (!seen)
			{
				const int8_t currentHeater = reprap.GetChamberHeater();
				if (currentHeater != -1)
				{
					reply.printf("Chamber heater %d is currently at %.1fC\n", currentHeater, reprap.GetHeat()->GetTemperature(currentHeater));
				}
				else
				{
					reply.copy("No chamber heater has been configured yet.\n");
				}
			}
		}
		break;

	case 144: // Set bed to standby
#if HOT_BED != -1
		reprap.GetHeat()->Standby(HOT_BED);
#else
		reply.copy("Hot bed is not present!\n");
		error = true;
#endif
		break;

//    case 160: //number of mixing filament drives  TODO: With tools defined, is this needed?
//    	if(gb->Seen('S'))
//		{
//			platform->SetMixingDrives(gb->GetIValue());
//		}
//		break;

	case 190: // Wait for bed temperature to reach target temp - deprecated
#if HOT_BED != -1
		if (!AllMovesAreFinishedAndMoveBufferIsLoaded())
			return false;

		if(gb->Seen('S'))
		{
			reprap.GetHeat()->SetActiveTemperature(HOT_BED, gb->GetFValue());
			reprap.GetHeat()->Activate(HOT_BED);
			result = reprap.GetHeat()->HeaterAtSetTemperature(HOT_BED);
		}
#else
		reply.copy("Hot bed is not present!\n");
		error = true;
#endif
		break;

	case 201: // Set/print axis accelerations  FIXME - should these be in /min not /sec ?
		{
			bool seen = false;
			for(int8_t axis = 0; axis < AXES; axis++)
			{
				if(gb->Seen(axisLetters[axis]))
				{
					platform->SetAcceleration(axis, gb->GetFValue() * distanceScale);
					seen = true;
				}
			}

			if(gb->Seen(EXTRUDE_LETTER))
			{
				seen = true;
				float eVals[DRIVES-AXES];
				int eCount = DRIVES-AXES;
				gb->GetFloatArray(eVals, eCount);
				for(uint8_t e = 0; e < eCount; e++)
				{
					platform->SetAcceleration(AXES + e, eVals[e] * distanceScale);
				}
			}

			if(!seen)
			{
				reply.printf("Accelerations: X: %.1f, Y: %.1f, Z: %.1f, E: ",
						platform->Acceleration(X_AXIS)/distanceScale, platform->Acceleration(Y_AXIS)/distanceScale,
						platform->Acceleration(Z_AXIS)/distanceScale);
				for(int8_t drive = AXES; drive < DRIVES; drive++)
				{
					reply.catf("%.1f", platform->Acceleration(drive)/distanceScale);
					if(drive < DRIVES-1)
					{
						reply.cat(":");
					}
				}
				reply.cat("\n");
			}
		}
		break;

    case 203: // Set/print maximum feedrates
		{
			bool seen = false;
			for(int8_t axis = 0; axis < AXES; axis++)
			{
				if(gb->Seen(axisLetters[axis]))
				{
					platform->SetMaxFeedrate(axis, gb->GetFValue() * distanceScale * secondsToMinutes); // G Code feedrates are in mm/minute; we need mm/sec
					seen = true;
				}
			}

			if(gb->Seen(EXTRUDE_LETTER))
			{
				seen = true;
				float eVals[DRIVES-AXES];
				int eCount = DRIVES-AXES;
				gb->GetFloatArray(eVals, eCount);
				for(uint8_t e = 0; e < eCount; e++)
				{
					platform->SetMaxFeedrate(AXES + e, eVals[e] * distanceScale * secondsToMinutes);
				}
			}

			if(!seen)
			{
				reply.printf("Maximum feedrates: X: %.1f, Y: %.1f, Z: %.1f, E: ",
						platform->MaxFeedrate(X_AXIS)/(distanceScale * secondsToMinutes), platform->MaxFeedrate(Y_AXIS)/(distanceScale * secondsToMinutes),
						platform->MaxFeedrate(Z_AXIS)/(distanceScale * secondsToMinutes));
				for(int8_t drive = AXES; drive < DRIVES; drive++)
				{
					reply.catf("%.1f", platform->MaxFeedrate(drive) / (distanceScale * secondsToMinutes));
					if(drive < DRIVES-1)
					{
						reply.cat(":");
					}
				}
				reply.cat("\n");
			}
		}
		break;

	case 205: //M205 advanced settings:  minimum travel speed S=while printing T=travel only,  B=minimum segment time X= maximum xy jerk, Z=maximum Z jerk
		// This is superseded in this firmware by M codes for the separate types (e.g. M566).
		break;

    case 206:  // Offset axes - Deprecated
    	result = OffsetAxes(gb);
    	break;

	case 208: // Set/print maximum axis lengths. If there is an S parameter with value 1 then we set the min value, else we set the max value.
		{
			bool setMin = (gb->Seen('S') ? (gb->GetIValue() == 1): false);
			bool seen = false;
			for (int8_t axis = 0; axis < AXES; axis++)
			{
				if (gb->Seen(axisLetters[axis]))
				{
					float value = gb->GetFValue() * distanceScale;
					if (setMin)
					{
						platform->SetAxisMinimum(axis, value);
					}
					else
					{
						platform->SetAxisMaximum(axis, value);
					}
					seen = true;
				}
			}

			if (!seen)
			{
				reply.copy("Axis limits - ");
				char comma = ',';
				for(int8_t axis = 0; axis < AXES; axis++)
				{
					if(axis == AXES - 1)
					{
						comma = '\n';
					}
					reply.catf("%c: %.1f min, %.1f max%c ", axisLetters[axis],
							platform->AxisMinimum(axis), platform->AxisMaximum(axis), comma);
				}
			}
		}
		break;

	case 210: // Set/print homing feedrates
		{
			bool seen = false;
			for (int8_t axis = 0; axis < AXES; axis++)
			{
				if (gb->Seen(axisLetters[axis]))
				{
					float value = gb->GetFValue() * distanceScale * secondsToMinutes;
					platform->SetHomeFeedRate(axis, value);
					seen = true;
				}
			}

			if(!seen)
			{
				reply.copy("Homing feedrates (mm/min) - ");
				char comma = ',';
				for(int8_t axis = 0; axis < AXES; axis++)
				{
					if(axis == AXES - 1)
					{
						comma = ' ';
					}

					reply.catf("%c: %.1f%c ", axisLetters[axis],
							platform->HomeFeedRate(axis) * 60.0 / distanceScale, comma);
				}
				reply.cat("\n");
			}
		}
		break;

	case 220: // Set/report speed factor override percentage
		if (gb->Seen('S'))
		{
			float speedFactor = gb->GetFValue() / 100.0;
			if (speedFactor > 0)
			{
				reprap.GetMove()->SetSpeedFactor(speedFactor);
			}
			else
			{
				reply.printf("Invalid speed factor specified.\n");
				error = true;
			}
		}
		else
		{
			reply.printf("Speed factor override: %.1f%%\n", reprap.GetMove()->GetSpeedFactor() * 100.0);
		}
		break;

	case 221: // Set/report extrusion factor override percentage
		{
			int extruder = 0;
			if (gb->Seen('D'))	// D parameter (if present) selects the extruder number
			{
				extruder = gb->GetIValue();
			}

			if (gb->Seen('S'))	// S parameter sets the override percentage
			{
				float extrusionFactor = gb->GetFValue()/100.0;
				if (extruder >= 0 && extruder < DRIVES - AXES && extrusionFactor >= 0)
				{
					reprap.GetMove()->SetExtrusionFactor(extruder, extrusionFactor);
				}
			}
			else
			{
				reply.printf("Extrusion factor override for extruder %d: %.1f%%\n", extruder,
						reprap.GetMove()->GetExtrusionFactor(extruder) * 100.0);
			}
		}
		break;

	// For case 226, see case 25

	case 300: // Beep
		if (gb->Seen('P'))
		{
			int ms = gb->GetIValue();
			if (gb->Seen('S'))
			{
				reprap.Beep(gb->GetIValue(), ms);
			}
		}
		break;

	case 301: // Set/report hot end PID values
		SetPidParameters(gb, 1, reply);
		break;

	case 302: // Allow, deny or report cold extrudes
		if (gb->Seen('P'))
		{
			if (gb->GetIValue() > 0)
			{
				reprap.AllowColdExtrude();
			}
			else
			{
				reprap.DenyColdExtrude();
			}
		}
		else
		{
			reply.printf("Cold extrudes are %s, use M302 P[1/0] to allow or deny them\n",
					reprap.ColdExtrude() ? "enabled" : "disabled");
		}
		break;

	case 304: // Set/report heated bed PID values
#if HOT_BED != -1
		SetPidParameters(gb, HOT_BED, reply);
#else
		reply.copy("Hot bed is not present!\n");
		error = true;
#endif
		break;

	case 305: // Set/report specific heater parameters
		SetHeaterParameters(gb, reply);
		break;

	case 400: // Wait for current moves to finish
		if (!AllMovesAreFinishedAndMoveBufferIsLoaded())
			return false;
		break;

	case 408: // Report JSON status response
		if (gb->Seen('S'))
		{
			const int type = gb->GetIValue();

			int seq = -1;
			if (gb->Seen('P'))
			{
				seq = gb->GetIValue();
			}

			switch(type)
			{
				case 0:
				case 1:
					reprap.GetLegacyStatusResponse(reply, type + 2, seq);
					break;

				case 2:
				case 3:
				case 4:
					reprap.GetStatusResponse(reply, type - 2, false);
					break;

				case 5:
					reprap.GetConfigResponse(reply);
					break;
			}
		}
		break;

	case 500: // Store parameters in EEPROM
		platform->WriteNvData();
		break;

	case 501: // Load parameters from EEPROM
		platform->ReadNvData();
		if (gb->Seen('S'))
		{
			platform->SetAutoSave(gb->GetIValue() > 0);
		}
		break;

	case 502: // Revert to default "factory settings"
		platform->ResetNvData();
		break;

	case 503: // List variable settings
		result = SendConfigToLine();
		break;

	case 540: // Set/report MAC address
		if(gb->Seen('P'))
		{
			SetMACAddress(gb);
		}
		else
		{
			const byte* mac = platform->MACAddress();
			reply.printf("MAC: %x:%x:%x:%x:%x:%x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		}
		break;

	case 550: // Set/report machine name
		if (gb->Seen('P'))
		{
			reprap.SetName(gb->GetString());
		}
		else
		{
			reply.printf("RepRap name: %s\n", reprap.GetName());
		}
		break;

	case 551: // Set password (no option to report it)
		if (gb->Seen('P'))
		{
			reprap.SetPassword(gb->GetString());
		}
		break;

	case 552: // Enable/Disable network and/or Set/Get IP address
		{
			bool seen = false;
			if (gb->Seen('S')) // Has the user turned the network off?
			{
				seen = true;
				if (gb->GetIValue())
				{
					reprap.GetNetwork()->Enable();
				}
				else
				{
					reprap.GetNetwork()->Disable();
				}
			}

			if (gb->Seen('P'))
			{
				seen = true;
				SetEthernetAddress(gb, code);
			}

			if (gb->Seen('R'))
			{
				seen = true;
				reprap.GetNetwork()->SetHttpPort(gb->GetIValue());
			}

			if (!seen)
			{
				const byte *ip = platform->IPAddress();
				reply.printf("Network is %s, IP address: %d.%d.%d.%d, HTTP port: %d\n ",
						reprap.GetNetwork()->IsEnabled() ? "enabled" : "disabled",
						ip[0], ip[1], ip[2], ip[3], reprap.GetNetwork()->GetHttpPort());
			}

		}
		break;

	case 553: // Set/Get netmask
		if (gb->Seen('P'))
		{
			SetEthernetAddress(gb, code);
		}
		else
		{
			const byte *nm = platform->NetMask();
			reply.printf("Net mask: %d.%d.%d.%d\n ", nm[0], nm[1], nm[2], nm[3]);
		}
		break;

	case 554: // Set/Get gateway
		if (gb->Seen('P'))
		{
			SetEthernetAddress(gb, code);
		}
		else
		{
			const byte *gw = platform->GateWay();
			reply.printf("Gateway: %d.%d.%d.%d\n ", gw[0], gw[1], gw[2], gw[3]);
		}
		break;

	case 555: // Set/report firmware type to emulate
		if (gb->Seen('P'))
		{
			platform->SetEmulating((Compatibility) gb->GetIValue());
		}
		else
		{
			reply.copy("Emulating ");
			switch(platform->Emulating())
			{
				case me:
				case reprapFirmware:
					reply.cat("RepRap Firmware (i.e. in native mode)");
					break;

				case marlin:
					reply.cat("Marlin");
					break;

				case teacup:
					reply.cat("Teacup");
					break;

				case sprinter:
					reply.cat("Sprinter");
					break;

				case repetier:
					reply.cat("Repetier");
					break;

				default:
					reply.catf("Unknown: (%d)", platform->Emulating());
			}
			reply.cat("\n");
		}
		break;

	case 556: // Axis compensation
		if (gb->Seen('S'))
		{
			float value = gb->GetFValue();
			for (int8_t axis = 0; axis < AXES; axis++)
			{
				if (gb->Seen(axisLetters[axis]))
				{
					reprap.GetMove()->SetAxisCompensation(axis, gb->GetFValue() / value);
				}
			}
		}
		else
		{
			reply.printf("Axis compensations - XY: %.5f, YZ: %.5f, ZX: %.5f\n",
					reprap.GetMove()->AxisCompensation(X_AXIS),
					reprap.GetMove()->AxisCompensation(Y_AXIS),
					reprap.GetMove()->AxisCompensation(Z_AXIS));
		}
		break;

	case 557: // Set/report Z probe point coordinates
		if (gb->Seen('P'))
		{
			int point = gb->GetIValue();
			bool seen = false;
			if (gb->Seen(axisLetters[X_AXIS]))
			{
				reprap.GetMove()->SetXBedProbePoint(point, gb->GetFValue());
				seen = true;
			}
			if (gb->Seen(axisLetters[Y_AXIS]))
			{
				reprap.GetMove()->SetYBedProbePoint(point, gb->GetFValue());
				seen = true;
			}

			if(!seen)
			{
				reply.printf("Probe point %d - [%.1f, %.1f]\n", point,
						reprap.GetMove()->XBedProbePoint(point),
						reprap.GetMove()->YBedProbePoint(point));
			}
		}
		break;

    case 558: // Set or report Z probe type and for which axes it is used
		{
			bool seenP = false, seenR = false;
			if (gb->Seen('P'))
			{
				platform->SetZProbeType(gb->GetIValue());
				seenP = true;
			}

			bool zProbeAxes[AXES];
			platform->GetZProbeAxes(zProbeAxes);
			for (size_t axis=0; axis<AXES; axis++)
			{
				if (gb->Seen(axisLetters[axis]))
				{
					zProbeAxes[axis] = (gb->GetIValue() > 0);
					seenP = true;
				}
			}

			if (gb->Seen('R'))
			{
				platform->SetZProbeChannel(gb->GetIValue());
				seenR = true;
			}

			if (seenP)
			{
				platform->SetZProbeAxes(zProbeAxes);
			}
			else if (!seenR)
			{
				reply.printf("Z Probe type is %d on channel %d and it is used for these axes:", platform->GetZProbeType(), platform->GetZProbeChannel());
				for(size_t axis=0; axis<AXES; axis++)
				{
					if (zProbeAxes[axis])
					{
						reply.catf(" %c", axisLetters[axis]);
					}
				}
				reply.cat("\n");
			}
		}
    	break;

	case 559: // Upload config.g or another gcode file to put in the sys directory
		{
			const char* str = (gb->Seen('P') ? gb->GetString() : platform->GetConfigFile());
			bool ok = OpenFileToWrite(platform->GetSysDir(), str, gb);
			if (ok)
			{
				reply.printf("Writing to file: %s\n", str);
			}
			else
			{
				reply.printf("Can't open file %s for writing.\n", str);
				error = true;
			}
		}
		break;

	case 560: // Upload reprap.htm or another web interface file
		{
			const char* str = (gb->Seen('P') ? gb->GetString() : INDEX_PAGE);
			bool ok = OpenFileToWrite(platform->GetWebDir(), str, gb);
			if (ok)
			{
				reply.printf("Writing to file: %s\n", str);
			}
			else
			{
				reply.printf("Can't open file %s for writing.\n", str);
				error = true;
			}
		}
		break;

	case 561: // Disable bed transform
		reprap.GetMove()->SetIdentityTransform();
		break;

	case 563: // Define or remove a tool
		ManageTool(gb, reply);
		break;

    case 566: // Set/print minimum feedrates
		{
			bool seen = false;
			for(int8_t axis = 0; axis < AXES; axis++)
			{
				if(gb->Seen(axisLetters[axis]))
				{
					platform->SetInstantDv(axis, gb->GetFValue() * distanceScale * secondsToMinutes); // G Code feedrates are in mm/minute; we need mm/sec
					seen = true;
				}
			}

			if(gb->Seen(EXTRUDE_LETTER))
			{
				seen = true;
				float eVals[DRIVES-AXES];
				int eCount = DRIVES-AXES;
				gb->GetFloatArray(eVals, eCount);
				for(uint8_t e = 0; e < eCount; e++)
				{
					platform->SetInstantDv(AXES + e, eVals[e] * distanceScale * secondsToMinutes);
				}
			}
			else if(!seen)
			{
				reply.printf("Minimum feedrates: X: %.1f, Y: %.1f, Z: %.1f, E: ",
						platform->InstantDv(X_AXIS)/(distanceScale * secondsToMinutes), platform->InstantDv(Y_AXIS)/(distanceScale * secondsToMinutes),
						platform->InstantDv(Z_AXIS)/(distanceScale * secondsToMinutes));
				for(int8_t drive = AXES; drive < DRIVES; drive++)
				{
					reply.catf("%.1f%c", platform->InstantDv(drive) / (distanceScale * secondsToMinutes),
							(drive < DRIVES - 1) ? ":" : "\n");
				}
			}
		}
		break;

	case 567: // Set/report tool mix ratios
		if(gb->Seen('P'))
		{
			int8_t tNumber = gb->GetIValue();
			Tool* tool = reprap.GetTool(tNumber);
			if(tool != NULL)
			{
				if(gb->Seen(EXTRUDE_LETTER))
				{
					float eVals[DRIVES-AXES];
					int eCount = tool->DriveCount();
					gb->GetFloatArray(eVals, eCount);
					if(eCount != tool->DriveCount())
					{
						reply.printf("Setting mix ratios - wrong number of E drives: %s\n", gb->Buffer());
					}
					else
					{
						tool->DefineMix(eVals);
					}
				}
				else
				{
					reply.printf("Tool %d mix ratios: ", tNumber);
					char sep = ':';
					for(uint8_t drive = 0; drive < tool->DriveCount(); drive++)
					{
						reply.catf("%.3f%c", tool->GetMix()[drive], sep);
						if(drive >= tool->DriveCount() - 2)
						{
							sep = '\n';
						}
					}
				}
			}
		}
		break;

	case 568: // Turn on/off automatic tool mixing
		if(gb->Seen('P'))
		{
			Tool* tool = reprap.GetTool(gb->GetIValue());
			if(tool != NULL)
			{
				if(gb->Seen('S'))
					if(gb->GetIValue() != 0)
						tool->TurnMixingOn();
					else
						tool->TurnMixingOff();
			}
		}
		break;

	case 570: // Set/report heater timeout
		if(gb->Seen('S'))
		{
			platform->SetTimeToHot(gb->GetFValue());
		}
		else
		{
			reply.printf("Time allowed to get to temperature: %.1f seconds.\n", platform->TimeToHot());
		}
		break;

	case 571: // Set output on extrude
		if (gb->Seen('S'))
		{
			platform->SetExtrusionAncilliaryPWM(gb->GetFValue());
		}
		else
		{
			reply.printf("Extrusion ancillary PWM: %.3f.\n", platform->GetExtrusionAncilliaryPWM());
		}
		break;

	case 573: // Report heater PWM
		if (gb->Seen('P'))
		{
			int heater = gb->GetIValue();
			if (heater >= 0 && heater < HEATERS)
			{
				reply.printf("Average heater %d PWM: %.3f.\n", heater, reprap.GetHeat()->GetAveragePWM(heater));
			}
			else
			{
				reply.printf("Invalid heater number: %d\n", heater);
			}
		}
		break;

	case 575: // Set communications parameters
		if (gb->Seen('P'))
		{
			size_t chan = gb->GetIValue();
			if (chan < NUM_SERIAL_CHANNELS)
			{
				bool seen = false;
				if (gb->Seen('B'))
				{
					platform->SetBaudRate(chan, gb->GetIValue());
					seen = true;
				}
				if (gb->Seen('S'))
				{
					uint32_t val = gb->GetIValue();
					platform->SetCommsProperties(chan, val);
					switch (chan)
					{
						case 0:
							serialGCode->SetCommsProperties(val);
							break;
						case 1:
							auxGCode->SetCommsProperties(val);
							break;
						default:
							break;
					}
					seen = true;
				}
				if (!seen)
				{
					uint32_t cp = platform->GetCommsProperties(chan);
					reply.printf("Channel %d: baud rate %d, %s checksum\n", chan, platform->GetBaudRate(chan),
								(cp & 1) ? "requires" : "does not require");
				}
			}
		}
		break;

    case 906: // Set/report Motor currents
		{
			bool seen = false;
			for(size_t axis = 0; axis < AXES; axis++)
			{
				if(gb->Seen(axisLetters[axis]))
				{
					platform->SetMotorCurrent(axis, gb->GetFValue());
					seen = true;
				}
			}

			if(gb->Seen(EXTRUDE_LETTER))
			{
				float eVals[DRIVES-AXES];
				int eCount = DRIVES-AXES;
				gb->GetFloatArray(eVals, eCount);
				for(size_t e = 0; e < eCount; e++)
				{
					platform->SetMotorCurrent(AXES + e, eVals[e]);
				}
			}

			if (gb->Seen('I'))
			{
				float idleFactor = gb->GetFValue();
				if (idleFactor >= 0 && idleFactor <= 100.0)
				{
					platform->SetIdleCurrentFactor(idleFactor/100.0);
					seen = true;
				}
			}

			if (!seen)
			{
				reply.printf("Axis currents (mA) - X:%.1f, Y:%.1f, Z:%.1f, E:",
						platform->MotorCurrent(X_AXIS), platform->MotorCurrent(Y_AXIS),
						platform->MotorCurrent(Z_AXIS));
				for(size_t drive = AXES; drive < DRIVES; drive++)
				{
					reply.catf("%.1f%c", platform->MotorCurrent(drive), (drive < DRIVES - 1) ? ':' : ',');
				}
				reply.catf(" idle factor %d\n", (int)(platform->GetIdleCurrentFactor() * 100.0));
			}
		}
		break;

	case 998: // Request resend of line
		if (gb->Seen('P'))
		{
			reply.printf("%d\n", gb->GetIValue());
			resend = true;
		}
		break;

	//****************************
	// These last are M codes only for the cognoscenti

	case 562: // Reset temperature fault - use with great caution
		if (gb->Seen('P'))
		{
			int heater = gb->GetIValue();
			reprap.ClearTemperatureFault(heater);
		}
		break;

    case 564: // Think outside the box?
    	if(gb->Seen('S'))
    	{
    	    limitAxes = (gb->GetIValue() != 0);
    	}
		break;

    case 569: // Set/report axis direction
		if(gb->Seen('P'))
		{
			int8_t drive = gb->GetIValue();
			if(gb->Seen('S'))
			{
				platform->SetDirectionValue(drive, gb->GetIValue());
			}
			else
			{
				reply.printf("Drive %d is going %s.\n", drive, (platform->GetDirectionValue(drive) == FORWARDS) ? "forwards" : "backwards");
			}
		}
		break;

	case 999: // Restart after being stopped by error
		result = DoDwellTime(0.5);		// wait half a second to allow the response to be sent back to the web server, otherwise it may retry
		if (result)
		{
			platform->SoftwareReset(SoftwareResetReason::user);			// doesn't return
		}
		break;

	default:
		error = true;
		reply.printf("invalid M Code: %s\n", gb->Buffer());
	}

	if (result)
	{
		HandleReply(error, reply.Pointer(), 'M', code, resend);
	}
	return result;
}

bool GCodes::HandleTcode(GCodeBuffer* gb)
{
	bool result = true;
	if (strlen(gb->Buffer()) > 1)
	{
		int code = gb->GetIValue();
		code += gb->GetToolNumberAdjust();
		result = ChangeTool(code);
		if(result)
		{
			HandleReply(false, "", 'T', code, false);
		}
	}
	else
	{
		char reply[SHORT_STRING_LENGTH];
		Tool *tool = reprap.GetCurrentTool();
		if (tool == NULL)
		{
			strcpy(reply, "No tool is selected.\n");
		}
		else
		{
			snprintf(reply, SHORT_STRING_LENGTH, "Tool %d is selected.\n", tool->Number());
		}
		HandleReply(false, reply, 'T', 0, false);
	}
    return result;
}

bool GCodes::ChangeTool(int newToolNumber)
{
	Tool* oldTool = reprap.GetCurrentTool();
	Tool* newTool = reprap.GetTool(newToolNumber);

	// If old and new are the same still follow the sequence -
	// The user may want the macros run.

	switch(toolChangeSequence)
	{
		case 0: // Pre-release sequence for the old tool (if any)
			if (oldTool != NULL)
			{
				scratchString.printf("tfree%d.g", oldTool->Number());
				if(DoFileMacro(scratchString.Pointer()))
				{
					toolChangeSequence++;
				}
			}
			else
			{
				toolChangeSequence++;
			}
			return false;

		case 1: // Release the old tool (if any)
			if (oldTool != NULL)
			{
				reprap.StandbyTool(oldTool->Number());
			}
			toolChangeSequence++;
			return false;

		case 2: // Run the pre-tool-change macro cycle for the new tool (if any)
			if (newTool != NULL)
			{
				scratchString.printf("tpre%d.g", newToolNumber);
				if (DoFileMacro(scratchString.Pointer()))
				{
					toolChangeSequence++;
				}
			}
			else
			{
				toolChangeSequence++;
			}
			return false;

		case 3: // Select the new tool (even if it doesn't exist - that just deselects all tools)
			reprap.SelectTool(newToolNumber);
			toolChangeSequence++;
			return false;

		case 4: // Run the post-tool-change macro cycle for the new tool (if any)
			if (newTool != NULL)
			{
				scratchString.printf("tpost%d.g", newToolNumber);
				if (DoFileMacro(scratchString.Pointer()))
				{
					toolChangeSequence++;
				}
			}
			else
			{
				toolChangeSequence++;
			}
			return false;

		case 5: // All done
			toolChangeSequence = 0;
			return true;

		default:
			platform->Message(BOTH_ERROR_MESSAGE, "Tool change - dud sequence number: %d\n", toolChangeSequence);
	}

	toolChangeSequence = 0;
	return true;
}

// Cancel the current SD card print
void GCodes::CancelPrint()
{
	while (internalCodeQueue != NULL)
	{
		CodeQueueItem *item = internalCodeQueue;
		internalCodeQueue = item->Next();
		item->SetNext(releasedQueueItems);
		releasedQueueItems = item;
	}

	totalMoves = movesCompleted = 0;
	moveAvailable = isPausing = isResuming = false;
	fractionOfFilePrinted = -1.0;

	fileGCode->Init();
	queuedGCode->Init();

	if (fileBeingPrinted.IsLive())
	{
		fileBeingPrinted.Close();
	}
	reprap.GetMove()->Cancel();

	reprap.GetPrintMonitor()->StoppedPrint();
}

// Return true if all the heaters for the specified tool are at their set temperatures
bool GCodes::ToolHeatersAtSetTemperatures(const Tool *tool) const
{
    if (tool != NULL)
    {
		for (int i = 0; i < tool->HeaterCount(); ++i)
		{
			if (!reprap.GetHeat()->HeaterAtSetTemperature(tool->Heater(i)))
			{
				return false;
			}
		}
    }
    return true;
}

// Called by the look-ahead to indicate a new (real) move
void GCodes::MoveQueued()
{
	totalMoves++;
}

// Called by the DDA class to indicate that a move has been completed (called by ISR)
void GCodes::MoveCompleted()
{
	movesCompleted++;
}

bool GCodes::HaveAux() const
{
	return auxDetected;
}

bool GCodes::IsPausing() const
{
	return (isPausing || reprap.GetMove()->IsPausing());
}

bool GCodes::IsResuming() const
{
	return (isResuming || reprap.GetMove()->IsResuming());
}

//*************************************************************************************

// This class is used to ensure commands are executed in the right order and independently from the look-ahead queue.

CodeQueueItem::CodeQueueItem(CodeQueueItem *n) : next(n), commandLength(0), source(NULL)
{
	command[0] = 0;
}

void CodeQueueItem::Init(GCodeBuffer *gb, unsigned int executeAtMove)
{
	moveToExecute = executeAtMove;
	next = NULL;
	executing = false;

	commandLength = strlen(gb->Buffer());
	if (commandLength >= ARRAY_SIZE(command))
	{
		reprap.GetPlatform()->Message(BOTH_ERROR_MESSAGE, "Invalid string passed to code queue initializer\n");
		command[0] = 0;
		commandLength = 0;
		source = NULL;
	}
	else
	{
		strncpy(command, gb->Buffer(), commandLength);
		command[commandLength] = 0;
		source = gb;
	}
}

void CodeQueueItem::SetNext(CodeQueueItem *n)
{
	next = n;
}

CodeQueueItem *CodeQueueItem::Next() const
{
	return next;
}

unsigned int CodeQueueItem::ExecuteAtMove() const
{
	return moveToExecute;
}

const char *CodeQueueItem::GetCommand() const
{
	return command;
}

size_t CodeQueueItem::GetCommandLength() const
{
	return commandLength;
}

GCodeBuffer *CodeQueueItem::GetSource() const
{
	return source;
}


void CodeQueueItem::Execute()
{
	executing = true;
}

bool CodeQueueItem::IsExecuting() const
{
	return executing;
}


//*************************************************************************************

// This class stores a single G Code and provides functions to allow it to be parsed

GCodeBuffer::GCodeBuffer(Platform* p, const char* id)
{
	platform = p;
	identity = id;
	writingFileDirectory = NULL; // Has to be done here as Init() is called every line.
	toolNumberAdjust = 0;
	checksumRequired = false;
}

void GCodeBuffer::Init()
{
	gcodePointer = 0;
	readPointer = -1;
	inComment = false;
	state = idle;
}

int GCodeBuffer::CheckSum()
{
	int cs = 0;
	for (int i = 0; gcodeBuffer[i] != '*' && gcodeBuffer[i] != 0; i++)
	{
		cs = cs ^ gcodeBuffer[i];
	}
	cs &= 0xff;  // Defensive programming...
	return cs;
}

// Add a byte to the code being assembled.  If false is returned, the code is
// not yet complete.  If true, it is complete and ready to be acted upon.
bool GCodeBuffer::Put(char c)
{
	if (c == '\r')
	{
		// Ignore carriage return, it messes up filenames sometimes if it appears in macro files etc.
		// Alternatively, we could handle it in the same way as linefeed, and add an optimisation to ignore blank lines.
		return false;
	}

	gcodeBuffer[gcodePointer] = c;

	if (c == ';')
	{
		inComment = true;
	}
	else if (c == '\n' || !c)
	{
		gcodeBuffer[gcodePointer] = 0;
		Init();
		if (reprap.Debug(moduleGcodes) && gcodeBuffer[0] && !writingFileDirectory) // Don't bother with blank/comment lines
		{
			platform->Message(HOST_MESSAGE, "%s%s\n", identity, gcodeBuffer);
		}

		// Deal with line numbers and checksums

		if (Seen('*'))
		{
			int csSent = GetIValue();
			int csHere = CheckSum();
			Seen('N');
			if (csSent != csHere)
			{
				snprintf(gcodeBuffer, GCODE_LENGTH, "M998 P%d", GetIValue());
				Init();
				return true;
			}

			// Strip out the line number and checksum

			gcodePointer = 0;
			while (gcodeBuffer[gcodePointer] != ' ' && gcodeBuffer[gcodePointer])
			{
				gcodePointer++;
			}

			// Anything there?

			if (!gcodeBuffer[gcodePointer])
			{
				// No...
				gcodeBuffer[0] = 0;
				Init();
				return false;
			}

			// Yes...

			gcodePointer++;
			int gp2 = 0;
			while (gcodeBuffer[gcodePointer] != '*' && gcodeBuffer[gcodePointer])
			{
				gcodeBuffer[gp2] = gcodeBuffer[gcodePointer++];
				gp2++;
			}
			gcodeBuffer[gp2] = 0;
		}
		else if (checksumRequired)
		{
			gcodeBuffer[0] = 0;
			Init();
			return false;
		}

		Init();
		state = executing;
		return true;
	}
	else if (!inComment || writingFileDirectory)
	{
		gcodeBuffer[gcodePointer++] = c;
		if (gcodePointer >= GCODE_LENGTH)
		{
			platform->Message(BOTH_ERROR_MESSAGE, "G-Code buffer length overflow.\n");
			gcodePointer = 0;
			gcodeBuffer[0] = 0;
		}
	}

	return false;
}

bool GCodeBuffer::Put(const char *str, size_t len)
{
	for(size_t i=0; i<=len; i++)
	{
		if (Put(str[i]))
		{
			return true;
		}
	}

	return false;
}

// Does this buffer contain any code?

bool GCodeBuffer::IsEmpty() const
{
	const char *buf = gcodeBuffer;
	while (*buf && strchr(" \t\n\r", *buf))
	{
		buf++;
	}
	return *buf == 0;
}

// How many bytes have been fed into this buffer (including terminating character)

unsigned int GCodeBuffer::Length() const
{
	unsigned int length = 0;
	const char *buf = gcodeBuffer;
	while (*buf++ != 0)
	{
		length++;
	}
	return length + 1;
}

// Is 'c' in the G Code string?
// Leave the pointer there for a subsequent read.

bool GCodeBuffer::Seen(char c)
{
	readPointer = 0;
	for (;;)
	{
		char b = gcodeBuffer[readPointer];
		if (b == 0 || b == ';') break;
		if (b == c) return true;
		++readPointer;
	}
	readPointer = -1;
	return false;
}

// Get a float after a G Code letter found by a call to Seen()

float GCodeBuffer::GetFValue()
{
	if (readPointer < 0)
	{
		platform->Message(BOTH_ERROR_MESSAGE, "GCodes: Attempt to read a GCode float before a search.\n");
		readPointer = -1;
		return 0.0;
	}
	float result = (float) strtod(&gcodeBuffer[readPointer + 1], 0);
	readPointer = -1;
	return result;
}

// Get a :-separated list of floats after a key letter

const void GCodeBuffer::GetFloatArray(float a[], int& returnedLength)
{
	int length = 0;
	if(readPointer < 0)
	{
		platform->Message(BOTH_ERROR_MESSAGE, "GCodes: Attempt to read a GCode float array before a search.\n");
		readPointer = -1;
		returnedLength = 0;
		return;
	}

	bool inList = true;
	while(inList)
	{
		if(length >= returnedLength) // Array limit has been set in here
		{
			platform->Message(BOTH_ERROR_MESSAGE, "GCodes: Attempt to read a GCode float array that is too long: %s\n", gcodeBuffer);
			readPointer = -1;
			returnedLength = 0;
			return;
		}
		a[length] = (float)strtod(&gcodeBuffer[readPointer + 1], 0);
		length++;
		readPointer++;
		while(gcodeBuffer[readPointer] && (gcodeBuffer[readPointer] != ' ') && (gcodeBuffer[readPointer] != LIST_SEPARATOR))
		{
			readPointer++;
		}
		if(gcodeBuffer[readPointer] != LIST_SEPARATOR)
		{
			inList = false;
		}
	}

	// Special case if there is one entry and returnedLength requests several.
	// Fill the array with the first entry.

	if(length == 1 && returnedLength > 1)
	{
		for(int8_t i = 1; i < returnedLength; i++)
		{
			a[i] = a[0];
		}
	}
	else
	{
		returnedLength = length;
	}

	readPointer = -1;
}

// Get a :-separated list of longs after a key letter

const void GCodeBuffer::GetLongArray(long l[], int& returnedLength)
{
	int length = 0;
	if(readPointer < 0)
	{
		platform->Message(BOTH_ERROR_MESSAGE, "GCodes: Attempt to read a GCode long array before a search.\n");
		readPointer = -1;
		return;
	}

	bool inList = true;
	while(inList)
	{
		if(length >= returnedLength) // Array limit has been set in here
		{
			platform->Message(BOTH_ERROR_MESSAGE, "GCodes: Attempt to read a GCode long array that is too long: %s\n", gcodeBuffer);
			readPointer = -1;
			returnedLength = 0;
			return;
		}
		l[length] = strtol(&gcodeBuffer[readPointer + 1], 0, 0);
		length++;
		readPointer++;
		while(gcodeBuffer[readPointer] && (gcodeBuffer[readPointer] != ' ') && (gcodeBuffer[readPointer] != LIST_SEPARATOR))
		{
			readPointer++;
		}
		if(gcodeBuffer[readPointer] != LIST_SEPARATOR)
		{
			inList = false;
		}
	}
	returnedLength = length;
	readPointer = -1;
}

// Get a string after a G Code letter found by a call to Seen().
// It will be the whole of the rest of the GCode string, so strings
// should always be the last parameter.

const char* GCodeBuffer::GetString()
{
	if (readPointer < 0)
	{
		platform->Message(BOTH_ERROR_MESSAGE, "GCodes: Attempt to read a GCode string before a search.\n");
		readPointer = -1;
		return "";
	}
	const char* result = &gcodeBuffer[readPointer + 1];
	readPointer = -1;
	return result;
}

// This returns a pointer to the end of the buffer where a
// string starts.  It assumes that an M or G search has
// been done followed by a GetIValue(), so readPointer will
// be -1.  It absorbs "M/Gnnn " (including the space) from the
// start and returns a pointer to the next location.

// This is provided for legacy use, in particular in the M23
// command that sets the name of a file to be printed.  In
// preference use GetString() which requires the string to have
// been preceded by a tag letter.

const char* GCodeBuffer::GetUnprecedentedString(bool optional)
{
	readPointer = 0;
	while (gcodeBuffer[readPointer] && gcodeBuffer[readPointer] != ' ')
	{
		readPointer++;
	}

	if (!gcodeBuffer[readPointer])
	{
		readPointer = -1;
		if (optional)
		{
			return NULL;
		}
		platform->Message(BOTH_ERROR_MESSAGE, "GCodes: String expected but not seen.\n");
		return gcodeBuffer; // Good idea?
	}

	const char* result = &gcodeBuffer[readPointer + 1];
	readPointer = -1;
	return result;
}

// Get an long after a G Code letter

long GCodeBuffer::GetLValue()
{
	if (readPointer < 0)
	{
		platform->Message(BOTH_ERROR_MESSAGE, "GCodes: Attempt to read a GCode int before a search.\n");
		readPointer = -1;
		return 0;
	}
	long result = strtol(&gcodeBuffer[readPointer + 1], 0, 0);
	readPointer = -1;
	return result;
}

