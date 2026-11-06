/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * calendar_color.c - colours of the calendar events (calendar_color.h).
 */
#include <ctui/calendar_color.h>

static const struct {
	const char *name;
	ctui_tone tone;
} COLORS[CTUI_CAL_COLORS] = {
    {"green", CTUI_TONE_OK},	{"red", CTUI_TONE_BAD},
    {"yellow", CTUI_TONE_BUSY}, {"blue", CTUI_TONE_INFO},
    {"grey", CTUI_TONE_IDLE},	{"dark", CTUI_TONE_DARK},
};

bool ctui_cal_color_valid(int index)
{
	return index >= 0 && index <= CTUI_CAL_COLORS;
}

bool ctui_cal_color_tone(int index, ctui_tone *tone)
{
	if (index < 1 || index > CTUI_CAL_COLORS)
		return false;
	if (tone)
		*tone = COLORS[index - 1].tone;
	return true;
}

const char *ctui_cal_color_name(int index)
{
	return index >= 1 && index <= CTUI_CAL_COLORS ? COLORS[index - 1].name
						      : "none";
}

int ctui_cal_color_next(int index, int delta)
{
	long n = CTUI_CAL_COLORS + 1;
	if (!ctui_cal_color_valid(index))
		index = 0;
	return (int)((((long)index + delta) % n + n) % n);
}

int ctui_cal_color_attr(int index, bool bold)
{
	ctui_tone tone;
	return ctui_cal_color_tone(index, &tone) ? ctui_attr_segment(tone, bold)
						 : 0;
}
