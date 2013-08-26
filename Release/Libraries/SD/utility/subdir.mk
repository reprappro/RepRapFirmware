################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
/usr/local/arduino-1.5.2/libraries/SD/utility/Sd2Card.cpp \
/usr/local/arduino-1.5.2/libraries/SD/utility/SdFile.cpp \
/usr/local/arduino-1.5.2/libraries/SD/utility/SdVolume.cpp 

CPP_DEPS += \
./Libraries/SD/utility/Sd2Card.cpp.d \
./Libraries/SD/utility/SdFile.cpp.d \
./Libraries/SD/utility/SdVolume.cpp.d 

LINK_OBJ += \
./Libraries/SD/utility/Sd2Card.cpp.o \
./Libraries/SD/utility/SdFile.cpp.o \
./Libraries/SD/utility/SdVolume.cpp.o 


# Each subdirectory must supply rules for building sources it contributes
Libraries/SD/utility/Sd2Card.cpp.o: /usr/local/arduino-1.5.2/libraries/SD/utility/Sd2Card.cpp
	@echo 'Building file: $<'
	@echo 'Starting C++ compile'
	-I"/usr/local/arduino-1.5.2/libraries/SD" -I"/usr/local/arduino-1.5.2/libraries/SD/utility" -I"/usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/Ethernet" -I"/usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/Ethernet/utility" -I"/usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/SPI" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -x c++ "$<"  "$@"
	@echo 'Finished building: $<'
	@echo ' '

Libraries/SD/utility/SdFile.cpp.o: /usr/local/arduino-1.5.2/libraries/SD/utility/SdFile.cpp
	@echo 'Building file: $<'
	@echo 'Starting C++ compile'
	-I"/usr/local/arduino-1.5.2/libraries/SD" -I"/usr/local/arduino-1.5.2/libraries/SD/utility" -I"/usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/Ethernet" -I"/usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/Ethernet/utility" -I"/usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/SPI" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -x c++ "$<"  "$@"
	@echo 'Finished building: $<'
	@echo ' '

Libraries/SD/utility/SdVolume.cpp.o: /usr/local/arduino-1.5.2/libraries/SD/utility/SdVolume.cpp
	@echo 'Building file: $<'
	@echo 'Starting C++ compile'
	-I"/usr/local/arduino-1.5.2/libraries/SD" -I"/usr/local/arduino-1.5.2/libraries/SD/utility" -I"/usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/Ethernet" -I"/usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/Ethernet/utility" -I"/usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/SPI" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -x c++ "$<"  "$@"
	@echo 'Finished building: $<'
	@echo ' '


