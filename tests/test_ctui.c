/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

#include <ctui/ctui.h>

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond)                                                    \
	do {                                                           \
		if (!(cond)) {                                         \
			fprintf(stderr, "%s:%d: check failed: %s\n",   \
				__FILE__, __LINE__, #cond);            \
			failures++;                                    \
		}                                                      \
	} while (0)

static void test_version(void)
{
	const char *version = ctui_version();

	CHECK(version != NULL);
	if (version == NULL)
		return;
	CHECK(version[0] != '\0');
	CHECK(strcmp(version, CTUI_VERSION) == 0);
}

int main(void)
{
	test_version();
	if (failures) {
		fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}
	puts("all tests passed");
	return 0;
}
