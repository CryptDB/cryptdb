################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CC_SRCS += \
../analyze.cc \
../embedmysql.cc \
../load-schema.cc \
../nz-foo.cc \
../print-back.cc \
../proxy.cc 

OBJS += \
./analyze.o \
./embedmysql.o \
./load-schema.o \
./nz-foo.o \
./print-back.o \
./proxy.o 

CC_DEPS += \
./analyze.d \
./embedmysql.d \
./load-schema.d \
./nz-foo.d \
./print-back.d \
./proxy.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.cc
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


