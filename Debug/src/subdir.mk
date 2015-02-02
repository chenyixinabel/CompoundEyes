################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/emit_ques_aux_funcs.cpp \
../src/hists_comp.cpp \
../src/main_flow.cpp \
../src/pre_proc_aux_funcs.cpp 

OBJS += \
./src/emit_ques_aux_funcs.o \
./src/hists_comp.o \
./src/main_flow.o \
./src/pre_proc_aux_funcs.o 

CPP_DEPS += \
./src/emit_ques_aux_funcs.d \
./src/hists_comp.d \
./src/main_flow.d \
./src/pre_proc_aux_funcs.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -I/usr/local/include/opencv -O0 -g3 -Wall -c -fmessage-length=0 -fopenmp -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


