/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * image_pixels.h - the pixel graphics half of image.c (image.h).
 */
#ifndef CTUI_IMAGE_PIXELS_H
#define CTUI_IMAGE_PIXELS_H

#include <ctui/image.h>

/* Draw the image with the graphics protocol of the terminal: blank the cells
 * it covers and keep its escape sequence for ctui_image_flush(). False,
 * drawing nothing, when the terminal has no protocol, the image is not valid
 * or memory ran out. */
bool ctui_image_pixels_draw(const ctui_image *img, const ctui_image_view *view,
			    int y, int x, int cols, int rows);

#endif /* CTUI_IMAGE_PIXELS_H */
