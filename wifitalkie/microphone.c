// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// microphone.c — the microphone, read by the ADC, made into the telephone's.
//
// The microphone is an electret with a preamplifier of its own, a MAX9814
// module, whose output sits at about 1.25 V and swings about it; it goes to
// PIN_MICROPHONE, on ADC1, read with 12 dB of attenuation, which takes up to
// about 2.5 V.
//
// The ADC runs by itself, in its continuous mode, SAMPLE_HZ samples a
// second put in memory by DMA; microphone_poll() takes what has come and
// does the rest in the loop, so it has to be called oftener than the
// driver's store fills, STORE_BYTES, a sixteenth of a second.
//
// Each sample has the level it sits at taken off, a running mean whose
// corner is about 20 Hz, is made 16-bit, and is multiplied by MIC: 50 is as
// it came, 100 twice that. Then the filter of telephone.c keeps one sample
// in two, 8000 a second, without what lies above 4 kHz folding down among
// the speech. Every 160 make a frame of twenty milliseconds, which voip.c
// takes with microphone_frame(); a few are kept, and the oldest is lost if
// nobody takes them.
//
// The samples of each second are measured for LEVELS, the peak and the rms
// after MIC and how many came, which says whether the ADC keeps the rate
// it was asked for: the packets go at the rate the microphone makes them.
//
// On 30 October 2026 the ADC was twice found giving nothing from the start;
// at other starts it ran. When it was next caught, it had stopped just as
// the WiFi came up, whose radio's calibration uses the same SAR ADC: that
// is the likeliest cause, not yet proved. So the results the driver gives
// are counted, those not of our channel too, and an ADC that has given
// nothing for STALL_MS is stopped and started again, and says so.

#include <string.h>

#include "esp_adc/adc_continuous.h"

#include "console.h"
#include "microphone.h"
#include "pins.h"
#include "settings.h"

#define SAMPLE_HZ DOUBLE_RATE_HZ
#define STORE_BYTES 4096
#define FRAME_BYTES 256
#define FRAMES_KEPT 4
#define ADC_MIDDLE 2048
#define TO_16_BITS 4
#define MEAN_SHIFT 7
#define MEAN_FRACTION_SHIFT 8
#define MIC_AS_IT_CAME 50
#define INT16_MOST 32767
#define MS_A_SECOND 1000
#define STALL_MS 500

static adc_continuous_handle_t handle;
static adc_channel_t channel;
static int is_open;
static int32_t mean;
static struct telephone_down down;
static int16_t frames[FRAMES_KEPT][TELEPHONE_FRAME_SAMPLES];
static int frame_in;
static int frame_out;
static int frame_count;
static int filled;
static int peak;
static int64_t squares;
static int counted;
static int results_a_second;
static int strangers_a_second;
static long last_result_ms;
static unsigned restarts;
static long second_began_ms;
static struct microphone_levels last_second;

void microphone_open(void) {
	adc_continuous_handle_cfg_t store = {
		.max_store_buf_size = STORE_BYTES,
		.conv_frame_size = FRAME_BYTES,
		.flags.flush_pool = 1,
	};
	adc_digi_pattern_config_t pattern;
	adc_continuous_config_t config;
	adc_unit_t unit;

	telephone_down_begin(&down);
	mean = ADC_MIDDLE << MEAN_FRACTION_SHIFT;
	if (adc_continuous_io_to_channel(PIN_MICROPHONE, &unit, &channel) != ESP_OK || unit != ADC_UNIT_1) {
		console_say("MIC: PIN %d IS NOT ON ADC1", PIN_MICROPHONE);
		return;
	}
	memset(&pattern, 0, sizeof(pattern));
	pattern.atten = ADC_ATTEN_DB_12;
	pattern.channel = channel;
	pattern.unit = ADC_UNIT_1;
	pattern.bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;
	memset(&config, 0, sizeof(config));
	config.pattern_num = 1;
	config.adc_pattern = &pattern;
	config.sample_freq_hz = SAMPLE_HZ;
	config.conv_mode = ADC_CONV_SINGLE_UNIT_1;
	config.format = ADC_DIGI_OUTPUT_FORMAT_TYPE2;
	if (adc_continuous_new_handle(&store, &handle) != ESP_OK
			|| adc_continuous_config(handle, &config) != ESP_OK
			|| adc_continuous_start(handle) != ESP_OK) {
		console_say("MIC: THE ADC DID NOT START");
		return;
	}
	is_open = 1;
}

static void frame_full(void) {
	frame_in = (frame_in + 1) % FRAMES_KEPT;
	if (frame_count == FRAMES_KEPT)
		frame_out = (frame_out + 1) % FRAMES_KEPT;
	else
		frame_count++;
	filled = 0;
}

static void measure(int sample) {
	if (sample < 0)
		sample = -sample;
	if (sample > peak)
		peak = sample;
	squares += (int64_t)sample * sample;
	counted++;
}

static void take(int raw) {
	int32_t sample;
	int16_t kept;

	mean += ((raw << MEAN_FRACTION_SHIFT) - mean) >> MEAN_SHIFT;
	sample = (raw - (mean >> MEAN_FRACTION_SHIFT)) * TO_16_BITS;
	sample = sample * (int32_t)settings.mic / MIC_AS_IT_CAME;
	if (sample > INT16_MOST)
		sample = INT16_MOST;
	if (sample < -INT16_MOST)
		sample = -INT16_MOST;
	if (!telephone_down_take(&down, (int16_t)sample, &kept))
		return;
	measure(kept);
	frames[frame_in][filled++] = kept;
	if (filled == TELEPHONE_FRAME_SAMPLES)
		frame_full();
}

static int square_root(int64_t value) {
	int64_t root = 0;
	int64_t bit = (int64_t)1 << 40;

	while (bit > value)
		bit >>= 2;
	while (bit != 0) {
		if (value >= root + bit) {
			value -= root + bit;
			root = (root >> 1) + bit;
		} else {
			root >>= 1;
		}
		bit >>= 2;
	}
	return (int)root;
}

void microphone_poll(long now) {
	uint8_t results[FRAME_BYTES];
	adc_digi_output_data_t *result;
	uint32_t got = 0;
	uint32_t i;

	if (!is_open)
		return;
	while (adc_continuous_read(handle, results, sizeof(results), &got, 0) == ESP_OK) {
		last_result_ms = now;
		for (i = 0; i + SOC_ADC_DIGI_RESULT_BYTES <= got; i += SOC_ADC_DIGI_RESULT_BYTES) {
			result = (adc_digi_output_data_t *)&results[i];
			results_a_second++;
			if (result->type2.unit == 0 && result->type2.channel == channel)
				take((int)result->type2.data);
			else
				strangers_a_second++;
		}
	}
	if (now - last_result_ms > STALL_MS) {
		restarts++;
		console_say("MIC: THE ADC GAVE NOTHING FOR %d MS, STARTED AGAIN (%u)", STALL_MS, restarts);
		adc_continuous_stop(handle);
		adc_continuous_start(handle);
		last_result_ms = now;
	}
	if (now - second_began_ms < MS_A_SECOND)
		return;
	last_second.results = results_a_second;
	last_second.strangers = strangers_a_second;
	last_second.restarts = restarts;
	results_a_second = 0;
	strangers_a_second = 0;
	last_second.peak = peak;
	last_second.rms = 0;
	if (counted > 0)
		last_second.rms = square_root(squares / counted);
	last_second.samples_a_second = (int)((int64_t)counted * 2 * MS_A_SECOND / (now - second_began_ms));
	peak = 0;
	squares = 0;
	counted = 0;
	second_began_ms = now;
}

int microphone_frame(int16_t samples[TELEPHONE_FRAME_SAMPLES]) {
	if (frame_count == 0)
		return 0;
	memcpy(samples, frames[frame_out], sizeof(frames[frame_out]));
	frame_out = (frame_out + 1) % FRAMES_KEPT;
	frame_count--;
	return 1;
}

void microphone_levels(struct microphone_levels *levels) {
	*levels = last_second;
}
