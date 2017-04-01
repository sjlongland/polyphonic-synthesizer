Polyphonic Synthesizer
======================

This project is intended to be a polyphonic synthesizer for use in
embedded microcontrollers.  It features multi-voice synthesis for up to 16
channels, with channels outputting either a fixed DC offset, sinusoidal
output or whitenoise.

The phase or amplitude of one channel may be modulated by the output of
another channel, allowing for various effects.

Configuration
=============

There are two ways to configure this library:

Using C preprocessor constants
------------------------------

The following C preprocessor definitions should be defined in your project
`Makefile`.

* `_POLY_NUM_CHANNELS`: The number of polyphonic channels (voices) that
  you wish to instantiate.  Each channel occupies 16 bytes.
* `_POLY_FREQ`: The output sample rate for the polyphonic synthesizer in
  Hz.

Using linker symbols
--------------------

Alternatively, these things can be decided at run-time.  Your application
should export the following symbols:

* `const uint8_t poly_num_channels`: The number of polyphonic channels
  currently configured.
* `const uint16_t poly_freq`: The output sample rate for the polyphonic 
  synthesizer in Hz.
* `const uint16_t poly_freq_max`: The maximum output frequency for the
  polyphonic synthesizer.  This should be set to `poly_freq/2` (the
  nyquist frequency).
* `struct poly_voice_t poly_voice[]`: The array of voice channels.

You may declare functions using these symbols, or you may use linker
aliasing to expose variables/structures with alternate names.

Usage
=====

Global structures
-----------------

A global counter, `poly_remain`, counts down the number of audio samples
remaining before the next set of events are due to be loaded.  When this
variable reaches 0, you should start calling `poly_load` with new data or
call `poly_reset` to stop playback.

Functions
---------

`poly_reset` clears the state of the polyphonic synthesizer, cancelling
any in-progress playback.  This should be done as part of your program
initialisation.

`poly_load` is used to load in the next event.  Events are covered below.

`poly_next` returns the next audio sample, or `0` if there is no more
audio left to be played.

Events
======

The structure of the event system is loosely based on ideas from the MIDI
standard.  Essentially, an "event" is a parameter change for a given
channel.

Each event is represented by a 4-byte data structure, a `struct
poly_evt_t` which has two fields, `flags` and `value`.  `poly.h` covers
each of the events.  See the comments up the top of that file for detail.
