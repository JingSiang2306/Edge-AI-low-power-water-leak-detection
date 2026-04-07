/*
 * peripheral_Initialize.c
 *
 *  Created on: Nov 1, 2025
 *      Author: FBBC
 */
#include "main.h"
#include "peripheral_Initialize.h"
#include "audio_processing.h"
#include "vibration_processing.h"
#include "lora.h"

static int i = 0;
int periTest = 1;
int powerSave = 0;
int inSaveMode = 0;
int firstEnterSave = 0;
int LEDTest = 1;
int accRead = 0;
int micRead = 0;
int duration = 3000;
int blinkCount = 3;
int micCount = 2;
int accCount = 3;

volatile uint8_t user_button_event = 0;

/////////////////// Accelerometer Parameters ///////////////////
BSP_MOTION_SENSOR_Axes_t axes;

int32_t x_0 = 0;
int32_t y_0 = 0;
int32_t z_0 = 0;

/////////////////// Audio Parameters ///////////////////
#define AUDIO_INSTANCE     0U
#define AUDIO_FS           16000U
#define AUDIO_CH           1U
#define AUDIO_RES          AUDIO_RESOLUTION_16B

int32_t st;

#define REC_BUFF_SIZE 16*1024
uint8_t          RecordBuff[REC_BUFF_SIZE];
volatile uint32_t   RecHalfBuffCplt  = 0;
volatile uint32_t   RecBuffCplt      = 0;


/////////////////// Accelerometer ///////////////////
static void read_accel_init(void)
{

  if (BSP_MOTION_SENSOR_GetAxes(0, MOTION_ACCELERO, &axes) == BSP_ERROR_NONE) {
	x_0 = axes.xval;
	y_0 = axes.yval;
	z_0 = axes.zval;
    printf("X: %ld mg\tY: %ld mg\tZ: %ld mg\r\n", (long)x_0, (long)y_0, (long)z_0);
  }
}

static void accel_init(void)
{
	printf("Accelerator Initializing...\r\n");
	// Instance 0: on-board ISM330DHCX (accel+gyro)
	st = BSP_MOTION_SENSOR_Init(0, MOTION_ACCELERO);
	if (st != BSP_ERROR_NONE) {
		printf("Accelerator Initialize Failed, state = %ld\r\n", st);
		Error_Handler();
	}

	// Enable first
	st = BSP_MOTION_SENSOR_Enable(0, MOTION_ACCELERO);
	if (st != BSP_ERROR_NONE) {
	    printf("Accelerometer Enable Failed, state = %ld\r\n", st);
	    Error_Handler();
	}

	// Set full scale
	st = BSP_MOTION_SENSOR_SetFullScale(0, MOTION_ACCELERO, 4);  // ±4 g
	if (st != BSP_ERROR_NONE) {
	    printf("Accelerometer SetFullScale Failed, state = %ld\r\n", st);
	}

	// Request ODR
	st = BSP_MOTION_SENSOR_SetOutputDataRate(0, MOTION_ACCELERO, 6667.0f);  // Hz
	if (st != BSP_ERROR_NONE) {
	    printf("Accelerometer SetODR Failed, state = %ld\r\n", st);
	}

	// Read back ODR
	{
	    float odr = 0.0f;
	    st = BSP_MOTION_SENSOR_GetOutputDataRate(0, MOTION_ACCELERO, &odr);
	    if (st == BSP_ERROR_NONE) {
	        printf("ODR applied = %.1f Hz (requested 6667.0)\r\n", odr);
	    } else {
	        printf("GetODR Failed, state = %ld\r\n", st);
	    }
	}

	printf("Current Accelerometer Reading:\r\n");
	read_accel_init();
}

static void accel_deInit(void){
    if (BSP_MOTION_SENSOR_DeInit(0) != BSP_ERROR_NONE) {
        Error_Handler();
    }
}

static void read_accel_once(void)
{
  BSP_MOTION_SENSOR_Axes_t axes;
  if (BSP_MOTION_SENSOR_GetAxes(0, MOTION_ACCELERO, &axes) == BSP_ERROR_NONE) {
    printf("X: %ld mg\tY: %ld mg\tZ: %ld mg\r\n", (long)axes.xval, (long)axes.yval, (long)axes.zval);
  }
}

/////////////////// Microphone ///////////////////
static void mic_init(void)
{
	printf("Microphone Initializing...\r\n");
    BSP_AUDIO_Init_t cfg = {
        .Device        = AUDIO_IN_DEVICE_DIGITAL_MIC1,
        .SampleRate    = AUDIO_FS,
        .BitsPerSample = AUDIO_RES,
        .ChannelsNbr   = AUDIO_CH,
		.Volume        = 100U,
    };

    st = BSP_AUDIO_IN_Init(AUDIO_INSTANCE, &cfg);
    if (st != BSP_ERROR_NONE) {
        printf("Audio Initialize Failed, state = %ld\r\n", st);
        Error_Handler();
    }
    else{
    	  HAL_NVIC_ClearPendingIRQ(GPDMA1_Channel6_IRQn);
    	  HAL_NVIC_SetPriority(GPDMA1_Channel6_IRQn, 6, 0); // MIC1 DMA
    	  HAL_NVIC_EnableIRQ(GPDMA1_Channel6_IRQn);
    }
}

static void mic_deInit(void){
    if (BSP_AUDIO_IN_DeInit(0) != BSP_ERROR_NONE) {
        Error_Handler();
    }
    HAL_NVIC_DisableIRQ(GPDMA1_Channel6_IRQn);
    HAL_NVIC_ClearPendingIRQ(GPDMA1_Channel6_IRQn);
    /* Reset global variables */
    RecBuffCplt      = 0;
    RecHalfBuffCplt  = 0;

}

void BSP_AUDIO_IN_HalfTransfer_CallBack(uint32_t Instance)
{
  RecHalfBuffCplt ++;
}

/**
* @brief  Manage the BSP audio in transfer complete event.
* @param  Instance Audio in instance.
* @retval None.
*/
void BSP_AUDIO_IN_TransferComplete_CallBack(uint32_t Instance)
{
  RecBuffCplt ++;
}

/**
* @brief  Manages the BSP audio in error event.
* @param  Instance Audio in instance.
* @retval None.
*/
void BSP_AUDIO_IN_Error_CallBack(uint32_t Instance)
{
	printf("Audio error on instance %lu\r\n", (unsigned long)Instance);
	Error_Handler();
}

/////////////////// LED ///////////////////
static void LED_Init(void){
	printf("LEDs Initializing...\r\n");
	BSP_LED_Init(LED_GREEN);
	BSP_LED_Init(LED_RED);
}

static void LED_Off(void){
	printf("Reset LEDs...\r\n");
	BSP_LED_Off(LED_GREEN);
	BSP_LED_Off(LED_RED);
}

static void LED_deInit(void){
	printf("LEDs De-initializing...\r\n");
	BSP_LED_DeInit(LED_GREEN);
	BSP_LED_DeInit(LED_RED);
}

void LED_Blink(int n){
	for(int k=0; k<n; k++){
		BSP_LED_Toggle(LED_GREEN);
		BSP_LED_Toggle(LED_RED);
		HAL_Delay(200);
		BSP_LED_Toggle(LED_GREEN);
		BSP_LED_Toggle(LED_RED);
		HAL_Delay(200);
	}
}

/////////////////// User Button ///////////////////
static void userButton_Init(void){
	printf("User Button Initializing...\r\n");
	BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);
}

void BSP_PB_Callback(Button_TypeDef Button)
{
    if (Button == BUTTON_USER) {
        // no printf / no delay in ISR
        static uint32_t last_ms = 0;
        uint32_t now = HAL_GetTick();
        if (now - last_ms >= 150) { // debounce
            last_ms = now;
            user_button_event = 1;
        }
    }
}

uint8_t user_button_wait_long_press(uint32_t threshold_ms)
{
    uint32_t start = HAL_GetTick();

    // If user already released before we got here, it's a short press
    if (!BSP_PB_GetState(BUTTON_USER)) {
        return 0U;
    }

    while (BSP_PB_GetState(BUTTON_USER)) {
        if ((HAL_GetTick() - start) >= threshold_ms) {
            // Long press detected; wait for release to avoid double-trigger
            while (BSP_PB_GetState(BUTTON_USER)) {
                HAL_Delay(10);
            }
            return 1U;
        }
        HAL_Delay(10);
    }

    return 0U;
}

/////////////////// Lora ///////////////////
extern UART_HandleTypeDef huart3;

void lora_app_init(void)
{
    lora_initialize(&huart3);

    printf("[LORA] P2P bridge init (UART -> RA-08H NODE)\r\n");
    printf("[LORA] Handshake: sending PING and waiting for PONG...\r\n");

    if (lora_handshake_ping(3000)) {
        printf("[LORA] Handshake OK (PONG received)\r\n");
    } else {
        printf("[LORA] Handshake FAIL (no PONG). Check:\r\n");
        printf("[LORA] - NODE firmware flashed with pingpong_node\r\n");
        printf("[LORA] - BASE firmware flashed with pingpong_base\r\n");
        printf("[LORA] - same frequency/region/settings on both RA-08H\r\n");
        printf("[LORA] - UART wiring TX/RX and common GND (STM32<->NODE)\r\n");
        printf("[LORA] - UART baud matches RA-08H UART0 (usually 115200)\r\n");
        printf("[LORA] - antennas attached + close range\r\n");
    }
}

/////////////////// Init & De-Init ///////////////////
void peripheral_Init(void){
	// clear screen & home cursor
	  printf("\033[2J\033[H");
	  printf("[INIT] Initializing...\r\n");

	  LED_Init();
	  printf("[INIT] LEDs Initialization Completed.\r\n");
	  LED_Off();
	  printf("[INIT] LEDs Reset Completed.\r\n");

	  userButton_Init();
	  printf("[INIT] User Button Initialization Completed.\r\n");

	  accel_init();
	  printf("[INIT] Accelerometer Initialization Completed.\r\n");

	  mic_init();
	  printf("[INIT] Microphone Initialization Completed.\r\n");

	  lora_app_init();
	  printf("[INIT] Lora P2P Initialization Completed.\r\n");

	  printf("[INIT] Initialization Completed.\r\n");
}

void peripheral_Sleep(void)
{
    /* Put peripherals into lowest-power state before entering STOP */
    accel_deInit();
    mic_deInit();
    LED_deInit();
}

void peripheral_Wake(void)
{
	printf("[TMR ] %s - Board wake up.\r\n",get_datetime_string_ms());

    /* Minimal init path used after STOP wake-up (avoid verbose prints) */
    LED_Init();
    BSP_LED_Off(LED_RED);
    BSP_LED_Off(LED_GREEN);

    /* USER button EXTI stays enabled across STOP, but re-init is harmless */
    userButton_Init();

    /* Re-init sensors + LoRa bridge */
    accel_init();
    mic_init();

    /* Keep original LoRa behaviour (handshake) */
    lora_initialize(&huart3);
    (void)lora_handshake_ping(3000);
}

/////////////////// Power Saving ///////////////////
void powerSaving(void){
    /* Legacy wrapper: main.c uses app_enter_power_save() for STOP2 + RTC wakeup */
    if(firstEnterSave == 1){
        peripheral_Sleep();
        firstEnterSave = 0;
    }
}

/////////////////// Peripheral Testing ///////////////////
void peripheral_Testing(void){
	while(LEDTest){
		if (i<blinkCount){
			printf("Blinking Test (Blink %i times).\r\n", blinkCount);
			for (i=0; i<blinkCount; i++){
				printf("No. of Blink Test = %i\r\n",i+1);
				BSP_LED_Toggle(LED_GREEN);
				BSP_LED_Toggle(LED_RED);
				HAL_Delay(500);
				BSP_LED_Toggle(LED_GREEN);
				BSP_LED_Toggle(LED_RED);
				HAL_Delay(500);
			}
			printf("Blinking Test Completed.\r\n");
		}
		// Reset
		i = 0;
		// Next State
		LEDTest = 0;
		accRead = 1;
	}
	while(accRead){
			if (i<accCount){
				printf("Accelerometer Test (Measure %i times).\r\n", accCount);
				for (i=0; i<accCount; i++){
					printf("No. of Accelerometer Test = %i\r\n",i+1);
					BSP_LED_Toggle(LED_GREEN);
					BSP_LED_Toggle(LED_RED);
					read_accel_once();
					HAL_Delay(500);
					BSP_LED_Toggle(LED_GREEN);
					BSP_LED_Toggle(LED_RED);
					HAL_Delay(200);
				}
				// Reset
				i = 0;
				// Next State
				accRead = 0;
				micRead = 1;
			}
		}
		while(micRead){
			if (i<micCount){
				printf("Microphone Test (Record %i times).\r\n", micCount);
				for (i=0; i<micCount; i++){
					printf("No. of Microphone Test = %i\r\n",i+1);
					printf("Start Recording...\r\n");
					st = BSP_AUDIO_IN_Record(0, (uint8_t*) RecordBuff, REC_BUFF_SIZE);

					  if (st != BSP_ERROR_NONE)
					  {
						printf("AUDIO IN : FAILED.\r\n");
						printf("AUDIO IN example Aborted.\r\n");
						Error_Handler();
					  }
					  else{
						  BSP_LED_Toggle(LED_GREEN);
						  BSP_LED_Toggle(LED_RED);
						  printf("Recording Started.\r\n");
					  }


					  HAL_Delay(duration);

					  /* Stop playback */
					  printf("Stop Recording...\r\n");
					  st = BSP_AUDIO_IN_Stop(0);

					  if (st != BSP_ERROR_NONE)
					  {
						printf("Recording Stop Failed (State:%ld).\r\n",st);
						Error_Handler();
					  }
					  else{
						  printf("%d ms audio recorded.\r\n", duration);
						  BSP_LED_Toggle(LED_GREEN);
						  BSP_LED_Toggle(LED_RED);
						  printf("Recording Stopped.\r\n");
					  }

					  // Reset global variables
					  RecBuffCplt      = 0;
					  RecHalfBuffCplt  = 0;

					  HAL_Delay(200);
				}
				printf("Microphone Test Completed.\r\n");
				// Reset
				i = 0;
				// Exit
				micRead = 0;
			}
		}
	printf("Peripherals Testing Completed.\r\n");
	LEDTest = 1;
}
