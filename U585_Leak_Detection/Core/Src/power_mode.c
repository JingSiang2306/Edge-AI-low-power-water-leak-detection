#include "power_mode.h"
#include "peripheral_Initialize.h"

/* These handles/functions live in main.c / CubeMX-generated code */
extern RTC_HandleTypeDef hrtc;
extern void SystemClock_Config(void);

/* Power/wake state (moved from main.c) */
uint32_t g_last_activity_ms = 0U;
volatile uint8_t g_rtc_wakeup_event = 0U;
uint8_t g_woke_from_sleep = 0U;
uint8_t g_periodic_det_pending = 0U;

/* Boot/wake context (needed because SHUTDOWN causes a reset) */
uint8_t g_boot_from_standby = 0U;
uint8_t g_boot_wuf2 = 0U;
uint8_t g_force_rtc_reset = 0U;

/* Shadow sleep mode (because g_sleep_mode remains static in main.c) */
static volatile app_sleep_mode_t s_sleep_mode = APP_SLEEP_MODE_STOP2;

void power_mode_set_sleep_mode(app_sleep_mode_t mode)
{
    s_sleep_mode = mode;
}

/* --- Low-power scheduling helpers (RTC wakeup + STOP2/STOP3/SHUTDOWN) --- */
void app_cancel_rtc_wakeup(void)
{
    /* Stop periodic wakeups while we are awake */
    (void)HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
    __HAL_RTC_WAKEUPTIMER_CLEAR_FLAG(&hrtc, RTC_FLAG_WUTF);
}

void app_schedule_rtc_wakeup(uint32_t seconds)
{
    /* Use 1 Hz ck_spre clock -> seconds resolution, 16-bit range */
    app_cancel_rtc_wakeup();
    (void)HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, seconds, RTC_WAKEUPCLOCK_CK_SPRE_16BITS, APP_RTC_WUT_AUTOCLEAR);
}

void HAL_RTCEx_WakeUpTimerEventCallback(RTC_HandleTypeDef *hrtc_cb)
{
    (void)hrtc_cb;
    g_rtc_wakeup_event = 1U;
}

void app_boot_reason_init(void)
{
    /* Read flags FIRST (before clearing) */
    uint8_t from_standby = (__HAL_PWR_GET_FLAG(PWR_FLAG_SBF) != 0U) ? 1U : 0U;
    uint8_t wuf2         = (__HAL_PWR_GET_FLAG(PWR_WAKEUP_FLAG2) != 0U) ? 1U : 0U;
    uint8_t pinrst       = (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST) != 0U) ? 1U : 0U;

    g_boot_from_standby = from_standby;
    g_boot_wuf2 = wuf2;

    /* Force RTC reset only on external RESET (NRST), NOT on SHUTDOWN/STANDBY wake */
    if ((from_standby == 0U) && (pinrst != 0U)) {
        g_force_rtc_reset = 1U;
    } else {
        g_force_rtc_reset = 0U;
    }

    __HAL_RCC_CLEAR_RESET_FLAGS();

    /* Clear low-power flags so they do not affect subsequent cycles */
    if (from_standby) {
        __HAL_PWR_CLEAR_FLAG(PWR_FLAG_SBF);
    }

    __HAL_PWR_CLEAR_FLAG(PWR_WAKEUP_FLAG1);
    __HAL_PWR_CLEAR_FLAG(PWR_WAKEUP_FLAG2);
    __HAL_PWR_CLEAR_FLAG(PWR_WAKEUP_FLAG3);
    __HAL_PWR_CLEAR_FLAG(PWR_WAKEUP_FLAG4);
    __HAL_PWR_CLEAR_FLAG(PWR_WAKEUP_FLAG5);
    __HAL_PWR_CLEAR_FLAG(PWR_WAKEUP_FLAG6);
    __HAL_PWR_CLEAR_FLAG(PWR_WAKEUP_FLAG7);
    __HAL_PWR_CLEAR_FLAG(PWR_WAKEUP_FLAG8);

    uint8_t wuf7 = (__HAL_PWR_GET_FLAG(PWR_WAKEUP_FLAG7) != 0U) ? 1U : 0U;

    /* SHUTDOWN/STANDBY + WKUP7 => RTC internal wake (WUT/ALARM/TS) */
    if ((from_standby != 0U) && (wuf7 != 0U)) {
        g_periodic_det_pending = 1U;
    }
}

void app_prepare_wkup2_before_sleep(void)
{
    /* USER button on PC13 is WKUP2: wake on LOW level (button press) */
    (void)HAL_PWR_DisableWakeUpPin(PWR_WAKEUP_PIN2_LOW_1);
    __HAL_PWR_CLEAR_FLAG(PWR_WAKEUP_FLAG2);
    (void)HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN2_LOW_1);
}

void app_disable_wkup2_after_wake(void)
{
    (void)HAL_PWR_DisableWakeUpPin(PWR_WAKEUP_PIN2_LOW_1);
    __HAL_PWR_CLEAR_FLAG(PWR_WAKEUP_FLAG2);

    (void)HAL_PWR_DisableWakeUpPin(PWR_WAKEUP_PIN7);
    __HAL_PWR_CLEAR_FLAG(PWR_WAKEUP_FLAG7);
}

void app_enter_power_save(void)
{
    /* Prepare peripherals for lowest current */
    peripheral_Sleep();

    /* Arm periodic wakeup: run detection every APP_PERIODIC_DET_SEC while sleeping */
    app_schedule_rtc_wakeup(APP_PERIODIC_DET_SEC);
    g_rtc_wakeup_event = 0U;
    g_periodic_det_pending = 0U;

    /* USER button (PC13) -> WKUP2 only while sleeping */
    app_prepare_wkup2_before_sleep();

    /* Enter low power */
    HAL_SuspendTick();
    __DSB();
    __ISB();

    if (s_sleep_mode == APP_SLEEP_MODE_STOP2) {
        __HAL_PWR_CLEAR_FLAG(PWR_FLAG_STOPF);
        (void)HAL_PWREx_EnterSTOP2Mode(PWR_STOPENTRY_WFI);
    } else if (s_sleep_mode == APP_SLEEP_MODE_STOP3) {
        __HAL_PWR_CLEAR_FLAG(PWR_FLAG_STOPF);
        (void)HAL_PWREx_EnterSTOP3Mode(PWR_STOPENTRY_WFI);
    } else {
        /* SHUTDOWN never returns (wake causes a reset) */
        HAL_PWR_EnableBkUpAccess();
        HAL_RTCEx_BKUPWrite(&hrtc, APP_RTC_BKP_SLEEP_REG, (uint32_t)s_sleep_mode);

        __HAL_PWR_CLEAR_FLAG(PWR_FLAG_SBF);
        __HAL_PWR_CLEAR_FLAG(PWR_WAKEUP_FLAG2);
        __HAL_PWR_CLEAR_FLAG(PWR_WAKEUP_FLAG7);
        /* WKUP7 (WUSEL=11) routes RTC internal wake events (RTC_WUT/ALRx/TS) */
        (void)HAL_PWR_DisableWakeUpPin(PWR_WAKEUP_PIN7_HIGH_3);
        (void)HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN7_HIGH_3);
        (void)HAL_PWREx_EnterSHUTDOWNMode();
        while (1) {}
    }

    /* We are awake again (STOP2/STOP3) */
    HAL_ResumeTick();
    SystemClock_Config();

    /* Stop periodic wakeups and restore normal GPIO behavior */
    app_cancel_rtc_wakeup();
    app_disable_wkup2_after_wake();

    peripheral_Wake();

    /* Mark wake reason */
    g_periodic_det_pending = (g_rtc_wakeup_event != 0U) ? 1U : 0U;
    g_rtc_wakeup_event = 0U;

    /* First USER press after STOP should be "wake only" */
    g_woke_from_sleep = 1U;

    g_last_activity_ms = HAL_GetTick();

    powerSave = 0; /* exit powerSave loop */
}
