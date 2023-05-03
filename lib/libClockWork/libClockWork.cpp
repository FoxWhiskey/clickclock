#include<Arduino.h>
#include<log.h>
#include<libClockWork.h>
#include<TimeLib.h>

ESP8266Timer ITimer;
ESP8266_ISR_Timer ISR_Timer;

extern TimeElements hand;

volatile byte DutyCycles = 0;          //counter to keep track of the duty time
volatile byte ISRcom = 0;
volatile uint ticks = 0;
volatile byte sec00 = 0;
volatile time_t tt_hands = 0;

/**
 *  --- ISR service routines ---
*/

/**
 * @brief ISR for clock hands fast forward
*/
void ISR_FastForward() {
    
    if (ISRcom & F_FSTFWD_EN) {                            // if fast forward enabled
      if (ISRcom & F_POLARITY) {
         digitalWrite(PIN_D1,LOW);                         // Power on, positive polarity
      } else {
         digitalWrite(PIN_D2,LOW);                         // Power on, negative polarity
      }                              
      ISRcom |= F_POWER;                                   // flag Power ON
    }
}

/**
 * @brief ISR for clockwork trigger
*/
void ISR_MinuteTrigger() {

    if (ISRcom & F_MINUTE_EN) {                            // if minute signal enabled
      if (ISRcom & F_POLARITY) {
         digitalWrite(PIN_D1,LOW);                         // Power on, positive polarity
      } else {
         digitalWrite(PIN_D2,LOW);                         // Power on, negative polarity
      }
      tt_hands += 60;                                      // update current clock hand setting                              
      ISRcom |= F_POWER;                                   // flag Power ON
    }

}

/**
 * @brief ISR to trigger seconds
 *
*/
void ISR_SecondTrigger() {

    sec00++;
    sec00 %= byte(60);
    if (sec00 == 0) ISRcom |= F_SEC00;
    else ISRcom &= ~F_SEC00;
    digitalWrite(PIN_LED,sec00 % 2);
}


/**
 * @brief ISR to set F_NTPSYNC
*/
void ISR_NTPsync() {
 
     ISRcom &= F_NTPSYNC;
}

/**
 * @brief the ISR for the hardware timer Timer1.
*/
void IRAM_ATTR TimerHandler()
{
  // check duty cycle counter
  // switch off clockwork drive, when HW_TIMER_INTERVAL * DUTYCYCLES is reached (50ms * 4 = 200ms)

 
   if (ISRcom & F_POWER) DutyCycles++;  // increase DutyCycle counter if ISRs are enabled
   if (DutyCycles > DUTYCYCLES) {                        // if duty time has been reached,
        
        digitalWrite(PIN_D1,HIGH);                        // power off
        digitalWrite(PIN_D2,HIGH);
        ISRcom &= ~F_POWER;                               // flag Power off,
        DutyCycles=0;                                     // reset Duty cycle counter and
        ISRcom ^= F_POLARITY;                             // flag inverse polarity for next cycle....
        
        if (ISRcom & F_FSTFWD_EN) {                       // if we run FASTFORWARD
            ticks--;                                      // decrease tick counter
            if (ticks == 0)  ISRcom &= ~F_FSTFWD_EN;      // if requested number of ticks have been reached, disable FASTFORWARD
            }
    }                                               
    

 //then run the ISR_Timer
  ISR_Timer.run();
}

/**
 * @brief synchronise the minute trigger to system time, so that the ISR triggers on full minutes
 * @return returns false, if unable to get time
*/
boolean sync_ISR_MinuteTrigger() {

    int sec_now = 0;
    time_t epoch = 0;

    if (timeStatus() != timeNotSet) {
        sec_now = second();
        log(DEBUG,__FUNCTION__,"Current second is %02i. Syncing...",sec_now);
        while (sec_now != 0) { 
           delay(200);
           if (timeStatus() != timeNotSet) sec_now = second();
        };
        ISR_Timer.setInterval(TIMER_INTERVAL_60S,ISR_MinuteTrigger);
        ISR_Timer.setInterval(TIMER_INTERVAL_1000MS,ISR_SecondTrigger);
        if (timeStatus() != timeNotSet) { 
           epoch = now();
           log(INFO,__FUNCTION__,"ISR synchronised with system time: %02i:%02i:%02i",hour(epoch),minute(epoch),second(epoch));
         } else return false;
    } else return false;
return true;
}


/**
 * @brief set up Interrupt handling for clockwork operations
*/
boolean setupInterrupts() {
    boolean timerStart = ITimer.attachInterruptInterval(HW_TIMER_INTERVAL*1000,TimerHandler);

    if (timerStart) {
        log(DEBUG,__FUNCTION__,"Starting ITimer");
        if (!sync_ISR_MinuteTrigger()) {
            log(FATAL,__FUNCTION__,"Cannot get system time! OMG!");
            return false;
        };
        ISR_Timer.setInterval(TIMER_INTERVAL_FASTF,ISR_FastForward);
        log(DEBUG,__FUNCTION__,"ISR_FastForward timer started...");
        ISR_Timer.setInterval(TIMER_INTERVAL_3H,ISR_NTPsync);



    } else {
        log(FATAL,__FUNCTION__,"Can't start ITimer. Malfunction!");
    }

return timerStart;
}

/**
 *   --- clock work operations ---
*/

/**
 * @brief turn the clock hands from one time display to another time display.
 * @brief As the absolute postion of the clock hands is unknown to the software, 
 * @brief this is a relative operation. Use this function to align system time to
 * @brief clock work time or to switch to or from DST
 * @param int from_hand_h hour hand source position 
 * @param int from_hand_min minute hand source position
 * @param int to_hand_h hour hand destination position
 * @param int to_hand_min minute hand destination position
 * @return void
*/
void setClockHands(int from_hand_h, int from_hand_min, int to_hand_h, int to_hand_min) {

   ticks = minute_steps(from_hand_h,from_hand_min,to_hand_h,to_hand_min);
   log(INFO,__FUNCTION__,"Setting clock hands to %02i:%02i (%i ticks)",to_hand_h,to_hand_min,ticks);
   ISRcom &= ~F_MINUTE_EN;
   ISRcom |= F_FSTFWD_EN;
}                                        
/**
 * @brief sync the clockwork to system time.
 * @brief Driving the hands may require more than 60s setting time,
 * @brief so an offset to the requested setting time needs to be considered.
 * @brief Setting of hands starts at second [00] of the minute to ensure maximum setting time!
 * @return true, if function is able to get system time, false otherwise
*/
boolean syncClockWork() {

    TimeElements systime;
    int offset = 0;
    int clicks;
    
    log(INFO,__FUNCTION__,"Waiting for setting window...");
    while (ISRcom & ~F_SEC00) delay(250);
    if (timeStatus() != timeNotSet) {
        breakTime(now(),systime);
    } else {
        log(FATAL,__FUNCTION__,"Cannot get system time! Ooops!");
        return false;
    }

    clicks = minute_steps(hand.Hour,hand.Minute,systime.Hour,systime.Hour);
    if (clicks*TIMER_INTERVAL_FASTF/1000 > 55) {
        offset = clicks*TIMER_INTERVAL_FASTF/60000; 
        log(DEBUG,__FUNCTION__,"Estimated hand setting time is %is. Offset required!",clicks*TIMER_INTERVAL_FASTF/1000+offset);
    }
    setClockHands(hand.Hour,hand.Minute,systime.Hour,systime.Minute+offset);
    while (ISRcom & F_FSTFWD_EN) delay(100);
    hand = systime;
    tt_hands = makeTime(hand);
    ISRcom |= F_MINUTE_EN;

    return true;
};


/**
 * @brief calculates the number of steps needed for the clockwork to move the hands between two time displays
 * @param int from_h the indicated hour [0-23]
 * @param int from_min the indicated minute [0-59]
 * @param int to_h the target hour [0-23]
 * @param int to_min the target minute [0-59]
 * @result int the number of clockwork clicks needed to set target display
*/
int minute_steps(int from_h, int from_min, int to_h, int to_min) {

    int from_mins_delta = (from_h % 12) * 60 + from_min;
    int to_mins_delta  =  (to_h % 12) * 60 + to_min;

    if ((to_mins_delta - from_mins_delta) < 0) return 720 + to_mins_delta - from_mins_delta;
    else return to_mins_delta - from_mins_delta;
}

void logISRcom() {
     
          log(DEBUG,__FUNCTION__," %s | %s | %s | %s | %s | %s",ISRcom & F_POLARITY ? "F_POLARITY" : "          ",ISRcom & F_POWER ? "F_POWER" : "       ",ISRcom & F_MINUTE_EN ? "F_MINUTE_EN" : "           ",ISRcom & F_FSTFWD_EN ? "F_FSTFWD_EN" : "           ",ISRcom & F_NTPSYNC ? "F_NTPSYNC" : "         ",ISRcom & F_SEC00 ? "F_SEC00" : "      ");
}