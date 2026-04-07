################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/Components/vl53l5cx/modules/vl53l5cx_api.c \
../Drivers/Components/vl53l5cx/modules/vl53l5cx_plugin_detection_thresholds.c \
../Drivers/Components/vl53l5cx/modules/vl53l5cx_plugin_motion_indicator.c \
../Drivers/Components/vl53l5cx/modules/vl53l5cx_plugin_xtalk.c 

OBJS += \
./Drivers/Components/vl53l5cx/modules/vl53l5cx_api.o \
./Drivers/Components/vl53l5cx/modules/vl53l5cx_plugin_detection_thresholds.o \
./Drivers/Components/vl53l5cx/modules/vl53l5cx_plugin_motion_indicator.o \
./Drivers/Components/vl53l5cx/modules/vl53l5cx_plugin_xtalk.o 

C_DEPS += \
./Drivers/Components/vl53l5cx/modules/vl53l5cx_api.d \
./Drivers/Components/vl53l5cx/modules/vl53l5cx_plugin_detection_thresholds.d \
./Drivers/Components/vl53l5cx/modules/vl53l5cx_plugin_motion_indicator.d \
./Drivers/Components/vl53l5cx/modules/vl53l5cx_plugin_xtalk.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/Components/vl53l5cx/modules/%.o Drivers/Components/vl53l5cx/modules/%.su Drivers/Components/vl53l5cx/modules/%.cyclo: ../Drivers/Components/vl53l5cx/modules/%.c Drivers/Components/vl53l5cx/modules/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m33 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32U585xx -c -I../Core/Inc -I../Drivers/STM32U5xx_HAL_Driver/Inc -I../Drivers/STM32U5xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32U5xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-Components-2f-vl53l5cx-2f-modules

clean-Drivers-2f-Components-2f-vl53l5cx-2f-modules:
	-$(RM) ./Drivers/Components/vl53l5cx/modules/vl53l5cx_api.cyclo ./Drivers/Components/vl53l5cx/modules/vl53l5cx_api.d ./Drivers/Components/vl53l5cx/modules/vl53l5cx_api.o ./Drivers/Components/vl53l5cx/modules/vl53l5cx_api.su ./Drivers/Components/vl53l5cx/modules/vl53l5cx_plugin_detection_thresholds.cyclo ./Drivers/Components/vl53l5cx/modules/vl53l5cx_plugin_detection_thresholds.d ./Drivers/Components/vl53l5cx/modules/vl53l5cx_plugin_detection_thresholds.o ./Drivers/Components/vl53l5cx/modules/vl53l5cx_plugin_detection_thresholds.su ./Drivers/Components/vl53l5cx/modules/vl53l5cx_plugin_motion_indicator.cyclo ./Drivers/Components/vl53l5cx/modules/vl53l5cx_plugin_motion_indicator.d ./Drivers/Components/vl53l5cx/modules/vl53l5cx_plugin_motion_indicator.o ./Drivers/Components/vl53l5cx/modules/vl53l5cx_plugin_motion_indicator.su ./Drivers/Components/vl53l5cx/modules/vl53l5cx_plugin_xtalk.cyclo ./Drivers/Components/vl53l5cx/modules/vl53l5cx_plugin_xtalk.d ./Drivers/Components/vl53l5cx/modules/vl53l5cx_plugin_xtalk.o ./Drivers/Components/vl53l5cx/modules/vl53l5cx_plugin_xtalk.su

.PHONY: clean-Drivers-2f-Components-2f-vl53l5cx-2f-modules

