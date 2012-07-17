/**
 * Custom brushed motor firmware for some cheap BLDC controller.
 *
 * This program is free software;  you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of
 * the License,  or (at your option) any later version.  Read the
 * full License at http://www.gnu.org/copyleft for more details.
 */

// Include files -----
//
#define F_CPU   8000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <util/delay.h>

// Reverse-engineered pinout
//
#define LS_A    _BV(PD0)
#define LS_B    _BV(PD1)
#define RC_IN   _BV(PD2)    // INT0
#define LS_C    _BV(PD3)
#define HS_C    _BV(PD4)
#define HS_A    _BV(PD5)
#define U_NULL  _BV(PD6)    // AIN0
#define HS_B    _BV(PD7)
#define U_A     _BV(PC2)    // ADC2
#define U_B     _BV(PC3)    // ADC3
#define U_C     _BV(PC4)    // ADC4

// Software PWM constants
//
#define PWM_PERIOD  117     // ca. 8kHz
#define PWM_MIN     8
#define PWM_MAX     (PWM_PERIOD-8)

// ADC conversion constants
//
#define ADC_PER_VOLT    ((1024 / 2.56) / ((10e3 + 390) / 390))

#define NUM_CELLS   4
#define U_BAT_MIN   (3.0 * NUM_CELLS * ADC_PER_VOLT)

uint8_t  pwm_state;
uint8_t  pwm_t0, pwm_out0;
uint8_t  pwm_t1, pwm_out1;
uint8_t  pwm_dead;

int16_t  rc_pulse;
int8_t   rc_rxcount;

static inline void deadtime_1us(void)
{
    PORTD = pwm_dead; PORTD = pwm_dead; PORTD = pwm_dead; PORTD = pwm_dead;
    PORTD = pwm_dead; PORTD = pwm_dead; PORTD = pwm_dead; PORTD = pwm_dead;
}

static inline void deadtime_8us(void)
{
    deadtime_1us(); deadtime_1us(); deadtime_1us(); deadtime_1us();
    deadtime_1us(); deadtime_1us(); deadtime_1us(); deadtime_1us();
}

ISR(TIMER0_OVF_vect)
{
    if (!pwm_state) {
        TCNT0 = pwm_t0;
        deadtime_8us();
        PORTD = pwm_out0;
        pwm_state = 1;
    }
    else {
        TCNT0 = pwm_t1;
        deadtime_8us();
        PORTD = pwm_out1;
        pwm_state = 0;
    }
    wdt_reset();
}


void set_pwm(int pwm)
{
    int dir = 1;
    if (pwm < 0) {
        pwm = -pwm;
        dir = -1;
    }
    if (pwm > PWM_PERIOD)
        pwm = PWM_PERIOD;

    pwm_t0 = 255 - pwm;
    pwm_t1 = 255 - (PWM_PERIOD - pwm);

    if (dir > 0) {
        pwm_dead = LS_A;
        pwm_out0 = LS_A | HS_B;
        pwm_out1 = LS_A | LS_B;
    }
    else {
        pwm_dead =        LS_B;
        pwm_out0 = HS_A | LS_B;
        pwm_out1 = LS_A | LS_B;
    }

    if (pwm > PWM_MAX)
        pwm_dead = pwm_out1 = pwm_out0;
    if (pwm < PWM_MIN)
        pwm_dead = pwm_out0 = pwm_out1;
}


ISR(INT0_vect)
{
    if (PIND & RC_IN) {
        TCNT1 = 0;
    }
    else {
        rc_pulse = TCNT1;
        rc_rxcount++;
    }
}


int main(void)
{
    // Enable the watchdog timer
    //
    wdt_enable(WDTO_15MS);

    // Initialize output pins
    //
    PORTD = 0;
    DDRD  = 0xBB;

    // Use timer 0 for software PWM output
    //
    TCCR0  = 2;             // 1us timebase
    TIMSK |= _BV(TOV0);     // interrupt on overflow
    set_pwm(0);

    // Use timer 1 and INT0 for RC input
    //
    TCCR1B = 2;             // 1us timebase
    MCUCR = _BV(ISC00);     // trigger on any change
    GICR  = _BV(INT0);      // enable interrupt

    // Use ADC7 for battery voltage measurement
    // 125kHz ADC Clock / 13 => ~4800 samples/s
    // Use internal 2.56V reference (TODO: Check this!)
    //
    ADMUX  = _BV(REFS1) | _BV(REFS0) | 7;
    ADCSRA = _BV(ADEN) | _BV(ADSC) | _BV(ADFR) | 7;

    sei();
    
    // Wait for RC receiver to start up, etc..
    //
    _delay_ms(250);

    for(;;) {
        cli(); int t = rc_pulse; sei();
        int u_bat = ADC;

        if (u_bat < U_BAT_MIN) {
            set_pwm(0);
        }
        else if (t > 500 && t < 2500) {
            set_pwm(((t - 1500) / 500.0) * PWM_MAX);
        }

        _delay_ms(10);
    }
}