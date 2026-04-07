################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/Components/iis2mdc/iis2mdc.c \
../Drivers/Components/iis2mdc/iis2mdc_reg.c 

OBJS += \
./Drivers/Components/iis2mdc/iis2mdc.o \
./Drivers/Components/iis2mdc/iis2mdc_reg.o 

C_DEPS += \
./Drivers/Components/iis2mdc/iis2mdc.d \
./Drivers/Components/iis2mdc/iis2mdc_reg.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/Components/iis2mdc/%.o Drivers/Components/iis2mdc/%.su Drivers/Components/iis2mdc/%.cyclo: ../Drivers/Components/iis2mdc/%.c Drivers/Components/iis2mdc/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m33 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32U585xx -c -I../Core/Inc -I"C:/Users/User/OneDrive/Desktop/UNM/EEEE4077_Code_U5/U585_Leak_Detection/Drivers/Components/iis2mdc" -I"C:/Users/User/OneDrive/Desktop/UNM/EEEE4077_Code_U5/U585_Leak_Detection/Drivers/Components/ism330dhcx" -I"C:/Users/User/OneDrive/Desktop/UNM/EEEE4077_Code_U5/U585_Leak_Detection/Drivers/Components/Common" -I"C:/Users/User/OneDrive/Desktop/UNM/EEEE4077_Code_U5/U585_Leak_Detection/Drivers/B-U585I-IOT02A" -I../Drivers/STM32U5xx_HAL_Driver/Inc -I../Drivers/STM32U5xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32U5xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-Components-2f-iis2mdc

clean-Drivers-2f-Components-2f-iis2mdc:
	-$(RM) ./Drivers/Components/iis2mdc/iis2mdc.cyclo ./Drivers/Components/iis2mdc/iis2mdc.d ./Drivers/Components/iis2mdc/iis2mdc.o ./Drivers/Components/iis2mdc/iis2mdc.su ./Drivers/Components/iis2mdc/iis2mdc_reg.cyclo ./Drivers/Components/iis2mdc/iis2mdc_reg.d ./Drivers/Components/iis2mdc/iis2mdc_reg.o ./Drivers/Components/iis2mdc/iis2mdc_reg.su

.PHONY: clean-Drivers-2f-Components-2f-iis2mdc

