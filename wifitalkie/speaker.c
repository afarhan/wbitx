// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ashhar Farhan
//
// speaker.c — the speech heard, through a queue, played by PWM.
//
// THE OUTPUT
//
// The C3 has no DAC. PIN_SPEAKER_LEFT and PIN_SPEAKER_RIGHT, the headset
// jack's two sides, carry PWM from two channels of the LEDC on one timer, at
// PWM_HZ, 8 bits, far above hearing. The VoIP is one voice, so both are
// given the same duty; music in stereo could one day give each its own. It drives earbuds, 16 to 32 ohms, without an
// amplifier: the pin is set to its strongest drive, so that it is little
// more than its few tens of ohms. Outside the chip, a resistor in the way
// would lose the level the earbuds need, so the filter is an inductor and
// a capacitor, 1 mH and 1 uF for 32 ohms, 470 uH and 2.2 uF for 16, about
// 5 kHz, its impedance the earbuds' so that they damp it; 10 ohms before it
// holds the pin's current down with the earbuds out, and 220 uF keeps the
// middle level, 1.65 V, out of the earbuds.
//
// A timer interrupts PLAYBACK_RATE_HZ times a second, every fifth cycle of
// the PWM, both counted from the same 80 MHz, and sets the next duty from
// the ring, which the loop keeps filled RING_MS ahead; an empty ring holds
// the duty at the middle, silence. The interrupt writes the LEDC's
// registers itself, through the HAL's inline functions, since the driver's
// calls take locks that an interrupt may not. playback.c makes the duties,
// why at that rate is written there.
//
// THE QUEUE
//
// Packets come over WiFi and the internet unevenly, and the sender's clock
// is not the talkie's. So the speech goes to a queue, at 8000 a second, and
// playing begins once it holds START_MS. A gap in the packets is played as
// silence. Half a second with nothing, END_MS, is the end of an over, and
// the next begins by filling the queue again.
//
// The two clocks are met by reading the queue at a pace a little faster or
// slower than one sample a sample, between 0.99 and 1.01, set from how full
// the queue has been; playback.c's filter makes the samples between. The
// author heard crackle in the web page's player on 12 October 2026 when it
// dropped a whole sample in a hundred to keep up; so here no whole sample
// is ever dropped or repeated while an over plays. Only a queue gone past
// MOST_MS, the internet having held up packets and let them go at once, is
// cut back to TARGET_MS, once.
//
// VOL is put on each sample made, and playback.c makes it a duty for the
// ring. While the PTT is down the queue is emptied and kept empty: the
// talkie is one way at a time.

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "driver/gptimer.h"
#include "esp_attr.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "hal/ledc_ll.h"
#include "soc/ledc_struct.h"

#include "pins.h"
#include "playback.h"
#include "settings.h"
#include "speaker.h"
#include "telephone.h"

#define PWM_BITS LEDC_TIMER_8_BIT
#define PWM_HZ 312500
#define PWM_MIDDLE PLAYBACK_DUTY_MIDDLE
#define PWM_LEFT LEDC_CHANNEL_0
#define PWM_RIGHT LEDC_CHANNEL_1
#define PWM_TIMER LEDC_TIMER_0
#define PWM_MODE LEDC_LOW_SPEED_MODE
#define TIMER_HZ 8000000
#define TICKS_A_SAMPLE (TIMER_HZ / PLAYBACK_RATE_HZ)
#define RING_SAMPLES 4096
#define RING_MS 32
#define RING_FILLED_MOST (PLAYBACK_RATE_HZ / 1000 * RING_MS)
#define QUEUE_SAMPLES 4096
#define SAMPLES_A_MS (TELEPHONE_RATE_HZ / 1000)
#define START_MS 80
#define TARGET_MS 80
#define MOST_MS 300
#define END_MS 500
#define PACE_ONE 65536
#define PACE_MOST_OFF 655
#define PACE_FULL_OFF_SAMPLES (120 * SAMPLES_A_MS)
#define FILL_SMOOTH_SHIFT 6
#define FILL_FRACTION_SHIFT 8

static uint8_t ring[RING_SAMPLES];
static volatile uint32_t ring_in;
static volatile uint32_t ring_out;
static int16_t queue[QUEUE_SAMPLES];
static uint32_t queue_in;
static uint32_t queue_out;
static int32_t fill_smoothed;
static int is_playing;
static int is_muted;
static long taken_ms;
static struct playback play;
static gptimer_handle_t timer;

_Static_assert(PWM_HZ % PLAYBACK_RATE_HZ == 0, "each sample is held for whole cycles of the PWM");
_Static_assert(TIMER_HZ % PLAYBACK_RATE_HZ == 0, "the timer counts a whole number to each sample");
_Static_assert(RING_FILLED_MOST < RING_SAMPLES, "the ring holds what the loop keeps ahead");
static uint32_t count_gap_samples;
static uint32_t count_cut;

static inline void IRAM_ATTR duty_set(ledc_channel_t channel, uint32_t duty) {
	ledc_ll_set_duty_int_part(&LEDC, PWM_MODE, channel, duty);
	ledc_ll_set_duty_direction(&LEDC, PWM_MODE, channel, LEDC_DUTY_DIR_INCREASE);
	ledc_ll_set_duty_num(&LEDC, PWM_MODE, channel, 1);
	ledc_ll_set_duty_cycle(&LEDC, PWM_MODE, channel, 1);
	ledc_ll_set_duty_scale(&LEDC, PWM_MODE, channel, 0);
	ledc_ll_set_duty_start(&LEDC, PWM_MODE, channel, true);
	ledc_ll_ls_channel_update(&LEDC, PWM_MODE, channel);
}

static bool IRAM_ATTR next_duty(gptimer_handle_t t, const gptimer_alarm_event_data_t *event, void *context) {
	uint32_t duty = PWM_MIDDLE;

	(void)t;
	(void)event;
	(void)context;
	if (ring_out != ring_in) {
		duty = ring[ring_out % RING_SAMPLES];
		ring_out++;
	}
	duty_set(PWM_LEFT, duty);
	duty_set(PWM_RIGHT, duty);
	return false;
}

void speaker_open(void) {
	ledc_timer_config_t pwm_timer = {
		.speed_mode = PWM_MODE,
		.duty_resolution = PWM_BITS,
		.timer_num = PWM_TIMER,
		.freq_hz = PWM_HZ,
		.clk_cfg = LEDC_AUTO_CLK,
	};
	ledc_channel_config_t left = {
		.gpio_num = PIN_SPEAKER_LEFT,
		.speed_mode = PWM_MODE,
		.channel = PWM_LEFT,
		.timer_sel = PWM_TIMER,
		.duty = PWM_MIDDLE,
		.hpoint = 0,
	};
	ledc_channel_config_t right = {
		.gpio_num = PIN_SPEAKER_RIGHT,
		.speed_mode = PWM_MODE,
		.channel = PWM_RIGHT,
		.timer_sel = PWM_TIMER,
		.duty = PWM_MIDDLE,
		.hpoint = 0,
	};
	gptimer_config_t timer_config = {
		.clk_src = GPTIMER_CLK_SRC_DEFAULT,
		.direction = GPTIMER_COUNT_UP,
		.resolution_hz = TIMER_HZ,
	};
	gptimer_alarm_config_t alarm = {
		.alarm_count = TICKS_A_SAMPLE,
		.reload_count = 0,
		.flags.auto_reload_on_alarm = true,
	};
	gptimer_event_callbacks_t callbacks = {
		.on_alarm = next_duty,
	};

	playback_open();
	playback_begin(&play);
	ledc_timer_config(&pwm_timer);
	ledc_channel_config(&left);
	ledc_channel_config(&right);
	gpio_set_drive_capability(PIN_SPEAKER_LEFT, GPIO_DRIVE_CAP_3);
	gpio_set_drive_capability(PIN_SPEAKER_RIGHT, GPIO_DRIVE_CAP_3);
	gptimer_new_timer(&timer_config, &timer);
	gptimer_set_alarm_action(timer, &alarm);
	gptimer_register_event_callbacks(timer, &callbacks, NULL);
	gptimer_enable(timer);
	gptimer_start(timer);
}

static uint32_t queue_fill(void) {
	return queue_in - queue_out;
}

static void queue_empty(void) {
	queue_out = queue_in;
	is_playing = 0;
	playback_begin(&play);
}

void speaker_take(const uint8_t *ulaw, long now) {
	int i;

	if (is_muted)
		return;
	if (queue_fill() + TELEPHONE_FRAME_SAMPLES > MOST_MS * SAMPLES_A_MS) {
		queue_out = queue_in - TARGET_MS * SAMPLES_A_MS;
		count_cut++;
	}
	for (i = 0; i < TELEPHONE_FRAME_SAMPLES; i++) {
		queue[queue_in % QUEUE_SAMPLES] = telephone_linear_of(ulaw[i]);
		queue_in++;
	}
	taken_ms = now;
}

static void ring_put(uint8_t duty) {
	ring[ring_in % RING_SAMPLES] = duty;
	ring_in++;
}

// the pace of reading the queue, one being PACE_ONE: faster when it holds
// more than TARGET_MS, slower when less
static uint32_t pace(void) {
	int32_t off;

	fill_smoothed += (((int32_t)queue_fill() << FILL_FRACTION_SHIFT) - fill_smoothed) >> FILL_SMOOTH_SHIFT;
	off = (int32_t)(((int64_t)(fill_smoothed >> FILL_FRACTION_SHIFT) - TARGET_MS * SAMPLES_A_MS)
		* PACE_MOST_OFF / PACE_FULL_OFF_SAMPLES);
	if (off > PACE_MOST_OFF)
		off = PACE_MOST_OFF;
	if (off < -PACE_MOST_OFF)
		off = -PACE_MOST_OFF;
	return (uint32_t)(PACE_ONE + off);
}

// the next of the queue's samples, or silence in a gap
static int16_t queue_next(void) {
	int16_t sample;

	if (queue_fill() == 0) {
		count_gap_samples++;
		return 0;
	}
	sample = queue[queue_out % QUEUE_SAMPLES];
	queue_out++;
	return sample;
}

void speaker_poll(long now) {
	int32_t level;
	uint32_t step;

	if (!is_playing && queue_fill() >= START_MS * SAMPLES_A_MS) {
		is_playing = 1;
		fill_smoothed = (int32_t)queue_fill() << FILL_FRACTION_SHIFT;
	}
	if (is_playing && queue_fill() == 0 && now - taken_ms > END_MS)
		queue_empty();
	if (!is_playing)
		return;
	step = (uint32_t)((uint64_t)pace() * TELEPHONE_RATE_HZ / PLAYBACK_RATE_HZ);
	while (ring_in - ring_out < RING_FILLED_MOST) {
		while (playback_wants(&play))
			playback_push(&play, queue_next());
		// a full-scale sample at VOL 100 is 128 steps of duty, in 256ths
		level = (int32_t)playback_make(&play, step) * (int32_t)settings.volume / SETTINGS_LEVEL_MOST;
		ring_put(playback_duty(&play, level));
	}
}

// the timer is stopped while the speaker is muted, the PWM held at the
// middle: its interrupts, 62500 a second, draw current from the supply in
// pulses the microphone hears, folded by the ADC's 16000 a second to
// about 1.6 kHz, as was found on 30 October 2026
void speaker_mute(int is_muting) {
	if (is_muting == is_muted)
		return;
	is_muted = is_muting;
	if (!is_muted) {
		gptimer_start(timer);
		return;
	}
	queue_empty();
	gptimer_stop(timer);
	ring_out = ring_in;
	ledc_set_duty(PWM_MODE, PWM_LEFT, PWM_MIDDLE);
	ledc_update_duty(PWM_MODE, PWM_LEFT);
	ledc_set_duty(PWM_MODE, PWM_RIGHT, PWM_MIDDLE);
	ledc_update_duty(PWM_MODE, PWM_RIGHT);
}

void speaker_describe(char *text, int room) {
	const char *doing = "QUIET";

	if (is_playing)
		doing = "PLAYING";
	snprintf(text, room, "%s QUEUE %u MS, SILENCE IN GAPS %u MS, CUT %u, VOL %u", doing,
		(unsigned)(queue_fill() / SAMPLES_A_MS), (unsigned)(count_gap_samples / SAMPLES_A_MS),
		(unsigned)count_cut, (unsigned)settings.volume);
}
