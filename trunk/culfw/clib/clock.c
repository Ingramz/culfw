#include <avr/io.h>
#include <avr/wdt.h>

#include "led.h"
#include "fncollection.h"
#include "clock.h"
#include "display.h"
#include "transceiver.h"
#include "battery.h"
#include "joy.h"
#include "mysleep.h"

uint8_t day,hour,minute,sec,hsec;

//Counted time
//clock_time_t clock_datetime = 0;
	
// count & compute in the interrupt, else long runnning tasks would block
// a "minute" task too long
ISR(TIMER0_COMPA_vect, ISR_BLOCK)
{
  hsec++;

  if(hsec >= 125) {

    wdt_reset();

    if(led_mode & 2)
      LED_TOGGLE();

    // one second, 1% duty cycle, 10ms resolution => this is simple ;-)
    if (credit_10ms < MAX_CREDIT)
      credit_10ms += 1;

    //clock_datetime++;

    sec++; hsec = 0;
    if(sec == 60) {
      minute++; sec = 0;
      if(minute == 60) {
        hour++; minute = 0;
        if(hour == 24) {
          day++; hour = 0;
        }
      }
    }
  }
}

void
gettime(char *unused)
{
  display_udec(day,3,'0');     DC(' ');
  display_udec(hour, 2,'0');   DC(':');
  display_udec(minute, 2,'0'); DC(':');
  display_udec(sec, 2,'0');    DC('.');
  display_udec(hsec*8, 3,'0');       // convert it to msec
  DNL();
}

#if 0
//Return time
clock_time_t
clock_time()
{
  clock_time_t time;
  cli();
  time = clock_datetime;
  sei();
  return time;
}
#endif

TASK(Minute_Task)
{
  static uint8_t lsec, lmin;

  if(lsec == sec)
    return;
  lsec = sec;
#ifdef JOY_PIN1
  if(sleep_time && (joy_inactive++ == sleep_time))
    dosleep();
#endif

  if(lmin == minute)
    return;
  lmin = minute;

#if defined(HAS_GLCD) && defined(BAT_PIN)
  bat_drawstate();
  joy_inactive = 0;
#endif
}
