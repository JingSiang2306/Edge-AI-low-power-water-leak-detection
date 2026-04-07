################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/Components/aps6408/aps6408.c 

OBJS += \
./Drivers/Components/aps6408/aps6408.o 

C_DEPS += \
./Drivers/Components/aps6408/aps6408.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/Components/aps6408/%.o Drivers/Components/aps6408/%.su Drivers/Components/aps6408/%.cyclo: ../Drivers/Components/aps6408/%.c Drivers/Components/aps6408/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m33 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32U585xx -c -I../Core/Inc -I../Drivers/STM32U5xx_HAL_Driver/Inc -I../Drivers/STM32U5xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32U5xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-Components-2f-aps6408

clean-Drivers-2f-Components-2f-aps6408:
	-$(RM) ./Drivers/Components/aps6408/aps6408.cyclo ./Drivers/Components/aps6408/aps6408.d ./Drivers/Components/aps6408/aps6408.o ./Drivers/Components/aps6408/aps6408.su

.PHONY: clean-Drivers-2f-Components-2f-aps6408

