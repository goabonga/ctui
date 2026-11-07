/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * test_inputrule.c - rules for the text of an input: the ready-made ones
 * (password, e-mail, phone, card), the helpers and a rule of one's own.
 */
#include "check.h"

#include <string.h>

#include <ctui/inputrule.h>

static void test_luhn(void)
{
	CHECK(ctui_luhn_valid("4111111111111111"));
	CHECK(ctui_luhn_valid("5500005555555559"));
	CHECK(ctui_luhn_valid("378282246310005"));
	CHECK(ctui_luhn_valid("0"));
	CHECK(!ctui_luhn_valid("4111111111111112"));
	CHECK(!ctui_luhn_valid("") && !ctui_luhn_valid(NULL));
	CHECK(!ctui_luhn_valid("4111 1111") && !ctui_luhn_valid("41a1"));
}

static void test_compact_and_separators(void)
{
	char out[32];
	CHECK(ctui_rule_compact(&ctui_rule_card, "4111 1111", out,
				sizeof out) == 8);
	CHECK_STR(out, "41111111");
	CHECK(ctui_rule_compact(&ctui_rule_phone, "+33 (6).12-34", out,
				sizeof out) == 8);
	CHECK_STR(out, "+336"
		       "1234");
	CHECK(ctui_rule_compact(NULL, "a b", out, sizeof out) == 3);
	CHECK_STR(out, "a b");
	CHECK(ctui_rule_compact(&ctui_rule_card, NULL, out, sizeof out) == 0);
	CHECK(ctui_rule_compact(&ctui_rule_card, "1234 5678", out, 5) == 4);
	CHECK_STR(out, "1234"); /* cut to the buffer, terminated */
	CHECK(ctui_rule_compact(&ctui_rule_card, "12", out, 0) == 0);
	CHECK(ctui_rule_is_separator(&ctui_rule_card, ' '));
	CHECK(!ctui_rule_is_separator(&ctui_rule_card, '-'));
	CHECK(!ctui_rule_is_separator(&ctui_rule_email, '.'));
	CHECK(!ctui_rule_is_separator(NULL, ' ') &&
	      !ctui_rule_is_separator(&ctui_rule_card, 0));
}

static void test_password(void)
{
	CHECK(ctui_rule_password.mask);
	CHECK(ctui_rule_accepts(&ctui_rule_password, "", 0, 'x'));
	CHECK(ctui_rule_accepts(&ctui_rule_password, "", 0, 0xe9));
	CHECK(ctui_rule_valid(&ctui_rule_password, ""));
}

static void test_email(void)
{
	const ctui_input_rule *r = &ctui_rule_email;
	CHECK(ctui_rule_accepts(r, "", 0, 'a') &&
	      ctui_rule_accepts(r, "a", 1, '.') &&
	      ctui_rule_accepts(r, "a", 1, '_') &&
	      ctui_rule_accepts(r, "a", 1, '+') &&
	      ctui_rule_accepts(r, "a", 1, '@'));
	CHECK(!ctui_rule_accepts(r, "a@b", 3, '@')); /* one @ only */
	CHECK(!ctui_rule_accepts(r, "a", 1, ' ') &&
	      !ctui_rule_accepts(r, "a", 1, ',') &&
	      !ctui_rule_accepts(r, "a", 1, 0xe9) &&
	      !ctui_rule_accepts(r, "a", 1, 0));
	CHECK(ctui_rule_valid(r, "ada@example.com"));
	CHECK(ctui_rule_valid(r, "ada.lovelace+x@mail.example.co.uk"));
	CHECK(!ctui_rule_valid(r, "") && !ctui_rule_valid(r, "ada"));
	CHECK(!ctui_rule_valid(r, "@example.com") &&
	      !ctui_rule_valid(r, "ada@") &&
	      !ctui_rule_valid(r, "ada@example") &&
	      !ctui_rule_valid(r, "ada@.com") &&
	      !ctui_rule_valid(r, "ada@example.") &&
	      !ctui_rule_valid(r, "ada@@example.com") &&
	      !ctui_rule_valid(r, "a..da@example.com") &&
	      !ctui_rule_valid(r, ".ada@example.com") &&
	      !ctui_rule_valid(r, "ada.@example.com") &&
	      !ctui_rule_valid(r, "ada@exa..mple.com"));
	char long_text[300];
	memset(long_text, 'a', sizeof long_text - 1);
	long_text[sizeof long_text - 1] = '\0';
	CHECK(!ctui_rule_accepts(r, long_text + 300 - 255, 254, 'a'));
}

static void test_phone(void)
{
	const ctui_input_rule *r = &ctui_rule_phone;
	CHECK(ctui_rule_accepts(r, "", 0, '+') &&
	      ctui_rule_accepts(r, "", 0, '6'));
	CHECK(!ctui_rule_accepts(r, "6", 1, '+')); /* only first */
	CHECK(!ctui_rule_accepts(r, "6", 1, 'a'));
	CHECK(ctui_rule_valid(r, "0612345678"));
	CHECK(ctui_rule_valid(r, "+33612345678"));
	CHECK(ctui_rule_valid(r, "1234567") &&
	      ctui_rule_valid(r, "123456789012345"));
	CHECK(!ctui_rule_valid(r, "123456") &&
	      !ctui_rule_valid(r, "1234567890123456") &&
	      !ctui_rule_valid(r, ""));
	CHECK(r->format == NULL); /* the text stays as typed */
	char out[32];
	ctui_rule_format(r, "06 12", out, sizeof out);
	CHECK_STR(out, "06 12");
	char tiny[4];
	ctui_rule_format(r, "0612345", tiny, sizeof tiny);
	CHECK_STR(tiny, "061");
	ctui_rule_format(r, "x", out, 0); /* nothing to write into */
}

static void test_card(void)
{
	const ctui_input_rule *r = &ctui_rule_card;
	char out[32];
	ctui_rule_format(r, "4111111111111111", out, sizeof out);
	CHECK_STR(out, "4111 1111 1111 1111");
	ctui_rule_format(r, "41111", out, sizeof out);
	CHECK_STR(out, "4111 1");
	ctui_rule_format(r, "", out, sizeof out);
	CHECK_STR(out, "");
	ctui_rule_format(r, "4111111111111111", out, 8);
	CHECK(strlen(out) < 8);
	CHECK(ctui_rule_accepts(r, "", 0, '4') &&
	      !ctui_rule_accepts(r, "", 0, 'x'));
	CHECK(!ctui_rule_accepts(r, "4111111111111111111", 19, '1')); /* 19 */
	CHECK(ctui_rule_valid(r, "4111111111111111"));
	CHECK(ctui_rule_valid(r, "378282246310005"));
	CHECK(!ctui_rule_valid(r, "4111111111111112") &&
	      !ctui_rule_valid(r, "411111111111") && !ctui_rule_valid(r, ""));
}

/* A rule of one's own: a code of two letters and four digits, "AB-1234". */
static bool code_accept(const char *compact, size_t index, int ch)
{
	(void)compact;
	return index < 2 ? (ch >= 'A' && ch <= 'Z') : (ch >= '0' && ch <= '9');
}

static void code_format(const char *compact, char *out, size_t cap)
{
	size_t o = 0;
	for (size_t i = 0; compact[i] && o + 2 < cap; i++) {
		if (i == 2)
			out[o++] = '-';
		out[o++] = compact[i];
	}
	out[o] = '\0';
}

static bool code_valid(const char *compact)
{
	return strlen(compact) == 6;
}

static void test_custom_rule(void)
{
	static const ctui_input_rule code = {.name = "code",
					     .separators = "-",
					     .max_length = 6,
					     .hint = "AB-1234",
					     .accept = code_accept,
					     .format = code_format,
					     .valid = code_valid};
	char out[16];
	CHECK(ctui_rule_accepts(&code, "", 0, 'Q') &&
	      !ctui_rule_accepts(&code, "", 0, '1'));
	CHECK(ctui_rule_accepts(&code, "QX", 2, '7') &&
	      !ctui_rule_accepts(&code, "QX", 2, 'Z'));
	CHECK(!ctui_rule_accepts(&code, "QX1234", 6, '5')); /* max_length */
	ctui_rule_format(&code, "QX1234", out, sizeof out);
	CHECK_STR(out, "QX-1234");
	CHECK(ctui_rule_valid(&code, "QX1234") &&
	      !ctui_rule_valid(&code, "QX12"));
	/* a rule with nothing set lets everything through */
	static const ctui_input_rule plain = {.name = "plain"};
	CHECK(ctui_rule_accepts(&plain, "", 0, 0xe9) &&
	      ctui_rule_accepts(NULL, "", 0, 'x'));
	CHECK(ctui_rule_valid(&plain, "anything") && ctui_rule_valid(NULL, ""));
	ctui_rule_format(NULL, "abc", out, sizeof out);
	CHECK_STR(out, "abc");
}

int main(void)
{
	RUN(test_luhn);
	RUN(test_compact_and_separators);
	RUN(test_password);
	RUN(test_email);
	RUN(test_phone);
	RUN(test_card);
	RUN(test_custom_rule);
	return CHECK_DONE();
}
