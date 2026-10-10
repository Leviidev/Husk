/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Text input for Flutter apps: the flutter/textinput channel and the editing state it keeps in step with Dart. */
#ifndef HUSK_TL_FLUTTER_TEXT_H
#define HUSK_TL_FLUTTER_TEXT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* What the keyboard should look like: a kind, and flags. */
enum {
    TL_FLUTTER_KB_TEXT = 0, TL_FLUTTER_KB_NUMBER = 1, TL_FLUTTER_KB_EMAIL = 2, TL_FLUTTER_KB_URL = 3,
    TL_FLUTTER_KB_MULTILINE = 0x10, TL_FLUTTER_KB_SECURE = 0x20,
};

/* A TextInput.* call from Dart (on the platform thread): the JSON reply. */
const char *tl_flutter_text_message(const char *method, const uint8_t *data, size_t len);

/* The app's keyboard: show (1, with the kind and flags) or hide (0). Called on the platform thread. */
void tl_flutter_text_set_keyboard_handler(void (*fn)(int show, int kind));

/* The keyboard's edits, from any thread: queued, then applied by tl_flutter_text_drain on the platform thread. */
void tl_flutter_text_insert(const char *utf8);
void tl_flutter_text_delete(void);
void tl_flutter_text_action(void);      /* Return: the field's action */
bool tl_flutter_text_multiline(void);   /* Return makes a new line instead */

/* Apply what the keyboard queued and tell Dart. Returns whether there was anything. */
bool tl_flutter_text_drain(void (*send)(const char *channel, const char *json));

#endif
