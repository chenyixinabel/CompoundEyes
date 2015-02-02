################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/Nest/cuckoo.cpp \
../src/Nest/lsh.cpp \
../src/Nest/nest.cpp \
../src/Nest/nest_test.cpp 

OBJS += \
./src/Nest/cuckoo.o \
./src/Nest/lsh.o \
./src/Nest/nest.o \
./src/Nest/nest_test.o 

CPP_DEPS += \
./src/Nest/cuckoo.d \
./src/Nest/lsh.d \
./src/Nest/nest.d \
./src/Nest/nest_test.d 


# Each subdirectory must supply rules for building sources it contributes
src/Nest/%.o: ../src/Nest/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -I/usr/local/include/opencv -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


