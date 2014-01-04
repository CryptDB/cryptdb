################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CC_SRCS += \
../attic/parsetest.cc \
../attic/xftest.cc 

OBJS += \
./attic/parsetest.o \
./attic/xftest.o 

CC_DEPS += \
./attic/parsetest.d \
./attic/xftest.d 


# Each subdirectory must supply rules for building sources it contributes
attic/%.o: ../attic/%.cc
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


