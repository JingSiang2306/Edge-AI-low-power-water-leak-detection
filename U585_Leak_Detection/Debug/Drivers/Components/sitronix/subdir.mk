################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/Components/sitronix/sitronix.c \
../Drivers/Components/sitronix/sitronix_reg.c 

OBJS += \
./Drivers/Components/sitronix/sitronix.o \
./Drivers/Components/sitronix/sitronix_reg.o 

C_DEPS += \
./Drivers/Components/sitronix/sitronix.d \
./Drivers/Components/sitronix/sitronix_reg.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/Components/sitronix/%.o Drivers/Components/sitronix/%.su Drivers/Components/sitronix/%.cyclo: ../Drivers/Components/sitronix/%.c Drivers/Components/sitronix/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m33 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32U585xx -c -I../Core/Inc -I../Drivers/STM32U5xx_HAL_Driver/Inc -I../Drivers/STM32U5xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32U5xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-Components-2f-sitronix

clean-Drivers-2f-Components-2f-sitronix:
	-$(RM) ./Drivers/Components/sitronix/sitronix.cyclo ./Drivers/Components/sitronix/sitronix.d ./Drivers/Components/sitronix/sitronix.o ./Drivers/Components/sitronix/sitronix.su ./Drivers/Components/sitronix/sitronix_reg.cyclo ./Drivers/Components/sitronix/sitronix_reg.d ./Drivers/Components/sitronix/sitronix_reg.o ./Drivers/Components/sitronix/sitronix_reg.su

.PHONY: clean-Drivers-2f-Components-2f-sitronix

