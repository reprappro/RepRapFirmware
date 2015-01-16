/****************************************************************************************************

RepRapFirmware - Reprap

RepRap is a simple class that acts as a container for an instance of all the others.

-----------------------------------------------------------------------------------------------------

Version 0.1

21 May 2013

Adrian Bowyer
RepRap Professional Ltd
http://reprappro.com

Licence: GPL

****************************************************************************************************/

#ifndef REPRAP_H
#define REPRAP_H

class RepRap
{    
  public:
      
    RepRap();
    void EmergencyStop();
    void Init();
    void Spin();
    void Exit();
    void Interrupt();
    void Diagnostics();
    void Timing();
    bool Debug() const;
    void SetDebug(bool d);
    void AddTool(Tool* t);
    void SelectTool(int toolNumber);
    void StandbyTool(int toolNumber);
    Tool* GetCurrentTool();
    Tool* GetTool(int toolNumber);
    //void SetToolVariables(int toolNumber, float* standbyTemperatures, float* activeTemperatures, float* offsets);
    void AllowColdExtrude();
    void DenyColdExtrude();
    bool ColdExtrude();
    void PrintTool(int toolNumber, char* reply);
    void PrintTools(char* reply);
	void FlagTemperatureFault(int8_t dudHeater);
	void ClearTemperatureFault(int8_t wasDudHeater);
    Platform* GetPlatform() const;
    Move* GetMove() const;
    Heat* GetHeat() const;
    GCodes* GetGCodes() const;
    Webserver* GetWebserver() const;
    
  private:
  
    Platform* platform;
    bool active;
    Move* move;
    Heat* heat;
    GCodes* gCodes;
    Webserver* webserver;
    Tool* toolList;
    Tool* currentTool;
    bool debug;
    float fastLoop, slowLoop;
    float lastTime;
    bool coldExtrude;
};

inline Platform* RepRap::GetPlatform() const { return platform; }
inline Move* RepRap::GetMove() const { return move; }
inline Heat* RepRap::GetHeat() const { return heat; }
inline GCodes* RepRap::GetGCodes() const { return gCodes; }
inline Webserver* RepRap::GetWebserver() const { return webserver; }
inline bool RepRap::Debug() const { return debug; }
inline Tool* RepRap::GetCurrentTool() { return currentTool; }
inline bool RepRap::ColdExtrude() { return coldExtrude; }
inline void RepRap::AllowColdExtrude() { coldExtrude = true; }

inline void RepRap::FlagTemperatureFault(int8_t dudHeater)
{
	if(toolList != NULL)
		toolList->FlagTemperatureFault(dudHeater);
}

inline void RepRap::ClearTemperatureFault(int8_t wasDudHeater)
{
	reprap.GetHeat()->ResetFault(wasDudHeater);
	if(toolList != NULL)
		toolList->ClearTemperatureFault(wasDudHeater);
}

inline void RepRap::SetDebug(bool d)
{
	debug = d;
	if(debug)
	{
		platform->Message(BOTH_MESSAGE, "Debugging enabled\n");
		platform->PrintMemoryUsage();
	}
	else
	{
		platform->Message(WEB_MESSAGE, "");
	}
}

inline void RepRap::Interrupt() { move->Interrupt(); }

#endif


