/**
 * Custom brushed motor firmware for some cheap BLDC controller.
 *
 * Copyright (c)2012 Thomas Kindler <mail@t-kindler.de>
 *
 * TODO
 * - Use LS_C/HS_C as a general purpose output
 *
 * This program is free software;  you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of
 * the License,  or (at your option) any later version.  Read the
 * full License at http://www.gnu.org/copyleft for more details.
 */

// Include files -----
//
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <util/delay.h>

// Configuration values
//
#define U_BAT_MIN       11200   // Minimum battery voltage (in mV)
#define U_BAT_MAX       17600   // Maximum battery voltage (in mV)

// ADC conversion constants
//
#define U_BAT_LSB       (2.56  * ((10000.0 + 390) / 390) / 1024)
#define U_BAT_GAIN      (17.95 / 17.18)

#define I2C_ADDR_BASE   0x42

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

#define BOARD_ID  _BV(PB6)  // BOARD_ID

#define I2C_SDA _BV(PC4)    // Conflicts with U_C -> Remove resistor network
#define I2C_SCL _BV(PC5)

// Software PWM constants
//
#define PWM_TCCR0       2   // 1us timebase
#define PWM_PERIOD      255 // ca. 4 kHz

// #define PWM_TCCR0       2   // 1us timebase
// #define PWM_PERIOD      117 // ca. 8kHz

// #define PWM_TCCR0       4   // 32us timebase
// #define PWM_PERIOD      255 // ca. 125 Hz

#define PWM_MIN         8
#define PWM_MAX         (PWM_PERIOD-8)


#define clamp(x, min, max)      \
( { typeof (x)   _x   = (x);    \
    typeof (min) _min = (min);  \
    typeof (max) _max = (max);  \
    _x < _min ? _min : (_x > _max ? _max : _x); \
} )


/********** Software-PWM **********/

uint8_t  pwm_state;
uint8_t  pwm_t0, pwm_out0;
uint8_t  pwm_t1, pwm_out1;
uint8_t  pwm_dead;


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
    if (pwm_state) {
        TCNT0 = pwm_t1;
        deadtime_8us();
        PORTD = pwm_out1;
        pwm_state = 0;
    }
    else {
        TCNT0 = pwm_t0;
        deadtime_8us();
        PORTD = pwm_out0;
        pwm_state = 1;
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

    cli();

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

    sei();
}


/********** RC remote receiver **********/

volatile int16_t    _rc_pulse;
uint8_t    rc_watchdog;

ISR(INT0_vect)
{
    if (PIND & RC_IN) {
        TCNT1 = 0;
    }
    else {
        _rc_pulse = TCNT1;
        rc_watchdog = 100;
    }
}

/********** I2C Slave **********/

uint8_t i2c_pointer;
uint8_t i2c_data[16];
uint8_t i2c_state;
uint8_t i2c_watchdog;

ISR(TWI_vect)
{
    uint8_t status = TWSR & 0xF8;

    switch (status) {
    case 0x60:  // Own SLA+W has been received
        i2c_state = 0;
        break;

    case 0x80:  // Data received
        if (i2c_state == 0) {
            i2c_pointer = TWDR;
            i2c_state = 1;
        }
        else {
            if (i2c_pointer >= sizeof(i2c_data))
                i2c_pointer = 0;
            i2c_data[i2c_pointer++] = TWDR;
        }
        break;

    case 0xA0:  // STOP or repeated START
        break;

    case 0xA8:  // Own SLA+R has been received
    case 0xB8:  // Data byte has been transmitted
        if (i2c_pointer >= sizeof(i2c_data))
            i2c_pointer = 0;
        TWDR = i2c_data[i2c_pointer++];
        break;

    default:    // Error state
        TWCR |= _BV(TWSTO);
        break;
    }

    // Acknowledge interrupt
    //
    TWCR |= _BV(TWINT);
    i2c_watchdog = 100;
}


/********** Main loop **********/
#define ERR_MASK            0x0F
#define ERR_UBAT_MIN        0x01
#define ERR_UBAT_MAX        0x02

#define WARN_MASK           0xF0
#define WARN_RC_TIMEOUT     0x10
#define WARN_I2C_TIMEOUT    0x20

uint8_t status;

int main(void)
{
    // Enable the watchdog timer
    //
    wdt_enable(WDTO_15MS);

    // Set board ID to input w/ pullup
    //
    DDRB  = 0;
    PORTB = BOARD_ID;

    // Initialize output pins
    //
    PORTD = 0;
    DDRD  = 0xBB;

    // Use timer 0 for software PWM output
    //
    TCCR0  = PWM_TCCR0;
    TIMSK |= _BV(TOV0);     // interrupt on overflow
    set_pwm(0);

    // Use timer 1 and INT0 for RC input
    //
    TCCR1B = 2;             // 1us timebase
    MCUCR = _BV(ISC00);     // trigger on any change
    GICR  = _BV(INT0);      // enable interrupt

    // Use ADC7 for battery voltage measurement
    // 125kHz ADC Clock / 13 => ~4800 samples/s
    // Use internal 2.56V reference
    //
    ADMUX  = _BV(REFS1) | _BV(REFS0) | 7;
    ADCSRA = _BV(ADEN) | _BV(ADSC) | _BV(ADFR) | 7;

    // Enable I2C slave with interrupts
    //
    if (PINB & BOARD_ID)
        TWAR = (I2C_ADDR_BASE + 0) << 1;
    else
        TWAR = (I2C_ADDR_BASE + 1) << 1;
    
    TWCR = _BV(TWEA) | _BV(TWEN) | _BV(TWIE);

    sei();

    // Wait for RC receiver to start up, etc..
    //
    _delay_ms(250);

    int pwm_ref  = 100;
    int pwm_act  = 0;
    int rc_pulse = 0;
    int outputs  = 0;
    int u_bat_mv = 0;

    for(;;) {
        // Check voltage limits and timeouts
        //
        u_bat_mv = ADC * 1000L * U_BAT_LSB * U_BAT_GAIN;

        if (u_bat_mv < U_BAT_MIN)
            status |= ERR_UBAT_MIN;
        if (u_bat_mv > U_BAT_MIN * 1.1)
            status &= ~ERR_UBAT_MIN;

        if (u_bat_mv > U_BAT_MAX)
            status |= ERR_UBAT_MAX;
        if (u_bat_mv < U_BAT_MAX * 0.9)
            status &= ~ERR_UBAT_MAX;

        if (i2c_watchdog > 0) {
            i2c_watchdog--;
            status &= ~WARN_I2C_TIMEOUT;
        }
        else {
            status |= WARN_I2C_TIMEOUT;
        }

        if (rc_watchdog > 0) {
            rc_watchdog--;
            status &= ~WARN_RC_TIMEOUT;
        }
        else {
            status |= WARN_RC_TIMEOUT;
        }

        cli(); rc_pulse = _rc_pulse; sei();
        
        // Reference values
        //
        pwm_ref = 0;  // Normalized to -255 .. 255

        if (!(status & WARN_I2C_TIMEOUT)) {
            pwm_ref = ((int8_t)i2c_data[0]) * 2;
        }

        if (!(status & WARN_RC_TIMEOUT)) {
            if (rc_pulse > 750 && rc_pulse < 2250) {
                // RC input overrides I2C commands
                //
                pwm_ref = (rc_pulse - 1560) * 256L / 300;
            }
        }

        if (status & ERR_MASK) {
            // Switch off in case of any error
            //
            pwm_ref = 0;
        }

        pwm_ref = clamp(pwm_ref, -255, 255);
        pwm_act = clamp(pwm_ref, pwm_act - 1, pwm_act + 1);

        outputs = i2c_data[1];

        // Actual values
        //
        i2c_data[2] = status;
        i2c_data[3] = pwm_act >> 1;
        
        i2c_data[4] = u_bat_mv & 0xFF;
        i2c_data[5] = u_bat_mv >> 8;

        i2c_data[6] = rc_pulse & 0xFF;
        i2c_data[7] = rc_pulse >> 8;

        set_pwm( ((long)pwm_act * PWM_PERIOD) / 255 );

        _delay_ms(1);
    }
}
