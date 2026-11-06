/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

#ifndef CTUI_H
#define CTUI_H

#include <ctui/dialog.h>
#include <ctui/frame.h>
#include <ctui/gauge.h>
#include <ctui/labels.h>
#include <ctui/list.h>
#include <ctui/meter.h>
#include <ctui/panel.h>
#include <ctui/scrollbar.h>
#include <ctui/sparkline.h>
#include <ctui/spinner.h>
#include <ctui/statusbar.h>
#include <ctui/tabbar.h>
#include <ctui/text.h>
#include <ctui/theme.h>

#define CTUI_VERSION "0.0.0"

/* Returns the library version as a static string, e.g. "0.0.0". */
const char *ctui_version(void);

#endif /* CTUI_H */
