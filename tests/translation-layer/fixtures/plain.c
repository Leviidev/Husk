/* SPDX-License-Identifier: GPL-2.0-or-later */
/* A library with the shapes the scanner has to see through: writable data,
 * read-only-after-relocation pointers, imports, and ELF TLS. The exported
 * tables give the linker enough RELATIVE and ABS64 relocations that its
 * packed formats (APS2, RELR) have real groups and bitmaps to encode. */
int counter;
extern int imported_fn(int);
extern int imported_data;
__thread int tls_counter;

int bump(void) { return ++counter; }
int call_import(int x) { return imported_fn(x) + 1; }
int tls_bump(void) { return ++tls_counter; }

/* .data: pointers into this image, relocated RELATIVE. */
const char *table[] = {
    "zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine",
    "ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen",
    "seventeen", "eighteen", "nineteen", "twenty", "twenty-one", "twenty-two",
    "twenty-three", "twenty-four", "twenty-five", "twenty-six", "twenty-seven",
    "twenty-eight", "twenty-nine", "thirty", "thirty-one", "thirty-two",
};

/* .data.rel.ro: written only while relocating. */
int (*const ops[])(void) = { bump, tls_bump, bump, tls_bump };

/* Imported, with addends: ABS64. */
int *const imported_ptrs[] = { &imported_data, &imported_data + 1, &imported_data + 7 };
