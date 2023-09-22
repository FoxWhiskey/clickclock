# CLICKCLOCK
```
 ██████╗██╗     ██╗ ██████╗██╗  ██╗     ██████╗██╗      ██████╗  ██████╗██╗  ██╗
██╔════╝██║     ██║██╔════╝██║ ██╔╝    ██╔════╝██║     ██╔═══██╗██╔════╝██║ ██╔╝
██║     ██║     ██║██║     █████╔╝     ██║     ██║     ██║   ██║██║     █████╔╝ 
██║     ██║     ██║██║     ██╔═██╗     ██║     ██║     ██║   ██║██║     ██╔═██╗ 
╚██████╗███████╗██║╚██████╗██║  ██╗    ╚██████╗███████╗╚██████╔╝╚██████╗██║  ██╗
 ╚═════╝╚══════╝╚═╝ ╚═════╝╚═╝  ╚═╝     ╚═════╝╚══════╝ ╚═════╝  ╚═════╝╚═╝  ╚═╝
                                                            
 A slave clock implementation for ESP8266 by Bernhard Damian 2023.
    for Johannes and Helene
```
## What is a slave clock?
Slave clocks are time pieces with a simple electrical clockwork to indicate the current time based on electrical impulses generated by an external timing circuit. The timing circuit is usally capable of driving several clocks connected in parallel. That way, time pieces can show the exact time, without the need of a time synchronising mechanism.
Most clocks in public places (train stations, streets, parks, squares) are implemented based on this concept.
## How to trigger a slave clockwork?
For a slave clockwork to turn its hands (or flip a digit platelet), a simple electrical pulse is needed, that changes its polarity each duty cycle. Most clockworks demand a 24V DC pulse with a duty cycle of 300ms - sometimes longer, sometimes shorter, depending on the particular clockwork.  12V variants are also available, but less common.
## What hardware is needed?
* any ESP8266 based microcontroller, eg. the [LOLIN D1 mini][1]
* a driver circuit to provide the required 12V/24V pulses. Any stepper motor interface will do. I used the [IDUINO DEBO DRV3][2] based on the (in?)famous L298N. Unfortunately this board has a design flaw and cannot be operated at voltages greater than 18V (although stated so). See [tech note](#tech-notes) below.
* a slave clock, based on 24V or 12V
## What are the features of `clickclock`
* `clickclock` fetches the actual time over NTP and Wifi and sets the clockwork  hands accordingly
* electrical impulses for the clockwork drive are generated on ESSP8266 timer1 interrupts.
* the minute-based timer interrupt is synchronised to the first second of the (NTP-)minute. The minute hand is moved within the first 100ms of the minute. If the ISR is synchronised to NTP, the onboard LED indicates each second (1Hz flash, at a duty cycle of 50ms).
* a basic mechanism to make the current hand position of the clockwork known to the software. Without knowledge of the hand position the correct time cannot be set automatically.
* correct clockwork display by setting a _compensation minute_ (see below).
* _fun with clocks_ - functions to help kids reading analogue clockworks (yet to be implemented)

## Software Setup
### `NTP/wificonfig.h`
Location for several system default constants such as Wifi credentials and other relevant data. All definitions in this file will be used by the _default constructor_ of `systemdata`-class, and can be altered with the `systemdata::collect()`-property in software. Required definitions are:
* `HOSTNAME           `: hostname of system used when requesting IP-address
* `SSID               `: the network SSID to connect to
* `PASSWD             `: optional network password
* `TIMEZONE           `: time zone of device (signed integer to specify hour offset to UTC, eg. 1 for Berlin/Paris)
* `NTPSERVER          `: the NTP-server to poll
* `NTPINTERVAL        `: interval in seconds to synchronise system time to NTP-time
* `HOUR_HAND_DEFAULT  `: inital position of the clockwork's hour hand at boot.
* `MINUTE_HOUR_DEFAULT`: initial position of the clockwork's minute hand at boot
* `DEFAULTLEVEL       `: debug level of serial console, either of `{DEBUG,INFO,WARN,FATAL}`   

Note, that as of `v1.2.1` the current clockwork setting will be saved in system EEPROM on long press of `BUTN_2`. The system can be powered off safely and will be able to set the correct time (connected to the same clockwork) without recompiling the code.
### `libClockWork/libClockWork.h`
Default pin assignments and timer interrupt relevant settings.

## Hardware setup
Provided the DEBO DRV3 developer board is used, find the default connections from the diagram below. Default pins are defined in `libClockWork.h`.   
```
BUTTON       ESP8266       DEBO DRV3    CLOCKWORK

                D1    <-->   IN1
                D2    <-->   IN2
                D5    <-->   ENA
BUTN_1  <-->    D6
BUTN_2  <-->    D7
                             MOTORA 1  <-->  1
                             MOTORA 2  <-->  2
                5V    <-->   5V
BUTN_x  <-->    GND   <-->   GND
                             VMS (24VDC)  
```

## Software versions
### v1.0.x
The the very first version of `clickclock`.   
* The clockwork is set to NTP automatically, however the intial clock hand position has to be made known to `clickclock` by defining the initial position in `main.cpp`.   
* a couple of bugfixes, most importantly a bug introduced by [khoih-prog/ESP8266TimerInterrupt][3] due to an incomplete library documentation - see [tech note](#tech-notes) below.
* ISR-routines to get the (debounced) states of two bush buttons connected to the ESP.
### v1.1.x
* detect mismatch between system time (NTP) and indicated clockwork time. Needed at least twice a year when DST is de-/activated.
* the code also detects _time drift_ caused by cpu-clock instabilities (temperature, bad designs, etc.).   
If NTP time differs more than 2 seconds from clockwork time, the interrupt service routines will be resynchronised. See [tech note](#tech-notes) below.
* Trigger different functions when push buttons are short pressed or long pressed
* A _short press_ is acknowledged with a 300ms LED indication, a _long press_ with two 100ms flashes (standard `HW_TIMER_INTERVAL` of 50ms provided)
* `BUTN_1` (long press) sets a compensation minute.   
When the system is started, no information is available on the polarity of the last electrical pulse. When setting the clockwork to the current time, there is a 50% chance that the first pulse is sent with an unmatched polarity and the clockwork does not move the hands with the first pulse. As a consequence, the time display is set one minute late. So a manual input is required to compensate for the late time display. A compensation minute can only be set once.
### v1.2.x
* Append 2 last bytes of MAC-address to hostname by default to make devices distinguishable in the same WiFi-network.
* Make important runtime data permanent in the system's EEPROM, most importantly flags from the `ISRcom`-register (polarity of the next pulse (`F_POLARITY`), state of compensation minute (`F_CM_SET`)) and the present clockwork hand setting (`tt_hands`). In order not to wear out the board's EEPROM-memory to fast, system state will only be stored upon user request (`BUTN_2`- long press). When system state is saved, the **clockwork will be halted**, so that the system can be switched off safely.   
On a reboot, connected to the same clockwork, the system will be able to resync to NTP-time without recompilation or any other user action.
* As of `v1.2.1`, due to the use EEPROM memory as initialisation data source, the code has undergone a greater rewrite: default constants will now be loaded by the _default constructor_ of the `systemdata`-class. To simplifly the definition of default constants, all default data will be provided by `wificonfig.h` (even, if not directly related to WiFi-data).

## Tech Notes
1. The [IDUINO DEBO DRV3][2] is a two-channel stepper motor developer board based on the L298N H-bridge chip. The board does not only drive stepper motors, but also has the ability to convert the input voltage (5-35V) down to 5V to supply power to connected microcontroller boards. The voltage regulation circuit is based on the 78M05. While the IDUINO datasheets allow input voltages of up to 35V, the maximum input ratings of the 78M05 cannot be told clearly from the relevant datasheet and may be limited to 18V. As a matter of fact, connecting the DEBO DRV3 to an input voltage of 24V destroys the on-board 78M05 immediately. Due to pin compatibility, it is possible to replace the chip by an LM341, granting a maximum input voltage of 35V.

2. After a couple of frustrating and unexplicable system crashes I found that the library documentation of [khoih-prog/ESP8266TimerInterrupt][3] is incomplete. The [ESP8266 interrupt handling documentation][4] states, that all interrupt service routines (ISR) need to be defined with the attribute `IRAM_ATTR`. This also applies for all functions called from the ISR itself. However, all examples based on the _ISR-based Timer_ lack the `IRAM_ATTR`-attribute!  
Unfortunately the GITHUB-repository has been archived and new issues cannot be posted.

3. For the clockwork to display local time, it was necessary to extend the [PaulStoffregen/Time][5] library with daylight saving time (DST) functionality. Not to reinvent the wheel, I have taken the maths from the excellent work of Andreas Spiess  @[SensorIOT/NTPtimeESP][6]. I highly recommend his YouTube-channel.   
Find the (DST-)extended library here: [FoxWhiskey/Time_DST][7]

4. During my tests I found, that the cpu clock of the ESP8266 is pretty unstable and also temperature-sensitive (to some extent at least).   
The first long time test showed, that the board drifts away from NTP time unacceptably fast (about 2 seconds early per day, in my case).   
`v1.1.3` (and greater) implements a simple mechanism to re-synchronise the relevant _ISRs_ to NTP-time, so that the clockwork is powered on time. An attentive clock-watcher therefore may notice a slightly longer or shorter (indicated) minute once in a while...   
An adaptive way to account for the time drift will be implemented with `v1.3.x` and greater. See milestone on Github.
## Why is `clickclock` a _slave clock_ implementation?
Err, well, the wording is misleading, to be honest! The software acutally implements a _mother clock_ :-).

[1]:<https://www.wemos.cc/en/latest/d1/index.html>
[2]:<https://www.reichelt.com/de/en/developer-boards-motor-control-dual-h-bridge-l298n-debo-drv3-l298n-p282647.html>
[3]:<https://github.com/khoih-prog/ESP8266TimerInterrupt>
[4]:<https://arduino-esp8266.readthedocs.io/en/latest/reference.html>
[5]:<https://github.com/PaulStoffregen/Time>
[6]:<https://github.com/SensorsIot/NTPtimeESP>
[7]:<https://github.com/FoxWhiskey/Time_DST>