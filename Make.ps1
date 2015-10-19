# PowerShell make script for RepRapFirmware
# written by Christian Hammacher
#
# Licensed under the terms of the GNU Public License v2
# see http://www.gnu.org/licenses/gpl-2.0.html
#

# Referenced component versions
$ARDUINO_VERSION = "1.6.5"
$GCC_VERSION = "4.8.3-2014q1"
$BOSSAC_VERSION = "1.3a-arduino"

$DUET_BOARD_VERSION = "1.0.4"

# Workspace paths
$LIBRARY_PATH = "$(Get-Location)\Libraries"
$BUILD_PATH = "$(Get-Location)\Release\obj"
$OUTPUT_DIR = "$(Get-Location)\Release"

# Compiler options
$OPTIMIZATION = "-O3"


# ================================ Prerequisites ====================================

# Determine Arduino path
$ARDUINO_PATH = "$($env:APPDATA)\Arduino15"
if (-not (Test-Path $ARDUINO_PATH))
{
    Write-Error "Arduino directory not found! Are you using $ARDUINO_VERSION?"
    Exit
}

# Detect Duet board path
$DUET_BOARD_PATH = "$ARDUINO_PATH\packages\RepRap\hardware\sam\$DUET_BOARD_VERSION"
if (-not (Test-Path $DUET_BOARD_PATH))
{
    Write-Error "Duet board not found! Install it first via Arduino's Boards Manager."
    Exit
}

# Detect GCC path
$GCC_PATH = "$ARDUINO_PATH\packages\arduino\tools\arm-none-eabi-gcc\$GCC_VERSION"
if (-not (Test-Path $GCC_PATH))
{
    Write-Error "GCC toolchain not found! Check your installation."
    Exit
}

# Detect bossac path
$BOSSAC_PATH = "$ARDUINO_PATH\packages\arduino\tools\bossac\$BOSSAC_VERSION\bossac.exe"
if (-not (Test-Path $BOSSAC_PATH))
{
    Write-Warning "Bossac not found! Uploading compiled binaries will not work."
}


# ================================ GCC Options ======================================

$CROSS_COMPILE = "arm-none-eabi-"
$CC = "$GCC_PATH\bin\$($CROSS_COMPILE)gcc.exe"
$CXX = "$GCC_PATH\bin\$($CROSS_COMPILE)g++.exe"
$LD = "$GCC_PATH\bin\$($CROSS_COMPILE)gcc.exe"
$OBJCOPY = "$GCC_PATH\bin\$($CROSS_COMPILE)objcopy.exe"

$INCLUDES = @("$(Get-Location)\Libraries\Flash", "$(Get-Location)\Libraries\EMAC", "$(Get-Location)\Libraries\Lwip", "$(Get-Location)\Libraries\MCP4461", "$(Get-Location)\Libraries\SD_HSMCI", "$(Get-Location)\Libraries\SD_HSMCI\utility", "$(Get-Location)\Libraries\Wire")
$INCLUDES += @("$DUET_BOARD_PATH\cores\arduino", "$DUET_BOARD_PATH\variants\duet")
$INCLUDES += @("$DUET_BOARD_PATH\system\libsam", "$DUET_BOARD_PATH\system\libsam\include", "$DUET_BOARD_PATH\system\CMSIS\CMSIS\Include", "$DUET_BOARD_PATH\system\CMSIS\Device\ATMEL")

$CFLAGS = "-c -g $OPTIMIZATION -w -ffunction-sections -fdata-sections -nostdlib --param max-inline-insns-single=500 -Dprintf=iprintf -std=gnu99"
$CPPFLAGS = "-c -g $OPTIMIZATION -w -ffunction-sections -fdata-sections -nostdlib -fno-threadsafe-statics --param max-inline-insns-single=500 -fno-rtti -fno-exceptions -Dprintf=iprintf -std=gnu++11"

$DEVICE_FLAGS = "-mcpu=cortex-m3 -DF_CPU=84000000L -DARDUINO=" + $ARDUINO_VERSION.Replace(".", "") + " -DARDUINO_SAM_DUE -DARDUINO_ARCH_SAM -D__SAM3X8E__ -mthumb -DUSB_VID=0x2341 -DUSB_PID=0x003e -DUSBCON -DUSB_MANUFACTURER=\`"Unknown\`" -DUSB_PRODUCT=\`"Duet\`""

$CFLAGS += $INCLUDES | % { " -I$_" }
$CPPFLAGS += $INCLUDES | % { " -I$_" }

$LDFLAGS_A = "$OPTIMIZATION -Wl,--gc-sections -mcpu=cortex-m3 `"-T$DUET_BOARD_PATH\variants\duet\linker_scripts\gcc\flash.ld`" `"-Wl,-Map,$OUTPUT_DIR\RepRapFirmware.map`" `"-L$BUILD_PATH`" -mthumb -Wl,--cref -Wl,--check-sections -Wl,--gc-sections -Wl,--entry=Reset_Handler -Wl,--unresolved-symbols=report-all -Wl,--warn-common -Wl,--warn-section-align -Wl,--warn-unresolved-symbols -Wl,--start-group"
$LDFLAGS_B = "`"$DUET_BOARD_PATH\variants\duet\libsam_sam3x8e_gcc_rel.a`" -lm -lgcc -Wl,--end-group"

$SOURCEPATHS = @("$DUET_BOARD_PATH\cores\arduino", "$DUET_BOARD_PATH\cores\arduino\USB", "$DUET_BOARD_PATH\variants\duet")
$SOURCEPATHS += @("$(Get-Location)\Libraries\EMAC", "$(Get-Location)\Libraries\Flash", "$(Get-Location)\Libraries\Lwip\contrib\apps\netbios", "$(Get-Location)\Libraries\Lwip\lwip\src\api", "$(Get-Location)\Libraries\Lwip\lwip\src\core", "$(Get-Location)\Libraries\Lwip\lwip\src\core\ipv4", "$(Get-Location)\Libraries\Lwip\lwip\src\netif", "$(Get-Location)\Libraries\Lwip\lwip\src\sam\netif", "$(Get-Location)\Libraries\MCP4461", "$(Get-Location)\Libraries\SD_HSMCI\utility", "$(Get-Location)\Libraries\Wire")

$C_SOURCES = $SOURCEPATHS | % { $(Get-ChildItem "$_\*.c") | % { "$_" } }
$C_SOURCES += Get-ChildItem "$(Get-Location)\*.c" | % { "$_" }
$CPP_SOURCES = $SOURCEPATHS | % { $(Get-ChildItem "$_\*.cpp") | % { "$_" } }
$CPP_SOURCES += Get-ChildItem "$(Get-Location)\*.cpp" | % { "$_" }


# ================================= Target all ======================================

if ($args.Count -eq 0 -or $args[0] -eq "all")
{
    # Create output directories
    New-Item -Force -ItemType Directory $BUILD_PATH | Out-Null
    New-Item -Force -ItemType Directory $OUTPUT_DIR | Out-Null


    # NOTE: The author is well aware the following build jobs are running quite slowly, however Microsoft's awful implementations of -Wait and -NoNewWindow,
    # which are a requirement to see the compiler output in the same console window, make it very difficult to speed up this process. If the stdout and
    # stderr pipes are redirected as per several code examples on the WWW, GCC gets stuck in a deadlock and no output is reported at all. Bypassing this 
    # limitation via Invoke-Expression does not work either, because PowerShell's syntax parser doesn't handle embedded quotes properly and will report errors
    # that are actually not present in the code. Eventually this script should be replaced by a Makefile, however MingW's version of make does not work with
    # the current RepRapFirmware Makefile, whereas recent versions of GNU make for Linux and OS X are able to process the same file without any problems.
    

    # Build C sources
    foreach($C_SOURCE in $C_SOURCES)
    {
        $filename_out = $C_SOURCE.Replace((Get-Location).Path + "\", "")
        $filename_out = $filename_out.Replace($DUET_BOARD_PATH + "\", "")
	    Write-Host "  CC      $filename_out"

        $proc = Start-Process -Wait -NoNewWindow -PassThru -FilePath $CC -ArgumentList @($CFLAGS, $DEVICE_FLAGS, $C_SOURCE, "-o $BUILD_PATH\$((Get-Item $C_SOURCE).Name).o")
        if ($proc.ExitCode -ne 0)
        {
            Write-Warning "Compilation aborted, GCC exited with error code $($proc.ExitCode)"
            Exit
        }
    }

    # Build CPP sources
    foreach($CPP_SOURCE in $CPP_SOURCES)
    {
        $filename_out = $CPP_SOURCE.Replace((Get-Location).Path + "\", "")
        $filename_out = $filename_out.Replace($DUET_BOARD_PATH + "\", "")
	    Write-Host "  CC      $filename_out"

        $proc = Start-Process -Wait -NoNewWindow -PassThru -FilePath $CXX -ArgumentList @($CPPFLAGS, $DEVICE_FLAGS, $CPP_SOURCE, "-o $BUILD_PATH\$((Get-Item $CPP_SOURCE).Name).o")
        if ($proc.ExitCode -ne 0)
        {
            Write-Warning "Compilation aborted, GCC exited with error code $($proc.ExitCode)"
            Exit
        }
   }

    # Link objects
    Write-Host "  LD      RepRapFirmware.elf"
    $objs = Get-ChildItem "$BUILD_PATH\*.o" | % { "$_" }
    $proc = Start-Process -Wait -NoNewWindow -PassThru -FilePath $LD -ArgumentList @("$LDFLAGS_A", "$objs", "$LDFLAGS_B", "-o $OUTPUT_DIR\RepRapFirmware.elf")
    if ($proc.ExitCode -ne 0)
    {
        Write-Warning "Compilation aborted, GCC exited with error code $($proc.ExitCode)"
        Exit
    }

    # Copy .bin file
    Write-Host "  BIN     RepRapFirmware.bin"
    $proc = Start-Process -Wait -NoNewWindow -PassThru -FilePath $OBJCOPY -ArgumentList @("-O binary", "$OUTPUT_DIR\RepRapFirmware.elf", "$OUTPUT_DIR\RepRapFirmware.bin")
    if ($proc.ExitCode -ne 0)
    {
        Write-Warning "Compilation aborted, GCC exited with error code $($proc.ExitCode)"
        Exit
    }
}


# ================================= Target clean ====================================

elseif ($args[0] -eq "clean")
{
    Remove-Item -Force -Recurse $BUILD_PATH
    Remove-ITem -Force -Recurse $OUTPUT_DIR
    Write-Host "Build directories removed."
}


# ================================= Target upload ===================================

elseif ($args[0] -eq "upload")
{
    Start-Process -Wait -NoNewWindow -FilePath $BOSSAC_PATH -ArgumentList @("-e", "-w", "-v", "-b", "$OUTPUT_DIR\RepRapFirmware.bin")
}
