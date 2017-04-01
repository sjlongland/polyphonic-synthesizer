/*!
 * Polyphonic synthesizer for microcontrollers.
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
#include <string.h>
#include <assert.h>
#include <stdlib.h>

#ifdef __AVR_ARCH__
#include <avr/pgmspace.h>
#endif

#ifdef _DEBUG
#include <stdio.h>
#define _DPRINTF(s, a...)	printf(__FILE__ ": %d " s, __LINE__, a)
#else
#define _DPRINTF(s, a...)
#endif

#ifdef _POLY_NUM_CHANNELS
static struct poly_voice_t poly_voice[_POLY_NUM_CHANNELS];
#endif

#define POLY_SINE_SZ 360
static const uint8_t _poly_sine[POLY_SINE_SZ];

/* Master sample clock */
/*static volatile uint16_t _poly_remain __attribute__((nocommon)) = 0;
extern const volatile uint16_t
	__attribute__((alias ("_poly_remain"))) poly_remain;*/
volatile uint16_t poly_remain;
#define _poly_remain poly_remain

/* Enabled channels */
static uint16_t _poly_enable = 0;
/* Muted channels */
static uint16_t _poly_mute = 0;

/*!
 * Reset the polyphonic synthesizer.
 */
void poly_reset() {
	memset(poly_voice, 0,
			sizeof(struct poly_voice_t)*poly_num_channels);
	_poly_remain = 0;
}

/*!
 * Load a sample event into the polyphonic registers.
 * @param	event	Polyphonic event to load.
 */
int poly_load(const struct poly_evt_t* const event) {
	uint16_t type = (event->flags) & POLY_EVT_TYPE_MASK;
	switch (type) {
		case POLY_EVT_TYPE_TIME:
			_poly_remain = event->value;
			return 0;
		case POLY_EVT_TYPE_END:
			poly_reset();
			return 0;
	}

	/* Forbid updating of voice states while we are waiting! */
	if (_poly_remain)
		return -EINPROGRESS;

	switch (type) {
		case POLY_EVT_TYPE_ENABLE:
			_poly_enable = event->value;
			return 0;
		case POLY_EVT_TYPE_MUTE:
			_poly_mute = event->value;
			return 0;
	}

	struct poly_voice_t* const voice = &poly_voice[
		(event->flags >> POLY_CH_BIT) & 0x0f];
	switch (type) {
		case POLY_EVT_TYPE_IFREQ:
			voice->freq = event->value;
			voice->time = 0;
			return 0;
		case POLY_EVT_TYPE_DFREQ:
			voice->dfreq = event->value;
			return 0;
		case POLY_EVT_TYPE_PMOD:
			if (event->value == UINT16_MAX)
				voice->pmod = 0;
			else
				voice->pmod = event->value | 0x80;
			return 0;
		case POLY_EVT_TYPE_IAMP:
			voice->amp = event->value;
			return 0;
		case POLY_EVT_TYPE_DAMP:
			voice->damp = event->value;
			return 0;
		case POLY_EVT_TYPE_AMOD:
			if (event->value == UINT16_MAX)
				voice->amod = 0;
			else
				voice->amod = event->value | 0x80;
			return 0;
		case POLY_EVT_TYPE_ASCALE:
			if (event->value > 31)
				return -ERANGE;
			voice->ascale = event->value;
			return 0;
		case POLY_EVT_TYPE_DSCALE:
			voice->dscale = event->value;
			return 0;
	}

	/* If we get here, then it was a bad event */
	return -EINVAL;
}

/*!
 * Emit the sinusoid at the given fixed-point angle in ¼ degrees.
 */
static int16_t poly_sine(uint16_t angle) {
	int16_t amp = 1;
	angle %= (POLY_SINE_SZ*4);
	if (angle >= (POLY_SINE_SZ*2)) {
		amp = -1;
		angle = (POLY_SINE_SZ*4) - angle - 1;
	}
	if (angle >= POLY_SINE_SZ)
		angle = (POLY_SINE_SZ*2) - angle - 1;

	assert(angle < POLY_SINE_SZ);
	return amp *
#ifdef __AVR_ARCH__
		pgm_read_byte(&_poly_sine[angle])
#else
		_poly_sine[angle]
#endif
		;
}

/*!
 * Compute the output of a single voice.
 */
static void poly_compute(struct poly_voice_t* const voice) {
	int32_t amp = voice->amp;
	int32_t sample = 0;

	/* Amplitude modulation? */
	if (voice->amod) {
		_DPRINTF("amplitude mod: %d + amp(%d)\n",
				amp, voice->amod & 0x0f);
		amp += poly_voice[voice->amod & 0x0f].sample;
	}

	_DPRINTF("amplitude %d\n", amp);
	if (amp) {
		/* Frequency modulation? */
		if (voice->freq) {
			if (voice->freq < UINT16_MAX) {
				/* 
				 * Time T is N/Fs.
				 * Angle in ¼° is 1440*F*T
				 */
				int64_t angle = (4*POLY_SINE_SZ)
					* voice->freq
					* (int64_t)voice->time;
				angle /= poly_freq;
				if (voice->pmod)
					angle += poly_voice[voice->pmod
						& 0x0f].sample;
				angle %= (4*POLY_SINE_SZ);
				sample = poly_sine(angle);
				_DPRINTF("sine %d Hz sample %d "
						"(angle %ld) = %d\n",
						voice->freq, voice->time,
						angle, sample);
			} else {
				sample = (rand() / (RAND_MAX/512)) - 256;
				_DPRINTF("noise %d @ %d\n", sample, amp);
			}
			sample *= amp;
			_DPRINTF("amplitude * sample = %d\n",
					sample * amp);
		} else {
			/* DC */
			_DPRINTF("DC = %d\n", amp);
			sample = amp;
		}
		sample >>= voice->ascale;
		_DPRINTF("scale %d = %d\n", voice->ascale, sample);
	} else {
		/* No signal */
		sample = 0;
	}

	if (voice->dscale && (!(voice->time % voice->dscale))) {
		/* Delta frequency adjustment */
		if (voice->dfreq) {
			int32_t freq = voice->freq;
			freq += voice->dfreq;
			if (freq < 0)
				voice->freq = 0;
			else if (freq > poly_freq_max)
				voice->freq = poly_freq_max;
			else
				voice->freq = freq;
		}

		/* Delta amplitude adjustment */
		if (voice->damp) {
			amp = (int32_t)voice->amp + (int32_t)voice->damp;
			if (amp < 0) {
				voice->amp = 0;
				voice->damp = 0;
			} else if (amp > UINT8_MAX) {
				voice->amp = UINT8_MAX;
				voice->damp = 0;
			} else
				voice->amp = amp;
		}
	}

	/* Clipping */
	if (sample > INT16_MAX)
		sample = INT16_MAX;
	else if (sample < INT16_MIN)
		sample = INT16_MIN;

	/* Update sample */
	voice->sample = sample;

	/* Time step update */
	voice->time++;
}

/*!
 * Retrieve the next output sample from the polyphonic synthesizer.
 */
int16_t poly_next() {
	/* Do not return samples unless we're in the waiting state. */
	if (!_poly_remain)
		return 0;

	/* Compute all the voices, tally up the samples */
	uint8_t vid;
	int16_t sample = 0;
	uint16_t mask = 1;
	for (vid = 0; vid < poly_num_channels; vid++) {
		if (_poly_enable & mask) {
			_DPRINTF("compute %d\n", vid);
			poly_compute(&poly_voice[vid]);
		} else {
			_DPRINTF("skip compute %d\n", vid);
		}
		if (!(_poly_mute & mask)) {
			sample += poly_voice[vid].sample;
		} else {
			_DPRINTF("muted %d\n", vid);
		}
		mask <<= 1;
	}

	/* Decrement our global sample counter */
	_poly_remain--;
	return sample;
}

static const uint8_t _poly_sine[POLY_SINE_SZ]
#ifdef __AVR_ARCH__
PROGMEM
#endif
= {

	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x0A,
	0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x14, 0x15,
	0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1F, 0x20,
	0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x2A, 0x2B,
	0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x35, 0x36,
	0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40,
	0x41, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B,
	0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x53, 0x54, 0x55, 0x56,
	0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60,
	0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A,
	0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74,
	0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E,
	0x7F, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88,
	0x89, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x90, 0x91,
	0x92, 0x93, 0x94, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A,
	0x9B, 0x9C, 0x9C, 0x9D, 0x9E, 0x9F, 0xA0, 0xA1, 0xA2, 0xA3,
	0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA8, 0xA9, 0xAA, 0xAB,
	0xAC, 0xAD, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB1, 0xB2, 0xB3,
	0xB4, 0xB5, 0xB5, 0xB6, 0xB7, 0xB8, 0xB8, 0xB9, 0xBA, 0xBB,
	0xBC, 0xBC, 0xBD, 0xBE, 0xBE, 0xBF, 0xC0, 0xC1, 0xC1, 0xC2,
	0xC3, 0xC4, 0xC4, 0xC5, 0xC6, 0xC6, 0xC7, 0xC8, 0xC8, 0xC9,
	0xCA, 0xCA, 0xCB, 0xCC, 0xCC, 0xCD, 0xCE, 0xCE, 0xCF, 0xD0,
	0xD0, 0xD1, 0xD2, 0xD2, 0xD3, 0xD4, 0xD4, 0xD5, 0xD5, 0xD6,
	0xD7, 0xD7, 0xD8, 0xD8, 0xD9, 0xDA, 0xDA, 0xDB, 0xDB, 0xDC,
	0xDC, 0xDD, 0xDD, 0xDE, 0xDF, 0xDF, 0xE0, 0xE0, 0xE1, 0xE1,
	0xE2, 0xE2, 0xE3, 0xE3, 0xE4, 0xE4, 0xE5, 0xE5, 0xE6, 0xE6,
	0xE7, 0xE7, 0xE8, 0xE8, 0xE8, 0xE9, 0xE9, 0xEA, 0xEA, 0xEB,
	0xEB, 0xEC, 0xEC, 0xEC, 0xED, 0xED, 0xEE, 0xEE, 0xEE, 0xEF,
	0xEF, 0xEF, 0xF0, 0xF0, 0xF1, 0xF1, 0xF1, 0xF2, 0xF2, 0xF2,
	0xF3, 0xF3, 0xF3, 0xF4, 0xF4, 0xF4, 0xF5, 0xF5, 0xF5, 0xF6,
	0xF6, 0xF6, 0xF6, 0xF7, 0xF7, 0xF7, 0xF7, 0xF8, 0xF8, 0xF8,
	0xF8, 0xF9, 0xF9, 0xF9, 0xF9, 0xFA, 0xFA, 0xFA, 0xFA, 0xFA,
	0xFB, 0xFB, 0xFB, 0xFB, 0xFB, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC,
	0xFC, 0xFC, 0xFD, 0xFD, 0xFD, 0xFD, 0xFD, 0xFD, 0xFD, 0xFD,
	0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE,
	0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE,

};

/*
 * vim: set sw=8 ts=8 noet si tw=72
 */
