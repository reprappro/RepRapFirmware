################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
/usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/Ethernet/Dhcp.cpp \
/usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/Ethernet/Dns.cpp \
/usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/Ethernet/Ethernet.cpp \
/usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/Ethernet/EthernetClient.cpp \
/usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/Ethernet/EthernetServer.cpp \
/usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/Ethernet/EthernetUdp.cpp 

CPP_DEPS += \
./Libraries/Ethernet/Dhcp.cpp.d \
./Libraries/Ethernet/Dns.cpp.d \
./Libraries/Ethernet/Ethernet.cpp.d \
./Libraries/Ethernet/EthernetClient.cpp.d \
./Libraries/Ethernet/EthernetServer.cpp.d \
./Libraries/Ethernet/EthernetUdp.cpp.d 

LINK_OBJ += \
./Libraries/Ethernet/Dhcp.cpp.o \
./Libraries/Ethernet/Dns.cpp.o \
./Libraries/Ethernet/Ethernet.cpp.o \
./Libraries/Ethernet/EthernetClient.cpp.o \
./Libraries/Ethernet/EthernetServer.cpp.o \
./Libraries/Ethernet/EthernetUdp.cpp.o 


# Each subdirectory must supply rules for building sources it contributes
Libraries/Ethernet/Dhcp.cpp.o: /usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/Ethernet/Dhcp.cpp
	@echo 'Building file: $<'
	@echo 'Starting C++ compile'
	-I"/usr/local/arduino-1.5.2/libraries/SD" -I"/usr/local/arduino-1.5.2/libraries/SD/utility" -I"/usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/Ethernet" -I"/usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/Ethernet/utility" -I"/usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/SPI" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -x c++ "$<"  "$@"
	@echo 'Finished building: $<'
	@echo ' '

Libraries/Ethernet/Dns.cpp.o: /usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/Ethernet/Dns.cpp
	@echo 'Building file: $<'
	@echo 'Starting C++ compile'
	-I"/usr/local/arduino-1.5.2/libraries/SD" -I"/usr/local/arduino-1.5.2/libraries/SD/utility" -I"/usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/Ethernet" -I"/usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/Ethernet/utility" -I"/usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/SPI" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -x c++ "$<"  "$@"
	@echo 'Finished building: $<'
	@echo ' '

Libraries/Ethernet/Ethernet.cpp.o: /usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/Ethernet/Ethernet.cpp
	@echo 'Building file: $<'
	@echo 'Starting C++ compile'
	-I"/usr/local/arduino-1.5.2/libraries/SD" -I"/usr/local/arduino-1.5.2/libraries/SD/utility" -I"/usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/Ethernet" -I"/usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/Ethernet/utility" -I"/usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/SPI" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -x c++ "$<"  "$@"
	@echo 'Finished building: $<'
	@echo ' '

Libraries/Ethernet/EthernetClient.cpp.o: /usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/Ethernet/EthernetClient.cpp
	@echo 'Building file: $<'
	@echo 'Starting C++ compile'
	-I"/usr/local/arduino-1.5.2/libraries/SD" -I"/usr/local/arduino-1.5.2/libraries/SD/utility" -I"/usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/Ethernet" -I"/usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/Ethernet/utility" -I"/usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/SPI" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -x c++ "$<"  "$@"
	@echo 'Finished building: $<'
	@echo ' '

Libraries/Ethernet/EthernetServer.cpp.o: /usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/Ethernet/EthernetServer.cpp
	@echo 'Building file: $<'
	@echo 'Starting C++ compile'
	-I"/usr/local/arduino-1.5.2/libraries/SD" -I"/usr/local/arduino-1.5.2/libraries/SD/utility" -I"/usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/Ethernet" -I"/usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/Ethernet/utility" -I"/usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/SPI" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -x c++ "$<"  "$@"
	@echo 'Finished building: $<'
	@echo ' '

Libraries/Ethernet/EthernetUdp.cpp.o: /usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/Ethernet/EthernetUdp.cpp
	@echo 'Building file: $<'
	@echo 'Starting C++ compile'
	-I"/usr/local/arduino-1.5.2/libraries/SD" -I"/usr/local/arduino-1.5.2/libraries/SD/utility" -I"/usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/Ethernet" -I"/usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/Ethernet/utility" -I"/usr/local/arduino-1.5.2/hardware/arduino/sam/libraries/SPI" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -x c++ "$<"  "$@"
	@echo 'Finished building: $<'
	@echo ' '


