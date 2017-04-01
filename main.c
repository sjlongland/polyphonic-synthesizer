/*!
 * Polyphonic synthesizer for microcontrollers: Atmel ATTiny85 port.
 * (C) 2016 Stuart Longland
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA  02110-1301  USA
 */

#include "poly.h"
#include "fifo.h"
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#define SAMPLE_LEN	16
static volatile uint8_t sample_buffer[SAMPLE_LEN];
static struct fifo_t sample_fifo;

int main(void) {
	struct poly_evt_t poly_evt;

	/* Turn on all except ADC */
	PRR = (1 << PRADC);

	/* Start up PLL */
	PLLCSR = (1 << PLLE);
	while (!(PLLCSR & (1<<PLOCK)));
	PLLCSR |= (1<<PCKE);

	fifo_init(&sample_fifo, sample_buffer, SAMPLE_LEN);

	/* Reset the synthesizer */
	poly_reset();

	/* Enable output on PB4 (PWM out) */
	DDRB |= (1 << 4) | (1 << 3);
	PORTB |= (1 << 4) | (1 << 3);

	/* Timer 1 configuration for PWM */
	OCR1B = 128;			/* Initial PWM value */
	OCR1C = 255;			/* Maximum PWM value */
	TCCR1 = (1 << CS10);		/* No prescaling, max speed */
	GTCCR = (1 << PWM1B)		/* Enable PWM */
		| (2 << COM1B0);	/* Clear output bit on match */

	/* Timer 0 configuration for sample rate interrupt */
	TCCR0A = (2 << WGM00);		/* CTC mode */
	TCCR0B = (1 << CS00);		/* No prescaling */
	OCR0A = F_CPU / _POLY_FREQ;	/* Sample rate */
	TIMSK |= (1 << OCIE0A);		/* Enable interrupts */

	/* Configure the synthesizer */
	poly_evt.flags = POLY_EVT_TYPE_ENABLE;
	poly_evt.value = 1;
	poly_load(&poly_evt);

	poly_evt.flags = POLY_EVT_TYPE_IFREQ;
	poly_evt.value = 1000;
	poly_load(&poly_evt);

	poly_evt.flags = POLY_EVT_TYPE_IAMP;
	poly_evt.value = 255;
	poly_load(&poly_evt);

	poly_evt.flags = POLY_EVT_TYPE_ASCALE;
	poly_evt.value = 8;
	poly_load(&poly_evt);

	sei();
	while(1) {
		poly_evt.flags = POLY_EVT_TYPE_TIME;
		poly_evt.value = 64000;
		poly_load(&poly_evt);

		while (poly_remain) {
			while (sample_fifo.stored_sz < SAMPLE_LEN) {
				int16_t s = poly_next();
				fifo_write_one(&sample_fifo,
						128 + (s >> 9));

			}
			PORTB ^= (1 << 3);
		}
	}
	return 0;
}

ISR(TIM0_COMPA_vect) {
	uint8_t sample = fifo_read_one(&sample_fifo);
	if (sample >= 0)
		OCR1B = sample;
	else
		OCR1B = 128;
}
