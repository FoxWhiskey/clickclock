#pragma once

#define PIN_D1 5                                       // IN1 on IDUINO L298N H-bridge
#define PIN_D2 4                                       // IN2 on IDUINO L298N H-bridge
#define PIN_D5 14                                      // ENA on IDUINO L298N H-bridge
#define PIN_LED 2                                      // LED
#define PIN_D6 12                                      // button 1
#define PIN_D7 13                                      // button 2

#define TIMER_INTERRUPT_DEBUG       0
#define _TIMERINTERRUPT_LOGLEVEL_   0

#define USING_TIM_DIV1                true           // for shortest and most accurate timer
#define USING_TIM_DIV16               false           // for medium time and medium accurate timer
#define USING_TIM_DIV256              false            // for longest timer but least accurate. Default

#define HW_TIMER_INTERVAL           50L               // ms
#define DUTYCYCLES                    3               // HW_TIMER_INTERVAL * DUTYCYCLES defines duty interval
#define DEBOUNCE_INT                  5               // HW_TIMER_INTERVAL * DEBOUNCE_INT defines debounce interval for buttons 1 & 2
#define BUTTON_LONG                  15               // HW_TIMER_INTERVAL * BUTTON_LONG defines time intervall for a long button press
#define TIMER_INTERVAL_60S          60000L
#define TIMER_INTERVAL_FASTF        249L              // roughly 249ms, prime number
#define TIMER_INTERVAL_1000MS       1000L             // 1000ms
#define MAX_NTP_DEVIATION             2               // maximum deviation between NTP-time and clockwork time

#define HOUR_HAND_DEFAULT 6                            // the default position of hour hand
#define  MIN_HAND_DEFAULT 23                           // the default position of minute hand

// flags to communicate to and from ISR via "ISRcom"
#define F_POLARITY               1      // polarity of drive pulse, false: negative, true: positive
#define F_POWER                  2      // power state of drive, false: power off, true: power on
#define F_MINUTE_EN              4      // ISR 60s-period enabled/disabled
#define F_FSTFWD_EN              8      // ISR 500ms-period enabled/disabled
#define F_SEC00                 16      // true: system time second is [00], false: system time second is [01-59]
#define F_CM_SET                32      // true: compensation minute set, false: not set      // issue #8
#define F_RFU1                  64      // RFU1
#define F_INTRUN               128      // true: software timer interrupts are set up and running, false: minute/second timers not running

// flags to signal button state
#define F_BUTN1                  1      // Button 1 pressed short (debounced)
#define F_BUTN1LONG              2      // Button 1 pressed long (debounced)
#define F_BUTN2                  4      // Button 2 pressed short (debounced)
#define F_BUTN2LONG              8      // Button 2 pressed long (debounced)

#include "ESP8266TimerInterrupt.h"
#include "ESP8266_ISR_Timer.hpp"

extern ESP8266_ISR_Timer ISR_Timer;  // declaration of the global variable ISRTimer
extern volatile byte ISRcom;
extern volatile byte ISRbtn;
extern volatile uint ticks;
extern volatile time_t tt_hands;

boolean setupInterrupts();           // start interrupt processing
void setClockHands(int from_hand_h, int from_hand_min, int to_hand_h, int to_hand_min);                                         
boolean syncClockWork();
boolean reSyncClockWork(int lag);
int minute_steps(int from_h, int from_min, int to_h, int to_min); // calculate the number steps needed between two times
void CompensateMinute();
void transformDHP(int defaultHour);
int  hour2clockface(int hour_24);
void logISR();