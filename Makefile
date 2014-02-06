#!/usr/bin/make
# Makefile for RepRapProFirmware.
#
# Peter Strapp 06/02/2014
#
# LICENSE: GPLv2
#
# Based on Makefile by Paul Dreik 20130503 http://www.pauldreik.se/ - https://github.com/pauldreik/arduino-due-makefile


######## Settings ##########
ADIR:=$(HOME)/arduino-1.5.5/hardware
RRPLIBS:=$(HOME)/reprappro-libraries
PORT:=ttyACM0
VERIFY:=-v
############################


CXX:=$(ADIR)/tools/g++_arm_none_eabi/bin/arm-none-eabi-g++
CC:=$(ADIR)/tools/g++_arm_none_eabi/bin/arm-none-eabi-gcc
C:=$(CC)
SAM:=arduino/sam
CMSIS:=arduino/sam/system/CMSIS
LIBSAM:=arduino/sam/system/libsam
TMPDIR:=$(PWD)/build
AR:=$(ADIR)/tools/g++_arm_none_eabi/bin/arm-none-eabi-ar

DEFINES:=-Dprintf=iprintf -DF_CPU=84000000L -DARDUINO=152 -D__SAM3X8E__ -DUSB_PID=0x003e -DUSB_VID=0x2341 -DUSBCON

INCLUDES:=-I$(ADIR)/$(LIBSAM) -I$(ADIR)/$(LIBSAM)/include -I$(ADIR)/$(CMSIS)/CMSIS/Include/ -I$(ADIR)/$(CMSIS)/Device/ATMEL/ -I$(ADIR)/$(SAM)/cores/arduino -I$(ADIR)/$(SAM)/variants/arduino_due_x -I$(ADIR)/$(SAM)/libraries/Wire
INCLUDES+= -I. -I$(RRPLIBS)/SamNonDuePin -I$(RRPLIBS)/EMAC -I$(RRPLIBS)/Lwip -I$(RRPLIBS)/MCP4461 -I$(RRPLIBS)/SD_HSMCI -Inetwork

COMMON_FLAGS:=-g -Os -w -ffunction-sections -fdata-sections -nostdlib --param max-inline-insns-single=500 -mcpu=cortex-m3  -mthumb

CFLAGS:=$(COMMON_FLAGS)
CXXFLAGS:=$(COMMON_FLAGS) -fno-rtti -fno-exceptions 

PROJNAME:=$(shell basename *.ino .ino)

NEWMAINFILE:=$(TMPDIR)/$(PROJNAME).ino.cpp

SRCFILESC:=$(NEWMAINFILE) $(shell ls *.cpp | grep -v RepRapFirmware.cpp 2>/dev/null)
SRCFILESCXX:=$(shell ls network/*.c 2>/dev/null)

LIBSC:= $(shell ls $(RRPLIBS)/SD_HSMCI/utility/*.c 2>/dev/null) \
	$(shell ls $(RRPLIBS)/EMAC/*.c 2>/dev/null) \
	$(shell ls $(RRPLIBS)/Lwip/lwip/src/api/*.c 2>/dev/null) \
	$(shell ls $(RRPLIBS)/Lwip/lwip/src/core/*.c 2>/dev/null) \
	$(shell ls $(RRPLIBS)/Lwip/lwip/src/core/ipv4/*.c 2>/dev/null) \
	$(shell ls $(RRPLIBS)/Lwip/lwip/src/netif/*.c 2>/dev/null) \
	$(shell ls $(RRPLIBS)/Lwip/lwip/src/sam/netif/*.c 2>/dev/null) 
LIBSCXX:=$(shell ls $(RRPLIBS)/SamNonDuePin/*.cpp 2>/dev/null) \
	$(shell ls $(RRPLIBS)/MCP4461/*.cpp 2>/dev/null) \
	$(ADIR)/$(SAM)/libraries/Wire/Wire.cpp

OBJFILES:=$(addsuffix .o,$(addprefix $(TMPDIR)/,$(notdir $(SRCFILESC))))
OBJFILES+=$(addsuffix .o,$(addprefix $(TMPDIR)/,$(notdir $(SRCFILESCXX))))
OBJFILES+=$(addsuffix .o,$(addprefix $(TMPDIR)/,$(notdir $(LIBSC))))
OBJFILES+=$(addsuffix .o,$(addprefix $(TMPDIR)/,$(notdir $(LIBSCXX))))

CORESRCXX:=$(shell ls ${ADIR}/${SAM}/cores/arduino/*.cpp ${ADIR}/${SAM}/cores/arduino/USB/*.cpp ${ADIR}/${SAM}/variants/arduino_due_x/variant.cpp)
CORESRC:=$(shell ls ${ADIR}/${SAM}/cores/arduino/*.c)

COREOBJSXX:=$(addprefix $(TMPDIR)/core/,$(notdir $(CORESRCXX)) )
COREOBJSXX:=$(addsuffix .o,$(COREOBJSXX))
COREOBJS:=$(addprefix $(TMPDIR)/core/,$(notdir $(CORESRC)) )
COREOBJS:=$(addsuffix .o,$(COREOBJS))

compile: $(TMPDIR)/$(PROJNAME).bin

define OBJ_template
$(2): $(1)
	$(C$(3)) -MD -c $(C$(3)FLAGS) $(DEFINES) $(INCLUDES) $(1) -o $(2)
endef

# Build Arduino core libraries
$(foreach src,$(CORESRCXX), $(eval $(call OBJ_template,$(src),$(addsuffix .o,$(addprefix $(TMPDIR)/core/,$(notdir $(src)))),XX) ) )
$(foreach src,$(CORESRC), $(eval $(call OBJ_template,$(src),$(addsuffix .o,$(addprefix $(TMPDIR)/core/,$(notdir $(src)))),) ) )

# Build RepRapProFirmware libraries
$(foreach src,$(LIBSC), $(eval $(call OBJ_template,$(src),$(addsuffix .o,$(addprefix $(TMPDIR)/,$(notdir $(src)))),) ) )
$(foreach src,$(LIBSCXX), $(eval $(call OBJ_template,$(src),$(addsuffix .o,$(addprefix $(TMPDIR)/,$(notdir $(src)))),XX) ) )

# Build firmware
$(foreach src,$(SRCFILESCXX), $(eval $(call OBJ_template,$(src),$(addsuffix .o,$(addprefix $(TMPDIR)/,$(notdir $(src)))),) ) )
$(foreach src,$(SRCFILESC), $(eval $(call OBJ_template,$(src),$(addsuffix .o,$(addprefix $(TMPDIR)/,$(notdir $(src)))),XX) ) )

clean:
	test ! -d $(TMPDIR) || rm -rf $(TMPDIR)

.PHONY: upload default

$(TMPDIR):
	mkdir -p $(TMPDIR)

$(TMPDIR)/core:
	mkdir -p $(TMPDIR)/core

# Create the modified cpp file
$(NEWMAINFILE): $(PROJNAME).ino
	cat $(ADIR)/arduino/sam/cores/arduino/main.cpp > $(NEWMAINFILE)
	cat $(PROJNAME).ino >> $(NEWMAINFILE)
	echo 'extern "C" void __cxa_pure_virtual() {while (true);}' >> $(NEWMAINFILE)

-include $(OBJFILES:.o=.d)

$(TMPDIR)/core.a: $(TMPDIR)/core $(COREOBJS) $(COREOBJSXX)
	$(AR) rcs $(TMPDIR)/core.a $(TMPDIR)/core/wiring_shift.c.o 
	$(AR) rcs $(TMPDIR)/core.a $(TMPDIR)/core/wiring_analog.c.o 
	$(AR) rcs $(TMPDIR)/core.a $(TMPDIR)/core/itoa.c.o 
	$(AR) rcs $(TMPDIR)/core.a $(TMPDIR)/core/cortex_handlers.c.o 
	$(AR) rcs $(TMPDIR)/core.a $(TMPDIR)/core/hooks.c.o 
	$(AR) rcs $(TMPDIR)/core.a $(TMPDIR)/core/wiring.c.o 
	$(AR) rcs $(TMPDIR)/core.a $(TMPDIR)/core/WInterrupts.c.o 
	$(AR) rcs $(TMPDIR)/core.a $(TMPDIR)/core/syscalls_sam3.c.o 
	$(AR) rcs $(TMPDIR)/core.a $(TMPDIR)/core/iar_calls_sam3.c.o 
	$(AR) rcs $(TMPDIR)/core.a $(TMPDIR)/core/wiring_digital.c.o 
	$(AR) rcs $(TMPDIR)/core.a $(TMPDIR)/core/Print.cpp.o 
	$(AR) rcs $(TMPDIR)/core.a $(TMPDIR)/core/USARTClass.cpp.o 
	$(AR) rcs $(TMPDIR)/core.a $(TMPDIR)/core/WString.cpp.o 
	$(AR) rcs $(TMPDIR)/core.a $(TMPDIR)/core/USBCore.cpp.o 
	$(AR) rcs $(TMPDIR)/core.a $(TMPDIR)/core/CDC.cpp.o 
	$(AR) rcs $(TMPDIR)/core.a $(TMPDIR)/core/HID.cpp.o 
	$(AR) rcs $(TMPDIR)/core.a $(TMPDIR)/core/wiring_pulse.cpp.o 
	$(AR) rcs $(TMPDIR)/core.a $(TMPDIR)/core/UARTClass.cpp.o 
	$(AR) rcs $(TMPDIR)/core.a $(TMPDIR)/core/main.cpp.o 
	$(AR) rcs $(TMPDIR)/core.a $(TMPDIR)/core/cxxabi-compat.cpp.o 
	$(AR) rcs $(TMPDIR)/core.a $(TMPDIR)/core/Stream.cpp.o 
	$(AR) rcs $(TMPDIR)/core.a $(TMPDIR)/core/RingBuffer.cpp.o 
	$(AR) rcs $(TMPDIR)/core.a $(TMPDIR)/core/IPAddress.cpp.o 
	$(AR) rcs $(TMPDIR)/core.a $(TMPDIR)/core/Reset.cpp.o 
	$(AR) rcs $(TMPDIR)/core.a $(TMPDIR)/core/WMath.cpp.o 
	$(AR) rcs $(TMPDIR)/core.a $(TMPDIR)/core/variant.cpp.o

# Generate elf file
$(TMPDIR)/$(PROJNAME).elf: $(TMPDIR)/core.a $(TMPDIR)/core/syscalls_sam3.c.o $(OBJFILES) 
	$(CXX) -Os -Wl,--gc-sections -mcpu=cortex-m3 -T$(ADIR)/$(SAM)/variants/arduino_due_x/linker_scripts/gcc/flash.ld -Wl,-Map,$(NEWMAINFILE).map -o $@ -L$(TMPDIR) -lm -lgcc -mthumb -Wl,--cref -Wl,--check-sections -Wl,--gc-sections -Wl,--entry=Reset_Handler -Wl,--unresolved-symbols=report-all -Wl,--warn-common -Wl,--warn-section-align -Wl,--warn-unresolved-symbols -Wl,--start-group $(TMPDIR)/core/syscalls_sam3.c.o $(OBJFILES) $(ADIR)/$(SAM)/variants/arduino_due_x/libsam_sam3x8e_gcc_rel.a $(TMPDIR)/core.a -Wl,--end-group

# Extract firmware from elf file
$(TMPDIR)/$(PROJNAME).bin: $(TMPDIR)/$(PROJNAME).elf 
	$(ADIR)/tools/g++_arm_none_eabi/bin/arm-none-eabi-objcopy -O binary $< $@

# Upload to the Duet by first resetting it (stty) and the running bossac
upload: $(TMPDIR)/$(PROJNAME).bin
	stty -F /dev/$(PORT) cs8 1200 hupcl
	$(ADIR)/tools/bossac --port=$(PORT) -U true -e -w $(VERIFY) -b $(TMPDIR)/$(PROJNAME).bin -R

# Monitor the serial port
monitor:
	screen $(PORT) 115200
