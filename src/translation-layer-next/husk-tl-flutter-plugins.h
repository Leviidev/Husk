/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Native answers for common Flutter plugins (husk-tl-flutter-plugins.c). */
#ifndef HUSK_TL_FLUTTER_PLUGINS_H
#define HUSK_TL_FLUTTER_PLUGINS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void tl_flutter_plugins_configure(const char *data_dir, const char *package, const char *label, const char *version, long build);
void tl_flutter_set_url_opener(void (*fn)(const char *url));

/* A message from Dart on a plugin channel. *handled says whether the channel is one of these; the reply is malloc'd (NULL: reply
 * with nothing). */
uint8_t *tl_flutter_plugin_message(const char *channel, const uint8_t *data, size_t len, size_t *reply_len, bool *handled);

#endif /* HUSK_TL_FLUTTER_PLUGINS_H */
