/****************************************************************************************************

RepRapFirmware - Tool

This class implements a tool in the RepRap machine, usually (though not necessarily) an extruder.

Tools may have zero or more drives associated with them and zero or more heaters.  There are a fixed number
of tools in a given RepRap, with fixed heaters and drives.  All this is specified on reboot, and cannot
be altered dynamically.  This restriction may be lifted in the future.  Tool descriptions are stored in
GCode macros that are loaded on reboot.

-----------------------------------------------------------------------------------------------------

Version 0.1

Created on: Apr 11, 2014

Adrian Bowyer
RepRap Professional Ltd
http://reprappro.com

Licence: GPL

****************************************************************************************************/

#ifndef TOOL_H_
#define TOOL_H_

class Tool
{
public:

	Tool(int toolNumber, long d[], int dCount, long h[], int hCount);
	void Init(long d[], int dCount, long h[], int hCount);
	int DriveCount();
	int Drive(int driveNumber);
	bool ToolCanDrive();
	int HeaterCount();
	int Heater(int heaterNumber);
	int Number();
	void SetTemperatureVariables(float* standby, float* active);
	void GetTemperatureVariables(float* standby, float* active);
	void SetOffsets(float* off);
	void GetOffsets(float* off);
	void DefineMix(float* m);
	const float* GetMix() const;
	void TurnMixingOn();
	void TurnMixingOff();
	bool Mixing();
	float MaxFeedrate();
	float InstantDv();
	void Print(char* reply);

	friend class RepRap;

protected:

	Tool* Next();
	void Activate(Tool* currentlyActive);
	void Standby();
	void AddTool(Tool* t);
	void FlagTemperatureFault(int8_t dudHeater);
	void ClearTemperatureFault(int8_t wasDudHeater);
	void PrintInternal(char* reply, int bufferSize);

private:

	void SetTemperatureFault(int8_t dudHeater);
	void ResetTemperatureFault(int8_t wasDudHeater);
	bool AllHeatersAtHighTemperature();
	int myNumber;
	int drives[DRIVES - AXES];
	float mix[DRIVES - AXES];
	bool mixing;
	int driveCount;
	int heaters[HEATERS];
	float activeTemperatures[HEATERS];
	float standbyTemperatures[HEATERS];
	float offsets[AXES];
	int heaterCount;
	Tool* next;
	bool active;
	bool heaterFault;
};


inline int Tool::Drive(int driveNumber)
{
	return drives[driveNumber];
}

inline int Tool::HeaterCount()
{
	return heaterCount;
}

inline int Tool::Heater(int heaterNumber)
{
	return heaters[heaterNumber];
}

inline Tool* Tool::Next()
{
	return next;
}

inline int Tool::Number()
{
	return myNumber;
}

inline void Tool::DefineMix(float* m)
{
	for(int8_t drive = 0; drive < driveCount; drive++)
		mix[drive] = m[drive];
}

inline const float* Tool::GetMix() const
{
	return mix;
}

inline void Tool::TurnMixingOn()
{
	mixing = true;
}

inline void Tool::TurnMixingOff()
{
	mixing = false;
}

inline bool Tool::Mixing()
{
	return mixing;
}

inline int Tool::DriveCount()
{
	return driveCount;
}



#endif /* TOOL_H_ */
