/****************************************************************************************************

RepRapFirmware - Platform: RepRapPro Mendel with Prototype Arduino Due controller

Platform contains all the code and definitons to deal with machine-dependent things such as control 
pins, bed area, number of extruders, tolerable accelerations and speeds and so on.

-----------------------------------------------------------------------------------------------------

Version 0.1

18 November 2012

Adrian Bowyer
RepRap Professional Ltd
http://reprappro.com

Licence: GPL

****************************************************************************************************/

#include "RepRapFirmware.h"

// Default values for the arrays.

// Drives

const int8_t step_pins[DRIVES] = STEP_PINS;
const int8_t direction_pins[DRIVES] = DIRECTION_PINS;
const bool directions_[DRIVES] = DIRECTIONS;
const int8_t enable_pins[DRIVES] = ENABLE_PINS;
const bool disable_drives[DRIVES] = DISABLE_DRIVES;
const int8_t low_stop_pins[DRIVES] = LOW_STOP_PINS;
const int8_t high_stop_pins[DRIVES] = HIGH_STOP_PINS;
const int8_t pot_wipes[DRIVES] = POT_WIPES;
const float max_feedrates[DRIVES] = MAX_FEEDRATES;
const float accelerations_[DRIVES] = ACCELERATIONS;
const float drive_steps_per_unit[DRIVES] = DRIVE_STEPS_PER_UNIT;
const float instant_dvs[DRIVES] = INSTANT_DVS;

// Axes

const float axis_lengths[AXES] = AXIS_LENGTHS;
const float home_feedrates[AXES] = HOME_FEEDRATES;
const float head_offsets[AXES] = HEAD_OFFSETS;

// Heaters

const int8_t temp_sense_pins[HEATERS] = TEMP_SENSE_PINS;
const int8_t heat_on_pins[HEATERS] = HEAT_ON_PINS;
const float thermistor_betas[HEATERS] = THERMISTOR_BETAS;
const float thermistor_series_rs[HEATERS] = THERMISTOR_SERIES_RS;
const float thermistor_25_rs[HEATERS] = THERMISTOR_25_RS;
const bool use_pids[HEATERS] = USE_PIDS;
const float pid_kis[HEATERS] = PID_KIS;
const float pid_kds[HEATERS] = PID_KDS;
const float pid_kps[HEATERS] = PID_KPS;
const float full_pid_bands[HEATERS] = FULL_PID_BANDS;
const float pid_mins[HEATERS] = PID_MINS;
const float pid_maxes[HEATERS] = PID_MAXES;
const float d_mixes[HEATERS] = D_MIXES;
const float standby_temperatures[HEATERS] = STANDBY_TEMPERATURES;
const float active_temperatures[HEATERS] = ACTIVE_TEMPERATURES;

// Network

const uint8_t ip_address[4] = IP_ADDRESS;
const uint8_t net_mask[4] = NET_MASK;
const uint8_t gate_way[4] = GATE_WAY;
const uint8_t mac_address[6] = MAC_ADDRESS;


#define WINDOWED_SEND_PACKETS	(2)

extern char _end;
extern "C" char *sbrk(int i);

const uint8_t memPattern = 0xA5;

// Arduino initialise and loop functions
// Put nothing in these other than calls to the RepRap equivalents

void setup()
{
  reprap.Init();
  //reprap.GetMove()->InterruptTime();  // Uncomment this line to time the interrupt routine on startup

  // Fill the free memory with a pattern so that we can check for stack usage and memory corruption
  char* heapend = sbrk(0);
  register const char * stack_ptr asm ("sp");
  while (heapend + 16 < stack_ptr)
  {
	  *heapend++ = memPattern;
  }
}
  
void loop()
{
  reprap.Spin();
}

//*************************************************************************************************

Platform::Platform()
{
  fileStructureInitialised = false;
  
  line = new Line();

  // Files
  
  massStorage = new MassStorage(this);
  
  for(int8_t i=0; i < MAX_FILES; i++)
    files[i] = new FileStore(this);
  
  network = new Network();
  
  active = false;
}

//*******************************************************************************************************************

void Platform::Init()
{ 
  uint8_t drive;
  uint8_t heater;
  uint8_t ip;
  uint8_t file;

  compatibility = me;

  line->Init();
  messageIndent = 0;

  massStorage->Init();

  for(file=0; file < MAX_FILES; file++)
    files[file]->Init();

  fileStructureInitialised = true;

  mcpDuet.begin(); //only call begin once in the entire execution, this begins the I2C comms on that channel for all objects
  mcpExpansion.setMCP4461Address(0x2E); //not required for mcpDuet, as this uses the default address
  sysDir = SYS_DIR;
  configFile = CONFIG_FILE;

  for(ip = 0; ip < 4; ip++)
  {
	  ipAddress[ip] = ip_address[ip];
	  netMask[ip] = net_mask[ip];
	  gateWay[ip] = gate_way[ip];
  }
  for(ip = 0; ip < 6; ip++)
	  macAddress[ip] = mac_address[ip];

  // DRIVES

  for(drive = 0; drive < DRIVES; drive++)
  {
	  stepPins[drive] = step_pins[drive];
	  directionPins[drive] = direction_pins[drive];
	  directions[drive] = directions_[drive];
	  enablePins[drive] = enable_pins[drive];
	  disableDrives[drive] = disable_drives[drive];
	  lowStopPins[drive] = low_stop_pins[drive];
	  highStopPins[drive] = high_stop_pins[drive];
	  maxFeedrates[drive] = max_feedrates[drive];
	  accelerations[drive] = accelerations_[drive];
	  driveStepsPerUnit[drive] = drive_steps_per_unit[drive];
	  instantDvs[drive] = instant_dvs[drive];
	  potWipes[drive] = pot_wipes[drive];
  }

  senseResistor = SENSE_RESISTOR;
  maxStepperDigipotVoltage = MAX_STEPPER_DIGIPOT_VOLTAGE;
  //numMixingDrives = NUM_MIXING_DRIVES;

  // Z PROBE

  zProbePin = Z_PROBE_PIN;
  zProbeModulationPin = Z_PROBE_MOD_PIN;
  zProbeType = 0;
  zProbeADValue = Z_PROBE_AD_VALUE;
  zProbeStopHeight = Z_PROBE_STOP_HEIGHT;
  InitZProbe();

  // AXES

  for(drive = 0; drive < AXES; drive++)
  {
	  axisLengths[drive] = axis_lengths[drive];
	  homeFeedrates[drive] = home_feedrates[drive];
	  headOffsets[drive] = head_offsets[drive];
  }

  SetSlowestDrive();

  extrusionAncilliaryPWM = 0.0;

  // HEATERS - Bed is assumed to be index 0

  for(heater = 0; heater < HEATERS; heater++)
  {
	  tempSensePins[heater] = temp_sense_pins[heater];
	  heatOnPins[heater] = heat_on_pins[heater];
	  thermistorBetas[heater] = thermistor_betas[heater];
	  thermistorSeriesRs[heater] = thermistor_series_rs[heater];
	  thermistorRAt25[heater] = thermistor_25_rs[heater];
	  usePIDs[heater] = use_pids[heater];
	  pidKis[heater] = pid_kis[heater];
	  pidKds[heater] = pid_kds[heater];
	  pidKps[heater] = pid_kps[heater];
	  fullPidBands[heater] = full_pid_bands[heater];
	  pidMins[heater] = pid_mins[heater];
	  pidMaxes[heater] = pid_maxes[heater];
	  dMixes[heater] = d_mixes[heater];
	  standbyTemperatures[heater] = standby_temperatures[heater];
	  activeTemperatures[heater] = active_temperatures[heater];
  }

  heatSampleTime = HEAT_SAMPLE_TIME;

  coolingFanPin = COOLING_FAN_PIN;
  timeToHot = TIME_TO_HOT;

  webDir = WEB_DIR;
  gcodeDir = GCODE_DIR;
  tempDir = TEMP_DIR;

  /*
  	FIXME Nasty having to specify individually if a pin is arduino or not.
    requires a unified variant file. If implemented this would be much better
	to allow for different hardware in the future
  */
  for(drive = 0; drive < DRIVES; drive++)
  {

	  if(stepPins[drive] >= 0)
	  {
		  if(drive == E0_DRIVE || drive == E3_DRIVE) //STEP_PINS {14, 25, 5, X2, 41, 39, X4, 49}
			  pinModeNonDue(stepPins[drive], OUTPUT);
		  else
			  pinMode(stepPins[drive], OUTPUT);
	  }
	  if(directionPins[drive] >= 0)
	  {
		  if(drive == E0_DRIVE) //DIRECTION_PINS {15, 26, 4, X3, 35, 53, 51, 48}
			  pinModeNonDue(directionPins[drive], OUTPUT);
		  else
			  pinMode(directionPins[drive], OUTPUT);
	  }
	  if(enablePins[drive] >= 0)
	  {
		  if(drive == Z_AXIS || drive==E0_DRIVE || drive==E2_DRIVE) //ENABLE_PINS {29, 27, X1, X0, 37, X8, 50, 47}
			  pinModeNonDue(enablePins[drive], OUTPUT);
		  else
			  pinMode(enablePins[drive], OUTPUT);
	  }
	  Disable(drive);
	  driveEnabled[drive] = false;
  }

  for(drive = 0; drive < DRIVES; drive++)
  {
	  if(lowStopPins[drive] >= 0)
	  {
		  pinMode(lowStopPins[drive], INPUT);
		  digitalWrite(lowStopPins[drive], HIGH); // Turn on pullup
	  }
	  if(highStopPins[drive] >= 0)
	  {
		  pinMode(highStopPins[drive], INPUT);
		  digitalWrite(highStopPins[drive], HIGH); // Turn on pullup
	  }
  }  
  
  for(heater = 0; heater < HEATERS; heater++)
  {
    if(heatOnPins[heater] >= 0)
    	if(heater == E0_HEATER || heater==E1_HEATER) //HEAT_ON_PINS {6, X5, X7, 7, 8, 9}
    		pinModeNonDue(heatOnPins[heater], OUTPUT);
    	else
    		pinMode(heatOnPins[heater], OUTPUT);
    thermistorRAt25[heater] = ( thermistorRAt25[heater]*exp(-thermistorBetas[heater]/(25.0 - ABS_ZERO)) );
    tempSum[heater] = 0;
  }

  if(coolingFanPin >= 0)
  {
	  //pinModeNonDue(coolingFanPin, OUTPUT); //not required as analogwrite does this automatically
	  analogWriteNonDue(coolingFanPin, 255); //inverse logic for Duet v0.6 amd later; this turns it off
  }

  InitialiseInterrupts();
  
  addToTime = 0.0;
  lastTimeCall = 0;
  lastTime = Time();
  longWait = lastTime;
  
  active = true;
}

void Platform::SetSlowestDrive()
{
	slowestDrive = 0;
	for(int8_t drive = 1; drive < DRIVES; drive++)
	{
		if(InstantDv(drive) < InstantDv(slowestDrive))
			slowestDrive = drive;
	}
}

void Platform::InitZProbe()
{
  zModOnThisTime = true;
  zProbeOnSum = 0;
  zProbeOffSum = 0;

  //if (zProbeType == 2)
  // Always enable and fire the modulation pin as long as it's defined.  That way the following works:
  //
  //              Probe type:  0  1  2
  // Probe selected
  //            0              X  X  X
  //            1              .  X  X
  //            2              .  .  X
  //
  // Where X means the user gets what's asked for.  This is the best we can do.

  if(zProbeModulationPin >= 0)
  {
	if(zProbeType == 3)
	{
		pinModeNonDue(zProbeModulationPin, OUTPUT);
		digitalWriteNonDue(zProbeModulationPin, HIGH);	// turn on the IR LED
	}else
	{
		pinMode(zProbeModulationPin, OUTPUT);
		digitalWrite(zProbeModulationPin, HIGH);
	}
  }
}

void Platform::StartNetwork()
{
	network->Init();
}

void Platform::Spin()
{
  if(!active)
    return;

  network->Spin();
  line->Spin();

  if(Time() - lastTime < POLL_TIME)
    return;
  PollZHeight();
  PollTemperatures();
  lastTime = Time();
  ClassReport("Platform", longWait);

}

//*****************************************************************************************************************

// Interrupts

void TC3_Handler()
{
  TC_GetStatus(TC1, 0);
  reprap.Interrupt();
}

void Platform::InitialiseInterrupts()
{
  pmc_set_writeprotect(false);
  pmc_enable_periph_clk((uint32_t)TC3_IRQn);
  TC_Configure(TC1, 0, TC_CMR_WAVE | TC_CMR_WAVSEL_UP_RC | TC_CMR_TCCLKS_TIMER_CLOCK4);
  TC1->TC_CHANNEL[0].TC_IER=TC_IER_CPCS;
  TC1->TC_CHANNEL[0].TC_IDR=~TC_IER_CPCS;
  SetInterrupt(STANDBY_INTERRUPT_RATE);
}

//void Platform::DisableInterrupts()
//{
//	NVIC_DisableIRQ(TC3_IRQn);
//}


//*************************************************************************************************

void Platform::Diagnostics() 
{
  Message(HOST_MESSAGE, "Platform Diagnostics:\n"); 
}

// Print memory stats to USB and append them to the current webserver reply, and
// give the main loop timing stats.

void Platform::PrintMemoryUsage()
{
	const char *ramstart=(char *)0x20070000;
	const char *ramend=(char *)0x20088000;
    const char *heapend=sbrk(0);
	register const char * stack_ptr asm ("sp");
	const struct mallinfo mi = mallinfo();
	Message(BOTH_MESSAGE, "\n");
	AppendMessage(BOTH_MESSAGE, "Memory usage:\n\n");
	snprintf(scratchString, STRING_LENGTH, "Program static ram used: %d\n", &_end - ramstart);
	AppendMessage(BOTH_MESSAGE, scratchString);
	snprintf(scratchString, STRING_LENGTH, "Dynamic ram used: %d\n", mi.uordblks);
	AppendMessage(BOTH_MESSAGE, scratchString);
	snprintf(scratchString, STRING_LENGTH, "Recycled dynamic ram: %d\n", mi.fordblks);
	AppendMessage(BOTH_MESSAGE, scratchString);
	snprintf(scratchString, STRING_LENGTH, "Current stack ram used: %d\n", ramend - stack_ptr);
	AppendMessage(BOTH_MESSAGE, scratchString);
	const char* stack_lwm = heapend;
	while (stack_lwm < stack_ptr && *stack_lwm == memPattern)
	{
		++stack_lwm;
	}
	snprintf(scratchString, STRING_LENGTH, "Maximum stack ram used: %d\n", ramend - stack_lwm);
	AppendMessage(BOTH_MESSAGE, scratchString);
	snprintf(scratchString, STRING_LENGTH, "Never used ram: %d\n", stack_lwm - heapend);
	AppendMessage(BOTH_MESSAGE, scratchString);
	reprap.Timing();
}

void Platform::ClassReport(char* className, float &lastTime)
{
	if(!reprap.Debug())
		return;
	if(Time() - lastTime < LONG_TIME)
		return;
	lastTime = Time();
	snprintf(scratchString, STRING_LENGTH, "Class %s spinning.\n", className);
	Message(HOST_MESSAGE, scratchString);
}


//===========================================================================
//=============================Thermal Settings  ============================
//===========================================================================

// See http://en.wikipedia.org/wiki/Thermistor#B_or_.CE.B2_parameter_equation

// BETA is the B value
// RS is the value of the series resistor in ohms
// R_INF is R0.exp(-BETA/T0), where R0 is the thermistor resistance at T0 (T0 is in kelvin)
// Normally T0 is 298.15K (25 C).  If you write that expression in brackets in the #define the compiler 
// should compute it for you (i.e. it won't need to be calculated at run time).

// If the A->D converter has a range of 0..1023 and the measured voltage is V (between 0 and 1023)
// then the thermistor resistance, R = V.RS/(1024 - V)
// and the temperature, T = BETA/ln(R/R_INF)
// To get degrees celsius (instead of kelvin) add -273.15 to T

// Result is in degrees celsius

float Platform::GetTemperature(int8_t heater)
{
  // If the ADC reading is N then for an ideal ADC, the input voltage is at least N/(AD_RANGE + 1) and less than (N + 1)/(AD_RANGE + 1), times the analog reference.
  // So we add 0.5 to to the reading to get a better estimate of the input.
  int rawTemp = tempSum[heater]/NUMBER_OF_A_TO_D_READINGS_AVERAGED; //GetRawTemperature(heater);

  // First, recognise the special case of thermistor disconnected.
//  if (rawTemp == AD_RANGE)
//  {
//	  // Thermistor is disconnected
//	  return ABS_ZERO;
//  }
  float r = (float)rawTemp + 0.5;
  r = ABS_ZERO + thermistorBetas[heater]/log( (r*thermistorSeriesRs[heater]/((AD_RANGE + 1) - r))/thermistorRAt25[heater] );
  return r;
}


// power is a fraction in [0,1]

void Platform::SetHeater(int8_t heater, const float& power)
{
  if(heatOnPins[heater] < 0)
    return;
  
  byte p = (byte)(255.0*fmin(1.0, fmax(0.0, power)));
  if(HEAT_ON == 0)
	  p = 255 - p;
  if(heater == E0_HEATER || heater == E1_HEATER) //HEAT_ON_PINS {6, X5, X7, 7, 8, 9}
	 analogWriteNonDue(heatOnPins[heater], p);
  else
	 analogWrite(heatOnPins[heater], p);
}


EndStopHit Platform::Stopped(int8_t drive)
{
	if(zProbeType > 0)
	{  // Z probe is used for both X and Z.
		if(drive != Y_AXIS)
		{
			if(ZProbe() > zProbeADValue)
				return lowHit;
			else
				return noStop;
		}
	}

	if(lowStopPins[drive] >= 0)
	{
		if(digitalRead(lowStopPins[drive]) == ENDSTOP_HIT)
			return lowHit;
	}
	if(highStopPins[drive] >= 0)
	{
		if(digitalRead(highStopPins[drive]) == ENDSTOP_HIT)
			return highHit;
	}
	return noStop;
}


/*********************************************************************************

  Files & Communication
  
*/

MassStorage::MassStorage(Platform* p)
{
   platform = p;
}

void MassStorage::Init()
{
	hsmciPinsinit();
	// Initialize SD MMC stack
	sd_mmc_init();
	delay(20);
	int sdPresentCount = 0;
	while ((CTRL_NO_PRESENT == sd_mmc_check(0)) && (sdPresentCount < 5))
	{
		//platform->Message(HOST_MESSAGE, "Please plug in the SD card.\n");
		//delay(1000);
		sdPresentCount++;
	}

	if(sdPresentCount >= 5)
	{
		platform->Message(HOST_MESSAGE, "Can't find the SD card.\n");
		return;
	}

	//print card info

//	SerialUSB.print("sd_mmc_card->capacity: ");
//	SerialUSB.print(sd_mmc_get_capacity(0));
//	SerialUSB.print(" bytes\n");
//	SerialUSB.print("sd_mmc_card->clock: ");
//	SerialUSB.print(sd_mmc_get_bus_clock(0));
//	SerialUSB.print(" Hz\n");
//	SerialUSB.print("sd_mmc_card->bus_width: ");
//	SerialUSB.println(sd_mmc_get_bus_width(0));

	memset(&fileSystem, 0, sizeof(FATFS));
	//f_mount (LUN_ID_SD_MMC_0_MEM, NULL);
	//int mounted = f_mount(LUN_ID_SD_MMC_0_MEM, &fileSystem);
	int mounted = f_mount(0, &fileSystem);
	if (mounted != FR_OK)
	{
		platform->Message(HOST_MESSAGE, "Can't mount filesystem 0: code ");
		snprintf(scratchString, STRING_LENGTH, "%d", mounted);
		platform->Message(HOST_MESSAGE, scratchString);
		platform->Message(HOST_MESSAGE, "\n");
	}
}

char* MassStorage::CombineName(const char* directory, const char* fileName)
{
  int out = 0;
  int in = 0;
  
//  scratchString[out] = '/';
//  out++;
  
  if(directory != NULL)
  {
    //if(directory[in] == '/')
    //  in++;
    while(directory[in] != 0 && directory[in] != '\n')// && directory[in] != '/')
    {
      scratchString[out] = directory[in];
      in++;
      out++;
      if(out >= STRING_LENGTH)
      {
         platform->Message(HOST_MESSAGE, "CombineName() buffer overflow.");
         out = 0;
      }
    }
  }
  
  //scratchString[out] = '/';
 // out++;
  
  in = 0;
  while(fileName[in] != 0 && fileName[in] != '\n')// && fileName[in] != '/')
  {
    scratchString[out] = fileName[in];
    in++;
    out++;
    if(out >= STRING_LENGTH)
    {
       platform->Message(HOST_MESSAGE, "CombineName() buffer overflow.");
       out = 0;
    }
  }
  scratchString[out] = 0;
  
  return scratchString;
}

// List the flat files in a directory.  No sub-directories or recursion.

char* MassStorage::FileList(const char* directory, bool fromLine)
{
//  File dir, entry;
  DIR dir;
  FILINFO entry;
  FRESULT res;
  char loc[64];
  int len = 0;
  char fileListBracket = FILE_LIST_BRACKET;
  char fileListSeparator = FILE_LIST_SEPARATOR;

  if(fromLine)
  {
	  if(platform->Emulating() == marlin)
	  {
		  fileListBracket = 0;
		  fileListSeparator = '\n';
	  }
  }

  len = strlen(directory);
  strncpy(loc,directory,len-1);
  loc[len - 1 ] = 0;

//  if(reprap.debug()) {
//	  platform->Message(HOST_MESSAGE, "Opening: ");
//	  platform->Message(HOST_MESSAGE, loc);
//	  platform->Message(HOST_MESSAGE, "\n");
//  }

  res = f_opendir(&dir,loc);
  if(res == FR_OK)
  {

//	  if(reprap.debug()) {
//		  platform->Message(HOST_MESSAGE, "Directory open\n");
//	  }

	  int p = 0;
//  int q;
	  int foundFiles = 0;

	  f_readdir(&dir,0);

	  while((f_readdir(&dir,&entry) == FR_OK) && (foundFiles < MAX_FILES))
	  {
		  foundFiles++;

		  if(strlen(entry.fname) > 0)
		  {
			int q = 0;
			if(fileListBracket)
				fileList[p++] = fileListBracket;
			while(entry.fname[q])
			{
			  fileList[p++] = entry.fname[q];
			  //SerialUSB.print(entry.fname[q]);
			  q++;
			  if(p >= FILE_LIST_LENGTH - 10) // Caution...
			  {
				platform->Message(HOST_MESSAGE, "FileList - directory: ");
				platform->Message(HOST_MESSAGE, directory);
				platform->Message(HOST_MESSAGE, " has too many files!\n");
				return "";
			  }
			}
			if(fileListBracket)
				fileList[p++] = fileListBracket;
			fileList[p++] = fileListSeparator;
		  }
	  }

	  if(foundFiles <= 0)
		return "NONE";

	  fileList[--p] = 0; // Get rid of the last separator
	  return fileList;
  }

	return "";
}

// Delete a file
bool MassStorage::Delete(const char* directory, const char* fileName)
{
	char* location = platform->GetMassStorage()->CombineName(directory, fileName);
	if( f_unlink (location) != FR_OK)
	{
		platform->Message(HOST_MESSAGE, "Can't delete file ");
		platform->Message(HOST_MESSAGE, location);
		platform->Message(HOST_MESSAGE, "\n");
		return false;
	}
	return true;
}

//------------------------------------------------------------------------------------------------


FileStore::FileStore(Platform* p)
{
   platform = p;  
}


void FileStore::Init()
{
  bufferPointer = 0;
  inUse = false;
  writing = false;
  lastBufferEntry = 0;
}


// Open a local file (for example on an SD card).
// This is protected - only Platform can access it.

bool FileStore::Open(const char* directory, const char* fileName, bool write)
{
  char* location = platform->GetMassStorage()->CombineName(directory, fileName);

  writing = write;
  lastBufferEntry = FILE_BUF_LEN - 1;
  bytesRead = 0;
  FRESULT openReturn;

  if(writing)
  {
	  openReturn = f_open(&file, location, FA_CREATE_ALWAYS | FA_WRITE);
	  if (openReturn != FR_OK)
	  {
		  platform->Message(HOST_MESSAGE, "Can't open ");
		  platform->Message(HOST_MESSAGE, location);
		  platform->Message(HOST_MESSAGE, " to write to.  Error code: ");
		  snprintf(scratchString, STRING_LENGTH, "%d", openReturn);
		  platform->Message(HOST_MESSAGE, scratchString);
		  platform->Message(HOST_MESSAGE, "\n");
		  return false;
	  }
	  bufferPointer = 0;
  } else
  {
	  openReturn = f_open(&file, location, FA_OPEN_EXISTING | FA_READ);
	  if (openReturn != FR_OK)
	  {
		  platform->Message(HOST_MESSAGE, "Can't open ");
		  platform->Message(HOST_MESSAGE, location);
		  platform->Message(HOST_MESSAGE, " to read from.  Error code: ");
		  snprintf(scratchString, STRING_LENGTH, "%d", openReturn);
		  platform->Message(HOST_MESSAGE, scratchString);
		  platform->Message(HOST_MESSAGE, "\n");
		  return false;
	  }
	  bufferPointer = FILE_BUF_LEN;
  }

  inUse = true;
  return true;
}

void FileStore::Close()
{
  if(writing)
	  WriteBuffer();
  f_close(&file);
  platform->ReturnFileStore(this);
  inUse = false;
  writing = false;
  lastBufferEntry = 0;
}

void FileStore::GoToEnd()
{
  if(!inUse)
  {
    platform->Message(HOST_MESSAGE, "Attempt to seek on a non-open file.\n");
    return;
  }
  unsigned long e = Length();
  f_lseek(&file, e);
}

unsigned long FileStore::Length()
{
  if(!inUse)
  {
    platform->Message(HOST_MESSAGE, "Attempt to size non-open file.\n");
    return 0;
  }
  return file.fsize;
	return 0;
}

float FileStore::FractionRead()
{
	unsigned long len = Length();
	if(len <= 0)
		return 0.0;
	return (float)bytesRead/(float)len;
}

int8_t FileStore::Status()
{
  if(!inUse)
    return nothing;

  if(lastBufferEntry == FILE_BUF_LEN)
	return byteAvailable;

  if(bufferPointer < lastBufferEntry)
    return byteAvailable;
    
  return nothing;
}

void FileStore::ReadBuffer()
{
	FRESULT readStatus;
	readStatus = f_read(&file, buf, FILE_BUF_LEN, &lastBufferEntry);	// Read a chunk of file
	if (readStatus)
	{
		platform->Message(HOST_MESSAGE, "Error reading file.\n");
	}
	bufferPointer = 0;
}

bool FileStore::Read(char& b)
{
  if(!inUse)
  {
    platform->Message(HOST_MESSAGE, "Attempt to read from a non-open file.\n");
    return false;
  }

  if(bufferPointer >= FILE_BUF_LEN)
	  ReadBuffer();

  if(bufferPointer >= lastBufferEntry)
  {
	  b = 0;  // Good idea?
	  return false;
  }

  b = (char)buf[bufferPointer];
  bufferPointer++;
  bytesRead++;
  return true;
}

void FileStore::WriteBuffer()
{
	FRESULT writeStatus;
	writeStatus = f_write(&file, buf, bufferPointer, &lastBufferEntry);
	if((writeStatus != FR_OK) || (lastBufferEntry != bufferPointer))
	{
		platform->Message(HOST_MESSAGE, "Error writing file.  Disc may be full.\n");
	}
	bufferPointer = 0;
}


void FileStore::Write(char b)
{
  if(!inUse)
  {
    platform->Message(HOST_MESSAGE, "Attempt to write byte to a non-open file.\n");
    return;
  }
  buf[bufferPointer] = b;
  bufferPointer++;
  if(bufferPointer >= FILE_BUF_LEN)
	  WriteBuffer();
}

void FileStore::Write(const char* b)
{
  if(!inUse)
  {
    platform->Message(HOST_MESSAGE, "Attempt to write string to a non-open file.\n");
    return;
  }
  int i = 0;
  while(b[i])
    Write(b[i++]);
}


//-----------------------------------------------------------------------------------------------------

FileStore* Platform::GetFileStore(const char* directory, const char* fileName, bool write)
{
  FileStore* result = NULL;

  if(!fileStructureInitialised)
	  return NULL;

  for(int i = 0; i < MAX_FILES; i++)
    if(!files[i]->inUse)
    {
      files[i]->inUse = true;
      if(files[i]->Open(directory, fileName, write))
        return files[i];
      else
      {
        files[i]->inUse = false;
        return NULL;
      }
    }
  Message(HOST_MESSAGE, "Max open file count exceeded.\n");
  return NULL;
}


MassStorage* Platform::GetMassStorage()
{
  return massStorage;
}

void Platform::ReturnFileStore(FileStore* fs)
{
  for(int i = 0; i < MAX_FILES; i++)
      if(files[i] == fs)
        {
          files[i]->inUse = false;
          return;
        }
}

void Platform::Message(char type, const char* message)
{
	switch(type)
	{
	case FLASH_LED:
		// Message that is to flash an LED; the next two bytes define
		// the frequency and M/S ratio.

		break;

	case DISPLAY_MESSAGE:
		// Message that is to appear on a local display;  \f and \n should be supported.

		break;

	case HOST_MESSAGE:
		// Message that is to be sent to the host via USB; the H is not sent.
		for(uint8_t i = 0; i < messageIndent; i++)
			line->Write(' ');
		line->Write(message);
		break;

	case WEB_MESSAGE:
		// Message that is to be sent to the web
		reprap.GetWebserver()->MessageStringToWebInterface(message, false);
		break;

	case WEB_ERROR_MESSAGE:
		// Message that is to be sent to the web - flags an error
		reprap.GetWebserver()->MessageStringToWebInterface(message, true);
		break;

	case BOTH_MESSAGE:
		// Message that is to be sent to the web & host
		for(uint8_t i = 0; i < messageIndent; i++)
			line->Write(' ');
		line->Write(message);
		reprap.GetWebserver()->MessageStringToWebInterface(message, false);
		break;

	case BOTH_ERROR_MESSAGE:
		// Message that is to be sent to the web & host - flags an error
		// Make this the default behaviour too.

	default:
		for(uint8_t i = 0; i < messageIndent; i++)
			line->Write(' ');
		line->Write(message);
		reprap.GetWebserver()->MessageStringToWebInterface(message, true);
		break;


	}
}

void Platform::AppendMessage(char type, const char* message)
{
	switch(type)
	{
	case FLASH_LED:
		// Message that is to flash an LED; the next two bytes define
		// the frequency and M/S ratio.

		break;

	case DISPLAY_MESSAGE:
		// Message that is to appear on a local display;  \f and \n should be supported.

		break;

	case HOST_MESSAGE:
		// Message that is to be sent to the host via USB; the H is not sent.
		for(uint8_t i = 0; i < messageIndent; i++)
			line->Write(' ');
		line->Write(message);
		break;

	case WEB_MESSAGE:
		// Message that is to be sent to the web
		reprap.GetWebserver()->AppendReplyToWebInterface(message, false);
		break;

	case WEB_ERROR_MESSAGE:
		// Message that is to be sent to the web - flags an error
		reprap.GetWebserver()->AppendReplyToWebInterface(message, true);
		break;

	case BOTH_MESSAGE:
		// Message that is to be sent to the web & host
		for(uint8_t i = 0; i < messageIndent; i++)
			line->Write(' ');
		line->Write(message);
		reprap.GetWebserver()->AppendReplyToWebInterface(message, false);
		break;

	case BOTH_ERROR_MESSAGE:
		// Message that is to be sent to the web & host - flags an error
		// Make this the default behaviour too.

	default:
		for(uint8_t i = 0; i < messageIndent; i++)
			line->Write(' ');
		line->Write(message);
		reprap.GetWebserver()->AppendReplyToWebInterface(message, true);
		break;


	}
}


void Platform::SetPidValues(size_t heater, float pVal, float iVal, float dVal)
{
	if (heater < HEATERS)
	{
		pidKps[heater] = pVal;
		pidKis[heater] = iVal / heatSampleTime;
		pidKds[heater] = dVal * heatSampleTime;
	}
}



//***************************************************************************************************

// Serial/USB class

Line::Line()
{
}

void Line::Init()
{
	getIndex = 0;
	numChars = 0;
//	alternateInput = NULL;
//	alternateOutput = NULL;
	SerialUSB.begin(BAUD_RATE);
	//while (!SerialUSB.available());
}

void Line::Spin()
{
	// Read the serial data in blocks to avoid excessive flow control
	if (numChars <= lineBufsize/2)
	{
		int16_t target = SerialUSB.available() + (int16_t)numChars;
		if (target > lineBufsize)
		{
			target = lineBufsize;
		}
		while ((int16_t)numChars < target)
		{
			int incomingByte = SerialUSB.read();
			if (incomingByte < 0) break;
			buffer[(getIndex + numChars) % lineBufsize] = (char)incomingByte;
			++numChars;
		}
	}
}

// This is only ever called on initialisation, so we
// know the buffer won't overflow

void Line::InjectString(char* string)
{
	int i = 0;
	while(string[i])
	{
		buffer[(getIndex + numChars) % lineBufsize] = string[i];
		numChars++;
		i++;
	}
}

//***************************************************************************************************

// Network/Ethernet class

// C calls to interface with LWIP (http://savannah.nongnu.org/projects/lwip/)
// These are implemented in, and called from, a modified version of httpd.c
// in the network directory.

extern "C"
{

//void ResetEther();

// Transmit data to the Network

void RepRapNetworkSendOutput(char* data, int length, void* pbuf, void* pcb, void* hs);

// When lwip releases storage, set the local copy of the pointer to 0 to stop
// it being used again.

void RepRapNetworkInputBufferReleased(void* pb)
{
	reprap.GetPlatform()->GetNetwork()->InputBufferReleased(pb);
}

void RepRapNetworkConnectionError(void* h)
{
	reprap.GetPlatform()->GetNetwork()->ConnectionError(h);
	reprap.GetWebserver()->ConnectionError();
}

// Called to put out a message via the RepRap firmware.

void RepRapNetworkMessage(char* s)
{
	reprap.GetPlatform()->Message(HOST_MESSAGE, s);
}

// Called to push data into the RepRap firmware.

void RepRapNetworkReceiveInput(char* data, int length, void* pbuf, void* pcb, void* hs)
{
	reprap.GetPlatform()->GetNetwork()->ReceiveInput(data, length, pbuf, pcb, hs);
}

// Called when transmission of outgoing data is complete to allow
// the RepRap firmware to write more.

void RepRapNetworkSentPacketAcknowledged()
{
	reprap.GetPlatform()->GetNetwork()->SentPacketAcknowledged();
}

bool RepRapNetworkHasALiveClient()
{
	return reprap.GetPlatform()->GetNetwork()->Status() & clientLive;
}

// This one is in ethernetif.c

void RepRapNetworkSetMACAddress(const u8_t mac[]);


}	// extern "C"


Network::Network()
{
	active = false;
	ethPinsInit();

	//ResetEther();

	// Construct the ring buffer

	netRingAddPointer = new NetRing(NULL);
	netRingGetPointer = netRingAddPointer;
	for(int8_t i = 1; i < HTTP_STATE_SIZE; i++)
		netRingGetPointer = new NetRing(netRingGetPointer);
	netRingAddPointer->SetNext(netRingGetPointer);
	enabled = true;
}

// Reset the network to its disconnected and ready state.

void Network::Reset()
{
	//reprap.GetPlatform()->Message(HOST_MESSAGE, "Reset.\n");
	inputPointer = 0;
	inputLength = -1;
	outputPointer = 0;
	writeEnabled = false;
	closePending = false;
	status = nothing;
	sentPacketsOutstanding = 0;
}

void Network::CleanRing()
{
	for(int8_t i = 0; i <= HTTP_STATE_SIZE; i++)
	{
		netRingGetPointer->Free();
		netRingGetPointer = netRingGetPointer->Next();
	}
	netRingAddPointer = netRingGetPointer;
}

void Network::Init()
{
	if(!enabled)
	{
		reprap.GetPlatform()->Message(HOST_MESSAGE, "Attempting to start the network when it is disabled.\n");
		return;
	}
	CleanRing();
	Reset();
	RepRapNetworkSetMACAddress(reprap.GetPlatform()->MACAddress());
	init_ethernet(reprap.GetPlatform()->IPAddress(), reprap.GetPlatform()->NetMask(), reprap.GetPlatform()->GateWay());
	active = true;
	sentPacketsOutstanding = 0;
	windowedSendPackets = WINDOWED_SEND_PACKETS;
}

void Network::Spin()
{
	if(!active || !enabled)
	{
		//ResetEther();
		return;
	}

	// Keep the Ethernet running

	ethernet_task();

	// Anything come in from the network to act on?

	if(!netRingGetPointer->Active())
		return;

	// Finished reading the active ring element?

	if(!netRingGetPointer->ReadFinished())
	{
		// No - Finish reading any data that's been received.

		if(inputPointer < inputLength)
			return;

		// Haven't started reading it yet - set that up.

		inputPointer = 0;
		inputLength = netRingGetPointer->Length();
		inputBuffer = netRingGetPointer->Data();
	}
}

// Webserver calls this to read bytes that have come in from the network

bool Network::Read(char& b)
{
	if(inputPointer >= inputLength)
	{
		inputLength = -1;
		inputPointer = 0;
		netRingGetPointer->SetReadFinished(); // Past tense...
		SetWriteEnable(true);
		//reprap.GetPlatform()->Message(HOST_MESSAGE, "Network - data read.\n");
		return false;
	}
	b = inputBuffer[inputPointer];
	inputPointer++;
	return true;
}

// Webserver calls this to write bytes that need to go out to the network

void Network::Write(char b)
{
	// Check for horrible things...

	if(!CanWrite())
	{
		reprap.GetPlatform()->Message(HOST_MESSAGE, "Network::Write(char b) - Attempt to write when disabled.\n");
		return;
	}

	if(outputPointer >= ARRAY_SIZE(outputBuffer))
	{
		reprap.GetPlatform()->Message(HOST_MESSAGE, "Network::Write(char b) - Output buffer overflow! \n");
		return;
	}

	// Add the byte to the buffer

	outputBuffer[outputPointer] = b;
	outputPointer++;

	// Buffer full?  If so, send it.

	if(outputPointer == ARRAY_SIZE(outputBuffer))
	{
		if(windowedSendPackets > 1)
			++sentPacketsOutstanding;
		else
			SetWriteEnable(false);  // Stop further writing from Webserver until the network tells us that this has gone

		RepRapNetworkSendOutput(outputBuffer, outputPointer, netRingGetPointer->Pbuf(), netRingGetPointer->Pcb(), netRingGetPointer->Hs());
		outputPointer = 0;
	}
}

void Network::InputBufferReleased(void* pb)
{
	if(netRingGetPointer->Pbuf() != pb)
	{
		reprap.GetPlatform()->Message(HOST_MESSAGE, "Network::InputBufferReleased() - Pointers don't match!\n");
		return;
	}
	netRingGetPointer->ReleasePbuf();
}

void Network::ConnectionError(void* h)
{
	// h points to an http state block that the caller is about to release, so we need to stop referring to it.
	// The state block is usually but not always in use by the current http request being processed, in which case we abandon the current request.
	if (netRingGetPointer != netRingAddPointer && netRingGetPointer->Hs() == h)
	{
		netRingGetPointer->Free();
		netRingGetPointer = netRingGetPointer->Next();
	}

	// Reset the network layer. In particular, this clears the output buffer to make sure nothing more gets sent,
	// and sets status to 'nothing' so that we can accept another connection attempt.
	Reset();
}


void Network::ReceiveInput(char* data, int length, void* pbuf, void* pcb, void* hs)
{
	status = clientLive;
	if(netRingAddPointer->Active())
	{
		reprap.GetPlatform()->Message(HOST_MESSAGE, "Network::ReceiveInput() - Ring buffer full!\n");
		return;
	}
	netRingAddPointer->Init(data, length, pbuf, pcb, hs);
	netRingAddPointer = netRingAddPointer->Next();
	//reprap.GetPlatform()->Message(HOST_MESSAGE, "Network - input received.\n");
}


bool Network::CanWrite() const
{
	if(!enabled)
		return false;
	if(windowedSendPackets > 1)
		return writeEnabled && sentPacketsOutstanding < windowedSendPackets;
	return writeEnabled;
}

void Network::SetWriteEnable(bool enable)
{
	writeEnabled = enable;
	if(!writeEnabled)
		return;
	if(closePending)
		Close();
}

void Network::SentPacketAcknowledged()
{
	if(windowedSendPackets > 1)
	{
		if (sentPacketsOutstanding != 0)
		{
			--sentPacketsOutstanding;
		}
		if (closePending && sentPacketsOutstanding == 0)
		{
			Close();
		}
	} else
		SetWriteEnable(true);
}


// This is not called for data, only for internally-
// generated short strings at the start of a transmission,
// so it should never overflow the buffer (which is checked
// anyway).

void Network::Write(const char* s)
{
	int i = 0;
	while(s[i])
		Write(s[i++]);
}


void Network::Close()
{
	if(Status() && clientLive)
	{
		if(outputPointer > 0)
		{
			SetWriteEnable(false);
			RepRapNetworkSendOutput(outputBuffer, outputPointer, netRingGetPointer->Pbuf(), netRingGetPointer->Pcb(), netRingGetPointer->Hs());
			outputPointer = 0;
			closePending = true;
			return;
		}
		RepRapNetworkSendOutput((char*)NULL, 0, netRingGetPointer->Pbuf(), netRingGetPointer->Pcb(), netRingGetPointer->Hs());
		netRingGetPointer->Free();
		netRingGetPointer = netRingGetPointer->Next();
		//reprap.GetPlatform()->Message(HOST_MESSAGE, "Network - output sent and closed.\n");
	} else
		reprap.GetPlatform()->Message(HOST_MESSAGE, "Network::Close() - Attempt to close a closed connection!\n");
	closePending = false;
	status = nothing;
	//Reset();
}

int8_t Network::Status() const
{
	if(inputPointer >= inputLength)
		return status;
	return status | clientConnected | byteAvailable;
}


NetRing::NetRing(NetRing* n)
{
	next = n;
	Free();
}

void NetRing::Free()
{
	pbuf = 0;
	pcb = 0;
	hs = 0;
	data = "";
	length = 0;
	read = false;
	active = false;
}

bool NetRing::Init(char* d, int l, void* pb, void* pc, void* h)
{
	if(active)
		return false;
	pbuf = pb;
	pcb = pc;
	hs = h;
	data = d;
	length = l;
	read = false;
	active = true;
	return true;
}

NetRing* NetRing::Next()
{
	return next;
}

char* NetRing::Data()
{
	return data;
}

int NetRing::Length()
{
	return length;
}

bool NetRing::ReadFinished()
{
	return read;
}

void NetRing::SetReadFinished()
{
	read = true;
}

bool NetRing::Active()
{
	return active;
}

void NetRing::SetNext(NetRing* n)
{
	next = n;
}

void* NetRing::Pbuf()
{
	return pbuf;
}

void NetRing::ReleasePbuf()
{
	pbuf = 0;
}

void* NetRing::Pcb()
{
	return pcb;
}

void* NetRing::Hs()
{
	return hs;
}

void NetRing::ReleaseHs()
{
	hs = 0;
}



















