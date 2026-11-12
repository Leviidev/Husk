/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef HUSK_TL_CODESIGN_H
#define HUSK_TL_CODESIGN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Verify the PFX integrity MAC without importing or exposing its private key.
 * 1 = verified, 0 = mismatch (wrong password or modified data), -1 = unavailable
 * (no MAC, unsupported parameters or malformed data). A verified MAC alone does
 * not establish that Security.framework can import a signing identity. */
int husk_p12_check_password(const uint8_t *p12, size_t p12_len, const char *password);

/* Repack a MAC-authenticated single identity in memory for Security.framework.
 * Supports PBES2/PBKDF2/AES-CBC and SHA1/3DES encryption. Preserves certificate
 * data, PKCS#8 key and localKeyID, dropping unrelated bag attributes. The output
 * uses SHA1/3DES with the same password. Caller frees *output with free().
 * Returns false for a bad MAC, unsupported structure/algorithm or allocation failure;
 * outputs are initialized to NULL/0, and no keychain or file is modified. */
bool husk_p12_copy_compatible(const uint8_t *p12, size_t p12_len, const char *password,
                              uint8_t **output, size_t *output_length);

/*
 * Reads Apple Developer certificate metadata from a PKCS#12 (.p12) bundle.
 *
 * Parameters:
 *   p12              - Pointer to .p12 bundle file contents in memory.
 *   p12_len          - Length of p12 data in bytes.
 *   password         - Passphrase for the .p12 bundle (may be empty string "" if none).
 *   team_id          - Output buffer for the 10-character Apple Developer Team ID
 *                      (extracted from Subject Organizational Unit 'OU', or Subject CN).
 *   team_id_len      - Size of team_id output buffer in bytes (at least 32 recommended).
 *   not_after_epoch  - Output pointer receiving certificate expiration date in seconds
 *                      since Unix epoch (UTC).
 *   err              - Output buffer for human-readable error description on failure.
 *   errlen           - Size of err buffer in bytes.
 *
 * Returns true on success, false on failure (with error in err).
 */
bool husk_p12_info(const uint8_t *p12, size_t p12_len, const char *password,
                   char *team_id, size_t team_id_len,
                   double *not_after_epoch,
                   char *err, size_t errlen);

/*
 * Signs an arm64 Mach-O dylib in-place on-device using Apple Security.framework and CommonCrypto.
 *
 * Works on iOS 15.5+ and macOS. No OpenSSL or AGPL dependencies.
 *
 * If p12 is non-NULL and p12_len > 0:
 *   Signs with the developer certificate and private key from the .p12 bundle.
 *   Generates a fully conformant Apple Embedded Signature SuperBlob containing:
 *     1. CodeDirectory v=0x20400 (SHA-256 code slots per 16 KiB page, Team ID, execSeg limits).
 *     2. Requirements blob (matching the dylib identifier).
 *     3. CMS SignedData (BlobWrapper 0xfade0b01) containing leaf + CA certificates,
 *        signedAttrs (contentType, signingTime, messageDigest, and Apple cdhashes plist),
 *        and cryptographic signature generated via SecKeyCreateSignature().
 *   The resulting dylib satisfies Apple's Library Validation on iOS 15 through iOS 26+
 *   when dlopen()ed by an app signed with the same Team ID.
 *
 * If p12 is NULL or p12_len is 0:
 *   Signs with a valid ad-hoc signature (CodeDirectory with CS_ADHOC flag set and Requirements).
 *   Useful for local developer testing on macOS.
 *
 * Parameters:
 *   path             - Path to the Mach-O dylib file to sign in-place.
 *   p12              - Pointer to .p12 bundle data (or NULL for ad-hoc).
 *   p12_len          - Length of .p12 bundle data (or 0 for ad-hoc).
 *   password         - Passphrase for the .p12 bundle.
 *   ident            - CodeDirectory identifier and designated-requirement ident. On iOS this must be the running app's
 *                      bundle ID (SideStore/AltStore rewrite it, TrollStore/enterprise may not). NULL uses the dylib's
 *                      install-name basename (Mac tests). LC_ID_DYLIB is left unique for dyld.
 *   err              - Output buffer for error description on failure.
 *   errlen           - Size of err buffer in bytes.
 *
 * Returns true on success, false on failure (with error in err).
 */
bool husk_codesign_dylib(const char *path, const uint8_t *p12, size_t p12_len, const char *password,
                         const char *ident, char *err, size_t errlen);

/*
 * Take the certificates from the code signature of the executable at `path` (the app itself: its installer signed it with the
 * developer certificate and the chain above it) and carry them in every signature made after this, beside the developer
 * certificate, so iOS can trust it when the .p12 holds the developer certificate alone. Returns how many were found.
 */
int husk_codesign_chain_from_executable(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* HUSK_TL_CODESIGN_H */
