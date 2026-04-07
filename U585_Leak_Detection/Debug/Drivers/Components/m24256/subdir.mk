################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/Components/m24256/m24256.c 

OBJS += \
./Drivers/Components/m24256/m24256.o 

C_DEPS += \
./Drivers/Components/m24256/m24256.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/Components/m24256/%.o Drivers/Components/m24256/%.su Drivers/Components/m24256/%.cyclo: ../Drivers/Components/m24256/%.c Drivers/Components/m24256/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m33 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32U585xx -c -I../Core/Inc -I../Drivers/STM32U5xx_HAL_Driver/Inc -I../Drivers/STM32U5xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32U5xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-Components-2f-m24256

clean-Drivers-2f-Components-2f-m24256:
	-$(RM) ./Drivers/Components/m24256/m24256.cyclo ./Drivers/Components/m24256/m24256.d ./Drivers/Components/m24256/m24256.o ./Drivers/Components/m24256/m24256.su

.PHONY: clean-Drivers-2f-Components-2f-m24256

