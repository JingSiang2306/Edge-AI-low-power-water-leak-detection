################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/audio_detection.c \
../Core/Src/audio_processing.c \
../Core/Src/data_logger.c \
../Core/Src/detection_logic.c \
../Core/Src/lora.c \
../Core/Src/main.c \
../Core/Src/peripheral_Initialize.c \
../Core/Src/power_mode.c \
../Core/Src/stm32u5xx_hal_msp.c \
../Core/Src/stm32u5xx_it.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c \
../Core/Src/system_stm32u5xx.c \
../Core/Src/vibration_detection.c \
../Core/Src/vibration_processing.c 

OBJS += \
./Core/Src/audio_detection.o \
./Core/Src/audio_processing.o \
./Core/Src/data_logger.o \
./Core/Src/detection_logic.o \
./Core/Src/lora.o \
./Core/Src/main.o \
./Core/Src/peripheral_Initialize.o \
./Core/Src/power_mode.o \
./Core/Src/stm32u5xx_hal_msp.o \
./Core/Src/stm32u5xx_it.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/system_stm32u5xx.o \
./Core/Src/vibration_detection.o \
./Core/Src/vibration_processing.o 

C_DEPS += \
./Core/Src/audio_detection.d \
./Core/Src/audio_processing.d \
./Core/Src/data_logger.d \
./Core/Src/detection_logic.d \
./Core/Src/lora.d \
./Core/Src/main.d \
./Core/Src/peripheral_Initialize.d \
./Core/Src/power_mode.d \
./Core/Src/stm32u5xx_hal_msp.d \
./Core/Src/stm32u5xx_it.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d \
./Core/Src/system_stm32u5xx.d \
./Core/Src/vibration_detection.d \
./Core/Src/vibration_processing.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m33 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32U585xx -c -I../Core/Inc -I"C:/Users/User/OneDrive/Desktop/UNM/EEEE4077_Code_U5/U585_Leak_Detection/Drivers/Components/iis2mdc" -I"C:/Users/User/OneDrive/Desktop/UNM/EEEE4077_Code_U5/U585_Leak_Detection/Drivers/Components/ism330dhcx" -I"C:/Users/User/OneDrive/Desktop/UNM/EEEE4077_Code_U5/U585_Leak_Detection/Drivers/Components/Common" -I"C:/Users/User/OneDrive/Desktop/UNM/EEEE4077_Code_U5/U585_Leak_Detection/Drivers/B-U585I-IOT02A" -I../Drivers/STM32U5xx_HAL_Driver/Inc -I../Drivers/STM32U5xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32U5xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/audio_detection.cyclo ./Core/Src/audio_detection.d ./Core/Src/audio_detection.o ./Core/Src/audio_detection.su ./Core/Src/audio_processing.cyclo ./Core/Src/audio_processing.d ./Core/Src/audio_processing.o ./Core/Src/audio_processing.su ./Core/Src/data_logger.cyclo ./Core/Src/data_logger.d ./Core/Src/data_logger.o ./Core/Src/data_logger.su ./Core/Src/detection_logic.cyclo ./Core/Src/detection_logic.d ./Core/Src/detection_logic.o ./Core/Src/detection_logic.su ./Core/Src/lora.cyclo ./Core/Src/lora.d ./Core/Src/lora.o ./Core/Src/lora.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/peripheral_Initialize.cyclo ./Core/Src/peripheral_Initialize.d ./Core/Src/peripheral_Initialize.o ./Core/Src/peripheral_Initialize.su ./Core/Src/power_mode.cyclo ./Core/Src/power_mode.d ./Core/Src/power_mode.o ./Core/Src/power_mode.su ./Core/Src/stm32u5xx_hal_msp.cyclo ./Core/Src/stm32u5xx_hal_msp.d ./Core/Src/stm32u5xx_hal_msp.o ./Core/Src/stm32u5xx_hal_msp.su ./Core/Src/stm32u5xx_it.cyclo ./Core/Src/stm32u5xx_it.d ./Core/Src/stm32u5xx_it.o ./Core/Src/stm32u5xx_it.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/system_stm32u5xx.cyclo ./Core/Src/system_stm32u5xx.d ./Core/Src/system_stm32u5xx.o ./Core/Src/system_stm32u5xx.su ./Core/Src/vibration_detection.cyclo ./Core/Src/vibration_detection.d ./Core/Src/vibration_detection.o ./Core/Src/vibration_detection.su ./Core/Src/vibration_processing.cyclo ./Core/Src/vibration_processing.d ./Core/Src/vibration_processing.o ./Core/Src/vibration_processing.su

.PHONY: clean-Core-2f-Src

