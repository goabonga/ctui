/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * inputrule.h - rules for the text of an input: which characters may be
 * typed, how the text is laid out and when it is complete and correct.
 *
 * A rule is a plain struct of settings and callbacks, so a program adds its
 * own the same way as the ready-made ones (password, e-mail address, phone
 * number, bank card number). The rule works on the compact text: what the
 * user typed without the characters listed in `separators`, which only lay
 * the text out (the spaces of a card number). Set the rule of an input with
 * ctui_input_set_rule() (input.h).
 *
 *   accept   may this character be typed as character `index` of the compact
 *            text? (NULL: any printable ASCII character)
 *   max_length  how long the compact text may get (0: no limit)
 *   format   lays the compact text out into the text shown, separators
 *            included; the input does it after every edit and keeps the
 *            cursor on the same character (NULL: the text stays as typed,
 *            separators too)
 *   valid    is the compact text complete and correct? (NULL: always)
 *
 * Only ASCII characters are accepted by a rule that has an `accept`
 * callback or a `max_length`; a rule without them lets any text through.
 * Everything here is pure and needs no screen.
 */
#ifndef CTUI_INPUTRULE_H
#define CTUI_INPUTRULE_H

#include <stdbool.h>
#include <stddef.h>

typedef struct ctui_input_rule {
	const char *name;
	const char *separators; /* layout-only characters, may be NULL */
	size_t max_length;	/* of the compact text, 0 = no limit */
	bool mask;		/* show '*' instead of the characters */
	const char *hint;	/* shown while empty, may be NULL */
	bool (*accept)(const char *compact, size_t index, int ch);
	void (*format)(const char *compact, char *out, size_t cap);
	bool (*valid)(const char *compact);
} ctui_input_rule;

/* The ready-made rules. */
extern const ctui_input_rule ctui_rule_password; /* masked, any text */
extern const ctui_input_rule ctui_rule_email;	 /* local@domain.tld */
extern const ctui_input_rule ctui_rule_phone;	 /* + and 7 to 15 digits */
extern const ctui_input_rule ctui_rule_card;	 /* 13 to 19 digits, Luhn */

/* Copy text into out (cap bytes, always terminated) without the separators
 * of the rule; the length of the result. A NULL rule copies the text. */
size_t ctui_rule_compact(const ctui_input_rule *rule, const char *text,
			 char *out, size_t cap);

/* Whether ch is one of the separators of the rule. */
bool ctui_rule_is_separator(const ctui_input_rule *rule, int ch);

/* Whether a character may be typed as character `index` of the compact
 * text (see accept, max_length); a NULL rule accepts everything. */
bool ctui_rule_accepts(const ctui_input_rule *rule, const char *compact,
		       size_t index, int ch);

/* Lay the compact text out (the compact text itself without a format
 * callback). */
void ctui_rule_format(const ctui_input_rule *rule, const char *compact,
		      char *out, size_t cap);

/* Whether the compact text satisfies the rule (always for a NULL rule or a
 * rule without a valid callback). */
bool ctui_rule_valid(const ctui_input_rule *rule, const char *compact);

/* Luhn checksum of a string of digits (at least one, nothing else). */
bool ctui_luhn_valid(const char *digits);

#endif /* CTUI_INPUTRULE_H */
