################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/Components/lsm6dso/lsm6dso.c \
../Drivers/Components/lsm6dso/lsm6dso_reg.c 

OBJS += \
./Drivers/Components/lsm6dso/lsm6dso.o \
./Drivers/Components/lsm6dso/lsm6dso_reg.o 

C_DEPS += \
./Drivers/Components/lsm6dso/lsm6dso.d \
./Drivers/Components/lsm6dso/lsm6dso_reg.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/Components/lsm6dso/%.o Drivers/Components/lsm6dso/%.su Drivers/Components/lsm6dso/%.cyclo: ../Drivers/Components/lsm6dso/%.c Drivers/Components/lsm6dso/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m33 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32U585xx -c -I../Core/Inc -I../Drivers/STM32U5xx_HAL_Driver/Inc -I../Drivers/STM32U5xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32U5xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-Components-2f-lsm6dso

clean-Drivers-2f-Components-2f-lsm6dso:
	-$(RM) ./Drivers/Components/lsm6dso/lsm6dso.cyclo ./Drivers/Components/lsm6dso/lsm6dso.d ./Drivers/Components/lsm6dso/lsm6dso.o ./Drivers/Components/lsm6dso/lsm6dso.su ./Drivers/Components/lsm6dso/lsm6dso_reg.cyclo ./Drivers/Components/lsm6dso/lsm6dso_reg.d ./Drivers/Components/lsm6dso/lsm6dso_reg.o ./Drivers/Components/lsm6dso/lsm6dso_reg.su

.PHONY: clean-Drivers-2f-Components-2f-lsm6dso

