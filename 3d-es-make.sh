#!/bin/bash
#----------------------------------------------------------------------------
#
# Builds RepRapPro, chrishamm's and DC42's RepRapFirmware repositories from source
#
# - Downloads the selected firmware from github.com
# - Compiles the RepRapFirmware source code
#
# Update 24/05/2015:
# - Use current directory instead of FIRMWARE folder
# - Arduino 1.6.4 Duet device files are used for compilation
# - Arduino files are no longer downloaded. Use Arduino boards manager instead.
# - Added chrishamm's RepRapFirmware fork
#
# Update 30/05/2014:
#
# - It only compiles changed files for faster recompilation.
# - It shows less output to keep the compilation result clean.
# - Added command line parameter 'clean' to clean project.
# - Added command line parameter 'verbose' to show output.
# - Removed the (not so interesting) ELF file statistics.
# - Probably supports the MacOS platform for jstck...
#
# Update 31/05/2014:
#
# - Added command line parameter 'install' to upload the firmware.
# - Added download of 'lsusb' utility for MacOS environment.
#
# Update 01/06/2014:
#
# - Added a check to skip the ArduinoCorePatches folder during compilation,
#   these files are only for installing, compiling them twice gives errors
#   with RepRapFirmware-059e-dc42: "multiple definition of `emac_phy_read"
#
# 3D-ES & chrishamm
#----------------------------------------------------------------------------

# Firmware repository.
FIRMWARE=RepRapFirmware

# Project folders.
LIB=${PWD}/Libraries
RELEASE=${PWD}/Release
BUILD=${RELEASE}/obj

# Arduino version and path.
ARDUINO=arduino-1.6.4
ARDUINO_VERSION=164
ARDUINO_DIR=${HOME}/.arduino15

# Mac OS X uses a different Arduino path
if [[ $OSTYPE == darwin* ]]; then
    ARDUINO_DIR=$(HOME)/Library/Arduino15
fi

# Arduino Duet board
BOARD_VERSION=1.0.3
BOARD_DIR=${ARDUINO_DIR}/packages/RepRap/hardware/sam/${BOARD_VERSION}

# Location of the ARM compiler.
GCC_ARM_DIR=${ARDUINO_DIR}/packages/arduino/tools/arm-none-eabi-gcc/4.8.3-2014q1

# Cross compiler binary locations.
GCC=${GCC_ARM_DIR}/bin/arm-none-eabi-gcc
GPP=${GCC_ARM_DIR}/bin/arm-none-eabi-g++
COPY=${GCC_ARM_DIR}/bin/arm-none-eabi-objcopy

# Atmel SAM flash programmer utility.
BOSSAC=${ARDUINO_DIR}/packages/arduino/tools/bossac/1.3a-arduino/bossac

# Flash options.
BOSSAC_OPTIONS=(
    -e # Erase entire flash
    -w # Write file to flash
    -v # Verify write to flash
    -R # Reset CPU after flash
    -b # Boot from flash
    ${RELEASE}/${FIRMWARE}.bin
)

# Which lsusb?
if [ ! -f lsusb ]
    then LSUSB=lsusb
    else LSUSB=./lsusb
fi

# Check if there are script parameters:
#
# - Supports 'clean' to remove the build output.
# - Supports 'verbose' to display script output.
# - Supports 'install' to upload the firmware.
#
if [ $# -gt 0 ]
then
    if [ $1 == "verbose" ]
    then
        set -x
    fi

    if [ $1 == "clean" ]
    then
        echo "Cleaning ${BUILD} and ${RELEASE}"
        echo
        rm -rf ${BUILD} ${RELEASE}
        exit
    fi

    if [ $1 == "install" ]
    then
        # Do we have a firmware binary?
        if [ ! -f ${RELEASE}/${FIRMWARE}.bin ]
        then
            echo "The firmware binary has not been compiled yet."
            echo "Please run '$0' and then '$0 install'."
            echo
            exit
        fi
    
        # Search for the Arduino device.
        ${LSUSB} -d 2341:003e &> /dev/null
        
        # Arduino found?
        if [ $? != 1 ]
        then
            # Erase and reset to reconnect as Atmel.
            echo "Erase your board to start the upload."
            echo "Please press MCU_ERASE and then RESET."
            echo
        else
            # Arduino not found! And Atmel?
            ${LSUSB} -d 03eb:6124 &> /dev/null
            
            # Also missing?
            if [ $? == 1 ]
            then
                # Then the board is not connected..!
                echo "Your board could not be found."
                echo "Please reconnect and try again."
                echo
                exit
            fi
        fi        

        while true
        do
            # Search for the Atmel device.
            ${LSUSB} -d 03eb:6124 &> /dev/null
        
            # Found it?
            if [ $? != 1 ]
            then
                # Give the user some time to press the reset button,
                # otherwise the board won't reset after programming.
                sleep 3

                # We should be able to program the board.
                echo "Found an empty board, starting upload."
                break
            fi
        done
        
        while true
        do
            # Show why we are waiting, it can take 30 seconds!
            echo "Bossac is searching, this can take a while..."

            # Search for the device.
            ${BOSSAC} -i &> /dev/null

            # Keep trying until bossac finds the device.
            if [ $? == 1 ]; then sleep 1 && continue; fi

            # Write the program to flash!
	    ${BOSSAC} ${BOSSAC_OPTIONS[@]}
            exit
        done
    fi
fi

# Exit immediately if a command exits with a non-zero status.
# Above this point it is normal to have a non-zero exit status.
set -e

# Check if the RepRapFirmware folder is available:
#
# - Ask which firmware version to download.
# - Download the right branch from the repository.
#
if [ ! -d ${FIRMWARE} ]; then
    echo "Which firmware version do you want to use?"
    echo ""
    echo "[R] = RepRapPro original"
    echo "[C] = chrishamm's (aka zpl) firmware fork"
    echo "[D] = dc42's excellent fork"
    echo ""
    read -p "Please answer R, C or D, or press RETURN to use the current directory: " input
    case $input in
        [Rr]* )
            FW_REPO=git://github.com/RepRapPro
            FW_BRANCH=master
            ;;
        [Cc]* )
            FW_REPO=git://github.com/chrishamm
            FW_BRANCH=dev
            ;;
        [Dd]* )
            FW_REPO=git://github.com/dc42
            FW_BRANCH=dev
            ;;
        * )
            echo "Using current directory"
            ;;
    esac

    if [ ! -z ${FW_REPO} ]; then
        git clone -b ${FW_BRANCH} ${FW_REPO}/${FIRMWARE}
    fi
fi

# Check if lsusb utility is available under OS X.
#
if [ ! -e ./lsusb ]
then
    # Test if we run on MacOS.
    if [[ $OSTYPE == darwin* ]]; then
        # Download MacOS 'lsusb' utility script from the repository.
        wget https://raw.githubusercontent.com/jlhonora/lsusb/master/lsusb
        
        # Energize!
        chmod +x lsusb
    fi
fi

PLATFORM=(
    -mcpu=cortex-m3
    -DF_CPU=84000000L
    -DARDUINO=${ARDUINO_VERSION}
    -D__SAM3X8E__
    -mthumb
)

USB_OPT=(
    -DUSB_PID=0x003e # Define the USB product ID
    -DUSB_VID=0x2341 # Define the USB vendor ID
    -DUSBCON # TODO: Why?
)

GCC_OPT=(
    -c # Compile the source files, but do not link
    -g # Produce debugging info in OS native format
    -O3 # Optimize for speed, makes binary larger
    -w # Inhibit all warning messages
    -ffunction-sections # Place each function into its own section
    -fdata-sections # Place each data item into its own section
    -nostdlib # Do not use standard system startup files or libraries
    -std=gnu99 # But use GNU99 C compiler features
    --param max-inline-insns-single=500 # Max. instructions to inline
    -Dprintf=iprintf # Prevent bloat by using integer printf function
    -MMD # Create dependency output file but only of user header files
    -MP # Add phony target for each dependency other than the main file
)

GPP_OPT=(
    -c # Compile the source files, but do not link
    -g # Produce debugging info in OS native format
    -O3 # Optimize for speed, makes binary larger
    -w # Inhibit all warning messages
    -ffunction-sections # Place each function into its own section
    -fdata-sections # Place each data item into its own section
    -nostdlib # Do not use standard system startup files or libraries
    -std=gnu++11
    --param max-inline-insns-single=500 # Max. instructions to inline
    -Dprintf=iprintf # Prevent bloat by using integer printf function
    -MMD # Create dependency output file but only of user header files
    -MP # Add phony target for each dependency other than the main file
    -fno-rtti # Don't produce Run-Time Type Information structures
    -fno-exceptions # Disable exception handling to prevent overhead
    -x c++ # Specify the language of the input files
)

COM_OPT=(
    -O3 # Use highest optimization
    -mcpu=cortex-m3 # Specify target processor
    -T${BOARD_DIR}/variants/duet/linker_scripts/gcc/flash.ld
    -L${BUILD} # Specify library path
    -lm # Link with the math library
    -lgcc # Link with the libgcc library
    -mthumb # Use the ARM Thumb instruction set
    -Wl,-Map,${BUILD}/${FIRMWARE}.map # Write a map file
    -Wl,--cref # Output cross reference table
    -Wl,--check-sections # Check section addresses for overlaps
    -Wl,--gc-sections # Remove unused sections
    -Wl,--entry=Reset_Handler # Set start address
    -Wl,--warn-unresolved-symbols # Report warning messages rather than errors
    -Wl,--unresolved-symbols=report-all # Report missing symbols
    -Wl,--warn-common #  Warn about duplicate common symbols
    -Wl,--warn-section-align # Warn if start of section changes due to alignment
    -Wl,--start-group # Start of objects group
    ${BUILD}/*.o # Location of compiled objects
    ${BOARD_DIR}/variants/duet/libsam_sam3x8e_gcc_rel.a
    -Wl,--end-group # End of objects group
)

# Folders to include.
INC=(
    -I"${LIB}/Flash"
    -I"${LIB}/EMAC"
    -I"${LIB}/Lwip"
    -I"${LIB}/MCP4461"
    -I"${LIB}/SD_HSMCI"
    -I"${LIB}/SD_HSMCI/utility"
    -I"${LIB}/Wire"
    -I"${BOARD_DIR}/cores/arduino"
    -I"${BOARD_DIR}/variants/duet"
    -I"${BOARD_DIR}/system/libsam"
    -I"${BOARD_DIR}/system/libsam/include"
    -I"${BOARD_DIR}/system/CMSIS/Device/ATMEL"
    -I"${BOARD_DIR}/system/CMSIS/CMSIS/Include"
)

# Create output folders.
mkdir -p ${BUILD} ${RELEASE}

# Locate and compile all the .c files that need to be compiled.
for file in $(find ${PWD} ${BOARD_DIR}/cores -type f -name "*.c")
do
    # Skip Hardware directory.
    if [[ $file == ${PWD}/Hardware/* ]]; then continue; fi
    
    # Intermediate build output.
    D=${BUILD}/$(basename $file).d
    O=${BUILD}/$(basename $file).o

    # Skip compile if the object is up-to-date.
    if [ ${O} -nt ${file} ]; then continue; fi

    # Show some progress.
    echo "Compiling ${file}"

    # The C source file is newer than the object output file, we need to compile it.
    ${GCC} ${GCC_OPT[@]} ${PLATFORM[@]} ${USB_OPT[@]} ${INC[@]} ${file} -MF${D} -MT${D} -o${O}
done

# Locate and compile all the .cpp files that need to be compiled.
for file in $(find ${PWD} ${BOARD_DIR} -type f -name "*.cpp")
do
    # Skip Hardware directory.
    if [[ $file == ${PWD}/Hardware/* ]]; then continue; fi
    
    # Intermediate build output.
    D=${BUILD}/$(basename $file).d
    O=${BUILD}/$(basename $file).o

    # Skip compile if the object is up-to-date.
    if [ ${O} -nt ${file} ]; then continue; fi

    # Show some progress.
    echo "Compiling ${file}"

    # The CPP source file is newer than the object output file, we need to compile it.
    ${GPP} ${GPP_OPT[@]} ${PLATFORM[@]} ${USB_OPT[@]} ${INC[@]} ${file} -MF${D} -MT${D} -o${O}
done

echo "Converting object files into ELF file."
${GPP} ${COM_OPT[@]} -o ${BUILD}/${FIRMWARE}.elf

echo "Converting ELF file into firmware binary."
${COPY} -O binary ${BUILD}/${FIRMWARE}.elf ${RELEASE}/${FIRMWARE}.bin

echo
echo "Created ${RELEASE}/${FIRMWARE}.bin"
echo "You can run '$0 install' to upload the file."
echo

