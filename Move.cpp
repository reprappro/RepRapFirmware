/****************************************************************************************************

RepRapFirmware - Move

This is all the code to deal with movement and kinematics.

-----------------------------------------------------------------------------------------------------

Version 0.1

18 November 2012

Adrian Bowyer
RepRap Professional Ltd
http://reprappro.com

Licence: GPL

****************************************************************************************************/

#include "RepRapFirmware.h"

const float zeroExtruderPositions[DRIVES - AXES] = ZERO_EXTRUDER_POSITIONS;

Move::Move(Platform* p, GCodes* g)
{
  active = false;
  platform = p;
  gCodes = g;
  
  // Build the DDA ring
  
  ddaRingAddPointer = new DDA(this, platform, NULL);
  dda = ddaRingAddPointer;
  for(uint8_t i = 1; i < DDA_RING_LENGTH; i++)
  {
    dda = new DDA(this, platform, dda);
  }
  ddaRingAddPointer->next = dda;
  
  dda = NULL;
  
  // Build the lookahead ring
  
  lookAheadRingAddPointer = new LookAhead(this, platform, NULL);
  lookAheadRingGetPointer = lookAheadRingAddPointer;
  for(size_t i = 1; i < LOOK_AHEAD_RING_LENGTH; i++)
  {
    lookAheadRingGetPointer = new LookAhead(this, platform, lookAheadRingGetPointer);
  }
  lookAheadRingAddPointer->next = lookAheadRingGetPointer;
  
  // Set the lookahead backwards pointers (some oxymoron, surely?)
  
  lookAheadRingGetPointer = lookAheadRingAddPointer; 
  for(size_t i = 0; i <= LOOK_AHEAD_RING_LENGTH; i++)
  {
    lookAheadRingAddPointer = lookAheadRingAddPointer->Next();
    lookAheadRingAddPointer->previous = lookAheadRingGetPointer;
    lookAheadRingGetPointer = lookAheadRingAddPointer;
  }    
  
  lookAheadDDA = new DDA(this, platform, NULL);

  // We need an isolated DDA entry to perform moves in case the look-ahead queue is paused

  isolatedMove = new LookAhead(this, platform, NULL);
  isolatedMove->previous = NULL;
  ddaIsolatedMove = new DDA(this, platform, NULL);
}

void Move::Init()
{
  long ep[DRIVES];
  
  for(size_t drive = 0; drive < DRIVES; drive++)
  {
    platform->SetDirection(drive, FORWARDS);
  }
  
  // Empty the rings
  
  ddaRingGetPointer = ddaRingAddPointer; 
  ddaRingLocked = false;
  
  for(uint8_t i = 0; i <= LOOK_AHEAD_RING_LENGTH; i++)
  {
    lookAheadRingAddPointer->Release();
    lookAheadRingAddPointer = lookAheadRingAddPointer->Next();
  }
  
  lookAheadRingGetPointer = lookAheadRingAddPointer;
  lookAheadRingCount = 0;
  
  addNoMoreMoves = false;

  // Put the origin on the lookahead ring with default velocity in the previous
  // position to the first one that will be used.
  
  lastRingMove = lookAheadRingAddPointer->Previous();
  
  for(size_t drive = 0; drive < DRIVES; drive++)
  {
	  ep[drive] = 0;
	  liveCoordinates[drive] = 0.0;
  }
  for(size_t extruder = 0; extruder < DRIVES - AXES; extruder++)
  {
	  rawExtruderPos[extruder] = 0.0;
  }

  int8_t slow = platform->SlowestDrive();
  lastRingMove->Init(ep, platform->HomeFeedRate(slow), platform->InstantDv(slow), platform->MaxFeedrate(slow), platform->Acceleration(slow), 0, zeroExtruderPositions);
  lastRingMove->Release();
  isolatedMove->Init(ep, platform->HomeFeedRate(slow), platform->InstantDv(slow), platform->MaxFeedrate(slow), platform->Acceleration(slow), 0, zeroExtruderPositions);
  isolatedMove->Release();
  readIsolatedMove = isolatedMoveAvailable = false;

  currentFeedrate = liveCoordinates[DRIVES] = platform->HomeFeedRate(slow);

  SetIdentityTransform();
  tanXY = 0.0;
  tanYZ = 0.0;
  tanXZ = 0.0;

  lastZHit = 0.0;
  zProbing = false;

  for(uint8_t point = 0; point < NUMBER_OF_PROBE_POINTS; point++)
  {
	  xBedProbePoints[point] = (0.3 + 0.6*(float)(point%2))*platform->AxisMaximum(X_AXIS);
	  yBedProbePoints[point] = (0.0 + 0.9*(float)(point/2))*platform->AxisMaximum(Y_AXIS);
	  zBedProbePoints[point] = 0.0;
	  probePointSet[point] = unset;
  }

  xRectangle = 1.0/(0.8*platform->AxisMaximum(X_AXIS));
  yRectangle = xRectangle;

  longWait = platform->Time();

  for(uint8_t extruder = 0; extruder < DRIVES - AXES; extruder++)
  {
    extrusionFactors[extruder] = 1.0;
  }
  speedFactor = 1.0;

  doingSplitMove = false;

  isResuming = false;
  state = running;
  active = true;
}

void Move::Exit()
{
  platform->Message(BOTH_MESSAGE, "Move class exited.\n");
  active = false;
}

void Move::Spin()
{
	if (!active)
		return;

	// Do some look-ahead work, if there's any to do

	DoLookAhead();

	// If there's space in the DDA ring, and there are completed moves in the look-ahead ring, transfer them.

	if (!DDARingFull())
	{
		LookAhead* nextFromLookAhead = LookAheadRingGet();
		if (nextFromLookAhead != NULL)
		{
			if (!DDARingAdd(nextFromLookAhead))
			{
				platform->Message(BOTH_ERROR_MESSAGE, "Can't add to non-full DDA ring!\n"); // Should never happen...
			}
		}
	}

	// If we're paused and there is no live movement, see if we can perform an isolated move.

	if (IsPaused() && isolatedMoveAvailable)
	{
		if (GetDDARingLock())
		{
			readIsolatedMove = true;
			isolatedMoveAvailable = false;
			ReleaseDDARingLock();
		}

		platform->ClassReport(longWait);
		return;
	}

	// If we're done purging all pending moves, see if we can reset our properties again.

	if (IsCancelled())
	{
		if (LookAheadRingEmpty() && DDARingEmpty())
		{
			// Make sure the last look-ahead entry points to the same coordinates we're at right now

			float currentCoordinates[DRIVES + 1];
			for(uint8_t axis=0; axis<AXES; axis++)
			{
				currentCoordinates[axis] = liveCoordinates[axis];
			}
			currentCoordinates[DRIVES] = currentFeedrate;
			SetPositions(currentCoordinates);

			// We've skipped all incoming moves, so reset our state again

			lookAheadRingAddPointer->Release();
			doingSplitMove = false;
			state = running;
		}

		platform->ClassReport(longWait);
		return;
	}

	// If we either don't want to, or can't, add to the look-ahead ring, go home.

	const bool splitNextMove = IsRunning() && doingSplitMove;
	if ((!splitNextMove && addNoMoreMoves) || LookAheadRingFull() || isolatedMoveAvailable)
	{
		platform->ClassReport(longWait);
		return;
	}

	// We don't need to obtain any move if we're still busy processing one.

	EndstopChecks endStopsToCheck = 0;
	if (splitNextMove)
	{
		for(size_t drive=0; drive<DRIVES; drive++)
		{
			nextMove[drive] = splitMove[drive];
		}
	}

	// Read a new move and apply extrusion factors right away.

	else if (gCodes->ReadMove(nextMove, endStopsToCheck))
	{
		for(size_t drive = AXES; drive < DRIVES; drive++)
		{
			rawEDistances[drive - AXES] = nextMove[drive];
			nextMove[drive] *= extrusionFactors[drive - AXES];
		}

		currentFeedrate = nextMove[DRIVES]; // Might be G1 with just an F field
	}

	// We cannot process any moves, so stop here.

	else
	{
		platform->ClassReport(longWait);
		return;
	}

	// If there's a new move available, split it up and add it to the look-ahead ring for processing.

	if (endStopsToCheck == 0)
	{
		doingSplitMove = SplitNextMove(); // TODO: Make this work with more than one inner probe point
	}

	Transform(nextMove);

	const LookAhead *lastMove = (IsPaused()) ? isolatedMove : lastRingMove;
	bool noMove = true;
	for(size_t drive = 0; drive < DRIVES; drive++)
	{
		nextMachineEndPoints[drive] = LookAhead::EndPointToMachine(drive, nextMove[drive]);
		if (drive < AXES)
		{
			if (nextMachineEndPoints[drive] - lastMove->MachineCoordinates()[drive] != 0)
			{
				platform->EnableDrive(drive);
				noMove = false;
			}
			normalisedDirectionVector[drive] = nextMove[drive] - lastMove->MachineToEndPoint(drive);
		}
		else
		{
			if (nextMachineEndPoints[drive] != 0)
			{
				platform->EnableDrive(drive);
				noMove = false;
			}
			normalisedDirectionVector[drive] = nextMove[drive];
		}
	}

	// Throw it away if there's no real movement.

	if (noMove)
	{
		platform->ClassReport(longWait);
		return;
	}

	// Compute the direction of motion, moved to the positive hyperquadrant

	Absolute(normalisedDirectionVector, DRIVES);
	if (Normalise(normalisedDirectionVector, DRIVES) <= 0.0)
	{
		platform->Message(BOTH_ERROR_MESSAGE, "Attempt to normalise zero-length move.\n");  // Should never get here - noMove above
		platform->ClassReport(longWait);
		return;
	}

	// Set the feedrate maximum and minimum, and the acceleration

	float minSpeed = VectorBoxIntersection(normalisedDirectionVector, platform->InstantDvs(), DRIVES);
	float acceleration = VectorBoxIntersection(normalisedDirectionVector, platform->Accelerations(), DRIVES);
	float maxSpeed = VectorBoxIntersection(normalisedDirectionVector, platform->MaxFeedrates(), DRIVES);

	if (IsPaused())
	{
		// Do not pass raw extruder distances here, because they would mess around with print time estimation
		if (!SetUpIsolatedMove(nextMachineEndPoints, currentFeedrate, minSpeed, maxSpeed, acceleration, endStopsToCheck))
		{
			platform->Message(BOTH_ERROR_MESSAGE, "Couldn't set up isolated move!\n");
		}
	}
	else
	{
		const float feedRate = (endStopsToCheck == 0) ? currentFeedrate * speedFactor : currentFeedrate;
		const float *unmodifiedEDistances = (doingSplitMove) ? zeroExtruderPositions : rawEDistances;
		if (LookAheadRingAdd(nextMachineEndPoints, feedRate, minSpeed, maxSpeed, acceleration, endStopsToCheck, unmodifiedEDistances))
		{
			// Tell GCodes class we're about to perform a new (regular) move
			reprap.GetGCodes()->MoveQueued();
		}
		else
		{
			platform->Message(BOTH_ERROR_MESSAGE, "Can't add to non-full look ahead ring!\n"); // Should never happen...
		}
	}

	platform->ClassReport(longWait);
}

/* Check if we need to split up the next move to make 5-point bed compensation work well.
 * Do this by verifying whether we cross either X or Y of the fifth bed compensation point.
 *
 * Returns true if the next move has been split up
 */

bool Move::SplitNextMove()
{
	if (!IsRunning() || doingSplitMove || identityBedTransform || NumberOfProbePoints() != 5)
		return false;

	// Get the last untransformed XYZ coordinates

	float lastXYZ[AXES];
	for(uint8_t axis=0; axis<AXES; axis++)
	{
		lastXYZ[axis] = lastRingMove->MachineToEndPoint(axis);
	}
	InverseTransform(lastXYZ);

	// Are we crossing X coordinate of 5th bed compensation point?

	const float x1 = lastXYZ[X_AXIS];
	const float x2 = nextMove[X_AXIS];
	const float xCenter = xBedProbePoints[4];
	bool crossingX = false;
	float scaleX;

	if ((fabs(x2 - x1) > MINIMUM_SPLIT_DISTANCE) && ((x1 < xCenter && x2 > xCenter) || (x2 < xCenter && x1 > xCenter)))
	{
		crossingX = true;
		scaleX = (xCenter - x1) / (x2 - x1);
	}

	// Are we crossing Y coordinate of 5th bed compensation point?

	const float y1 = lastXYZ[Y_AXIS];
	const float y2 = nextMove[Y_AXIS];
	const float yCenter = yBedProbePoints[4];
	bool crossingY = false;
	float scaleY;

	if ((fabs(y2 - y1) > MINIMUM_SPLIT_DISTANCE) && ((y1 < yCenter && y2 > yCenter) || (y2 < yCenter && y1 > yCenter)))
	{
		crossingY = true;
		scaleY = (yCenter - y1) / (y2 - y1);
	}

	// Split components of the next move proportionally into two move endpoints

	if (crossingX || crossingY)
	{
		float splitFactor;
		if (crossingX && crossingY)
		{
			splitFactor = 0.5 * (scaleX + scaleY);
		}
		else
		{
			splitFactor = (crossingX) ? scaleX : scaleY;
		}

		for(uint8_t drive=0; drive<DRIVES; drive++)
		{
			if (drive < AXES)
			{
				splitMove[drive] = nextMove[drive];
				nextMove[drive] = lastXYZ[drive] + (nextMove[drive] - lastXYZ[drive]) * splitFactor;
			}
			else
			{
				splitMove[drive] = nextMove[drive] * (1.0 - splitFactor);
				nextMove[drive] *= splitFactor;
			}
		}

		return true;
	}

	return false;
}


/*
 * Take a unit positive-hyperquadrant vector, and return the factor needed to obtain
 * length of the vector as projected to touch box[].
 */

float Move::VectorBoxIntersection(const float v[], const float box[], int8_t dimensions)
{
	// Generate a vector length that is guaranteed to exceed the size of the box

	float biggerThanBoxDiagonal = 2.0*Magnitude(box, dimensions);
	float magnitude = biggerThanBoxDiagonal;
	for(int8_t d = 0; d < dimensions; d++)
	{
		if(biggerThanBoxDiagonal*v[d] > box[d])
		{
			float a = box[d]/v[d];
			if(a < magnitude)
			{
				magnitude = a;
			}
		}
	}
	return magnitude;
}

// Normalise a vector, and also return its previous magnitude
// If the vector is of 0 length, return a negative magnitude

float Move::Normalise(float v[], int8_t dimensions)
{
	float magnitude = Magnitude(v, dimensions);
	if(magnitude <= 0.0)
		return -1.0;
	Scale(v, 1.0/magnitude, dimensions);
	return magnitude;
}

// Return the magnitude of a vector

float Move::Magnitude(const float v[], int8_t dimensions)
{
	float magnitude = 0.0;
	for(int8_t d = 0; d < dimensions; d++)
	{
		magnitude += v[d]*v[d];
	}
	magnitude = sqrt(magnitude);
	return magnitude;
}

// Multiply a vector by a scalar

void Move::Scale(float v[], float scale, int8_t dimensions)
{
	for(int8_t d = 0; d < dimensions; d++)
	{
		v[d] = scale*v[d];
	}
}

// Move a vector into the positive hyperquadrant

void Move::Absolute(float v[], int8_t dimensions)
{
	for(int8_t d = 0; d < dimensions; d++)
	{
		v[d] = fabs(v[d]);
	}
}

// These are the actual numbers we want in the positions, so don't transform them.

void Move::SetPositions(float move[])
{
	LookAhead *lastMove = (IsPaused()) ? isolatedMove : lastRingMove;
	for(uint8_t drive = 0; drive < DRIVES; drive++)
	{
		lastMove->SetDriveCoordinate(move[drive], drive);
	}
	currentFeedrate = move[DRIVES];
	lastMove->SetFeedRate(currentFeedrate);
}

void Move::Diagnostics() 
{
	platform->AppendMessage(BOTH_MESSAGE, "Move Diagnostics:\n");
	platform->AppendMessage(BOTH_MESSAGE, "State: ");
	switch (state)
	{
		case running:
			platform->AppendMessage(BOTH_MESSAGE, "running\n");
			break;

		case pausing:
			platform->AppendMessage(BOTH_MESSAGE, "pausing\n");
			break;

		case paused:
			platform->AppendMessage(BOTH_MESSAGE, "paused\n");
			break;

		case cancelled:
			platform->AppendMessage(BOTH_MESSAGE, "cancelled\n");
			break;

		default:
			platform->AppendMessage(BOTH_MESSAGE, "unknown\n");
			break;
	}

/*  if(active)
    platform->Message(HOST_MESSAGE, " active\n");
  else
    platform->Message(HOST_MESSAGE, " not active\n");
  
  platform->Message(HOST_MESSAGE, " look ahead ring count: ");
  snprintf(scratchString, STRING_LENGTH, "%d\n", lookAheadRingCount);
  platform->Message(HOST_MESSAGE, scratchString);
  if(dda == NULL)
    platform->Message(HOST_MESSAGE, " dda: NULL\n");
  else
  {
    if(dda->Active())
      platform->Message(HOST_MESSAGE, " dda: active\n");
    else
      platform->Message(HOST_MESSAGE, " dda: not active\n");
    
  }
  if(ddaRingLocked)
    platform->Message(HOST_MESSAGE, " dda ring is locked\n");
  else
    platform->Message(HOST_MESSAGE, " dda ring is not locked\n");
  if(addNoMoreMoves)
    platform->Message(HOST_MESSAGE, " addNoMoreMoves is true\n\n");
  else
    platform->Message(HOST_MESSAGE, " addNoMoreMoves is false\n\n"); 
    */
}

// Return the untransformed machine coordinates
// This returns false if it is not possible
// to use the result as the basis for the
// next move because the look ahead ring
// is full.  True otherwise.

bool Move::GetCurrentMachinePosition(float m[]) const
{
	// If moves are still running, use the last look-ahead entry to retrieve the current position
	if (IsRunning())
	{
		if(LookAheadRingFull() || doingSplitMove)
			return false;

		for(size_t drive = 0; drive < DRIVES; drive++)
		{
			m[drive] = lastRingMove->MachineToEndPoint(drive);
		}
		m[DRIVES] = currentFeedrate;
		return true;
	}
	// If there is no real movement, return liveCoordinates instead
	else if (NoLiveMovement())
	{
		for(size_t drive = 0; drive <= DRIVES; drive++)
		{
			m[drive] = liveCoordinates[drive];
		}
		return true;
	}
	return false;
}

// Return the transformed machine coordinates

bool Move::GetCurrentUserPosition(float m[]) const
{
	if(!GetCurrentMachinePosition(m))
		return false;
	InverseTransform(m);
	return true;
}

// Take an item from the look-ahead ring and add it to the DDA ring, if
// possible.

bool Move::DDARingAdd(LookAhead* lookAhead)
{
  if(GetDDARingLock())
  {
    if(DDARingFull())
    {
      ReleaseDDARingLock();
      return false;
    }
    if(ddaRingAddPointer->Active())  // Should never happen...
    {
      platform->Message(BOTH_ERROR_MESSAGE, "Attempt to alter an active ring buffer entry!\n");
      ReleaseDDARingLock();
      return false;
    }

    // We don't care about Init()'s return value - that should all have been sorted out by LookAhead.
    
    float u, v;
    ddaRingAddPointer->Init(lookAhead, u, v);
    ddaRingAddPointer = ddaRingAddPointer->Next();
    ReleaseDDARingLock();
    return true;
  }
  return false;
}

// Get a movement from the DDA ring or from an isolated move, if we can.

DDA* Move::DDARingGet()
{
	DDA* result = NULL;
	if(GetDDARingLock())
	{
		// If we're paused and have a valid DDA, perform an isolated move

		if (IsPaused())
		{
			if (readIsolatedMove)
			{
				result = ddaIsolatedMove;
				readIsolatedMove = false;
			}

			ReleaseDDARingLock();
			return result;
		}

		// If we've finished the last move while pausing or ran out of moves, stop here

		if (IsPausing() || DDARingEmpty())
		{
			ReleaseDDARingLock();
			return NULL;
		}

		// Get an ordinary entry from the DDA ring

		result = ddaRingGetPointer;
		ddaRingGetPointer = ddaRingGetPointer->Next();
		ReleaseDDARingLock();
		return result;
	}
	return NULL;
}

// Do the look-ahead calculations

void Move::DoLookAhead()
{
	if ((!IsRunning() && !IsCancelled()) || LookAheadRingEmpty())
	{
		return;
	}

	LookAhead* n0;
	LookAhead* n1;
	LookAhead* n2;

	// If there are a reasonable number of moves in there (LOOK_AHEAD), or if we are
	// doing single moves with no other move immediately following on, run up and down
	// the moves using the DDA Init() function to reduce the start or the end speed
	// or both to the maximum that can be achieved because of the requirements of
	// the adjacent moves.

	if(addNoMoreMoves || !gCodes->HaveIncomingData() || lookAheadRingCount > LOOK_AHEAD)
	{

		// Run up the moves

		n1 = lookAheadRingGetPointer;
		n0 = n1->Previous();
		while (n1 != lookAheadRingAddPointer)
		{
			if(!(n0->Processed() & complete))
			{
				if(n0->Processed() & vCosineSet)
				{
					float u = n0->V();
					float v = n1->V();
					if(lookAheadDDA->Init(n1, u, v) & change)
					{
						n0->SetV(u);
						n1->SetV(v);
					}
				}
			}
			n0 = n1;
			n1 = n1->Next();
		}

		// Now run down

		do
		{
			if(!(n1->Processed() & complete))
			{
				if(n1->Processed() & vCosineSet)
				{
					float u = n0->V();
					float v = n1->V();
					if(lookAheadDDA->Init(n1, u, v) & change)
					{
						n0->SetV(u);
						n1->SetV(v);
					}

					n1->SetProcessed(complete);
				}
			}
			n1 = n0;
			n0 = n0->Previous();
		} while (n0 != lookAheadRingGetPointer);
		n0->SetProcessed(complete);
	}

	// If there are any new unprocessed moves in there, set their end speeds
	// according to the cosine of the angle between them.

	if(addNoMoreMoves || !gCodes->HaveIncomingData() || lookAheadRingCount > 1)
	{
		n1 = lookAheadRingGetPointer;
		n0 = n1->Previous();
		n2 = n1->Next();
		while(n2 != lookAheadRingAddPointer)
		{
			if(n1->Processed() == unprocessed)
			{
				float c = n1->V();
				float m = min<float>(n1->MinSpeed(), n2->MinSpeed());  // FIXME we use min as one move's max may not be able to cope with the min for the other.  But should this be max?
				c = c*n1->Cosine();
				if(c < m)
				{
					c = m;
				}
				n1->SetV(c);
				n1->SetProcessed(vCosineSet);
			}
			n0 = n1;
			n1 = n2;
			n2 = n2->Next();
		}

		// If we have no more moves to process, set the last move's end velocity to an appropriate minimum speed.

		if(!doingSplitMove && (addNoMoreMoves || !gCodes->HaveIncomingData()))
		{
			n1->SetV(platform->InstantDv(platform->SlowestDrive())); // The next thing may be the slowest; be prepared.
			n1->SetProcessed(complete);
		}
	}
}

// This is the function that's called by the timer interrupt to step the motors.

void Move::Interrupt()
{
	// Have we got a live DDA?

	if(dda == NULL)
	{
		// No - see if a new one is available.

		dda = DDARingGet();
		if(dda != NULL)
		{
			if (IsCancelled())
			{
				dda->Release();		// Yes - but don't use it. All pending moves have been cancelled.
				dda = NULL;
			}
			else
			{
				dda->Start();		// Yes - got it.  So fire it up if the print is still running.
				dda->Step();		// And take the first step.
			}
		}
		return;
	}

	// We have a DDA.  Has it finished?

	if(dda->Active())
	{
		// No - it's still live.  Step it and return.

		dda->Step();
		return;
	}

	// Yes - it's finished.  Throw it away so the code above will then find a new one.

	dda->Release();
	dda = NULL;
}

// Records a new lookahead object and adds it to the lookahead ring, returns false if it's full

bool Move::LookAheadRingAdd(long ep[], float requestedFeedRate, float minSpeed, float maxSpeed,
		float acceleration, EndstopChecks ce, const float extrDiffs[])
{
    if(LookAheadRingFull())
    {
    	return false;
    }

    if(!(lookAheadRingAddPointer->Processed() & released)) // Should never happen...
    {
		platform->Message(BOTH_ERROR_MESSAGE, "Attempt to alter a non-released lookahead ring entry!\n");
		return false;
    }

	lookAheadRingAddPointer->Init(ep, requestedFeedRate, minSpeed, maxSpeed, acceleration, ce, extrDiffs);
	lastRingMove = lookAheadRingAddPointer;
	lookAheadRingAddPointer = lookAheadRingAddPointer->Next();
	lookAheadRingCount++;

    return true;
}

LookAhead* Move::LookAheadRingGet()
{
  LookAhead* result;
  if(LookAheadRingEmpty())
    return NULL;
  result = lookAheadRingGetPointer;
  if(!(result->Processed() & complete))
    return NULL;
  lookAheadRingGetPointer = lookAheadRingGetPointer->Next();
  lookAheadRingCount--;
  return result;
}

// Sets up a single lookahead entry to perform an isolated move ( start velocity = end velocity = instantDv )

bool Move::SetUpIsolatedMove(long ep[], float requestedFeedRate, float minSpeed, float maxSpeed, float acceleration, EndstopChecks ce)
{
	if (isolatedMoveAvailable)
	{
		return false;
	}

    if(!(isolatedMove->Processed() & released)) // Should never happen...
    {
		platform->Message(BOTH_ERROR_MESSAGE, "Attempt to alter a non-released isolated lookahead entry!\n");
		return false;
    }

	isolatedMove->Init(ep, requestedFeedRate, minSpeed, maxSpeed, acceleration, ce, zeroExtruderPositions);

	// Perform acceleration calculation

	const float instantDv = platform->InstantDv(platform->SlowestDrive());
	float u = instantDv, v = instantDv;
	ddaIsolatedMove->Init(isolatedMove, u, v);

	isolatedMoveAvailable = true;

//	reprap.GetPlatform()->Message(BOTH_MESSAGE, "minSpeed: %f maxSpeed: %f instantDv: %f\n", minSpeed, maxSpeed, instantDv);
//	reprap.GetPlatform()->AppendMessage(BOTH_MESSAGE, "DDA-v: %f timeStep: %f\n", ddaIsolatedMove->velocity, ddaIsolatedMove->timeStep);
//	reprap.GetPlatform()->AppendMessage(BOTH_MESSAGE, "stopAStep: %u startDStep: %u totalSteps: %u\n", ddaIsolatedMove->stopAStep, ddaIsolatedMove->startDStep, ddaIsolatedMove->totalSteps);
//	reprap.GetPlatform()->AppendMessage(BOTH_MESSAGE, "V: %f Feedrate: %f\n", isolatedMove->V(), ddaIsolatedMove->feedRate);

	return true;
}

bool Move::SetUpIsolatedMove(float to[], float feedRate, bool axesOnly)
{
	if (isolatedMoveAvailable)
	{
		return false;
	}

    if(!(isolatedMove->Processed() & released)) // Should never happen...
    {
		platform->Message(BOTH_ERROR_MESSAGE, "Attempt to alter a non-released isolated lookahead entry!\n");
		return false;
    }

    // Analyze this move basically the same way as in Spin()

    long ep[DRIVES];
	for(size_t drive = 0; drive < DRIVES; drive++)
	{
		if (drive < AXES)
		{
			normalisedDirectionVector[drive] = to[drive] - liveCoordinates[drive];
			ep[drive] = LookAhead::EndPointToMachine(drive, to[drive]);
		}
		else
		{
			if (!axesOnly)
			{
				normalisedDirectionVector[drive] = to[drive];
				ep[drive] = LookAhead::EndPointToMachine(drive, to[drive]);
			}
			else
			{
				ep[drive] = 0;
			}
		}
	}

	Absolute(normalisedDirectionVector, DRIVES);
	if (Normalise(normalisedDirectionVector, DRIVES) <= 0.0)
	{
		platform->Message(BOTH_ERROR_MESSAGE, "Attempt to normalise zero-length move.\n");  // Should never get here - noMove above
		return false;
	}

	float minSpeed = VectorBoxIntersection(normalisedDirectionVector, platform->InstantDvs(), DRIVES);
	float acceleration = VectorBoxIntersection(normalisedDirectionVector, platform->Accelerations(), DRIVES);
	float maxSpeed = VectorBoxIntersection(normalisedDirectionVector, platform->MaxFeedrates(), DRIVES);

	isolatedMove->Init(ep, feedRate, minSpeed, maxSpeed, acceleration, 0, zeroExtruderPositions);

	// Perform acceleration calculation

	const float instantDv = platform->InstantDv(platform->SlowestDrive());
	float u = instantDv, v = instantDv;
	ddaIsolatedMove->Init(isolatedMove, u, v);

	isolatedMoveAvailable = true;

	return true;
}

// Do the bed transform AFTER the axis transform

void Move::BedTransform(float xyzPoint[]) const
{
	if(identityBedTransform)
		return;

	switch(NumberOfProbePoints())
	{
	case 0:
		return;

	case 3:
		xyzPoint[Z_AXIS] = xyzPoint[Z_AXIS] + aX*xyzPoint[X_AXIS] + aY*xyzPoint[Y_AXIS] + aC;
		break;

	case 4:
		xyzPoint[Z_AXIS] = xyzPoint[Z_AXIS] + SecondDegreeTransformZ(xyzPoint[X_AXIS], xyzPoint[Y_AXIS]);
		break;

	case 5:
		xyzPoint[Z_AXIS] = xyzPoint[Z_AXIS] + TriangleZ(xyzPoint[X_AXIS], xyzPoint[Y_AXIS]);
		break;

	default:
		platform->Message(BOTH_ERROR_MESSAGE, "BedTransform: wrong number of sample points.");
	}
}

// Invert the bed transform BEFORE the axis transform

void Move::InverseBedTransform(float xyzPoint[]) const
{
	if(identityBedTransform)
		return;

	switch(NumberOfProbePoints())
	{
	case 0:
		return;

	case 3:
		xyzPoint[Z_AXIS] = xyzPoint[Z_AXIS] - (aX*xyzPoint[X_AXIS] + aY*xyzPoint[Y_AXIS] + aC);
		break;

	case 4:
		xyzPoint[Z_AXIS] = xyzPoint[Z_AXIS] - SecondDegreeTransformZ(xyzPoint[X_AXIS], xyzPoint[Y_AXIS]);
		break;

	case 5:
		xyzPoint[Z_AXIS] = xyzPoint[Z_AXIS] - TriangleZ(xyzPoint[X_AXIS], xyzPoint[Y_AXIS]);
		break;

	default:
		platform->Message(BOTH_ERROR_MESSAGE, "InverseBedTransform: wrong number of sample points.");
	}
}

// Do the Axis transform BEFORE the bed transform

void Move::AxisTransform(float xyzPoint[]) const
{
	xyzPoint[X_AXIS] = xyzPoint[X_AXIS] + tanXY*xyzPoint[Y_AXIS] + tanXZ*xyzPoint[Z_AXIS];
	xyzPoint[Y_AXIS] = xyzPoint[Y_AXIS] + tanYZ*xyzPoint[Z_AXIS];
}

// Invert the Axis transform AFTER the bed transform

void Move::InverseAxisTransform(float xyzPoint[]) const
{
	xyzPoint[Y_AXIS] = xyzPoint[Y_AXIS] - tanYZ*xyzPoint[Z_AXIS];
	xyzPoint[X_AXIS] = xyzPoint[X_AXIS] - (tanXY*xyzPoint[Y_AXIS] + tanXZ*xyzPoint[Z_AXIS]);
}


void Move::Transform(float xyzPoint[]) const
{
	AxisTransform(xyzPoint);
	BedTransform(xyzPoint);
}

void Move::InverseTransform(float xyzPoint[]) const
{
	InverseBedTransform(xyzPoint);
	InverseAxisTransform(xyzPoint);
}


void Move::SetAxisCompensation(int8_t axis, float tangent)
{
	switch(axis)
	{
	case X_AXIS:
		tanXY = tangent;
		break;
	case Y_AXIS:
		tanYZ = tangent;
		break;
	case Z_AXIS:
		tanXZ = tangent;
		break;
	default:
		platform->Message(BOTH_ERROR_MESSAGE, "SetAxisCompensation: dud axis.\n");
	}
}

void Move::BarycentricCoordinates(int8_t p1, int8_t p2, int8_t p3, float x, float y, float& l1, float& l2, float& l3) const
{
	float y23 = baryYBedProbePoints[p2] - baryYBedProbePoints[p3];
	float x3 = x - baryXBedProbePoints[p3];
	float x32 = baryXBedProbePoints[p3] - baryXBedProbePoints[p2];
	float y3 = y - baryYBedProbePoints[p3];
	float x13 = baryXBedProbePoints[p1] - baryXBedProbePoints[p3];
	float y13 = baryYBedProbePoints[p1] - baryYBedProbePoints[p3];
	float iDet = 1.0 / (y23 * x13 + x32 * y13);
	l1 = (y23 * x3 + x32 * y3) * iDet;
	l2 = (-y13 * x3 + x13 * y3) * iDet;
	l3 = 1.0 - l1 - l2;
}

/*
 * Interpolate on a triangular grid.  The triangle corners are indexed:
 *
 *   ^  [1]      [2]
 *   |
 *   Y      [4]
 *   |
 *   |  [0]      [3]
 *      -----X---->
 *
 */
float Move::TriangleZ(float x, float y) const
{
	float l1, l2, l3;
	int8_t j;
	for(int8_t i = 0; i < 4; i++)
	{
		j = (i + 1) % 4;
		BarycentricCoordinates(i, j, 4, x, y, l1, l2, l3);
		if(l1 > TRIANGLE_0 && l2 > TRIANGLE_0 && l3 > TRIANGLE_0)
		{
			return l1 * baryZBedProbePoints[i] + l2 * baryZBedProbePoints[j] + l3 * baryZBedProbePoints[4];
		}
	}
	platform->Message(BOTH_ERROR_MESSAGE, "Triangle interpolation: point outside all triangles!");
	return 0.0;
}

void Move::SetProbedBedEquation(StringRef& reply)
{
	float x10, y10, z10;

	switch(NumberOfProbePoints())
	{
	case 3:
		/*
		 * Transform to a plane
		 */
		float a, b, c, d;   // Implicit plane equation - what we need to do a proper job
		float x20, y20, z20;

		x10 = xBedProbePoints[1] - xBedProbePoints[0];
		y10 = yBedProbePoints[1] - yBedProbePoints[0];
		z10 = zBedProbePoints[1] - zBedProbePoints[0];
		x20 = xBedProbePoints[2] - xBedProbePoints[0];
		y20 = yBedProbePoints[2] - yBedProbePoints[0];
		z20 = zBedProbePoints[2] - zBedProbePoints[0];
		a = y10 * z20 - z10 * y20;
		b = z10 * x20 - x10 * z20;
		c = x10 * y20 - y10 * x20;
		d = -(xBedProbePoints[1] * a + yBedProbePoints[1] * b + zBedProbePoints[1] * c);
		aX = -a / c;
		aY = -b / c;
		aC = -d / c;
		identityBedTransform = false;
		break;

	case 4:
		/*
		 * Transform to a ruled-surface quadratic.  The corner points for interpolation are indexed:
		 *
		 *   ^  [1]      [2]
		 *   |
		 *   Y
		 *   |
		 *   |  [0]      [3]
		 *      -----X---->
		 *
		 *   These are the scaling factors to apply to x and y coordinates to get them into the
		 *   unit interval [0, 1].
		 */
		xRectangle = 1.0 / (xBedProbePoints[3] - xBedProbePoints[0]);
		yRectangle = 1.0 / (yBedProbePoints[1] - yBedProbePoints[0]);
		identityBedTransform = false;
		break;

	case 5:
		for(int8_t i = 0; i < 4; i++)
		{
			x10 = xBedProbePoints[i] - xBedProbePoints[4];
			y10 = yBedProbePoints[i] - yBedProbePoints[4];
			z10 = zBedProbePoints[i] - zBedProbePoints[4];
			baryXBedProbePoints[i] = xBedProbePoints[4] + 2.0 * x10;
			baryYBedProbePoints[i] = yBedProbePoints[4] + 2.0 * y10;
			baryZBedProbePoints[i] = zBedProbePoints[4] + 2.0 * z10;
		}
		baryXBedProbePoints[4] = xBedProbePoints[4];
		baryYBedProbePoints[4] = yBedProbePoints[4];
		baryZBedProbePoints[4] = zBedProbePoints[4];
		identityBedTransform = false;
		break;

	default:
		platform->Message(BOTH_ERROR_MESSAGE, "Attempt to set bed compensation before all probe points have been recorded.");
	}

	reply.copy("Bed equation fits points");
	for(size_t point = 0; point < NumberOfProbePoints(); point++)
	{
		reply.catf(" [%.1f, %.1f, %.3f]", xBedProbePoints[point], yBedProbePoints[point], zBedProbePoints[point]);
	}
	reply.cat("\n");
}

// FIXME
// This function is never normally called.  It is a test to time
// the interrupt function.  To activate it, uncomment the line that calls
// this in Platform.ino.

void Move::InterruptTime()
{
/*  char buffer[50];
  float a[] = {1.0, 2.0, 3.0, 4.0, 5.0};
  float b[] = {2.0, 3.0, 4.0, 5.0, 6.0};
  float u = 50;
  float v = 50;
  lookAheadDDA->Init(a, b, u, v);
  lookAheadDDA->Start(false);
  float t = platform->Time();
  for(long i = 0; i < 100000; i++) 
    lookAheadDDA->Step(false);
  t = platform->Time() - t;
  platform->Message(HOST_MESSAGE, "Time for 100000 calls of the interrupt function: ");
  snprintf(buffer, 50, "%ld", t);
  platform->Message(HOST_MESSAGE, buffer);
  platform->Message(HOST_MESSAGE, " microseconds.\n");*/
}

//****************************************************************************************************

DDA::DDA(Move* m, Platform* p, DDA* n)
{
  active = false;
  move = m;
  platform = p;
  next = n;
}

/*

DDA::Init(...) 

Sets up the DDA to take us between two positions and extrude states.
The start velocity is u, and the end one is v.  The requested maximum feedrate
is in myLookAheadEntry->FeedRate().

Almost everything that needs to be done to set this up is also useful
for GCode look-ahead, so this one function is used for both.  It flags when
u and v cannot be satisfied with the distance available and reduces them 
proportionately to give values that can just be achieved, which is why they
are passed by reference.

The return value is indicates if the move is a trapezium or triangle, and if
the u and v values need to be changed.

In the case of only extruders moving, the distance moved is taken to be the Pythagoran distance in
the configuration space of the extruders.

TODO: Worry about having more than eight drives
TODO: Expand this code for live calculations where we can't change u or v

*/

MovementProfile DDA::AccelerationCalculation(float& u, float& v, MovementProfile result)
{
	// At which DDA step should we stop accelerating?  myLookAheadEntry->FeedRate() gives
	// the desired feedrate.

	feedRate = myLookAheadEntry->FeedRate();

	float d = 0.5*(fabs(feedRate*feedRate - u*u))/acceleration; // d = (v1^2 - v0^2)/2a
	stopAStep = (long)roundf((d*totalSteps)/distance);

	// At which DDA step should we start decelerating?

	d = 0.5*(v*v - feedRate*feedRate)/acceleration;  // This should be 0 or negative...
	startDStep = totalSteps + (long)roundf((d*totalSteps)/distance);

	// If acceleration stop is at or after deceleration start, then the distance moved
	// is not enough to get to full speed.

	if(stopAStep >= startDStep)
	{
		result = noFlat;

		// Work out the point at which to stop accelerating and then
		// immediately start decelerating.

		float dCross = 0.5*(0.5*(v*v - u*u)/acceleration + distance);

		// dc42's better version

		if(dCross < 0.0 || dCross > distance)
		{
			// With the acceleration available, it is not possible
			// to satisfy u and v within the distance; reduce the greater of u and v
			// to get ones that work and flag the fact.
			// The result is two velocities that can just be accelerated
			// or decelerated between over the distance to get
			// from one to the other.

			result = change;
			float temp = 2.0 * acceleration * distance;
			if (v > u)
			{
				// Accelerating, reduce v
				v = sqrt((u * u) + temp);
				dCross = distance;
			}
			else
			{
				// Decelerating, reduce u
				u = sqrt((v * v) + temp);
				dCross = 0.0;
			}
		}

		// The DDA steps at which acceleration stops and deceleration starts

		stopAStep = (long)((dCross*totalSteps)/distance);
		startDStep = stopAStep + 1;
	}

	return result;
}


MovementProfile DDA::Init(LookAhead* lookAhead, float& u, float& v)
{
  active = isDecelerating = false;
  myLookAheadEntry = lookAhead;
  MovementProfile result = moving;
  totalSteps = -1;
  distance = 0.0;
  endStopsToCheck = myLookAheadEntry->EndStopsToCheck();
  int8_t bigDirection = 0;

  const long* targetPosition = myLookAheadEntry->MachineCoordinates();
  long positionNow[DRIVES];
  if (myLookAheadEntry->Previous() == NULL)
  {
	  move->LiveMachineCoordinates(positionNow);
	  // u and v must be passed, because we don't have any other look-ahead items to process
  }
  else
  {
	  const long *prevCoords = myLookAheadEntry->Previous()->MachineCoordinates();
	  for(uint8_t drive=0; drive<DRIVES; drive++)
	  {
		  positionNow[drive] = prevCoords[drive];
	  }
	  u = myLookAheadEntry->Previous()->V();
	  v = myLookAheadEntry->V();
  }

  // How far are we going, both in steps and in mm?
  
  for(uint8_t drive = 0; drive < DRIVES; drive++)
  {
    if(drive < AXES) // X, Y, & Z
    {
      delta[drive] = targetPosition[drive] - positionNow[drive];  // XYZ Absolute
    }
    else
    {
      delta[drive] = targetPosition[drive];  // Es Relative
    }

    float d = myLookAheadEntry->MachineToEndPoint(drive, delta[drive]);
    distance += d*d;
    
    if(delta[drive] >= 0)
    {
      directions[drive] = FORWARDS;
    }
    else
    {
      directions[drive] = BACKWARDS;
      delta[drive] = -delta[drive];
    }
    
    // Keep track of the biggest drive move in totalSteps
    
    if(delta[drive] > totalSteps)
    {
      totalSteps = delta[drive];
      bigDirection = drive;
    }
  }
  
  // Not going anywhere?  Should have been chucked away before we got here.
  
  if(totalSteps <= 0)
  {
	if(reprap.Debug(moduleMove))
	{
		platform->Message(BOTH_ERROR_MESSAGE, "DDA.Init(): Null movement.\n");
	}
    myLookAheadEntry->Release();
    return result;
  }
  
  // Set up the DDA
  
  counter[0] = -totalSteps/2;
  for(size_t drive = 1; drive < DRIVES; drive++)
  {
    counter[drive] = counter[0];
  }
  
  // Acceleration and velocity calculations
  
  distance = sqrt(distance);
  
  // Decide the appropriate acceleration and instantDv values
  // timeStep is set here to the distance of the
  // biggest-move axis step.  It will be divided
  // by a velocity later.

  acceleration = lookAhead->Acceleration();
  instantDv = lookAhead->MinSpeed();
  timeStep = 1.0/platform->DriveStepsPerUnit(bigDirection);

  result = AccelerationCalculation(u, v, result);
  
  // The initial velocity
  
  velocity = u;
  
  // Sanity check
  
  if(velocity <= 0.0)
  {
    velocity = instantDv;
    if(reprap.Debug(moduleMove))
    {
    	platform->Message(BOTH_ERROR_MESSAGE, "DDA.Init(): Zero or negative initial velocity!\n");
    }
  }
  
  // How far have we gone?
  
  stepCount = 0;
  
  // timeStep is an axis step distance at this point; divide it by the
  // velocity to get time.
  
  timeStep = timeStep/velocity;
  //timeStep = sqrt(2.0*timeStep/acceleration);
  
  /*if(bigDirection == E0_DRIVE)
  {
	  //myLookAheadEntry->PrintMove();

	  snprintf(scratchString, STRING_LENGTH, "DDA startV: %.2f, distance: %.1f, steps: %d, stopA: %d, startD: %d, timestep: %.5f, feedrate: %.5f\n",
			  velocity, distance, totalSteps, stopAStep, startDStep, timeStep, feedRate);
	  platform->Message(HOST_MESSAGE, scratchString);
  }*/

  return result;
}

void DDA::Start()
{
	for(size_t drive = 0; drive < DRIVES; drive++)
	{
		platform->SetDirection(drive, directions[drive]);
	}

	bool extrusionMove = false;
	for(size_t extruder = AXES; extruder < DRIVES; extruder++)
	{
		if (delta[extruder] > 0)
		{
			extrusionMove = true;
			break;
		}
	}

	if (extrusionMove)
	{
		reprap.GetExtruderCapabilities(eMoveAllowed, directions);
		platform->ExtrudeOn();
	}
	else
	{
		platform->ExtrudeOff();
	}

	platform->SetInterrupt(timeStep); // seconds
	active = true;
}

// This function is called from the ISR.
// Any variables it modifies that are also read by code outside the ISR must be declared 'volatile'.
void DDA::Step()
{
  if (!active || !move->active)
  {
	  return;
  }
  
  // Try to slow down the current move to achieve a better deceleration profile

  if (move->IsPausing() && !isDecelerating)
  {
	  float u = velocity, v = instantDv;
	  if (AccelerationCalculation(u, v, moving) & change)	// calculate stopAStep and startDStep again
	  {
		  if (next != NULL)
		  {
			  next->velocity = v;
		  }
	  }
	  isDecelerating = true;
  }

  // Step each drive and possibly check for endstops

  int drivesMoving = 0;
  for(size_t drive = 0; drive < DRIVES; drive++)
  {
    counter[drive] += delta[drive];
    if(counter[drive] > 0)
    {
      // zpl-2014-10-03: My fork contains an alternative cold extrusion/retraction check due to code queuing, so
      // step E drives only if this is actually possible.

      if (drive < AXES || eMoveAllowed[drive - AXES])
      {
        platform->Step(drive);
      }

      counter[drive] -= totalSteps;
      
      drivesMoving |= 1<<drive;
        
      // Hit anything?
  
      if((endStopsToCheck & (1 << drive)) != 0)
      {
        switch(platform->Stopped(drive))
        {
        case lowHit:
          move->HitLowStop(drive, myLookAheadEntry, this);
          active = false;
          break;
        case highHit:
          move->HitHighStop(drive, myLookAheadEntry, this);
          active = false;
          break;
        case lowNear:
          velocity = instantDv;		// slow down because we are getting close
          break;
        default:
          break;
        }
      }        
    }
  }
  
  // May have hit a stop, so test active here
  
  if(active)
  {
	timeStep = distance/(totalSteps * velocity);	// dc42 use the average distance per step

    // Simple Euler integration to get velocities.
    // Maybe one day do a Runge-Kutta?
  
    if(stepCount < stopAStep)
    {
      velocity += acceleration*timeStep;
      if (velocity > feedRate)
      {
    	  velocity = feedRate;
      }
    }
    else if(stepCount >= startDStep)
    {
      velocity -= acceleration*timeStep;
      if (velocity < instantDv)
      {
    	  velocity = instantDv;
      }
    }
      
    stepCount++;
    active = stepCount < totalSteps;
    
    platform->SetInterrupt(timeStep);
  }
  
  if(!active)
  {
	for(uint8_t drive = 0; drive < DRIVES; drive++)
	{
	  if (drive < AXES)
	  {
	    move->liveCoordinates[drive] = myLookAheadEntry->MachineToEndPoint(drive); // XYZ absolute
	  }
	  else
	  {
		move->liveCoordinates[drive] += myLookAheadEntry->MachineToEndPoint(drive); // Es relative
		move->rawExtruderPos[drive - AXES] += myLookAheadEntry->RawExtruderDiff(drive - AXES);
	  }
	}
	move->liveCoordinates[DRIVES] = myLookAheadEntry->FeedRate();

	// Don't tell GCodes about any completed moves if we're performing an isolated move
	if (move->IsRunning() || move->IsPausing())
	{
	  reprap.GetGCodes()->MoveCompleted();
	}
  }
}

// Called when the DDA is complete
void DDA::Release()
{
    myLookAheadEntry->Release();
    platform->SetInterrupt(STANDBY_INTERRUPT_RATE);
}

//***************************************************************************************************

LookAhead::LookAhead(Move* m, Platform* p, LookAhead* n)
{
  move = m;
  platform = p;
  next = n;
}

void LookAhead::Init(long ep[], float fRate, float minS, float maxS, float acc, EndstopChecks ce, const float extrDiffs[])
{
  v = fRate;
  requestedFeedrate = fRate;
  minSpeed = minS;
  maxSpeed = maxS;
  acceleration = acc;

  if(v < minSpeed)
  {
	  requestedFeedrate = minSpeed;
	  v = minSpeed;
  }
  if(v > maxSpeed)
  {
	  requestedFeedrate = maxSpeed;
	  v = maxSpeed;
  }

  for(size_t drive = 0; drive < DRIVES; drive++)
  {
	  endPoint[drive] = ep[drive];
  }
  
  endStopsToCheck = ce;
  
  for(size_t extruder = 0; extruder < DRIVES - AXES; extruder++)
  {
	  rawExDiff[extruder] = extrDiffs[extruder];
  }

  // Cosines are lazily evaluated; flag this as unevaluated
  
  cosine = 2.0;
    
  // Only bother with lookahead when we are printing a file, so set processed complete when we aren't.
  
  processed = (reprap.GetGCodes()->HaveIncomingData())
    			? unprocessed
  	  	  	  	  : complete|vCosineSet;
}


// This returns the cosine of the angle between
// the movement up to this, and the movement
// away from this.  Uses lazy evaluation.

float LookAhead::Cosine()
{
  if(cosine < 1.5)
    return cosine;
    
  cosine = 0.0;
  float a2 = 0.0;
  float b2 = 0.0;
  float m1;
  float m2;
  for(int8_t drive = 0; drive < DRIVES; drive++)
  {
	  m1 = MachineToEndPoint(drive);

	  // Absolute moves for axes; relative for extruders

	  if(drive < AXES)
	  {
		  m2 = Next()->MachineToEndPoint(drive) - m1;
		  m1 = m1 - Previous()->MachineToEndPoint(drive);
	  } else
	  {
		  m2 = Next()->MachineToEndPoint(drive);
	  }
	  a2 += m1*m1;
	  b2 += m2*m2;
	  cosine += m1*m2;
  }
  
  if(a2 <= 0.0 || b2 <= 0.0) // Avoid division by 0.0
  {
	cosine = 0.0;
    return cosine;
  }
 
  cosine = cosine/( (float)sqrt(a2*b2) );
  return cosine;
}

// Returns units (mm) from steps for a particular drive
float LookAhead::MachineToEndPoint(int8_t drive, long coord)
{
	return ((float)coord)/reprap.GetPlatform()->DriveStepsPerUnit(drive);
}

// Returns steps from units (mm) for a particular drive
long LookAhead::EndPointToMachine(int8_t drive, float coord)
{
	return (long)roundf(coord*reprap.GetPlatform()->DriveStepsPerUnit(drive));
}

void LookAhead::MoveAborted(float done)
{
	for (size_t drive = 0; drive < AXES; ++drive)
	{
		long prev = previous->endPoint[drive];
		endPoint[drive] = prev + (long)((endPoint[drive] - prev) * done);
	}
	v = platform->InstantDv(platform->SlowestDrive());
	cosine = 2.0;		// not sure this is needed
}

// Returns the untransformed relative E move length (in mm)
float LookAhead::RawExtruderDiff(uint8_t extruder) const
{
	return rawExDiff[extruder];
}

// Sets the untransformed relative E move length for a specified extruder
void LookAhead::SetRawExtruderDiff(uint8_t extruder, float diff)
{
	rawExDiff[extruder] = diff;
}

/*
 * For diagnostics
 */

void LookAhead::PrintMove()
{
	platform->Message(HOST_MESSAGE, "X,Y,Z: %.1f %.1f %.1f, min v: %.2f, max v: %.1f, acc: %.1f, feed: %.1f, u: %.3f, v: %.3f\n",
			MachineToEndPoint(X_AXIS), MachineToEndPoint(Y_AXIS), MachineToEndPoint(Z_AXIS),
			MinSpeed(), MaxSpeed(), Acceleration(), FeedRate(), Previous()->V(), V());
}

