#include<Arduino.h>
#include<log.h>
#include<TimeLib.h>
#include<libClockWork.h>

ESP8266Timer ITimer;
ESP8266_ISR_Timer ISR_Timer;
int minute_id = 0;                      // the id of ISR_Minute_Trigger
int sec_id = 0;                         // the id of ISR_Second_Trigger
uint16 virt_ms = 1000;                  // value representing a true millisecond based on multiple microseconds (virtual millisecond)

extern TimeElements hand;

volatile byte DutyCycles = 0;          //counter to keep track of the duty time
volatile byte Btn1Cntr = 0;            //debounce counter button 1
volatile byte Btn2Cntr = 0;            //debounce counter button 2
//volatile byte ISRcom = 0|F_POLARITY;   // Polarity of first pulse
volatile byte ISRcom = 0;                // Polarity of first pulse
volatile byte ISRbtn = 0;                // state of buttons
volatile uint ticks = 0;
volatile byte sec00 = 0;
volatile byte LEDshift = 0;           // shift register variable to control LED
volatile time_t tt_hands = 0;

/**
 *  --- ISR service routines ---
*/

/**
 * @brief ISR for clock hands fast forward
*/
void IRAM_ATTR ISR_FastForward() {
    
    if (ISRcom & F_FSTFWD_EN) {                            // if fast forward enabled
      if (ISRcom & F_POLARITY) {
         digitalWrite(PIN_D1,LOW);                         // Power on, positive polarity
      } else {
         digitalWrite(PIN_D2,LOW);                         // Power on, negative polarity
      }
      tt_hands += 60;                                      // update current clock hand setting // bugfix issue #9                              
      ISRcom |= F_POWER;                                   // flag Power ON
    }
}

/**
 * @brief ISR for clockwork trigger
*/
void IRAM_ATTR ISR_MinuteTrigger() {

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
void IRAM_ATTR ISR_SecondTrigger() {

    if (Btn1Cntr+Btn2Cntr == 0) LEDshift = B00000001; // indicate elapsed second only if no button is pressed
                                                      // results in 50ms flash each second (at standard HW_TIMER_INTERVAL)
    sec00++;
    sec00 %= byte(60);
    if (sec00 == 0) ISRcom |= F_SEC00;
    else ISRcom &= ~F_SEC00;
    
}

/**
 * @brief the ISR for the hardware timer Timer1.
*/
void IRAM_ATTR TimerHandler()
{
  // handle LED activity - implementing issue #4
  // LED activity depends on bit pattern of LEDshift.

    digitalWrite(LED_BUILTIN,~LEDshift & 1);    // invert bit 0 and write it to LED control register
    LEDshift >>= 1;                             // shift LED register to the right by one 

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
            if (ticks == 0)  {                            // if requested number of ticks have been reached, 
                ISRcom &= ~F_FSTFWD_EN;                   // disable FASTFORWARD,
                ISRcom |= F_MINUTE_EN;                    // enable minute-trigger
            }
        }
    }

 // check buttons (distinguish between long and short press)
 // and flash LED accordingly (> v1.1.3)

    if (!digitalRead(PIN_D6)) {                                 // if button pressed,
        if (Btn1Cntr++ > BUTTON_LONG) {                         // and BUTTON_LONG cycles have been counted,
            if (!(ISRbtn & F_BUTN1LONG)) LEDshift = B00110011;  // load LED-indication pattern into LED-register (only on first pass)
            ISRbtn |= F_BUTN1LONG;                              // and set F_BUTNxLONG
        }
    }
    else {
        if (ISRbtn & F_BUTN1LONG) {                             // if button not pressed and  F_BUTN1LONG set
            Btn1Cntr = 0;                                       // signal "button release" (long press)
            ISRbtn &= ~ F_BUTN1LONG;
        }
        if (Btn1Cntr > DEBOUNCE_INT) {                          // if button is pressed and DEBOUNCE_INT have been counted
            Btn1Cntr--;                                         // keep flag set 
            ISRbtn |= F_BUTN1;
        }
        else {                                                  // if releasing button after "short press"
           if (Btn1Cntr > 0) LEDshift = B00111111;              // load LED-indication pattern into LED-register (only on first pass)
           Btn1Cntr = 0;
           ISRbtn &= ~F_BUTN1;                                  // and signal "button release" (short press)
        };
    };

    if (!digitalRead(PIN_D7)) {
        if (Btn2Cntr++ > BUTTON_LONG) {
            if (!(ISRbtn & F_BUTN2LONG)) LEDshift = B00110011; 
             ISRbtn |= F_BUTN2LONG;
        }
    }
    else {
        if (ISRbtn & F_BUTN2LONG) {
            Btn2Cntr = 0;
            ISRbtn &= ~ F_BUTN2LONG;
        }
        if (Btn2Cntr > DEBOUNCE_INT) {
            Btn2Cntr--;
            ISRbtn |= F_BUTN2;
        }
        else {
           if (Btn2Cntr > 0) LEDshift = B00111111;
           Btn2Cntr = 0;
           ISRbtn &= ~F_BUTN2;
        };
    };

 //then run the ISR_Timer
  ISR_Timer.run();
}

/**
 * @brief synchronise the minute trigger to system time, so that the ISR triggers on full minutes
 * @return always true
*/
boolean sync_ISR_MinuteTrigger() {

   time_t epoch = now();

    if (timeStatus() == timeNeedsSync) log(WARN,__FUNCTION__,"System time needs NTP-resync...");
    log(INFO,__FUNCTION__,"Current second is %02i. Syncing...",second(epoch));
    while (second(epoch) != 0) { 
        delay(200);
        epoch = now();
    };
    sec00 = 0;
    minute_id = ISR_Timer.setInterval(TIMER_INTERVAL_60S,ISR_MinuteTrigger);
    sec_id =    ISR_Timer.setInterval(TIMER_INTERVAL_1000MS,ISR_SecondTrigger);
    ISRcom |= (F_INTRUN + F_SEC00);   // at this point in time we have synchronised both ISRs to full minute (second=0) of the hour. Flag ISR running and second==0
    log(INFO,__FUNCTION__,"ISRs synchronised with system time: %02i:%02i:%02i",hour(epoch),minute(epoch),second(epoch));
    
return true;
}


/**
 * @brief set up Interrupt handling for clockwork operations
*/
boolean setupInterrupts() {
    boolean timerStart = ITimer.attachInterruptInterval(HW_TIMER_INTERVAL*virt_ms,TimerHandler);

    if (timerStart) {
        log(DEBUG,__FUNCTION__,"Starting ITimer, virtual ms is %iµs",virt_ms);
        sync_ISR_MinuteTrigger();
        ISR_Timer.setInterval(TIMER_INTERVAL_FASTF,ISR_FastForward);
        log(DEBUG,__FUNCTION__,"ISR_FastForward timer started...");

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
   log(INFO,__FUNCTION__,"Setting clock hands from %02i:%02i to %02i:%02i (%i ticks)",from_hand_h,from_hand_min,to_hand_h,to_hand_min,ticks);
   if (ticks > 0) {
      ISRcom &= ~F_MINUTE_EN;
      ISRcom |= F_FSTFWD_EN;
   };
}                                        
/**
 * @brief sync the clockwork to system time.
 * @brief Driving the hands may require more than 60s setting time,
 * @brief so an offset to the requested setting time needs to be considered.
 * @brief Setting of hands starts at second [00] of the minute to ensure maximum setting time!
 * @return always true
*/
boolean syncClockWork() {

    time_t systime;
    int offset = 0;
    int clicks;
    int prevMinute = 0;
    
    log(INFO,__FUNCTION__,"Waiting for setting window...");
    while (!(ISRcom & F_SEC00)) delay(100);          // on F_SEC00,
    if (timeStatus() == timeNeedsSync) log(WARN,__FUNCTION__,"System time needs NTP-resync...");
    systime = now();
    clicks = minute_steps(hand.Hour,hand.Minute,hour(systime),minute(systime));
    if (clicks*int(TIMER_INTERVAL_FASTF)/1000 > 55) {
        offset = clicks*int(TIMER_INTERVAL_FASTF)/1000;  // bugfix issue #7
        log(DEBUG,__FUNCTION__,"Hands @ %02i:%02i. Estimated hand setting time is %is. Offset required!",hand.Hour,hand.Minute,offset);
    }
    setClockHands(hand.Hour,hand.Minute,hour(systime+time_t(offset)),minute(systime+time_t(offset)));  // bugfix issue #7
    while (ISRcom & F_FSTFWD_EN) {
        delay(100);
        if (minute(tt_hands) != prevMinute)
        {
          log(DEBUG,__FUNCTION__,"hand position: %02i:%02i",hour2clockface(hour(tt_hands)),minute(tt_hands));
          prevMinute = minute(tt_hands);
        }
    }
    //ISRcom |= F_MINUTE_EN; --> moved into TimerHandler (fast forward handling)

    return true;
};

/**
 * @brief Triggers a resynchronisation of interrupt service routines to NTP time due to time drift of processor clock
 * @brief when MAX_NTP_DEVIATION has been reached, stop and resync ISRs to NTP-time
 * @brief must only be called when deviation is below 60secs (thus 1 minute/click)
 * @param int negative value to indicate clockwork is early, positive value to indicate clockwork is late (NTP-time - clockwork-time)
 * @param ulong millis-value on calling this function
 * @param ulong& millis value when interrupts have been resynced last time
 * @return always true
*/
int16_t reSyncClockWork(int lag,u_long millis_now,u_long& millis_start) {
    
    int16_t _drift;
    
    log(WARN,__FUNCTION__,"Clockwork is %s",lag > 0 ? "late" : "early.");
    log(DEBUG,__FUNCTION__,"millis_start: %lums millis_now: %lums lag: %is",millis_start,millis_now,lag);

    /* the return value of millis() provides sufficient storage for a time period of roughly 50 days (based on milliseconds).
       When the maximum value has been reached, millis() starts over and returns values starting at 0. To calculate the proper
       time span between two resync-intervals, a potential overrun has to be considered and taken care of.
       The size of 'ulong' also determines the smallest drift correction value of MAX_NTP_DEVIATION/50 days.
       An oscillator time drift smaller than this value cannot be compensated for without more complex code.
    */
    if ((millis_now - millis_start) > 0) {                              // no overrun !
        _drift = 1000000L*lag/((millis_now-millis_start)/60000L);
    } else {                                                            // overrun has occured
        _drift = 1000000L*lag/((0xFFFFFFFF-millis_start+millis_now)/60000L);
    }
    virt_ms = (virt_ms+(1000-_drift))/2;   // approximation by arithmetic mean
    log(INFO,__FUNCTION__,"Calculated drift is %iµs, new virtual ms will be set to %iµs",_drift,virt_ms);
    ISR_Timer.deleteTimer(sec_id);
    ISR_Timer.deleteTimer(minute_id);
    ISRcom &= ~F_INTRUN;                   // flag no interrupt service routines are running
    ITimer.detachInterrupt();              // stop entire interrupt handling
    //sync_ISR_MinuteTrigger();            // re-sync the ISR routines to NTP time
    setupInterrupts();                     // restart interrupt handling
    millis_start = millis();               // reset millis since last resync

    // if clockwork is late, drive clockwork by a single minute
    if (lag > 0) setClockHands(hour(tt_hands),minute(tt_hands),hour(tt_hands+SECS_PER_MIN),minute(tt_hands+SECS_PER_MIN));
    // else do nothing (ISR sync is sufficient)
    ISRcom |= F_MINUTE_EN;   // enable clockwork
    
    return _drift;   
};



/**
 * @brief calculates the number of steps needed for the clockwork to move the hands between two time displays
 * @param int from_h the indicated hour [0-23]
 * @param int from_min the indicated minute [0-59]
 * @param int to_h the target hour [0-23]
 * @param[in] int to_min the target minute [0-59]
 * @result int the number of clockwork clicks needed to set target display
*/
int minute_steps(int from_h, int from_min, int to_h, int to_min) {

    int from_mins_delta = (from_h % 12) * 60 + from_min;
    int to_mins_delta  =  (to_h % 12) * 60 + to_min;

    if ((to_mins_delta - from_mins_delta) < 0) return 720 + to_mins_delta - from_mins_delta;
    else return to_mins_delta - from_mins_delta;
}

/**
 * @brief Sets the compensation minute, if requested by user.
 * @brief On system start, no information is available on the required polarity of the first pulse. 
 * @brief If the wrong pulse is generated, the displayed time lacks a minute (clock is late).
 * @brief The user can compensate the missing pulse by pressing F_BUTN1.
*/
void CompensateMinute() {
    
    time_t comp_time;
    if (!(ISRcom & F_CM_SET)) 
    {
        comp_time = tt_hands;
        tt_hands -= 60;  // bugfix issue 10
        log(INFO,__FUNCTION__,"! Compensation minute set !");
        setClockHands(hour(tt_hands),minute(tt_hands),hour(tt_hands+60),minute(tt_hands+60));  // set the clockwork + 1min
        ISRcom |= F_CM_SET;                                                                     // flag compensate minute is set
    }
}

/**
 * @brief modifies a given time stamp, so that the new value represents the hand position of the clockwork drive
 * @brief that is closest and _prior_ to the actual time.
 * @brief The function expects the seconds-value set to zero and default minute hand position set correctly
 * @brief Needed once during system boot to set "tt_hands"
 * @param int default hour hand position 
*/ 
void transformDHP(int defaultHour) {
 
 int current_hour = hour(tt_hands);
 int delta_hour = 0;
 
 defaultHour += 12;                                        // set the hour to max possible value
 delta_hour = defaultHour - current_hour;
 while (delta_hour > 0) delta_hour -= 12;                     // make sure that time difference is negative

 tt_hands += SECS_PER_HOUR * delta_hour;                  // and set handstamp to the closest time *prior* current time
 log(DEBUG,__FUNCTION__,"Hand position set: tt_hands=%lld",tt_hands);
 log(INFO,__FUNCTION__,"fictional clockwork date is [%02i.%02i.%02i] %02i:%02i",day(tt_hands),month(tt_hands),year(tt_hands),hour(tt_hands),minute(tt_hands));

}

/**
 * @brief transforms the current hour into a clock-face equivalent
 * @param int an hour value between 0 and 23
 * @return int corresponding hour value between 1 and 12
*/
int hour2clockface(int hour_24) {
    
    if ((hour_24 % 12) == 0) return int(12);
    else return int(hour_24 % 12);
}

/**
 * @brief logger function to display state of ISRcom and ISRbtn
*/
void logISR() {
     
          log(DEBUG,__FUNCTION__," %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s |",ISRcom & F_INTRUN ? "F_INTRUN" : "        ",ISRcom & F_POLARITY ? "F_POLARITY" : "          ",ISRcom & F_POWER ? "F_POWER" : "       ",ISRcom & F_MINUTE_EN ? "F_MINUTE_EN" : "           ",ISRcom & F_FSTFWD_EN ? "F_FSTFWD_EN" : "           ",ISRcom & F_SEC00 ? "F_SEC00" : "       ",ISRcom & F_CM_SET ? "F_CM_SET" : "        ",ISRbtn & F_BUTN1 ? "F_BUTN1" : "       ",ISRbtn & F_BUTN2 ? "F_BUTN2" : "       ",ISRbtn & F_BUTN1LONG ? "F_BUTN1LONG" : "           ",ISRbtn & F_BUTN2LONG ? "F_BUTN2LONG" : "           ");
}