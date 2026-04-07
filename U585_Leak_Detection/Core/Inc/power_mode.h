#ifndef INC_POWER_MODE_H_
#define INC_POWER_MODE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

/* RTC WakeupTimer AutoClear compatibility (varies by HAL version) */
#ifndef APP_RTC_WUT_AUTOCLEAR
  #if defined(RTC_WAKEUPTIMER_AUTOCLEAR)
    #define APP_RTC_WUT_AUTOCLEAR RTC_WAKEUPTIMER_AUTOCLEAR
  #elif defined(RTC_WAKEUPTIMER_AUTOCLR)
    #define APP_RTC_WUT_AUTOCLEAR RTC_WAKEUPTIMER_AUTOCLR
  #elif defined(RTC_WAKEUPTIMER_CLEAR)
    #define APP_RTC_WUT_AUTOCLEAR RTC_WAKEUPTIMER_CLEAR
  #else
    #define APP_RTC_WUT_AUTOCLEAR 0U
  #endif
#endif

#define APP_IDLE_TIMEOUT_MS        (10000U)  // 30 s idle -> sleep
#define APP_PERIODIC_DET_SEC       (10U)    // 3 min periodic detection while asleep

/* RTC Backup registers: keep RTC calendar across SHUTDOWN and normal resets */
#define APP_RTC_BKP_MAGIC      (0xA55A5AA5u)
#define APP_RTC_BKP_MAGIC_REG  RTC_BKP_DR0
#define APP_RTC_BKP_SLEEP_REG  RTC_BKP_DR1

typedef enum {
    APP_SLEEP_MODE_STOP2 = 0,
    APP_SLEEP_MODE_STOP3 = 1,
    APP_SLEEP_MODE_SHUTDOWN = 2,
} app_sleep_mode_t;

/* NOTE: g_sleep_mode stays static in main.c (per your requirement).
 * So power_mode.c uses an internal shadow. Call power_mode_set_sleep_mode()
 * before app_enter_power_save().
 */
void power_mode_set_sleep_mode(app_sleep_mode_t mode);

extern uint32_t g_last_activity_ms;
extern volatile uint8_t g_rtc_wakeup_event;
extern uint8_t g_woke_from_sleep;
extern uint8_t g_periodic_det_pending;

extern uint8_t g_boot_from_standby;
extern uint8_t g_boot_wuf2;
extern uint8_t g_force_rtc_reset;

void app_schedule_rtc_wakeup(uint32_t seconds);
void app_cancel_rtc_wakeup(void);
void app_enter_power_save(void);
void app_boot_reason_init(void);
void app_prepare_wkup2_before_sleep(void);
void app_disable_wkup2_after_wake(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_POWER_MODE_H_ */
