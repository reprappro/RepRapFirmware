################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
/usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/Ethernet/utility/socket.cpp \
/usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/Ethernet/utility/w5100.cpp 

CPP_DEPS += \
./Libraries/Ethernet/utility/socket.cpp.d \
./Libraries/Ethernet/utility/w5100.cpp.d 

LINK_OBJ += \
./Libraries/Ethernet/utility/socket.cpp.o \
./Libraries/Ethernet/utility/w5100.cpp.o 


# Each subdirectory must supply rules for building sources it contributes
Libraries/Ethernet/utility/socket.cpp.o: /usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/Ethernet/utility/socket.cpp
	@echo 'Building file: $<'
	@echo 'Starting C++ compile'
	-I"/usr/local/arduino-1.5.2/libraries/SD" -I"/usr/local/arduino-1.5.2/libraries/SD/utility" -I"/usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/Ethernet" -I"/usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/Ethernet/utility" -I"/usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/SPI" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -x c++ "$<"  "$@"
	@echo 'Finished building: $<'
	@echo ' '

Libraries/Ethernet/utility/w5100.cpp.o: /usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/Ethernet/utility/w5100.cpp
	@echo 'Building file: $<'
	@echo 'Starting C++ compile'
	-I"/usr/local/arduino-1.5.2/libraries/SD" -I"/usr/local/arduino-1.5.2/libraries/SD/utility" -I"/usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/Ethernet" -I"/usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/Ethernet/utility" -I"/usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/SPI" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -x c++ "$<"  "$@"
	@echo 'Finished building: $<'
	@echo ' '


