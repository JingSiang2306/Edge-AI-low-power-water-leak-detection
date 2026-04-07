################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/Components/tcpp0203/tcpp0203.c \
../Drivers/Components/tcpp0203/tcpp0203_reg.c 

OBJS += \
./Drivers/Components/tcpp0203/tcpp0203.o \
./Drivers/Components/tcpp0203/tcpp0203_reg.o 

C_DEPS += \
./Drivers/Components/tcpp0203/tcpp0203.d \
./Drivers/Components/tcpp0203/tcpp0203_reg.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/Components/tcpp0203/%.o Drivers/Components/tcpp0203/%.su Drivers/Components/tcpp0203/%.cyclo: ../Drivers/Components/tcpp0203/%.c Drivers/Components/tcpp0203/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m33 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32U585xx -c -I../Core/Inc -I../Drivers/STM32U5xx_HAL_Driver/Inc -I../Drivers/STM32U5xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32U5xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-Components-2f-tcpp0203

clean-Drivers-2f-Components-2f-tcpp0203:
	-$(RM) ./Drivers/Components/tcpp0203/tcpp0203.cyclo ./Drivers/Components/tcpp0203/tcpp0203.d ./Drivers/Components/tcpp0203/tcpp0203.o ./Drivers/Components/tcpp0203/tcpp0203.su ./Drivers/Components/tcpp0203/tcpp0203_reg.cyclo ./Drivers/Components/tcpp0203/tcpp0203_reg.d ./Drivers/Components/tcpp0203/tcpp0203_reg.o ./Drivers/Components/tcpp0203/tcpp0203_reg.su

.PHONY: clean-Drivers-2f-Components-2f-tcpp0203

