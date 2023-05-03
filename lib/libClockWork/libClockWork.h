#pragma once

#define PIN_D1 5                                       // IN1 on IDUINO L298N H-bridge
#define PIN_D2 4                                       // IN2 on IDUINO L298N H-bridge
#define PIN_D5 14                                      // ENA on IDUINO L298N H-bridge
#define PIN_LED 2                                      // LED

#define TIMER_INTERRUPT_DEBUG       0
#define _TIMERINTERRUPT_LOGLEVEL_   0

#define USING_TIM_DIV1                false           // for shortest and most accurate timer
#define USING_TIM_DIV16               false           // for medium time and medium accurate timer
#define USING_TIM_DIV256              true            // for longest timer but least accurate. Default

#define HW_TIMER_INTERVAL           50L
#define DUTYCYCLES                    6               // HW_TIMER_INTERVAL * DUTYCYCLES defines duty interval
#define TIMER_INTERVAL_60S          60000L
#define TIMER_INTERVAL_FASTF        499L              // roughly 500ms, prime number
#define TIMER_INTERVAL_1000MS       1000L             // 1000ms, prime number
#define TIMER_INTERVAL_3H           10800001L         // roughly 3h for NTP resync, prime number

// flags to communicate to and from ISR via "ISRcom"
#define F_POLARITY               1      // polarity of drive pulse, false: negative, true: positive
#define F_POWER                  2      // power state of drive, false: power off, true: power on
#define F_MINUTE_EN              4      // ISR 60s-period enabled/disabled
#define F_FSTFWD_EN              8      // ISR 500ms-period enabled/disabled
#define F_NTPSYNC               16      // true: system time sync requested
#define F_SEC00                 32      // true: system time second is [00], false: system time second is [01-59]
#define F_RFU1                  64      // RFU1
#define F_RFU2                 128      // RFU2

#include "ESP8266TimerInterrupt.h"
#include "ESP8266_ISR_Timer.hpp"

extern ESP8266_ISR_Timer ISR_Timer;  // declaration of the global variable ISRTimer
extern volatile byte ISRcom;
extern volatile uint ticks;

boolean setupInterrupts();           // start interrupt processing
void setClockHands(int from_hand_h, int from_hand_min, int to_hand_h, int to_hand_min);                                         
boolean syncClockWork();
int minute_steps(int from_h, int from_min, int to_h, int to_min); // calculate the number steps needed between two times
void logISRcom();