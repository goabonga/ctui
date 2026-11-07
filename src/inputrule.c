/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * inputrule.c - rules for the text of an input (inputrule.h).
 */
#include <ctui/inputrule.h>

#include <string.h>

/* ---- helpers on the compact text -------------------------------- */

static bool digit(int ch)
{
	return ch >= '0' && ch <= '9';
}

static bool alnum(int ch)
{
	return digit(ch) || (ch >= 'a' && ch <= 'z') ||
	       (ch >= 'A' && ch <= 'Z');
}

static size_t digits_in(const char *s)
{
	size_t n = 0;
	for (; *s; s++)
		n += digit((unsigned char)*s) ? 1U : 0U;
	return n;
}

bool ctui_luhn_valid(const char *digits)
{
	if (!digits || !*digits)
		return false;
	size_t len = strlen(digits);
	int sum = 0;
	bool second = false;
	for (size_t i = len; i-- > 0;) {
		if (!digit((unsigned char)digits[i]))
			return false;
		int d = digits[i] - '0';
		if (second) {
			d *= 2;
			if (d > 9)
				d -= 9;
		}
		sum += d;
		second = !second;
	}
	return sum % 10 == 0;
}

/* ---- the ready-made rules --------------------------------------- */

const ctui_input_rule ctui_rule_password = {
    .name = "password",
    .mask = true,
};

static bool email_accept(const char *compact, size_t index, int ch)
{
	(void)index;
	if (ch == '@')
		return strchr(compact, '@') == NULL;
	return alnum(ch) || ch == '.' || ch == '_' || ch == '%' || ch == '+' ||
	       ch == '-';
}

/* local@domain.tld: a local part, one @, a domain of labels separated by
 * dots with at least one dot, no empty label. */
static bool email_valid(const char *compact)
{
	const char *at = strchr(compact, '@');
	if (!at || at == compact || strchr(at + 1, '@'))
		return false;
	const char *domain = at + 1;
	if (!*domain || *domain == '.' || domain[strlen(domain) - 1] == '.' ||
	    !strchr(domain, '.') || strstr(compact, ".."))
		return false;
	return compact[0] != '.' && at[-1] != '.';
}

const ctui_input_rule ctui_rule_email = {
    .name = "email",
    .max_length = 254,
    .hint = "name@example.com",
    .accept = email_accept,
    .valid = email_valid,
};

static bool phone_accept(const char *compact, size_t index, int ch)
{
	(void)compact;
	return digit(ch) || (ch == '+' && index == 0);
}

static bool phone_valid(const char *compact)
{
	size_t n = digits_in(compact);
	return n >= 7 && n <= 15;
}

const ctui_input_rule ctui_rule_phone = {
    .name = "phone",
    .separators = " .-()",
    .max_length = 16,
    .hint = "+33 6 12 34 56 78",
    .accept = phone_accept,
    .valid = phone_valid,
};

static bool card_accept(const char *compact, size_t index, int ch)
{
	(void)compact;
	(void)index;
	return digit(ch);
}

/* Groups of four digits. */
static void card_format(const char *compact, char *out, size_t cap)
{
	size_t o = 0;
	for (size_t i = 0; compact[i] && o + 2 < cap; i++) {
		if (i && i % 4 == 0)
			out[o++] = ' ';
		out[o++] = compact[i];
	}
	if (cap)
		out[o] = '\0';
}

static bool card_valid(const char *compact)
{
	size_t n = strlen(compact);
	return n >= 13 && n <= 19 && ctui_luhn_valid(compact);
}

const ctui_input_rule ctui_rule_card = {
    .name = "card",
    .separators = " ",
    .max_length = 19,
    .hint = "1234 5678 9012 3456",
    .accept = card_accept,
    .format = card_format,
    .valid = card_valid,
};

/* ---- using a rule ----------------------------------------------- */

bool ctui_rule_is_separator(const ctui_input_rule *rule, int ch)
{
	return rule && rule->separators && ch > 0 &&
	       strchr(rule->separators, ch) != NULL;
}

size_t ctui_rule_compact(const ctui_input_rule *rule, const char *text,
			 char *out, size_t cap)
{
	size_t n = 0;
	if (!cap)
		return 0;
	for (; text && *text && n + 1 < cap; text++)
		if (!ctui_rule_is_separator(rule, (unsigned char)*text))
			out[n++] = *text;
	out[n] = '\0';
	return n;
}

bool ctui_rule_accepts(const ctui_input_rule *rule, const char *compact,
		       size_t index, int ch)
{
	if (!rule || (!rule->accept && !rule->max_length))
		return true;
	if (ch < 32 || ch > 126)
		return false;
	if (rule->max_length && strlen(compact) >= rule->max_length)
		return false;
	return rule->accept ? rule->accept(compact, index, ch) : true;
}

void ctui_rule_format(const ctui_input_rule *rule, const char *compact,
		      char *out, size_t cap)
{
	if (!cap)
		return;
	if (rule && rule->format) {
		rule->format(compact, out, cap);
		return;
	}
	size_t n = strlen(compact);
	if (n >= cap)
		n = cap - 1;
	memcpy(out, compact, n);
	out[n] = '\0';
}

bool ctui_rule_valid(const ctui_input_rule *rule, const char *compact)
{
	return !rule || !rule->valid || rule->valid(compact);
}
