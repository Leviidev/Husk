/* SPDX-License-Identifier: GPL-2.0-or-later */
/* White-box probe of the production MAC parser. Synthetic fixtures only. */
#include "../src/translation-layer-next/husk-tl-codesign.c"
#include <stdarg.h>

void tl_log_line(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    fputc('\n', stderr);
    va_end(args);
}

int main(int argc, char **argv)
{
    if (argc != 3 && argc != 4 && argc != 5) return 2;
    FILE *file = fopen(argv[1], "rb");
    if (!file || fseek(file, 0, SEEK_END)) return 2;
    long size = ftell(file);
    if (size < 0 || fseek(file, 0, SEEK_SET)) return 2;
    uint8_t *bytes = malloc(size ? (size_t)size : 1);
    if (!bytes || fread(bytes, 1, (size_t)size, file) != (size_t)size) return 2;
    fclose(file);
    /* Read fixture passwords over stdin so the probe can never put one in process arguments. */
    char *password = NULL;
    size_t capacity = 0;
    ssize_t count = getline(&password, &capacity, stdin);
    if (count < 0) return 2;
    if (count && password[count - 1] == '\n') password[--count] = '\0';
    int result = pkcs12_mac_check(bytes, (size_t)size, password);
    printf("MAC=%d\n", result);
    bool ok = result == atoi(argv[2]);
    if (argc == 5) {
        uint8_t *compatible = NULL; size_t length = 0;
        if (!husk_p12_copy_compatible(bytes, (size_t)size, password, &compatible, &length)) {
            printf("COMPATIBLE=0\n"); free(bytes); free(password); return 1;
        }
        ok = ok && pkcs12_mac_check(compatible, length, password) == 1;
        FILE *output = fopen(argv[4], "wb");
        if (!output || fwrite(compatible, 1, length, output) != length || fclose(output)) return 2;
        free(bytes); bytes = compatible; size = (long)length;
        printf("COMPATIBLE=1\n");
    }
    if (argc >= 4 && strcmp(argv[3], "-")) {
        char team[64], error[512];
        double expiry = 0;
        bool imported = husk_p12_info(bytes, (size_t)size, password, team, sizeof(team), &expiry, error, sizeof(error));
        printf("IMPORT=%d\n", imported);
        if (!imported) printf("ERROR=%s\n", error);
        ok = ok && imported == (atoi(argv[3]) != 0);
    }
    free(bytes);
    free(password);
    return ok ? 0 : 1;
}
