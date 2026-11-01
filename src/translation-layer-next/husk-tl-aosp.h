/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef HUSK_TL_AOSP_H
#define HUSK_TL_AOSP_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Optional AOSP userland next to the bundled java-runtime (framework.jar, not
 * copied into Husk's sources). When present it is on the boot path *after*
 * husk-framework.dex, so Husk still starts the APK and AOSP only fills classes
 * the stand-ins never defined.
 */
bool husk_aosp_present(const char *java_root);
/* Fill boot[] with existing jar paths. paths is a caller array of rows. Returns count. */
int husk_aosp_boot_jars(const char *java_root, char (*paths)[1200], int max, const char **boot);
void husk_aosp_start_system(void);
bool husk_aosp_using(void);
const char *husk_aosp_framework_path(const char *java_root, char *out, int cap);
/* The running APK and the Metal surface, so WMS/AMS Binder replies are not zeros. */
void husk_aosp_set_session(const char *package, int width, int height);

#ifdef __cplusplus
}
#endif

#endif
