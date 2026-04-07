/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "power_mode.h"
#include "peripheral_Initialize.h"
#include "audio_processing.h"
#include "vibration_processing.h"
#include "audio_detection.h"
#include "vibration_detection.h"
#include "detection_logic.h"
#include "data_logger.h"
#include "lora.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c1;

RTC_HandleTypeDef hrtc;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */

/* Default sleep mode (can be changed at runtime if needed) */
static volatile app_sleep_mode_t g_sleep_mode = APP_SLEEP_MODE_STOP2;

int mic_record_duration = 3;
int acc_record_duration = 1;

static uint32_t logging_duration_ms = 10000U;				// Logging duration in milliseconds
static const uint32_t dataset_log_per_press       = 5U;     // how many logs per press
static const uint32_t dataset_inter_log_delay_ms  = 10000U;  // delay between logs (ms)

double verdict_conf = 0.0;

typedef enum {
    APP_MODE_DATASET = 0,
    APP_MODE_DETECTION = 1,
} app_mode_t;

static app_mode_t g_mode = APP_MODE_DETECTION;
static const uint32_t g_long_press_ms = 1200U;
static const uint32_t DETL_CHUNK_MS = 3000U;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void SystemPower_Config(void);
static void MX_GPIO_Init(void);
static void MX_ICACHE_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_ADC1_Init(void);
static void MX_RTC_Init(void);
static void MX_USART3_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
uint32_t get_time_ms_of_day(void)
{
    RTC_TimeTypeDef t;
    RTC_DateTypeDef d;    // must read date after time (RTC requirement)

    HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN);

    uint32_t ms =
        ((uint32_t)t.Hours   * 3600U +
         (uint32_t)t.Minutes * 60U +
         (uint32_t)t.Seconds) * 1000U +
         (uint32_t)(t.SubSeconds * 1000U) / hrtc.Init.SynchPrediv;

    return ms;
}

const char *get_date_string(void)
{
    static char date_buf[16];  // persists after function return

    RTC_TimeTypeDef t;
    RTC_DateTypeDef d;

    HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN);

    // Format: DD/MM/YYYY
    snprintf(date_buf, sizeof(date_buf),
             "%02u/%02u/20%02u",
             d.Date, d.Month, d.Year);

    return date_buf;
}

const char *get_datetime_string_ms(void)
{
    static char dt_buf[32];

    RTC_TimeTypeDef t;
    RTC_DateTypeDef d;

    /* Read time first, then date (RTC requirement) */
    HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN);

    /* On STM32, SubSeconds counts down from SynchPrediv to 0 */
    uint32_t sub = t.SubSeconds;
    uint32_t prediv = (uint32_t)hrtc.Init.SynchPrediv;
    uint32_t ms = 0U;

    if (prediv > 0U) {
        /* Convert downcounter to elapsed ms within the current second */
        uint32_t ticks = (prediv - sub);
        ms = (ticks * 1000U) / (prediv + 1U);
    }
    ms %= 1000U;

    /* Format: DD/MM/YYYY HH:MM:SS.mmm */
    snprintf(dt_buf, sizeof(dt_buf),
             "%02u/%02u/20%02u %02u:%02u:%02u.%03u",
             d.Date, d.Month, d.Year,
             t.Hours, t.Minutes, t.Seconds,
             (unsigned)ms);

    return dt_buf;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the System Power */
  SystemPower_Config();

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* Determine reset / low-power wake context (needed for SHUTDOWN) */
  app_boot_reason_init();

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ICACHE_Init();
  MX_USART1_UART_Init();
  MX_I2C1_Init();
  MX_ADC1_Init();
  MX_RTC_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
  printf("[BTN ] \r\n");
  printf("[BTN ] ======================================== RESET ========================================\r\n");
  printf("[BTN ] \r\n");
  printf("[BTN ] %s\r\n", get_datetime_string_ms());
  // peripheral_Init();
  /* On SHUTDOWN/STANDBY wake, skip peripheral testing and use lightweight wake init */
  if (g_boot_from_standby) {
	  periTest = 0;
      peripheral_Wake();
      app_cancel_rtc_wakeup(); /* stop periodic wakeups while we are awake */
      app_disable_wkup2_after_wake();
      g_woke_from_sleep = 1U;
    } else {
      peripheral_Init();
    }
  g_last_activity_ms = HAL_GetTick();
  audio_detection_init();
  vibration_detection_init();
  g_neai_num_windows = 30;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	while(periTest){
		printf("[TEST] Start peripherals testing...\r\n");
		peripheral_Testing();
		periTest = 0;
		printf("[TEST] Peripherals testing completed.\r\n");
		printf("[MODE] Detection period: %u minutes \r\n",APP_PERIODIC_DET_SEC/60);
		printf("[MODE] Current mode: %s\r\n", (g_mode == APP_MODE_DATASET) ? "DATASET" : "DETECTION");
		lora_sendf("RESET: %s",get_datetime_string_ms());
		HAL_Delay(200);
		lora_sendf("MODE: %s", (g_mode == APP_MODE_DATASET) ? "DATASET" : "DETECTION");
		g_last_activity_ms = HAL_GetTick();
	}
	while(powerSave){
		printf("[TMR ] %s - Enter power saving mode.\r\n",get_datetime_string_ms());
		lora_sendf("SLEEP: %s",get_datetime_string_ms());
		power_mode_set_sleep_mode(g_sleep_mode);
		app_enter_power_save();
	}
	/* If we just woke from STOP, discard the wake button press (enter idle state) */
	if (g_woke_from_sleep) {
		HAL_Delay(200);
		lora_sendf("WAKE: %s",get_datetime_string_ms());
		HAL_Delay(200);
		lora_sendf("MODE: %s", (g_mode == APP_MODE_DATASET) ? "DATASET" : "DETECTION");
		printf("[TMR ] Current mode: %s\r\n", (g_mode == APP_MODE_DATASET) ? "DATASET" : "DETECTION");
	    /* If USER button was the wake source, discard that press (wake only) */
	    if (user_button_event) {
	        user_button_event = 0U;
	        g_last_activity_ms = HAL_GetTick();
	    }

	    /* Enter idle state visual immediately after wake */
	    if (g_mode == APP_MODE_DATASET) {
	        BSP_LED_Off(LED_GREEN);
	        BSP_LED_On(LED_RED);
	    }
	    else if (g_mode == APP_MODE_DETECTION) {
	        BSP_LED_On(LED_GREEN);
	        BSP_LED_Off(LED_RED);
	    }
	    else {
	        LED_Blink(1);
	    }

	    g_woke_from_sleep = 0U;
	}
    while (user_button_event) {
        // Clear the button flag so this loop only runs once per press
        user_button_event = 0;
        g_last_activity_ms = HAL_GetTick();

        // Long press toggles mode
		if (user_button_wait_long_press(g_long_press_ms)) {
			g_mode = (g_mode == APP_MODE_DATASET) ? APP_MODE_DETECTION : APP_MODE_DATASET;
			printf("[MODE] Switched to %s\r\n", (g_mode == APP_MODE_DATASET) ? "DATASET" : "DETECTION");
			lora_sendf("MODE: %s", (g_mode == APP_MODE_DATASET) ? "DATASET" : "DETECTION");

			// LED feedback: DATASET=1 blink, DETECTION=3 blinks
			LED_Blink(1);
			if (g_mode == APP_MODE_DETECTION) {
				LED_Blink(2);
			}
			g_last_activity_ms = HAL_GetTick();
			break;
		}

        if(g_mode == APP_MODE_DATASET){	// Dataset mode
            /* Initial delay: gives you time to remove your hand,
               and avoids capturing the vibration from the button press itself. */
            HAL_Delay(1000);
            lora_sendf("DATA: Start - %s",get_datetime_string_ms());
            BSP_LED_On(LED_GREEN);

			for (uint32_t n = 0U; n < dataset_log_per_press; n++) {

				printf("[BTN ] Logging %lu s now... (%lu/%lu)\r\n",
					   (unsigned long)logging_duration_ms / 1000U,
					   (unsigned long)(n + 1U),
					   (unsigned long)dataset_log_per_press);

				// Sequential logging
				int rc = logging_now(logging_duration_ms);

				// Simultaneous dataset logging (audio + vib) - currently not achieving 6.6 kHz for vibration
	            //int rc = logging_syn_dataset(logging_duration_ms);

				printf("[BTN ] Logging completed. Status = %d\r\n", rc);

				// Delay between recordings so the PC logger has time to finish writing before the next run.
				if ((dataset_log_per_press > 1U) && ((n + 1U) <= dataset_log_per_press)){
					printf("[BTN ] Intermediate delay started. Delay for %lu s\r\n", (unsigned long)dataset_inter_log_delay_ms/1000);
					HAL_Delay(dataset_inter_log_delay_ms);
					printf("[BTN ] Intermediate delay ended.\r\n");
				}
			}
			printf("[BTN ] All Logging completed. Total = %lu datasets.\r\n", (unsigned long)dataset_log_per_press);
			BSP_LED_Off(LED_GREEN);
			lora_sendf("DATA: End - %s",get_datetime_string_ms());
        }
        else { // Detection mode

            HAL_Delay(500);
            BSP_LED_On(LED_RED);
            printf("[BTN ] Running %lus (10 x 3s) late-fusion detection...\r\n",DETL_CHUNK_MS/1000);

            int dl_rc = detection_logic_run_30s_latefusion(DETL_CHUNK_MS);
            bool lr_pl = lora_send_detection_payload();
            printf("[LORA] Detection payload sent %s \r\n", lr_pl ? "successfully" : "failed");

            const char *verdict = dl_rc ? "leak" : "background";
            printf("[BTN ] 30s late-fusion detection result = %s (%.3f) \r\n", verdict, verdict_conf);

            BSP_LED_Off(LED_RED);

        }
        g_last_activity_ms = HAL_GetTick();

    }

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

/*    //Periodic dataset collection trigger (RTC wake while sleeping)
    if (g_periodic_det_pending) {
		HAL_Delay(10000);
    	printf("[TMR ] Periodically (every %lu min) dataset collection start.\r\n", (unsigned long)APP_PERIODIC_DET_SEC/60);
        g_periodic_det_pending = 0U;

        lora_sendf("DATA: Start - %s",get_datetime_string_ms());
        BSP_LED_On(LED_GREEN);

		for (uint32_t n = 0U; n < dataset_log_per_press; n++) {

			printf("[BTN ] Logging %lu s now... (%lu/%lu)\r\n",
				   (unsigned long)logging_duration_ms / 1000U,
				   (unsigned long)(n + 1U),
				   (unsigned long)dataset_log_per_press);

			// Sequential logging
			int rc = logging_now(logging_duration_ms);

			// Simultaneous dataset logging (audio + vib) - currently not achieving 6.6 kHz for vibration
            //int rc = logging_syn_dataset(logging_duration_ms);

			printf("[BTN ] Logging completed. Status = %d\r\n", rc);

			// Delay between recordings so the PC logger has time to finish writing before the next run.
			if ((dataset_log_per_press > 1U) && ((n + 1U) <= dataset_log_per_press)){
				printf("[BTN ] Intermediate delay started. Delay for %lu s\r\n", (unsigned long)dataset_inter_log_delay_ms/1000);
				HAL_Delay(dataset_inter_log_delay_ms);
				printf("[BTN ] Intermediate delay ended.\r\n");
			}
		}
		printf("[BTN ] All Logging completed. Total = %lu datasets.\r\n", (unsigned long)dataset_log_per_press);
		BSP_LED_Off(LED_GREEN);
		lora_sendf("DATA: End - %s",get_datetime_string_ms());

        // Periodic (RTC) detection: sleep immediately after detection + LoRa
        powerSave = 1U;
        continue;  // jump to while(powerSave) and enter low power now
    }*/

    if (g_periodic_det_pending) {
    	HAL_Delay(10000);
    	printf("[TMR ] Periodically (every %lu) detection start.\r\n", (unsigned long)APP_PERIODIC_DET_SEC);
        g_periodic_det_pending = 0U;

        BSP_LED_On(LED_RED);
        BSP_LED_On(LED_GREEN);

        printf("[TMR ] Running %lus (10 x 3s) late-fusion detection...\r\n", DETL_CHUNK_MS/1000);

        int dl_rc = detection_logic_run_30s_latefusion(DETL_CHUNK_MS);
        bool lr_pl = lora_send_detection_payload();
        printf("[LORA] Detection payload sent %s \r\n", lr_pl ? "successfully" : "failed");

        const char *verdict = dl_rc ? "leak" : "background";
        printf("[TMR ] 30s late-fusion detection result = %s (%.3f) \r\n", verdict, verdict_conf);

        BSP_LED_Off(LED_RED);
        BSP_LED_Off(LED_GREEN);

        // Periodic (RTC) detection: sleep immediately after detection + LoRa
        powerSave = 1U;
        continue;  // Jump to while(powerSave) and enter low power now
    }

    // Idle timeout: return to power saving mode after APP_IDLE_TIMEOUT_MS
    if ((HAL_GetTick() - g_last_activity_ms) >= APP_IDLE_TIMEOUT_MS) {
        powerSave = 1;
    }

	if(g_mode == APP_MODE_DATASET){
		BSP_LED_Off(LED_GREEN);
		BSP_LED_On(LED_RED);
	}
	else if(g_mode == APP_MODE_DETECTION){
		BSP_LED_On(LED_GREEN);
		BSP_LED_Off(LED_RED);
	}
	else{
		LED_Blink(1);
	}
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSE
                              |RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON_RTC_ONLY;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_4;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLMBOOST = RCC_PLLMBOOST_DIV1;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 80;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLLVCIRANGE_0;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_PCLK3;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Power Configuration
  * @retval None
  */
static void SystemPower_Config(void)
{
  HAL_PWREx_EnableVddIO2();

  /*
   * Switch to SMPS regulator instead of LDO
   */
  if (HAL_PWREx_ConfigSupply(PWR_SMPS_SUPPLY) != HAL_OK)
  {
    Error_Handler();
  }
/* USER CODE BEGIN PWR */
/* USER CODE END PWR */
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc1.Init.Resolution = ADC_RESOLUTION_14B;
  hadc1.Init.GainCompensation = 0;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.TriggerFrequencyMode = ADC_TRIGGER_FREQ_HIGH;
  hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc1.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
  hadc1.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DR;
  hadc1.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x30909DEC;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief ICACHE Initialization Function
  * @param None
  * @retval None
  */
static void MX_ICACHE_Init(void)
{

  /* USER CODE BEGIN ICACHE_Init 0 */

  /* USER CODE END ICACHE_Init 0 */

  /* USER CODE BEGIN ICACHE_Init 1 */

  /* USER CODE END ICACHE_Init 1 */

  /** Enable instruction cache in 1-way (direct mapped cache)
  */
  if (HAL_ICACHE_ConfigAssociativityMode(ICACHE_1WAY) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_ICACHE_Enable() != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ICACHE_Init 2 */

  /* USER CODE END ICACHE_Init 2 */

}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  RTC_PrivilegeStateTypeDef privilegeState = {0};
  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 255;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutRemap = RTC_OUTPUT_REMAP_NONE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  hrtc.Init.OutPutPullUp = RTC_OUTPUT_PULLUP_NONE;
  hrtc.Init.BinMode = RTC_BINARY_NONE;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }
  privilegeState.rtcPrivilegeFull = RTC_PRIVILEGE_FULL_NO;
  privilegeState.backupRegisterPrivZone = RTC_PRIVILEGE_BKUP_ZONE_NONE;
  privilegeState.backupRegisterStartZone2 = RTC_BKP_DR0;
  privilegeState.backupRegisterStartZone3 = RTC_BKP_DR0;
  if (HAL_RTCEx_PrivilegeModeSet(&hrtc, &privilegeState) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Check_RTC_BKUP */
  HAL_PWR_EnableBkUpAccess();
  /* USER CODE END Check_RTC_BKUP */

  /** Initialize RTC and set the Time and Date
  */
  sTime.Hours = 0x0;
  sTime.Minutes = 0x0;
  sTime.Seconds = 0x0;
  sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sTime.StoreOperation = RTC_STOREOPERATION_RESET;
  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  sDate.WeekDay = RTC_WEEKDAY_SATURDAY;
  sDate.Month = RTC_MONTH_FEBRUARY;
  sDate.Date = 0x7;
  sDate.Year = 0x26;

  if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enable the WakeUp
  */
  if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, 0, RTC_WAKEUPCLOCK_RTCCLK_DIV16, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 921600;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart3, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart3, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOI_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(UCPD_PWR_GPIO_Port, UCPD_PWR_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOH, LED_RED_Pin|LED_GREEN_Pin|Mems_VL53_xshut_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(WRLS_WKUP_B_GPIO_Port, WRLS_WKUP_B_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOF, Mems_STSAFE_RESET_Pin|WRLS_WKUP_W_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : PE2 PE4 PE1 PE5
                           PE3 PE0 PE6 PE10
                           PE9 PE14 PE7 PE13
                           PE15 PE12 */
  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_4|GPIO_PIN_1|GPIO_PIN_5
                          |GPIO_PIN_3|GPIO_PIN_0|GPIO_PIN_6|GPIO_PIN_10
                          |GPIO_PIN_9|GPIO_PIN_14|GPIO_PIN_7|GPIO_PIN_13
                          |GPIO_PIN_15|GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : PI6 PI1 PI5 PI4
                           PI0 PI7 PI3 PI2 */
  GPIO_InitStruct.Pin = GPIO_PIN_6|GPIO_PIN_1|GPIO_PIN_5|GPIO_PIN_4
                          |GPIO_PIN_0|GPIO_PIN_7|GPIO_PIN_3|GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);

  /*Configure GPIO pins : WRLS_FLOW_Pin Mems_VLX_GPIO_Pin Mems_INT_LPS22HH_Pin */
  GPIO_InitStruct.Pin = WRLS_FLOW_Pin|Mems_VLX_GPIO_Pin|Mems_INT_LPS22HH_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pins : PG9 PG10 PG12 PG7
                           PG1 PG8 PG4 PG0
                           PG3 */
  GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_12|GPIO_PIN_7
                          |GPIO_PIN_1|GPIO_PIN_8|GPIO_PIN_4|GPIO_PIN_0
                          |GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pins : PC11 PC10 PC12 PC9
                           PC8 PC7 PC6 PC1
                           PC2 PC3 PC4 PC5 */
  GPIO_InitStruct.Pin = GPIO_PIN_11|GPIO_PIN_10|GPIO_PIN_12|GPIO_PIN_9
                          |GPIO_PIN_8|GPIO_PIN_7|GPIO_PIN_6|GPIO_PIN_1
                          |GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PA15 PA14 PA13 PA12
                           PA8 PA11 PA7 PA0
                           PA5 PA1 PA2 PA4
                           PA3 PA6 */
  GPIO_InitStruct.Pin = GPIO_PIN_15|GPIO_PIN_14|GPIO_PIN_13|GPIO_PIN_12
                          |GPIO_PIN_8|GPIO_PIN_11|GPIO_PIN_7|GPIO_PIN_0
                          |GPIO_PIN_5|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_4
                          |GPIO_PIN_3|GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PH15 PH12 PH14 PH13
                           PH10 PH11 PH8 PH9
                           PH4 PH5 PH2 PH0 */
  GPIO_InitStruct.Pin = GPIO_PIN_15|GPIO_PIN_12|GPIO_PIN_14|GPIO_PIN_13
                          |GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_8|GPIO_PIN_9
                          |GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_2|GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

  /*Configure GPIO pins : PB6 PB4 PB3 PB7
                           PB0 PB10 PB2 PB11
                           PB12 PB15 PB14 PB1
                           PB13 */
  GPIO_InitStruct.Pin = GPIO_PIN_6|GPIO_PIN_4|GPIO_PIN_3|GPIO_PIN_7
                          |GPIO_PIN_0|GPIO_PIN_10|GPIO_PIN_2|GPIO_PIN_11
                          |GPIO_PIN_12|GPIO_PIN_15|GPIO_PIN_14|GPIO_PIN_1
                          |GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PD6 PD0 PD4 PD7
                           PD3 PD5 PD1 PD2
                           PD15 PD12 PD11 */
  GPIO_InitStruct.Pin = GPIO_PIN_6|GPIO_PIN_0|GPIO_PIN_4|GPIO_PIN_7
                          |GPIO_PIN_3|GPIO_PIN_5|GPIO_PIN_1|GPIO_PIN_2
                          |GPIO_PIN_15|GPIO_PIN_12|GPIO_PIN_11;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pin : PH3_BOOT0_Pin */
  GPIO_InitStruct.Pin = PH3_BOOT0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(PH3_BOOT0_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : UCPD_PWR_Pin */
  GPIO_InitStruct.Pin = UCPD_PWR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(UCPD_PWR_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PF0 PF8 PF1 PF2
                           PF7 PF9 PF5 PF3
                           PF4 PF10 PF6 PF12
                           PF13 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_8|GPIO_PIN_1|GPIO_PIN_2
                          |GPIO_PIN_7|GPIO_PIN_9|GPIO_PIN_5|GPIO_PIN_3
                          |GPIO_PIN_4|GPIO_PIN_10|GPIO_PIN_6|GPIO_PIN_12
                          |GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  /*Configure GPIO pin : USER_Button_Pin */
  GPIO_InitStruct.Pin = USER_Button_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(USER_Button_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LED_RED_Pin LED_GREEN_Pin Mems_VL53_xshut_Pin */
  GPIO_InitStruct.Pin = LED_RED_Pin|LED_GREEN_Pin|Mems_VL53_xshut_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

  /*Configure GPIO pin : WRLS_WKUP_B_Pin */
  GPIO_InitStruct.Pin = WRLS_WKUP_B_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(WRLS_WKUP_B_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : WRLS_NOTIFY_Pin Mems_INT_IIS2MDC_Pin USB_IANA_Pin */
  GPIO_InitStruct.Pin = WRLS_NOTIFY_Pin|Mems_INT_IIS2MDC_Pin|USB_IANA_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pin : USB_UCPD_FLT_Pin */
  GPIO_InitStruct.Pin = USB_UCPD_FLT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(USB_UCPD_FLT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : USB_VBUS_SENSE_Pin */
  GPIO_InitStruct.Pin = USB_VBUS_SENSE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(USB_VBUS_SENSE_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : Mems_STSAFE_RESET_Pin WRLS_WKUP_W_Pin */
  GPIO_InitStruct.Pin = Mems_STSAFE_RESET_Pin|WRLS_WKUP_W_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  /*Configure GPIO pin : PE11 */
  GPIO_InitStruct.Pin = GPIO_PIN_11;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI11_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI11_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
int __io_putchar(int ch) {
  HAL_UART_Transmit(&huart1, (uint8_t*)&ch, 1, HAL_MAX_DELAY);
  return ch;
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
