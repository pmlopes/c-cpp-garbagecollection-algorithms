################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
O_SRCS += \
../gc.o \
../main.o 

CPP_SRCS += \
../gc.cpp \
../main.cpp \
../main_smartptr.cpp 

OBJS += \
./gc.o \
./main.o \
./main_smartptr.o 

CPP_DEPS += \
./gc.d \
./main.d \
./main_smartptr.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -I"../include" -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


