################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/B-U585I-IOT02A/b_u585i_iot02a.c \
../Drivers/B-U585I-IOT02A/b_u585i_iot02a_audio.c \
../Drivers/B-U585I-IOT02A/b_u585i_iot02a_bus.c \
../Drivers/B-U585I-IOT02A/b_u585i_iot02a_motion_sensors.c 

OBJS += \
./Drivers/B-U585I-IOT02A/b_u585i_iot02a.o \
./Drivers/B-U585I-IOT02A/b_u585i_iot02a_audio.o \
./Drivers/B-U585I-IOT02A/b_u585i_iot02a_bus.o \
./Drivers/B-U585I-IOT02A/b_u585i_iot02a_motion_sensors.o 

C_DEPS += \
./Drivers/B-U585I-IOT02A/b_u585i_iot02a.d \
./Drivers/B-U585I-IOT02A/b_u585i_iot02a_audio.d \
./Drivers/B-U585I-IOT02A/b_u585i_iot02a_bus.d \
./Drivers/B-U585I-IOT02A/b_u585i_iot02a_motion_sensors.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/B-U585I-IOT02A/%.o Drivers/B-U585I-IOT02A/%.su Drivers/B-U585I-IOT02A/%.cyclo: ../Drivers/B-U585I-IOT02A/%.c Drivers/B-U585I-IOT02A/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m33 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32U585xx -c -I../Core/Inc -I"C:/Users/User/OneDrive/Desktop/UNM/EEEE4077_Code_U5/U585_Leak_Detection/Drivers/Components/iis2mdc" -I"C:/Users/User/OneDrive/Desktop/UNM/EEEE4077_Code_U5/U585_Leak_Detection/Drivers/Components/ism330dhcx" -I"C:/Users/User/OneDrive/Desktop/UNM/EEEE4077_Code_U5/U585_Leak_Detection/Drivers/Components/Common" -I"C:/Users/User/OneDrive/Desktop/UNM/EEEE4077_Code_U5/U585_Leak_Detection/Drivers/B-U585I-IOT02A" -I../Drivers/STM32U5xx_HAL_Driver/Inc -I../Drivers/STM32U5xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32U5xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-B-2d-U585I-2d-IOT02A

clean-Drivers-2f-B-2d-U585I-2d-IOT02A:
	-$(RM) ./Drivers/B-U585I-IOT02A/b_u585i_iot02a.cyclo ./Drivers/B-U585I-IOT02A/b_u585i_iot02a.d ./Drivers/B-U585I-IOT02A/b_u585i_iot02a.o ./Drivers/B-U585I-IOT02A/b_u585i_iot02a.su ./Drivers/B-U585I-IOT02A/b_u585i_iot02a_audio.cyclo ./Drivers/B-U585I-IOT02A/b_u585i_iot02a_audio.d ./Drivers/B-U585I-IOT02A/b_u585i_iot02a_audio.o ./Drivers/B-U585I-IOT02A/b_u585i_iot02a_audio.su ./Drivers/B-U585I-IOT02A/b_u585i_iot02a_bus.cyclo ./Drivers/B-U585I-IOT02A/b_u585i_iot02a_bus.d ./Drivers/B-U585I-IOT02A/b_u585i_iot02a_bus.o ./Drivers/B-U585I-IOT02A/b_u585i_iot02a_bus.su ./Drivers/B-U585I-IOT02A/b_u585i_iot02a_motion_sensors.cyclo ./Drivers/B-U585I-IOT02A/b_u585i_iot02a_motion_sensors.d ./Drivers/B-U585I-IOT02A/b_u585i_iot02a_motion_sensors.o ./Drivers/B-U585I-IOT02A/b_u585i_iot02a_motion_sensors.su

.PHONY: clean-Drivers-2f-B-2d-U585I-2d-IOT02A

