################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/Components/mx_wifi/checksumutils.c \
../Drivers/Components/mx_wifi/mx_address.c \
../Drivers/Components/mx_wifi/mx_rtos_abs.c \
../Drivers/Components/mx_wifi/mx_wifi.c \
../Drivers/Components/mx_wifi/mx_wifi_hci.c \
../Drivers/Components/mx_wifi/mx_wifi_ipc.c \
../Drivers/Components/mx_wifi/mx_wifi_slip.c \
../Drivers/Components/mx_wifi/mx_wifi_spi.c \
../Drivers/Components/mx_wifi/mx_wifi_uart.c 

OBJS += \
./Drivers/Components/mx_wifi/checksumutils.o \
./Drivers/Components/mx_wifi/mx_address.o \
./Drivers/Components/mx_wifi/mx_rtos_abs.o \
./Drivers/Components/mx_wifi/mx_wifi.o \
./Drivers/Components/mx_wifi/mx_wifi_hci.o \
./Drivers/Components/mx_wifi/mx_wifi_ipc.o \
./Drivers/Components/mx_wifi/mx_wifi_slip.o \
./Drivers/Components/mx_wifi/mx_wifi_spi.o \
./Drivers/Components/mx_wifi/mx_wifi_uart.o 

C_DEPS += \
./Drivers/Components/mx_wifi/checksumutils.d \
./Drivers/Components/mx_wifi/mx_address.d \
./Drivers/Components/mx_wifi/mx_rtos_abs.d \
./Drivers/Components/mx_wifi/mx_wifi.d \
./Drivers/Components/mx_wifi/mx_wifi_hci.d \
./Drivers/Components/mx_wifi/mx_wifi_ipc.d \
./Drivers/Components/mx_wifi/mx_wifi_slip.d \
./Drivers/Components/mx_wifi/mx_wifi_spi.d \
./Drivers/Components/mx_wifi/mx_wifi_uart.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/Components/mx_wifi/%.o Drivers/Components/mx_wifi/%.su Drivers/Components/mx_wifi/%.cyclo: ../Drivers/Components/mx_wifi/%.c Drivers/Components/mx_wifi/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m33 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32U585xx -c -I../Core/Inc -I../Drivers/STM32U5xx_HAL_Driver/Inc -I../Drivers/STM32U5xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32U5xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-Components-2f-mx_wifi

clean-Drivers-2f-Components-2f-mx_wifi:
	-$(RM) ./Drivers/Components/mx_wifi/checksumutils.cyclo ./Drivers/Components/mx_wifi/checksumutils.d ./Drivers/Components/mx_wifi/checksumutils.o ./Drivers/Components/mx_wifi/checksumutils.su ./Drivers/Components/mx_wifi/mx_address.cyclo ./Drivers/Components/mx_wifi/mx_address.d ./Drivers/Components/mx_wifi/mx_address.o ./Drivers/Components/mx_wifi/mx_address.su ./Drivers/Components/mx_wifi/mx_rtos_abs.cyclo ./Drivers/Components/mx_wifi/mx_rtos_abs.d ./Drivers/Components/mx_wifi/mx_rtos_abs.o ./Drivers/Components/mx_wifi/mx_rtos_abs.su ./Drivers/Components/mx_wifi/mx_wifi.cyclo ./Drivers/Components/mx_wifi/mx_wifi.d ./Drivers/Components/mx_wifi/mx_wifi.o ./Drivers/Components/mx_wifi/mx_wifi.su ./Drivers/Components/mx_wifi/mx_wifi_hci.cyclo ./Drivers/Components/mx_wifi/mx_wifi_hci.d ./Drivers/Components/mx_wifi/mx_wifi_hci.o ./Drivers/Components/mx_wifi/mx_wifi_hci.su ./Drivers/Components/mx_wifi/mx_wifi_ipc.cyclo ./Drivers/Components/mx_wifi/mx_wifi_ipc.d ./Drivers/Components/mx_wifi/mx_wifi_ipc.o ./Drivers/Components/mx_wifi/mx_wifi_ipc.su ./Drivers/Components/mx_wifi/mx_wifi_slip.cyclo ./Drivers/Components/mx_wifi/mx_wifi_slip.d ./Drivers/Components/mx_wifi/mx_wifi_slip.o ./Drivers/Components/mx_wifi/mx_wifi_slip.su ./Drivers/Components/mx_wifi/mx_wifi_spi.cyclo ./Drivers/Components/mx_wifi/mx_wifi_spi.d ./Drivers/Components/mx_wifi/mx_wifi_spi.o ./Drivers/Components/mx_wifi/mx_wifi_spi.su ./Drivers/Components/mx_wifi/mx_wifi_uart.cyclo ./Drivers/Components/mx_wifi/mx_wifi_uart.d ./Drivers/Components/mx_wifi/mx_wifi_uart.o ./Drivers/Components/mx_wifi/mx_wifi_uart.su

.PHONY: clean-Drivers-2f-Components-2f-mx_wifi

