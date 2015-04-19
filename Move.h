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

#ifndef MOVE_H
#define MOVE_H

#define DDA_RING_LENGTH 5
#define LOOK_AHEAD_RING_LENGTH 30
#define LOOK_AHEAD 20				// Must be less than LOOK_AHEAD_RING_LENGTH

#define ZERO_EXTRUDER_POSITIONS { 0.0, 0.0, 0.0, 0.0, 0.0 }
#define MINIMUM_SPLIT_DISTANCE 2.0	// Don't split any moves unless one of their axes has a bigger delta than this (in mm)

enum MovementProfile
{
  moving = 0,  // Ordinary trapezoidal-velocity-profile movement
  noFlat = 1,  // Triangular profile movement
  change = 2   // To make this movement, the initial and/or final velocities must change
};

// The possible states of a movement in the look-ahead ring as the look-ahead is
// being done.

enum MovementState
{
  unprocessed = 0,
  vCosineSet = 1,
  complete = 2,
  released = 4
};


enum PointCoordinateSet
{
	unset = 0,
	xSet = 1,
	ySet = 2,
	zSet = 4
};

enum MoveStatus
{
	running,
	pausing,
	paused,
	cancelled
};

/**
 * This class implements a look-ahead buffer for moves.  It allows colinear
 * moves not to decelerate between them, sets velocities at ends and beginnings
 * for angled moves, and so on.  Entries are joined in a doubly-linked list
 * to form a ring buffer.
 */
class LookAhead
{  
	friend class Move;
	friend class DDA;

protected:

	LookAhead(Move* m, Platform* p, LookAhead* n);
	void Init(long ep[], float requsestedFeedRate, float minSpeed, 		// Set up this move
			float maxSpeed, float acceleration, EndstopChecks ce,
			const float extrDiffs[]);
	LookAhead* Next() const;											// Next one in the ring
	LookAhead* Previous() const;										// Previous one in the ring
	const long* MachineCoordinates() const;								// Endpoints of a move in machine coordinates
	float MachineToEndPoint(int8_t drive) const;						// Convert a move endpoint to real mm coordinates
	static float MachineToEndPoint(int8_t drive, long coord);			// Convert any number to a real coordinate
	static long EndPointToMachine(int8_t drive, float coord);			// Convert real mm to a machine coordinate
	float FeedRate() const;												// How fast is the set speed for this move
	float MinSpeed() const;												// What is the slowest that this move can be
	float MaxSpeed() const;												// What is the fastest this move can be
	float Acceleration() const;											// What is the acceleration available for this move
	float V() const;													// The speed at the end of the move
	float RawExtruderDiff(uint8_t extruder) const;
	void SetV(float vv);												// Set the end speed
	void SetFeedRate(float f);											// Set the desired feedrate
	int8_t Processed() const;											// Where we are in the look-ahead prediction sequence
	void SetProcessed(MovementState ms);								// Set where we are the the look ahead processing
	void SetDriveCoordinate(float a, int8_t drive);						// Force an end point
	void SetRawExtruderDiff(uint8_t extruder, float value);
	EndstopChecks EndStopsToCheck() const;								// Which endstops we are checking on this move
	void Release();														// This move has been processed and executed
	void PrintMove();													// Print diagnostics
	void MoveAborted(float done);

private:

	Move* move;						// The main movement control class
	Platform* platform;				// The RepRap machine
	LookAhead* next;				// Next entry in the ring
	LookAhead* previous;			// Previous entry in the ring
	long endPoint[DRIVES+1];  		// Machine coordinates of the endpoint.  Should never use the +1, but safety first
	float Cosine();					// The angle between the previous move and this one
	EndstopChecks endStopsToCheck;	// Endstops to check for this move
    float cosine;					// Store for the cosine value - the function uses lazy evaluation
    float v;        				// The feedrate we can actually do
    float requestedFeedrate; 		// The requested feedrate
    float minSpeed;					// The slowest that this move may run at
    float maxSpeed;					// The fastest this move may run at
    float acceleration;				// The fastest acceleration allowed
    float rawExDiff[DRIVES - AXES];	// The original (relative) E difference
    volatile int8_t processed;		// The stage in the look ahead process that this move is at.
};

/**
 * This implements an integer space machine coordinates Bressenham-style DDA to step the drives.
 * DDAs are stored in a linked list forming a ring buffer.
 */
class DDA
{
	friend class Move;
	friend class LookAhead;

protected:

	DDA(Move* m, Platform* p, DDA* n);
	MovementProfile Init(LookAhead* lookAhead, float& u, float& v);				// Set up the DDA.  Also used experimentally in look ahead.
	void Start();																// Start executing the DDA.  I.e. move the move.
	void Step();																// Take one step of the DDA.  Called by timed interrupt.
	void Release();																// Called when the DDA is complete
	bool Active() const;
	DDA* Next();																// Next entry in the ring
	float InstantDv() const;

private:

	MovementProfile AccelerationCalculation(float& u, float& v, 	// Compute acceleration profiles
			MovementProfile result);

	Move* move;								// The main movement control class
	Platform* platform;						// The RepRap machine
	DDA* next;								// The next one in the ring
	LookAhead* myLookAheadEntry;			// The look-ahead entry corresponding to this DDA
	long counter[DRIVES];					// Step counters
	long delta[DRIVES];						// How far to move each drive
	bool directions[DRIVES];				// Forwards or backwards?
	long totalSteps;						// Total number of steps for this move
	long stepCount;							// How many steps we have already taken
	EndstopChecks endStopsToCheck;			// Endstops to check for this move
    float timeStep;							// The current timestep (seconds)
    float velocity;							// The current velocity
    long stopAStep;							// The stepcount at which we stop accelerating
    long startDStep;						// The stepcount at which we start decelerating
    float distance;							// How long is the move in real distance
    float acceleration;						// The acceleration to use
    float instantDv;						// The lowest possible velocity
    float feedRate;
    bool eMoveAllowed[DRIVES-AXES];			// Which extruder is allowed to move?
    bool isDecelerating;					// Is the DDA is trying to slow down while pausing?
    volatile bool active;					// Is the DDA running?
};

/**
 * This is the master movement class.  It controls all movement in the machine.
 */
class Move
{
    friend class DDA;

  public:
  
    Move(Platform* p, GCodes* g);
    void Init();								// Start me up
    void Spin();								// Called in a tight loop to keep the class going
    void Exit();								// Shut down

    bool IsRunning() const;						// Are we running any moves?
    bool Pause();								// Pause any moves in progress
    bool IsPaused() const;						// Are all the moves paused?
    bool IsPausing() const;						// Are we still waiting for the last move to finish?
    bool IsResuming() const;					// Are we resuming a paused print?
    bool IsCancelled() const;					// Are we busy destroying look-ahead entries?
    bool Resume();								// Resume paused moves
    void Cancel();								// Cancel any pending moves

    bool GetCurrentUserPosition(float m[]) const; 	// Return the current position in transformed coords if possible. Returns false otherwise
    void LiveCoordinates(float m[]) const;		// Gives the last point at the end of the last complete DDA transformed to user coords
    bool GetPauseCoordinates(float m[]) const;	// Gives the coordinates at which the print was paused and returns true on success
    void GetRawExtruderPositions(float e[]) const;	// Get the original extruder positions without the extusion multiplier applied
    void ResetExtruderPositions();				// Resets the extruder positions to zero
    void Interrupt();							// The hardware's (i.e. platform's)  interrupt should call this.
    void InterruptTime();						// Test function - not used
    bool AllMovesAreFinished();					// Is the look-ahead ring empty?  Stops more moves being added as well.
    void AddMoreMoves();						// Allow moves to be added after a call to AllMovesAreFinished()
    void DoLookAhead();							// Run the look-ahead procedure
    void HitLowStop(int8_t drive,				// What to do when a low endstop is hit
    		LookAhead* la, DDA* hitDDA);
    void HitHighStop(int8_t drive, 				// What to do when a high endstop is hit
    		LookAhead* la, DDA* hitDDA);
    bool NoLiveMovement() const;				// Is a move running, or are there any queued if we're still running?
    void SetPositions(float move[]);			// Force the coordinates to be these
    void SetLiveCoordinates(float coords[]);	// Force the live coordinates (see above) to be these
    void SetXBedProbePoint(int index, float x);	// Record the X coordinate of a probe point
    void SetYBedProbePoint(int index, float y);	// Record the Y coordinate of a probe point
    void SetZBedProbePoint(int index, float z);	// Record the Z coordinate of a probe point
    float XBedProbePoint(int index) const;		// Get the X coordinate of a probe point
    float YBedProbePoint(int index) const;		// Get the Y coordinate of a probe point
    float ZBedProbePoint(int index) const;		// Get the Z coordinate of a probe point
    int NumberOfProbePoints() const;			// How many points to probe have been set? 0 if incomplete
    int NumberOfXYProbePoints() const;			// How many XY coordinates of probe points have been set (Zs may not have been probed yet)
    bool AllProbeCoordinatesSet(int index) const;	// XY, and Z all set for this one?
    bool XYProbeCoordinatesSet(int index) const;	// Just XY set for this one?
    void SetZProbing(bool probing);				// Set the Z probe live
    void SetProbedBedEquation(StringRef& reply);	// When we have a full set of probed points, work out the bed's equation
    float SecondDegreeTransformZ(float x, float y) const; // Used for second degree bed equation
    float GetLastProbedZ() const;				// What was the Z when the probe last fired?
    void SetAxisCompensation(int8_t axis, float tangent); // Set an axis-pair compensation angle
    float AxisCompensation(int8_t axis);		// The tangent value
    void SetIdentityTransform();				// Cancel the bed equation; does not reset axis angle compensation
    void Transform(float move[]) const;			// Take a position and apply the bed and the axis-angle compensations
    void InverseTransform(float move[]) const;	// Go from a transformed point back to user coordinates77
    void Diagnostics();							// Report useful stuff
    void UpdateCurrentCoordinates(LookAhead* la,	// Turn a DDA value back into a real world coordinate
    		DDA* runningDDA);
    float Normalise(float v[], int8_t dimensions);  // Normalise a vector to unit length
    void Absolute(float v[], int8_t dimensions);	// Put a vector in the positive hyperquadrant
    float Magnitude(const float v[], int8_t dimensions);  // Return the length of a vector
    void Scale(float v[], float scale,				// Multiply a vector by a scalar
    		int8_t dimensions);
    float VectorBoxIntersection(const float v[],	// Compute the length that a vector would have to have to...
    		const float box[], int8_t dimensions);	// ...just touch the surface of a hyperbox.
    float GetExtrusionFactor(uint8_t extruder) const;
    void SetExtrusionFactor(uint8_t extruder, float factor);
    float GetSpeedFactor() const;					// Factor by which we changed the speed factor since the last move
    void SetSpeedFactor(float factor);

  private:

    void BedTransform(float move[]) const;			    // Take a position and apply the bed compensations
    bool GetCurrentMachinePosition(float m[]) const;	// Get the current position in untransformed coords if possible. Return false otherwise
    void InverseBedTransform(float move[]) const;	    // Go from a bed-transformed point back to user coordinates
    void AxisTransform(float move[]) const;			    // Take a position and apply the axis-angle compensations
    void InverseAxisTransform(float move[]) const;	    // Go from an axis transformed point back to user coordinates
    void BarycentricCoordinates(int8_t p0, int8_t p1,   // Compute the barycentric coordinates of a point in a triangle
    		int8_t p2, float x, float y, float& l1,     // (see http://en.wikipedia.org/wiki/Barycentric_coordinate_system).
    		float& l2, float& l3) const;
    float TriangleZ(float x, float y) const;			// Interpolate onto a triangular grid
    bool DDARingAdd(LookAhead* lookAhead);				// Add a processed look-ahead entry to the DDA ring
    DDA* DDARingGet();									// Get the next DDA ring entry to be run
    bool DDARingEmpty() const;							// Anything there?
    bool DDARingFull() const;							// Any more room?
    bool GetDDARingLock();								// Lock the ring so only this function may access it
    void ReleaseDDARingLock();							// Release the DDA ring lock
    bool LookAheadRingEmpty() const;					// Anything there?
    bool LookAheadRingFull() const;						// Any more room?
    bool LookAheadRingAdd(long ep[], float requestedFeedRate, 	// Add an entry to the look-ahead ring for processing
            float minSpeed, float maxSpeed,
            float acceleration, EndstopChecks ce,
            const float extrDiffs[]);
    LookAhead* LookAheadRingGet();						// Get the next entry from the look-ahead ring
    void LiveMachineCoordinates(long m[]) const;		// Same as LiveCoordinates, but returns machine coordinates and no feedrate
    bool SetUpIsolatedMove(long ep[], float requestedFeedRate,	// Set up a single look-ahead entry for only one move
    		float minSpeed, float maxSpeed, float acceleration,
			EndstopChecks ce);
    bool SetUpIsolatedMove(float to[], float feedRate,
    		bool axesOnly);
    bool SplitNextMove();								// Split the next move to improve 5-point bed compensation

    Platform* platform;									// The RepRap machine
    GCodes* gCodes;										// The G Codes processing class
    
    // These implement the DDA ring
    
    DDA* dda;
    DDA* ddaRingAddPointer;
    DDA* ddaRingGetPointer;
    DDA* ddaIsolatedMove;
    bool readIsolatedMove;
    volatile bool ddaRingLocked;
    
    // These implement the look-ahead ring

    LookAhead* lookAheadRingAddPointer;
    LookAhead* lookAheadRingGetPointer;
    LookAhead* lastRingMove;
    LookAhead* isolatedMove;
    bool isolatedMoveAvailable;
    DDA* lookAheadDDA;
    int lookAheadRingCount;

    bool addNoMoreMoves;							// If true, allow no more moves to be added to the look-ahead
    bool active;									// Are we live and running?
    float currentFeedrate;                          // Err... the current feed rate...
    volatile float liveCoordinates[DRIVES + 1];		// The last endpoint that the machine moved to
    float pauseCoordinates[DRIVES + 1];				// The endpoint we were at when the machine was paused
    volatile float rawExtruderPos[DRIVES - AXES];	// The raw and unmodified extruder positions
    float rawEDistances[DRIVES - AXES];				// The raw and untransformed E distances of the next move
    float nextMove[DRIVES + 1];  					// The endpoint of the next move to processExtra entry is for feedrate
    bool doingSplitMove;							// We need to split the move into two for five-point bed compensation
    float splitMove[DRIVES];						// The endpoint of the next move to be split up into two moves
    float normalisedDirectionVector[DRIVES];		// Used to hold a unit-length vector in the direction of motion
    long nextMachineEndPoints[DRIVES+1];			// The next endpoint in machine coordinates (i.e. steps)
    float xBedProbePoints[NUMBER_OF_PROBE_POINTS];	// The X coordinates of the points on the bed at which to probe
    float yBedProbePoints[NUMBER_OF_PROBE_POINTS];	// The Y coordinates of the points on the bed at which to probe
    float zBedProbePoints[NUMBER_OF_PROBE_POINTS];	// The Z coordinates of the points on the bed at which to probe
    float baryXBedProbePoints[NUMBER_OF_PROBE_POINTS];	// The X coordinates of the triangle corner points
    float baryYBedProbePoints[NUMBER_OF_PROBE_POINTS];	// The Y coordinates of the triangle corner points
    float baryZBedProbePoints[NUMBER_OF_PROBE_POINTS];	// The Z coordinates of the triangle corner points
    uint8_t probePointSet[NUMBER_OF_PROBE_POINTS];	// Has the XY of this point been set?  Has the Z been probed?
    float aX, aY, aC; 								// Bed plane explicit equation z' = z + aX*x + aY*y + aC
    float tanXY, tanYZ, tanXZ; 						// Axis compensation - 90 degrees + angle gives angle between axes
    bool identityBedTransform;						// Is the bed transform in operation?
    float xRectangle, yRectangle;					// The side lengths of the rectangle used for second-degree bed compensation
    volatile float lastZHit;						// The last Z value hit by the probe
    bool zProbing;									// Are we bed probing as well as moving?
    float longWait;									// A long time for things that need to be done occasionally

    // Additional Move information

    float extrusionFactors[DRIVES - AXES];			// Extrusion factors (normally 1.0)
    float speedFactor;								// Speed factor, changed feedrates are multiplied by this

    bool isResuming;
    volatile MoveStatus state;
};

//********************************************************************************************************

inline LookAhead* LookAhead::Next() const
{
  return next;
}

inline LookAhead* LookAhead::Previous() const
{
  return previous;
}

inline float LookAhead::MachineToEndPoint(int8_t drive) const
{
	if(drive >= DRIVES)
	{
		platform->Message(BOTH_MESSAGE, "MachineToEndPoint() called for feedrate!\n");
		return 0.0;
	}
	return ((float)(endPoint[drive]))/platform->DriveStepsPerUnit(drive);
}

inline float LookAhead::FeedRate() const
{
	return requestedFeedrate;
}

inline float LookAhead::MinSpeed() const
{
	return minSpeed;
}

inline float LookAhead::MaxSpeed() const
{
	return maxSpeed;
}

inline float LookAhead::Acceleration() const
{
	return acceleration;
}

inline void LookAhead::SetV(float vv)
{
  v = vv;
}

inline float LookAhead::V() const
{
  return v;
}

inline void LookAhead::SetFeedRate(float f)
{
	requestedFeedrate = f;
}

inline int8_t LookAhead::Processed() const
{
  return processed;
}

inline void LookAhead::SetProcessed(MovementState ms)
{
  if(ms == unprocessed)
    processed = unprocessed;
  else
    processed |= ms;
}

inline void LookAhead::Release()
{
  processed = released;
}

inline EndstopChecks LookAhead::EndStopsToCheck() const
{
  return endStopsToCheck;
}

// This is called from the step ISR. Any variables it modifies that are also read by code outside the ISR should be declared 'volatile'.
inline void LookAhead::SetDriveCoordinate(float a, int8_t drive)
{
  endPoint[drive] = EndPointToMachine(drive, a);
}

inline const long* LookAhead::MachineCoordinates() const
{
	return endPoint;
}

//******************************************************************************************************

inline bool DDA::Active() const
{
  return active;
}

inline DDA* DDA::Next()
{
  return next;
}

inline float DDA::InstantDv() const
{
  return instantDv;
}


//***************************************************************************************

inline bool Move::IsRunning() const
{
	return state == running;
}

// Note: This method should be called at least twice:
// - First call initiates a movement stop
// - Other calls check if all moves have finished
// Returns true on success and false if it needs to be called again

inline bool Move::Pause()
{
	if (state == running)
	{
		// This will make the current move slow down to an appropriate end speed
		state = pausing;
	}
	else if (state == pausing)
	{
		// If there is no live movement...

		if (!NoLiveMovement() || !GetDDARingLock())
		{
			return false;
		}

		// ...and if we don't have any pending moves left...

		if (readIsolatedMove)
		{
			ReleaseDDARingLock();
			return false;
		}

		// ...we can record the current coordinates, so the machine knows where to resume the print

		for(size_t drive=0; drive<DRIVES; drive++)
		{
			pauseCoordinates[drive] = liveCoordinates[drive];
			isolatedMove->endPoint[drive] = LookAhead::EndPointToMachine(drive, pauseCoordinates[drive]);
		}
		pauseCoordinates[DRIVES] = currentFeedrate;
		isolatedMove->Release();

		// Take care of our state and of the next move to be executed when the print resumes

		state = paused;
		if (ddaRingGetPointer != ddaRingAddPointer)
		{
			// Our current move has almost stopped (velocity is instantDv), so make sure the next one accelerates nicely
			float u = ddaRingGetPointer->instantDv, v = ddaRingGetPointer->feedRate;
			ddaRingGetPointer->Init(ddaRingGetPointer->myLookAheadEntry, u, v);
		}
		ReleaseDDARingLock();
	}

	return true;
}

inline bool Move::IsPaused() const
{
	return state == paused;
}

inline bool Move::IsPausing() const
{
	return state == pausing;
}

inline bool Move::IsResuming() const
{
	return isResuming;
}

inline bool Move::IsCancelled() const
{
	return state == cancelled;
}

// Returns true when the class is ready to read moves again

inline bool Move::Resume()
{
	if (state == paused)
	{
		// Wait for isolated move to complete (if any)

		isResuming = true;
		if (!NoLiveMovement())
		{
			return false;
		}

		// Check if we're at the right XYZ coordinates to resume queued moves

		bool needMove = false;
		for(uint8_t axis=0; axis<AXES; axis++)
		{
			if (liveCoordinates[axis] != pauseCoordinates[axis])
			{
				needMove = true;
				break;
			}
		}

		// No - Go back to the right coordinates first

		if (needMove)
		{
			SetUpIsolatedMove(pauseCoordinates, currentFeedrate, true);
			return false;
		}

		// Yes - We're basically good to go again

		for(uint8_t drive=0; drive<=DRIVES; drive++)
		{
			liveCoordinates[drive] = pauseCoordinates[drive];
		}
		currentFeedrate = pauseCoordinates[DRIVES];
		isResuming = false;
		state = running;
	}

	return (state != cancelled);
}

inline void Move::Cancel()
{
	if (state != running)
	{
		state = cancelled;
	}
}

inline bool Move::DDARingEmpty() const
{
  return ddaRingGetPointer == ddaRingAddPointer;
}

inline bool Move::NoLiveMovement() const
{
	return	(dda == NULL) &&
			(state != paused || !isolatedMoveAvailable) &&
			(state != running || DDARingEmpty());
}

inline void Move::LiveMachineCoordinates(long m[]) const
{
	for(uint8_t drive=0; drive<DRIVES; drive++)
	{
		m[drive] = LookAhead::EndPointToMachine(drive, liveCoordinates[drive]);
	}
}

// Leave a gap of 2 as the last Get result may still be being processed

inline bool Move::DDARingFull() const
{
  return ddaRingAddPointer->Next()->Next() == ddaRingGetPointer;
}

inline bool Move::LookAheadRingEmpty() const
{
  return lookAheadRingCount == 0;
}

// Leave a gap of 2 as the last Get result may still be being processed

inline bool Move::LookAheadRingFull() const
{
  if(!(lookAheadRingAddPointer->Processed() & released))
    return true;
  return lookAheadRingAddPointer->Next()->Next() == lookAheadRingGetPointer;  // probably not needed; just return the bool in the if above
}

inline bool Move::GetDDARingLock()
{
  if(ddaRingLocked)
    return false;
  ddaRingLocked = true;
  return true;
}

inline void Move::ReleaseDDARingLock()
{
  ddaRingLocked = false;
}

inline void Move::LiveCoordinates(float m[]) const
{
	for(int8_t drive = 0; drive <= DRIVES; drive++)
	{
		m[drive] = liveCoordinates[drive];
	}
	InverseTransform(m);
}

inline bool Move::GetPauseCoordinates(float m[]) const
{
	if (!IsPaused())
		return false;

	for(uint8_t drive = 0; drive < DRIVES; drive++)
	{
		m[drive] = pauseCoordinates[drive];
	}
	InverseTransform(m);

	return true;
}

inline void Move::GetRawExtruderPositions(float e[]) const
{
	for(uint8_t extruder = 0; extruder < DRIVES - AXES; extruder++)
	{
		e[extruder] = rawExtruderPos[extruder];
	}
}


// These are the actual numbers that we want to be the coordinates, so don't transform them.
inline void Move::SetLiveCoordinates(float coords[])
{
	for(int8_t drive = 0; drive <= DRIVES; drive++)
	{
		liveCoordinates[drive] = coords[drive];
	}
}

// Resets the extruder positions to zero

inline void Move::ResetExtruderPositions()
{
	for(uint8_t drive = AXES; drive < DRIVES; drive++)
	{
		liveCoordinates[drive] = 0.0;
		rawExtruderPos[drive - AXES] = 0.0;
	}
}

// To wait until all the current moves in the buffers are
// complete, call this function repeatedly and wait for it to
// return true.  Then do whatever you wanted to do after all
// current moves have finished.  THEN CALL THE ResumeMoving() FUNCTION
// OTHERWISE NOTHING MORE WILL EVER HAPPEN.

inline bool Move::AllMovesAreFinished()
{
  addNoMoreMoves = true;
  return (IsPausing() || IsPaused() || LookAheadRingEmpty()) && NoLiveMovement();
}

inline void Move::AddMoreMoves()
{
  addNoMoreMoves = false;
}

inline void Move::SetXBedProbePoint(int index, float x)
{
	if(index < 0 || index >= NUMBER_OF_PROBE_POINTS)
	{
		platform->Message(BOTH_MESSAGE, "Z probe point X index out of range.\n");
		return;
	}
	xBedProbePoints[index] = x;
	probePointSet[index] |= xSet;
}

inline void Move::SetYBedProbePoint(int index, float y)
{
	if(index < 0 || index >= NUMBER_OF_PROBE_POINTS)
	{
		platform->Message(BOTH_MESSAGE, "Z probe point Y index out of range.\n");
		return;
	}
	yBedProbePoints[index] = y;
	probePointSet[index] |= ySet;
}

inline void Move::SetZBedProbePoint(int index, float z)
{
	if(index < 0 || index >= NUMBER_OF_PROBE_POINTS)
	{
		platform->Message(BOTH_MESSAGE, "Z probe point Z index out of range.\n");
		return;
	}
	zBedProbePoints[index] = z;
	probePointSet[index] |= zSet;
}

inline float Move::XBedProbePoint(int index) const
{
	return xBedProbePoints[index];
}

inline float Move::YBedProbePoint(int index) const
{
	return yBedProbePoints[index];
}

inline float Move::ZBedProbePoint(int index) const
{
	return zBedProbePoints[index];
}

inline void Move::SetZProbing(bool probing)
{
	zProbing = probing;
}

inline float Move::GetLastProbedZ() const
{
	return lastZHit;
}

// Note that we don't set the tan values to 0 here.  This means that the bed probe
// values will be a fraction of a millimeter out in X and Y, which, as the bed should
// be nearly flat (and the probe doesn't coincide with the nozzle anyway), won't matter.
// But it means that the tan values can be set for the machine
// at the start in the configuration file and be retained, without having to know and reset
// them after every Z probe of the bed.

inline void Move::SetIdentityTransform()
{
	identityBedTransform = true;
}

inline bool Move::AllProbeCoordinatesSet(int index) const
{
	return probePointSet[index] == (xSet | ySet | zSet);
}

inline bool Move::XYProbeCoordinatesSet(int index) const
{
	return (probePointSet[index]  & xSet) &&  (probePointSet[index]  & ySet);
}

inline int Move::NumberOfProbePoints() const
{
	for(int i = 0; i < NUMBER_OF_PROBE_POINTS; i++)
	{
		if(!AllProbeCoordinatesSet(i))
			return i;
	}
	return NUMBER_OF_PROBE_POINTS;
}

inline int Move::NumberOfXYProbePoints() const
{
	for(int i = 0; i < NUMBER_OF_PROBE_POINTS; i++)
	{
		if(!XYProbeCoordinatesSet(i))
			return i;
	}
	return NUMBER_OF_PROBE_POINTS;
}

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
 *   The values of x and y are transformed to put them in the interval [0, 1].
 */
inline float Move::SecondDegreeTransformZ(float x, float y) const
{
	x = (x - xBedProbePoints[0])*xRectangle;
	y = (y - yBedProbePoints[0])*yRectangle;
	return (1.0 - x)*(1.0 - y)*zBedProbePoints[0] + x*(1.0 - y)*zBedProbePoints[3] + (1.0 - x)*y*zBedProbePoints[1] + x*y*zBedProbePoints[2];
}


// This is called from the step ISR. Any variables it modifies that are also read by code outside the ISR must be declared 'volatile'.
inline void Move::HitLowStop(int8_t drive, LookAhead* la, DDA* hitDDA)
{
	UpdateCurrentCoordinates(la, hitDDA);
	float hitPoint = (hitDDA->directions[drive] == FORWARDS) ? platform->AxisMaximum(drive) : platform->AxisMinimum(drive);
	if(drive == Z_AXIS)
	{
		if(zProbing)
		{
			// Executing G32, so record the Z position at which we hit the end stop
			if (gCodes->GetAxisIsHomed(drive))
			{
				// Z-axis has already been homed, so just record the height of the bed at this point
				lastZHit = la->MachineToEndPoint(drive) - platform->ZProbeStopHeight();
				return;
			}
			else
			{
				// Z axis has not yet been homed, so treat this probe as a homing command
				lastZHit = 0.0;
				hitPoint = platform->ZProbeStopHeight();
			}
		}
		else
		{
			// Executing G30, so set the current Z height to the value at which the end stop is triggered
			// Transform it first so that the height is correct in user coordinates
			float xyzPoint[DRIVES + 1];
			LiveCoordinates(xyzPoint);
			xyzPoint[Z_AXIS] = lastZHit = platform->ZProbeStopHeight();
			Transform(xyzPoint);
			hitPoint = xyzPoint[Z_AXIS];
		}
	}
	la->SetDriveCoordinate(hitPoint, drive);
	gCodes->SetAxisIsHomed(drive);
}

// This is called from the step ISR. Any variables it modifies that are also read by code outside the ISR must be declared 'volatile'.
inline void Move::HitHighStop(int8_t drive, LookAhead* la, DDA* hitDDA)
{
	UpdateCurrentCoordinates(la, hitDDA);
	la->SetDriveCoordinate(platform->AxisMaximum(drive), drive);
	gCodes->SetAxisIsHomed(drive);
}

// This updates the end coordinates in the lookahead struct to take account of an aborted move
inline void Move::UpdateCurrentCoordinates(LookAhead* la, DDA* runningDDA)
{
	la->MoveAborted(runningDDA->totalSteps > 0 ? (float)runningDDA->stepCount/(float)runningDDA->totalSteps : 0.0);
}

inline float Move::AxisCompensation(int8_t axis)
{
	switch(axis)
	{
		case X_AXIS:
			return tanXY;

		case Y_AXIS:
			return tanYZ;

		case Z_AXIS:
			return tanXZ;

		default:
			platform->Message(BOTH_MESSAGE, "Axis compensation requested for non-existent axis.");
	}
	return 0.0;
}

inline float Move::GetExtrusionFactor(uint8_t extruder) const
{
	return extrusionFactors[extruder];
}

inline void Move::SetExtrusionFactor(uint8_t extruder, float factor)
{
	extrusionFactors[extruder] = factor;
}

inline float Move::GetSpeedFactor() const
{
	return speedFactor;
}

inline void Move::SetSpeedFactor(float factor)
{
	speedFactor = factor;
}

#endif
