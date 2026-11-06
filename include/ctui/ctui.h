/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

#ifndef CTUI_H
#define CTUI_H

#include <ctui/barchart.h>
#include <ctui/bigtext.h>
#include <ctui/burger.h>
#include <ctui/canvas.h>
#include <ctui/dialog.h>
#include <ctui/durationinput.h>
#include <ctui/form.h>
#include <ctui/frame.h>
#include <ctui/gauge.h>
#include <ctui/heatmap.h>
#include <ctui/helpview.h>
#include <ctui/hexview.h>
#include <ctui/histogram.h>
#include <ctui/input.h>
#include <ctui/inputrule.h>
#include <ctui/keyboard.h>
#include <ctui/knob.h>
#include <ctui/labels.h>
#include <ctui/layout.h>
#include <ctui/linechart.h>
#include <ctui/list.h>
#include <ctui/logview.h>
#include <ctui/meter.h>
#include <ctui/numinput.h>
#include <ctui/pads.h>
#include <ctui/palette.h>
#include <ctui/panel.h>
#include <ctui/picker.h>
#include <ctui/piechart.h>
#include <ctui/richtext.h>
#include <ctui/scrollbar.h>
#include <ctui/scrollview.h>
#include <ctui/slider.h>
#include <ctui/sparkline.h>
#include <ctui/spinner.h>
#include <ctui/statusbar.h>
#include <ctui/stepseq.h>
#include <ctui/tabbar.h>
#include <ctui/table.h>
#include <ctui/tabpanel.h>
#include <ctui/text.h>
#include <ctui/textarea.h>
#include <ctui/textview.h>
#include <ctui/theme.h>
#include <ctui/timeinput.h>
#include <ctui/toast.h>
#include <ctui/tree.h>
#include <ctui/waveform.h>
#include <ctui/wizard.h>

#define CTUI_VERSION "0.0.0"

/* Returns the library version as a static string, e.g. "0.0.0". */
const char *ctui_version(void);

#endif /* CTUI_H */
