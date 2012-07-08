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

// Reverse-enginered pinout
//
#define LS_A    _BV(PD0)
#define LS_B    _BV(PD1)
#define RC_IN   _BV(PD2)
#define LS_C    _BV(PD3)
#define HS_C    _BV(PD4)
#define HS_A    _BV(PD5)
#define U_NULL  _BV(PD6)
#define HS_B    _BV(PD7)
#define U_A     _BV(PC2)
#define U_B     _BV(PC3)
#define U_C     _BV(PC4)
// #define ???     PC5
#define U_BAT   ADC7

#define PWM_PERIOD    117     // ca. 8kHz
#define PWM_MIN       8
#define PWM_MAX       (PWM_PERIOD-8)

uint8_t  pwm_state;
uint8_t  pwm_t0, pwm_out0;
uint8_t  pwm_t1, pwm_out1;
uint8_t  pwm_dead;
int16_t  rc_pulse;
int8_t   rc_rxcount;

inline void deadtime_1us(void)
{
    PORTD = pwm_dead; PORTD = pwm_dead; PORTD = pwm_dead; PORTD = pwm_dead;
    PORTD = pwm_dead; PORTD = pwm_dead; PORTD = pwm_dead; PORTD = pwm_dead;
}

inline void deadtime_8us(void)
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
        return;
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
    TCCR0  = 2;
    TIMSK |= _BV(TOV0);
    set_pwm(0);

    // Use timer 1 and INT0 for RC input
    //
    TCCR1B = 2;           // 1us timebase
    MCUCR = _BV(ISC00);   // trigger on any change
    GICR  = _BV(INT0);    // enable interrupt

    sei();

    _delay_ms(500);

    for(;;) {
        // TODO:
        // * Check battery voltage
        // * Check for RC timeouts
        // * Limit PWM slew rate
        // * Arm/Disarm motors, beep on startup
        // * Can we estimate the motor current?!
        // * Make use of the third output channel
        //
        cli(); int t = rc_pulse; sei();

        if (t > 500 && t < 2500) {
            int pwm = ((t - 1500) / 500.0) * PWM_MAX;
            set_pwm(pwm);
        }

        _delay_ms(10);
    }
}
