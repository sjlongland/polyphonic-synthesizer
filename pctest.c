#include "poly.h"
#include <stdio.h>
#include <string.h>
#include <ao/ao.h>

const uint16_t poly_freq = 32000;
const uint16_t poly_freq_max = 16000;
const uint8_t poly_num_channels = 8;
struct poly_voice_t poly_voice[8];

int main(int argc, char** argv) {
	struct poly_evt_t event;
	int voice = 0;
	int16_t samples[8192];
	uint16_t samples_sz = 0;
	ao_device* device;
	ao_sample_format format;
	ao_initialize();
	poly_reset();
	FILE* out = fopen("out.raw", "wb");

	{
		int driver = ao_default_driver_id();
		memset(&format, 0, sizeof(format));
		format.bits = 16;
		format.channels = 1;
		format.rate = poly_freq;
		format.byte_format = AO_FMT_NATIVE;
		device = ao_open_live(driver, &format, NULL);
		if (!device) {
			fprintf(stderr, "Failed to open audio device\n");
			return 1;
		}
	}

	argc--;
	argv++;
	while (argc > 0) {
		int res = 0;
		if (!strcmp(argv[0], "end"))
			break;
		if (!strcmp(argv[0], "voice")) {
			voice = atoi(argv[1]);
			argv++;
			argc--;
		} else if (!strcmp(argv[0], "mute")) {
			int mute = atoi(argv[1]);
			event.flags = POLY_EVT_TYPE_MUTE;
			event.value = mute;
			res = poly_load(&event);
			argv++;
			argc--;
		} else if (!strcmp(argv[0], "en")) {
			int en = atoi(argv[1]);
			event.flags = POLY_EVT_TYPE_ENABLE;
			event.value = en;
			res = poly_load(&event);
			argv++;
			argc--;
		} else if (!strcmp(argv[0], "freq")) {
			int freq = atoi(argv[1]);
			event.flags = (voice << POLY_CH_BIT)
				| POLY_EVT_TYPE_IFREQ;
			event.value = freq;
			res = poly_load(&event);
			argv++;
			argc--;
		} else if (!strcmp(argv[0], "dfreq")) {
			int freq = atoi(argv[1]);
			event.flags = (voice << POLY_CH_BIT)
				| POLY_EVT_TYPE_DFREQ;
			event.value = freq;
			res = poly_load(&event);
			argv++;
			argc--;
		} else if (!strcmp(argv[0], "ascale")) {
			int amp = atoi(argv[1]);
			event.flags = (voice << POLY_CH_BIT)
				| POLY_EVT_TYPE_ASCALE;
			event.value = amp;
			res = poly_load(&event);
			argv++;
			argc--;
		} else if (!strcmp(argv[0], "amp")) {
			int amp = atoi(argv[1]);
			event.flags = (voice << POLY_CH_BIT)
				| POLY_EVT_TYPE_IAMP;
			event.value = amp;
			res = poly_load(&event);
			argv++;
			argc--;
		} else if (!strcmp(argv[0], "damp")) {
			int damp = atoi(argv[1]);
			event.flags = (voice << POLY_CH_BIT)
				| POLY_EVT_TYPE_DAMP;
			event.value = damp;
			res = poly_load(&event);
			argv++;
			argc--;
		} else if (!strcmp(argv[0], "pmod")) {
			int pmod = atoi(argv[1]);
			event.flags = (voice << POLY_CH_BIT)
				| POLY_EVT_TYPE_PMOD;
			event.value = pmod;
			res = poly_load(&event);
			argv++;
			argc--;
		} else if (!strcmp(argv[0], "amod")) {
			int amod = atoi(argv[1]);
			event.flags = (voice << POLY_CH_BIT)
				| POLY_EVT_TYPE_AMOD;
			event.value = amod;
			res = poly_load(&event);
			argv++;
			argc--;
		} else if (!strcmp(argv[0], "dscale")) {
			int dt = atoi(argv[1]);
			event.flags = (voice << POLY_CH_BIT)
				| POLY_EVT_TYPE_DSCALE;
			event.value = dt;
			res = poly_load(&event);
			argv++;
			argc--;
		} else if (!strcmp(argv[0], "time")) {
			int time = atoi(argv[1]);
			argv++;
			argc--;
			event.flags = POLY_EVT_TYPE_TIME;
			event.value = time;
			res = poly_load(&event);
		}
		if (res < 0) {
			fprintf(stderr, "Failed: %s\n",
					strerror(-res));
			break;
		}
		argv++;
		argc--;

		/* Play out any remaining samples */
		while (poly_remain) {
			int16_t* sample_ptr = samples;
			uint16_t samples_remain = 8192;
			/* Fill the buffer as much as we can */
			while (poly_remain && samples_remain) {
				int16_t s = poly_next();
				//printf("%d: %d\n", samples_sz, s);
				*sample_ptr = s << 7;
				sample_ptr++;
				samples_sz++;
				samples_remain--;
			}
			fwrite(samples, samples_sz, 2, out);
			ao_play(device, (char*)samples, 2*samples_sz);
			samples_sz = 0;
		}
	}

	poly_reset();
	fclose(out);

	ao_close(device);
	ao_shutdown();
	return 0;
}
