#include <avr/io.h>
#include "board.h"
#include "display.h"
#include "delay.h"
#ifdef HAS_LCD
#include "pcf8833.h"
#endif
#include <MyUSB/Drivers/USB/USB.h>     // USB_IsConnected


//*****************************************************************************
// Function: get_adcw
// Parameters: none.
// Returns: none.
//
// Description: start AD conversion to read specific ADC channel
//*****************************************************************************

uint16_t
get_adcw(uint8_t mux)
{
  uint8_t i;
  uint16_t result = 0;
  
  // Frequenzvorteiler setzen auf 8 (1) und ADC aktivieren (1)
  ADCSRA = (1<<ADEN) | (1<<ADPS1) | (1<<ADPS0); 

  ADMUX = mux;                      // Kanal waehlen
  ADMUX |= (1<<REFS1) | (1<<REFS0); // interne Referenzspannung nutzen

  for(i=0; i<2; i++) {
    ADCSRA |= (1<<ADSC);              // Dummy readout
    while ( ADCSRA & (1<<ADSC) );     // wait for converter to finish
  }
  my_delay_ms(1);
  
  /* Eigentliche Messung - Mittelwert aus 4 aufeinanderfolgenden Wandlungen */
  for(i=0; i<4; i++) {
    ADCSRA |= (1<<ADSC);            // eine Wandlung "single conversion"
    while ( ADCSRA & (1<<ADSC) );   // auf Abschluss der Konvertierung warten
    result += ADCW;                 // Wandlungsergebnisse aufaddieren
  }
  ADCSRA &= ~(1<<ADEN);             // ADC deaktivieren (2)
  
  result /= 4; 
  
  return result;
}


void
batfunc(char *mode)
{
  uint8_t s1, b;
  uint16_t bv;

  bv  = get_adcw(BAT_MUX);
  s1  = (bit_is_set( BAT_PIN, BAT_PIN1) ? 2 : 0);
  s1 |= (bit_is_set( BAT_PIN, BAT_PIN2) ? 1 : 0);
  if(s1==0) DS_P( PSTR("discharge"));
  if(s1==1) DS_P( PSTR("charged"));
  if(s1==2) DS_P( PSTR("charging"));
  if(s1==3) DS_P( PSTR("error"));

  if(fromhex(mode+1,&b,1) && b == 1) {

    DU(bv,4);    // result, decimal

  } else {
    
   static uint8_t bat[] = { 0,109,136,141,143,152,164,179,196,214,240 };

    if(bv < 761) bv = 761;
    bv -= 760;
    if(bv > 240) bv = 240;
    b = (uint8_t)bv;

    for(s1 = 1; s1 < sizeof(bat); s1++)
      if(b <= bat[s1])
        break;
    
    uint8_t s2 = s1-1;
    b = 10*s2+(10*(uint16_t)(b-bat[s2]))/(uint16_t)(bat[s1]-bat[s2]);

    DU(b,3);
    DC('%');

  }
  DNL();
}


#if defined(HAS_LCD) && defined(BAT_PIN)
void
bat_drawstate(void)
{
  if(!lcd_on)
    return;

  uint16_t v1 = get_adcw(BAT_MUX);
  if(v1 < 750) v1 = 750;
  if(v1 > 950) v1 = 950;

  uint8_t s1;
  s1  = (bit_is_set( BAT_PIN, BAT_PIN1) ? 2 : 0);
  s1 |= (bit_is_set( BAT_PIN, BAT_PIN2) ? 1 : 0);

  uint16_t v2 = ((s1 == 2 && USB_IsConnected) ?
                0x00f0 : 0xf000);    // blue: charging, Red: else

  v1 = (v1-750)*VISIBLE_WIDTH/200;
  lcd_line(WINDOW_LEFT,TITLE_HEIGHT-1,          // the green/blue part
                   WINDOW_LEFT+v1,TITLE_HEIGHT-1, 0x0f00);

  lcd_line(WINDOW_LEFT+v1,TITLE_HEIGHT-1,       // and the red part
                   WINDOW_RIGHT,TITLE_HEIGHT-1, v2);
}
#endif

void
bat_init(void)
{
  BAT_DDR  &= ~(_BV(BAT_PIN1) | _BV(BAT_PIN1));
  BAT_PORT |= (_BV(BAT_PIN1) | _BV(BAT_PIN1));          // pull Battery stati
}
