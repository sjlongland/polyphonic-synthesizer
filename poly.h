#ifndef _POLY_H
#define _POLY_H

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

#include <stdint.h>
#include <errno.h>

/*
 * _POLY_CFG_H is a definition that can be given when compiling poly.c
 * and firmware code to define a header file that contains definitions for
 * the Polyphonic synthesizer.
 */
#ifdef _POLY_CFG
#include _POLY_CFG
#endif

/*!
 * Polyphonic event.  This is a base struct for describing what happens
 * within a voice channel.  An array of these forms a musical piece or
 * sound effect.  Each event can take the form of a chnage to a voice's
 * parameters, or timing events.
 */
struct poly_evt_t {
	uint16_t flags;	/*!< Event flags */
	uint16_t value;	/*!< New register value */
};

/* Event flags */

/*!
 * Bits 15-12 represent the event type.
 */
#define POLY_EVT_TYPE_BIT	(12)

/*!
 * Bit mask for event type.
 */
#define POLY_EVT_TYPE_MASK	(0x0f << POLY_EVT_TYPE_BIT)

/*!
 * END event.  This terminates the musical piece and causes a reset of
 * the synthesizer state.  Event is valid at any time.
 */
#define POLY_EVT_TYPE_END	(0x00 << POLY_EVT_TYPE_BIT)

/*!
 * TIME event.  This indicates the synthesizer should emit samples with the
 * currently defined parameters for the number of samples given in the
 * value field.  Event is valid at any time.
 */
#define POLY_EVT_TYPE_TIME	(0x01 << POLY_EVT_TYPE_BIT)

/*!
 * ENABLE event.  This turns on and off computation of the named channels.
 */
#define POLY_EVT_TYPE_ENABLE	(0x02 << POLY_EVT_TYPE_BIT)

/*!
 * MUTE event.  This turns on and off inclusion of a channel in the output.
 */
#define POLY_EVT_TYPE_MUTE	(0x03 << POLY_EVT_TYPE_BIT)

/*!
 * IFREQ change event.  This indicates the immediate frequency for a voice
 * channel is to change to the value given in Hz, or, if the frequency is
 * zero, the channel is to emit a DC or linearly varying signal.
 *
 * Channel number is given in bits 12-8 of the flags register.
 */
#define POLY_EVT_TYPE_IFREQ	(0x04 << POLY_EVT_TYPE_BIT)

/*!
 * DFREQ change event.  This indicates the frequency step is to change to
 * the given value in Hz.  The frequency of the channel will step by this
 * amount every N samples, where N is set by DSCALE.
 *
 * Channel number is given in bits 12-8 of the flags register.
 */
#define POLY_EVT_TYPE_DFREQ	(0x05 << POLY_EVT_TYPE_BIT)

/*!
 * PMOD change event.  Change phase modulation configuration.
 *
 * If value is UINT16_MAX: Disable phase modulation
 * Otherwise, modulate the phase using the channel number given.
 *
 * Channel number is given in bits 12-8 of the flags register.
 */
#define POLY_EVT_TYPE_PMOD	(0x06 << POLY_EVT_TYPE_BIT)

/*!
 * IAMP change event.  This indicates the immediate amplitude of the
 * channel is to be set to the value given.
 *
 * Channel number is given in bits 12-8 of the flags register.
 */
#define POLY_EVT_TYPE_IAMP	(0x08 << POLY_EVT_TYPE_BIT)

/*!
 * DAMP change event.  This indicates the amplitude step is to change to
 * the given value.  The amplitude will change by this amount every N
 * samples, where N is set by DSCALE.
 *
 * Channel number is given in bits 12-8 of the flags register.
 */
#define POLY_EVT_TYPE_DAMP	(0x09 << POLY_EVT_TYPE_BIT)

/*!
 * AMOD change event.  Change amplitude modulation configuration.
 *
 * If value is UINT16_MAX: Disable amplitude modulation
 * Otherwise, modulate the amplitude using the channel number given.
 *
 * Channel number is given in bits 12-8 of the flags register.
 */
#define POLY_EVT_TYPE_AMOD	(0x0a << POLY_EVT_TYPE_BIT)

/*!
 * ASCALE change event.  Amplitudes are to be left-shifted by this number
 * of bits after multiplication with a sample.
 *
 * Channel number is given in bits 12-8 of the flags register.
 */
#define POLY_EVT_TYPE_ASCALE	(0x0b << POLY_EVT_TYPE_BIT)

/*!
 * DSCALE change event.  Every N samples (given here), the amplitude and
 * frequency of the channel will be adjusted.
 *
 * Channel number is given in bits 12-8 of the flags register.
 */
#define POLY_EVT_TYPE_DSCALE	(0x0f << POLY_EVT_TYPE_BIT)

/*!
 * Position of channel number field.
 */
#define POLY_CH_BIT		(8)

/*!
 * Mask for channel number field.
 */
#define POLY_CH_MASK		(0x0f << POLY_VOICE_CH_BIT)

/*!
 * Voice state machine.  A "voice" is simply a sinusoidal channel.  It
 * may be modulated by a static linear function, or by taking the output
 * from another channel and summing that.
 *
 * Each voice has its own sample timing counter which starts at zero and
 * counts upwards.
 */
struct poly_voice_t {
	int16_t		sample;	/*!< Sample last computed */
	uint16_t	time;	/*!< Time (samples) for voice */
	uint16_t	freq;	/*!< Current frequency */
	int16_t		dfreq;	/*!< Delta frequency */
	uint16_t	dscale;	/*!< Delta time scale */
	uint8_t		amp;	/*!< Current amplitude */
	int8_t		damp;	/*!< Delta amplitude */
	uint8_t		ascale;	/*!< Amplitude scale */
	uint8_t		pmod;	/*!< Phase modulation channel */
	uint8_t		amod;	/*!< Amplitude modulation channel */
	uint8_t		flags;	/*!< Flags register */
};

#ifndef _POLY_NUM_CHANNELS
/*!
 * Number of voice channels: this needs to be declared in the application.
 * Overridden by defining _POLY_NUM_CHANNELS.
 */
extern const uint8_t __attribute__((weak)) poly_num_channels;
#else
#define poly_num_channels	_POLY_NUM_CHANNELS
#endif

#ifndef _POLY_FREQ
/*!
 * Sample rate for the synthesizer: this needs to be declared in the
 * application.
 */
extern const uint16_t __attribute__((weak)) poly_freq;
#else
#define poly_freq		_POLY_FREQ
#endif

#ifndef _POLY_FREQ
/*!
 * Maximum frequency for the synthesizer: this needs to be declared in the
 * application.  This should be set at the nyquist frequency.
 * (poly_freq/2)
 */
extern const uint16_t __attribute__((weak)) poly_freq_max;
#else
#define poly_freq_max		(poly_freq/2)
#endif

#ifndef _POLY_NUM_CHANNELS
/*!
 * Voice channel array: this needs to be declared in the application.
 */
extern struct __attribute__((weak)) poly_voice_t poly_voice[];
#endif

/*!
 * Number of samples remaining before the next set of events.
 */
extern volatile uint16_t poly_remain;

/*!
 * Reset the polyphonic synthesizer.
 */
void poly_reset();

/*!
 * Load a sample event into the polyphonic registers.
 * @param	event		Polyphonic event to load.
 * @retval	0		Success
 * @retval	-EINVAL		Bad event
 * @retval	-ERANGE		Bad value
 * @retval	-EINPROGRESS	Waiting for timing event
 */
int poly_load(const struct poly_evt_t* event);

/*!
 * Retrieve the next output sample from the polyphonic synthesizer.
 */
int16_t poly_next();

/*
 * vim: set sw=8 ts=8 noet si tw=72
 */

#endif
