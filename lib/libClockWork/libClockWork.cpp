#include<Arduino.h>
#include<log.h>
#include<TimeLib.h>
#include<libClockWork.h>

ESP8266Timer ITimer;
ESP8266_ISR_Timer ISR_Timer;

extern TimeElements hand;

volatile byte DutyCycles = 0;          //counter to keep track of the duty time
volatile byte Btn1Cntr = 0;            //debounce counter button 1
volatile byte Btn2Cntr = 0;            //debounce counter button 2
//volatile byte ISRcom = 0|F_POLARITY;   // Polarity of first pulse
volatile byte ISRcom = 0;                // Polarity of first pulse
volatile byte ISRbtn = 0;                // state of buttons
volatile uint ticks = 0;
volatile byte sec00 = 0;
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

    sec00++;
    sec00 %= byte(60);
    if (sec00 == 0) ISRcom |= F_SEC00;
    else ISRcom &= ~F_SEC00;
    digitalWrite(PIN_LED,sec00 % 2);
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

 // check buttons (distinguish between long and short press)

    if (!digitalRead(PIN_D6)) {                                 // if button pressed,
        if (Btn1Cntr++ > BUTTON_LONG) ISRbtn |= F_BUTN1LONG;    // set F_BUTNxLONG, as soon as BUTTON_LONG cycles have been counted
    }
    else {
        if (ISRbtn & F_BUTN1LONG) {                             // if button not pressed and  F_BUTN1LONG set
            Btn1Cntr = 0;                                       // signal "button release"
            ISRbtn &= ~ F_BUTN1LONG;
        }
        if (Btn1Cntr > DEBOUNCE_INT) {                          // if button is pressed and DEBOUNCE_INT have been counted
            Btn1Cntr--;                                         // keep flag set 
            ISRbtn |= F_BUTN1;
        }
        else {
           Btn1Cntr = 0;
           ISRbtn &= ~F_BUTN1;
        };
    };

    if (!digitalRead(PIN_D7)) {
        if (Btn2Cntr++ > BUTTON_LONG) ISRbtn |= F_BUTN2LONG;
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
           Btn2Cntr = 0;
           ISRbtn &= ~F_BUTN2;
        };
    };

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

    time_t systime;
    int offset = 0;
    int clicks;
    int prevMinute = 0;
    
    log(INFO,__FUNCTION__,"Waiting for setting window...");
    while (!(ISRcom & F_SEC00)) delay(100);          // on F_SEC00,
    if (timeStatus() != timeNotSet) {
        systime = now();                             // get actual system time
    } else {
        log(FATAL,__FUNCTION__,"Cannot get system time! Ooops!");
        return false;
    }

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
        setClockHands(hour(comp_time),minute(comp_time),hour(comp_time+60),minute(comp_time+60));  // set the clockwork + 1min
        ISRcom |= F_CM_SET;                                                                     // flag compensate minute is set
    }
}

/**
 * @brief modifies a given time stamp, so that the new value represents the hand position of the clockwork drive
 * @brief that is closest and _prior_ to the actual time.
 * @brief The function expects the seconds-value set to zero and default minute hand position set correctly
 * @brief Needed once during system boot to set "tt_hands"
 * @param time_t time stamp to represent current hand position
 * @param int default hour hand position 
*/ 
void transformDHP(time_t &handstamp, int defaultHour) {
 
 int current_hour = hour(handstamp);
 int delta_hour = 0;
 
 defaultHour += 12;                                        // set the hour to max possible value
 delta_hour = defaultHour - current_hour;
 while (delta_hour > 0) delta_hour -= 12;                     // make sure that time difference is negative

 handstamp += SECS_PER_HOUR * delta_hour;                  // and set handstamp to the closest time *prior* current time
 log(DEBUG,__FUNCTION__,"Hand position set: tt_hands=%lld",handstamp);
 log(INFO,__FUNCTION__,"fictional clockwork date is [%02i.%02i.%02i] %02i:%02i",day(handstamp),month(handstamp),year(handstamp),hour(handstamp),minute(handstamp));

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
     
          log(DEBUG,__FUNCTION__," %s | %s | %s | %s | %s | %s | %s | %s | %s | %s |",ISRcom & F_POLARITY ? "F_POLARITY" : "          ",ISRcom & F_POWER ? "F_POWER" : "       ",ISRcom & F_MINUTE_EN ? "F_MINUTE_EN" : "           ",ISRcom & F_FSTFWD_EN ? "F_FSTFWD_EN" : "           ",ISRcom & F_SEC00 ? "F_SEC00" : "       ",ISRcom & F_CM_SET ? "F_CM_SET" : "        ",ISRbtn & F_BUTN1 ? "F_BUTN1" : "       ",ISRbtn & F_BUTN2 ? "F_BUTN2" : "       ",ISRbtn & F_BUTN1LONG ? "F_BUTN1LONG" : "           ",ISRbtn & F_BUTN2LONG ? "F_BUTN2LONG" : "           ");
}