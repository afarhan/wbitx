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
// What comes out of the 8000 a second goes as it is: speech.c's high-pass
// was taken out on 6 November 2026. speech.c's gate sends the gaps between words as
// comfort noise, while settings.is_gate, the verb GATE, has it on; it is on
// unless turned off. It was made for a mic element whose voice stood little
// above the noise, and keeps the gaps quiet with any.
//
// Each sample also goes to vox.c, before the gate, which keys the talkie
// by the voice while settings.vox is on: microphone_vox() says whether it
// has, and microphone_vox_hold() holds it off while voip.c hears a voice.
// LEVELS measures before the gate.
//
// microphone_pin() reads another pin from now on, until the next power-up:
// GPIO0 to GPIO4 are ADC1's, for trying which is quietest.
//
// The battery shares the ADC: PIN_BATTERY, behind a 100k and 100k divider
// from the 5 V line, is the pattern's second channel, so the ADC runs at
// twice SAMPLE_HZ and each channel has SAMPLE_HZ. On the hand-wired talkie
// the divider is on the 5 V line, the cell through the Schottky,
// BATTERY_DIODE_MV below it, and what is read is taken to be the cell's,
// the diode added back; with the USB in it is the USB's, over the top of
// the ADC, and reads high, but it is shown all the same, at the author's
// asking. On the wificomms board, BATTERY_DIVIDER_ON_CELL, it is on the
// cell, 10k and 10k against the SuperMini's 4.7k pull-up, and battery.c
// turns the pin's volts into the cell's, with the board's offset,
// settings.battery_offset_mv.
// (The C3 cannot read a pin by itself, one-shot, while ADC1 runs
// continuously.)
//
// Each pin the ADC reads has its pull-up and pull-down turned off first,
// unheard(): the C3's weak pull-up, some 45k, is on at reset on some of
// them, GPIO2 among them, a strapping pin, and it reads 3.3 V with nothing
// on it, the author's measure of 2026-11-10. Against a divider of two 100k,
// 50k seen from the pin, it would pull the reading half way to 3.3 V; on a
// microphone's pin it would move the preamplifier's bias.
//
// PIN_MIC_POWER feeds both microphones' preamplifiers, and holds the
// PAM8403's shutdown up: microphone_open() raises it, at its strongest.
//
// On 30 October 2026 the ADC was twice found giving nothing from the start;
// at other starts it ran. When it was next caught, it had stopped just as
// the WiFi came up, whose radio's calibration uses the same SAR ADC: that
// is the likeliest cause, not yet proved. So the results the driver gives
// are counted, those not of our channel too, and an ADC that has given
// nothing for STALL_MS is stopped and started again, and says so.

#include <string.h>

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_continuous.h"
#include "driver/gpio.h"
#include "esp_timer.h"

#include "battery.h"
#include "console.h"
#include "microphone.h"
#include "pins.h"
#include "settings.h"
#include "speech.h"
#include "vox.h"

#define SAMPLE_HZ DOUBLE_RATE_HZ
#define STORE_BYTES 4096
#define FRAME_BYTES 256
#define FRAMES_KEPT 4
#define ADC_MIDDLE 2048
#define TO_16_BITS 4
#define MEAN_SHIFT 7
#define MEAN_FRACTION_SHIFT 8
#define MIC_AS_IT_CAME 50
// the mic's level raised four times over, the author's of 2026-11-10:
// the twelve bits of the ADC then fill the sixteen at MIC 50
#define MIC_BOOST 4
#define INT16_MOST 32767
#define MS_A_SECOND 1000
#define STALL_MS 500
#define ADC_CODES 4096
#define BATTERY_SHIFT 10           // its mean's corner, well under 1 Hz
#if BATTERY_DIVIDER_ON_CELL
#define BATTERY_DIODE_MV 0
#else
#define BATTERY_DIODE_MV 300       // the Schottky between the cell and 5 V
#endif

static adc_continuous_handle_t handle;
static adc_channel_t channel;
static adc_channel_t battery_channel;
static int is_battery_read;
static int32_t battery_mean;
static int battery_count;
static adc_cali_handle_t cali;
static int is_cali;
static int is_open;
static int32_t mean;
static struct telephone_down down;
static struct speech speech;
static struct vox vox;
static int is_vox_held;
static int16_t frames[FRAMES_KEPT][TELEPHONE_FRAME_SAMPLES];
static int frame_in;
static int frame_out;
static int frame_count;
static int filled;
static int peak;
static int vox_peak;
static int64_t squares;
static int counted;
static int results_a_second;
static int strangers_a_second;
static long last_result_ms;
static unsigned restarts;
static long second_began_ms;
static struct microphone_levels last_second;

// a pin the ADC reads let float: neither pull, so that only what is on
// it is read
static void unheard(int pin) {
	gpio_set_pull_mode((gpio_num_t)pin, GPIO_FLOATING);
}

static void open_on(int pin) {
	adc_continuous_handle_cfg_t store = {
		.max_store_buf_size = STORE_BYTES,
		.conv_frame_size = FRAME_BYTES,
		.flags.flush_pool = 1,
	};
	adc_digi_pattern_config_t pattern[2];
	adc_continuous_config_t config;
	adc_unit_t unit;
	adc_unit_t battery_unit;

	telephone_down_begin(&down);
	speech_begin(&speech, TELEPHONE_RATE_HZ);
	vox_begin(&vox);
	mean = ADC_MIDDLE << MEAN_FRACTION_SHIFT;
	if (adc_continuous_io_to_channel(pin, &unit, &channel) != ESP_OK || unit != ADC_UNIT_1) {
		console_say("MIC: PIN %d IS NOT ON ADC1", pin);
		return;
	}
	unheard(pin);
	unheard(PIN_BATTERY);
	memset(pattern, 0, sizeof(pattern));
	pattern[0].atten = ADC_ATTEN_DB_12;
	pattern[0].channel = channel;
	pattern[0].unit = ADC_UNIT_1;
	pattern[0].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;
	pattern[1] = pattern[0];
	is_battery_read = pin != PIN_BATTERY
		&& adc_continuous_io_to_channel(PIN_BATTERY, &battery_unit, &battery_channel) == ESP_OK
		&& battery_unit == ADC_UNIT_1;
	pattern[1].channel = battery_channel;
	battery_count = 0;
	memset(&config, 0, sizeof(config));
	config.pattern_num = is_battery_read ? 2 : 1;
	config.adc_pattern = pattern;
	config.sample_freq_hz = SAMPLE_HZ * config.pattern_num;
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

// the preamplifiers and the PAM8403, on or off together
void microphone_power(int is_on) {
	gpio_set_level(PIN_MIC_POWER, is_on ? 1 : 0);
}

// idle (power.c): the preamplifiers and the PAM8403 off and the ADC
// stopped; awake, both again
static int is_idle;

void microphone_idle(int is_idling) {
	if (is_idling == is_idle)
		return;
	is_idle = is_idling;
	microphone_power(!is_idle);
	if (!is_open)
		return;
	if (is_idle) {
		adc_continuous_stop(handle);
	} else {
		adc_continuous_start(handle);
		last_result_ms = (long)(esp_timer_get_time() / 1000);
	}
}

void microphone_open(void) {
	gpio_config_t power = {
		.pin_bit_mask = 1ULL << PIN_MIC_POWER,
		.mode = GPIO_MODE_OUTPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE,
	};
	adc_cali_curve_fitting_config_t fitting = {
		.unit_id = ADC_UNIT_1,
		.chan = ADC_CHANNEL_0,
		.atten = ADC_ATTEN_DB_12,
		.bitwidth = ADC_BITWIDTH_12,
	};

	gpio_config(&power);
	gpio_set_drive_capability(PIN_MIC_POWER, GPIO_DRIVE_CAP_3);
	microphone_power(1);
	is_cali = adc_cali_create_scheme_curve_fitting(&fitting, &cali) == ESP_OK;
	open_on(settings.is_headset ? PIN_HEADSET_MIC : PIN_MICROPHONE);
}

void microphone_pin(int pin) {
	if (is_open) {
		adc_continuous_stop(handle);
		adc_continuous_deinit(handle);
		is_open = 0;
	}
	open_on(pin);
	if (is_open)
		console_say("MIC ON GPIO%d", pin);
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
	sample = sample * (int32_t)settings.mic * MIC_BOOST / MIC_AS_IT_CAME;
	if (sample > INT16_MOST)
		sample = INT16_MOST;
	if (sample < -INT16_MOST)
		sample = -INT16_MOST;
	if (!telephone_down_take(&down, (int16_t)sample, &kept))
		return;
	measure(kept);
	vox_take(&vox, kept, (int)settings.vox, is_vox_held, TELEPHONE_RATE_HZ);
	if (vox_size(&vox) > vox_peak)
		vox_peak = vox_size(&vox);
	if (settings.is_gate)
		kept = speech_gate(&speech, kept, TELEPHONE_RATE_HZ);
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

// what a code is in millivolts, by the ADC's own calibration, or 0
// without one
static int code_millivolts(int code) {
	int millivolts = 0;

	if (!is_cali || adc_cali_raw_to_voltage(cali, code, &millivolts) != ESP_OK)
		return 0;
	return millivolts;
}

static int adc_millivolts(void) {
	return code_millivolts((int)(mean >> MEAN_FRACTION_SHIFT));
}

static void take_battery(int raw) {
	if (battery_count == 0)
		battery_mean = raw << MEAN_FRACTION_SHIFT;
	else
		battery_mean += ((raw << MEAN_FRACTION_SHIFT) - battery_mean) >> BATTERY_SHIFT;
	if (battery_count < (1 << BATTERY_SHIFT))
		battery_count++;
}

// the cell, in millivolts: MICROPHONE_BATTERY when read,
// MICROPHONE_UNKNOWN before a second of readings or while MICPIN reads the battery's pin
// for BATTERY: the battery pin's mean code, as the ADC gives it, and the
// millivolts at the pin by the ADC's calibration, before the divider and
// the diode are put back
void microphone_battery_raw(int *code, int *pin_millivolts) {
	*code = (int)(battery_mean >> MEAN_FRACTION_SHIFT);
	*pin_millivolts = code_millivolts(*code);
}

int microphone_battery(int *millivolts) {
	int code = (int)(battery_mean >> MEAN_FRACTION_SHIFT);

	*millivolts = 0;
	if (!is_open || !is_battery_read || battery_count < (1 << BATTERY_SHIFT) || !is_cali)
		return MICROPHONE_UNKNOWN;
#if BATTERY_DIVIDER_ON_CELL
	*millivolts = battery_cell_mv(code_millivolts(code), settings.battery_offset_mv);
#else
	*millivolts = 2 * code_millivolts(code) + BATTERY_DIODE_MV;
#endif
	return MICROPHONE_BATTERY;
}

void microphone_poll(long now) {
	uint8_t results[FRAME_BYTES];
	adc_digi_output_data_t *result;
	uint32_t got = 0;
	uint32_t i;

	if (!is_open || is_idle)
		return;
	while (adc_continuous_read(handle, results, sizeof(results), &got, 0) == ESP_OK) {
		last_result_ms = now;
		for (i = 0; i + SOC_ADC_DIGI_RESULT_BYTES <= got; i += SOC_ADC_DIGI_RESULT_BYTES) {
			result = (adc_digi_output_data_t *)&results[i];
			results_a_second++;
			if (result->type2.unit == 0 && result->type2.channel == channel)
				take((int)result->type2.data);
			else if (is_battery_read && result->type2.unit == 0
					&& result->type2.channel == battery_channel)
				take_battery((int)result->type2.data);
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
	last_second.vox_peak = vox_peak;
	last_second.vox_background = vox_background(&vox);
	last_second.vox_threshold = vox_threshold(&vox, (int)settings.vox);
	vox_peak = 0;
	peak = 0;
	squares = 0;
	counted = 0;
	second_began_ms = now;
}

// 1 while the voice keys the talkie
int microphone_vox(void) {
	return is_open && vox_is_keyed(&vox);
}

// held off, the voice does not key the talkie, nor is the background learnt
void microphone_vox_hold(int is_held) {
	is_vox_held = is_held;
}

// for the screen's meter: the voice's size now, and the size that keys
void microphone_vox_meter(int *size, int *threshold) {
	*size = vox_size(&vox);
	*threshold = vox_threshold(&vox, (int)settings.vox);
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
