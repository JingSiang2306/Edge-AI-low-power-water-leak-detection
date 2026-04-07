/*
 * peripheral_Initialize.h
 *
 *  Created on: Nov 1, 2025
 *      Author: FBBC
 */

#ifndef INC_PERIPHERAL_INITIALIZE_H_
#define INC_PERIPHERAL_INITIALIZE_H_

#include "main.h"

void peripheral_Init(void);
void peripheral_Wake(void);
void peripheral_Sleep(void);
void peripheral_Testing(void);
void powerSaving(void);
void LED_Blink(int);

// Globals
extern int periTest;     // set to 1 initially inside the .c
extern int powerSave;    // set to 0 initially inside the .c
extern volatile uint8_t user_button_event;
uint8_t user_button_wait_long_press(uint32_t threshold_ms);

#endif /* INC_PERIPHERAL_INITIALIZE_H_ */
