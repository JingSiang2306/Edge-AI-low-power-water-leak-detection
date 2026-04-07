################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/Components/stm32wb_at/stm32wb_at.c \
../Drivers/Components/stm32wb_at/stm32wb_at_ble.c \
../Drivers/Components/stm32wb_at/stm32wb_at_client.c \
../Drivers/Components/stm32wb_at/stm32wb_at_ll.c 

OBJS += \
./Drivers/Components/stm32wb_at/stm32wb_at.o \
./Drivers/Components/stm32wb_at/stm32wb_at_ble.o \
./Drivers/Components/stm32wb_at/stm32wb_at_client.o \
./Drivers/Components/stm32wb_at/stm32wb_at_ll.o 

C_DEPS += \
./Drivers/Components/stm32wb_at/stm32wb_at.d \
./Drivers/Components/stm32wb_at/stm32wb_at_ble.d \
./Drivers/Components/stm32wb_at/stm32wb_at_client.d \
./Drivers/Components/stm32wb_at/stm32wb_at_ll.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/Components/stm32wb_at/%.o Drivers/Components/stm32wb_at/%.su Drivers/Components/stm32wb_at/%.cyclo: ../Drivers/Components/stm32wb_at/%.c Drivers/Components/stm32wb_at/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m33 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32U585xx -c -I../Core/Inc -I../Drivers/STM32U5xx_HAL_Driver/Inc -I../Drivers/STM32U5xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32U5xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-Components-2f-stm32wb_at

clean-Drivers-2f-Components-2f-stm32wb_at:
	-$(RM) ./Drivers/Components/stm32wb_at/stm32wb_at.cyclo ./Drivers/Components/stm32wb_at/stm32wb_at.d ./Drivers/Components/stm32wb_at/stm32wb_at.o ./Drivers/Components/stm32wb_at/stm32wb_at.su ./Drivers/Components/stm32wb_at/stm32wb_at_ble.cyclo ./Drivers/Components/stm32wb_at/stm32wb_at_ble.d ./Drivers/Components/stm32wb_at/stm32wb_at_ble.o ./Drivers/Components/stm32wb_at/stm32wb_at_ble.su ./Drivers/Components/stm32wb_at/stm32wb_at_client.cyclo ./Drivers/Components/stm32wb_at/stm32wb_at_client.d ./Drivers/Components/stm32wb_at/stm32wb_at_client.o ./Drivers/Components/stm32wb_at/stm32wb_at_client.su ./Drivers/Components/stm32wb_at/stm32wb_at_ll.cyclo ./Drivers/Components/stm32wb_at/stm32wb_at_ll.d ./Drivers/Components/stm32wb_at/stm32wb_at_ll.o ./Drivers/Components/stm32wb_at/stm32wb_at_ll.su

.PHONY: clean-Drivers-2f-Components-2f-stm32wb_at

