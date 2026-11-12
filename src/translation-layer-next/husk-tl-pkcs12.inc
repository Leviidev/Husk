/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Authenticated, in-memory PKCS#12 compatibility export. No keychain or OpenSSL.
 * The existing Security importer remains responsible for constructing/validating
 * the signing identity. Unsupported structures fail without changing the input. */

typedef struct { const uint8_t *p; size_t n; } p12_view;
static const uint8_t p12_data_oid[] = {0x2a,0x86,0x48,0x86,0xf7,0x0d,1,7,1};
static const uint8_t p12_encrypted_oid[] = {0x2a,0x86,0x48,0x86,0xf7,0x0d,1,7,6};
static const uint8_t p12_pbes2_oid[] = {0x2a,0x86,0x48,0x86,0xf7,0x0d,1,5,13};
static const uint8_t p12_pbkdf2_oid[] = {0x2a,0x86,0x48,0x86,0xf7,0x0d,1,5,12};
static const uint8_t p12_des_oid[] = {0x2a,0x86,0x48,0x86,0xf7,0x0d,1,12,1,3};
static const uint8_t p12_bag_oid[] = {0x2a,0x86,0x48,0x86,0xf7,0x0d,1,12,10,1};
static const uint8_t p12_local_id_oid[] = {0x2a,0x86,0x48,0x86,0xf7,0x0d,1,9,21};

static void p12_secret_free(cs_buf *b)
{
    if (b->b) p12_wipe(b->b, b->cap);
    buf_free(b);
}

static bool p12_take(p12_view *input, uint8_t expected, p12_view *value)
{
    size_t pos = 0, header, length; uint8_t tag;
    if (!parse_tlv(input->p, input->n, &pos, &tag, &header, &length) || tag != expected) return false;
    *value = (p12_view){input->p + header, length};
    input->p += header + length; input->n -= header + length;
    return true;
}

static bool p12_oid(p12_view *input, const uint8_t *oid, size_t length)
{
    p12_view value;
    return p12_take(input, 6, &value) && oid_eq(value.p, value.n, oid, length);
}

static bool p12_count(p12_view *input, unsigned *count)
{
    p12_view value;
    if (!p12_take(input, 2, &value) || !value.n || value.n > 4 || (value.p[0] & 0x80)) return false;
    unsigned n = 0;
    for (size_t i = 0; i < value.n; i++) n = (n << 8) | value.p[i];
    if (!n || n > 1000000) return false;
    *count = n;
    return true;
}

/* Checked DER writing, including allocation failures. */
static bool p12_der(cs_buf *out, uint8_t tag, const void *bytes, size_t length)
{
    if (length > 8 * 1024 * 1024) return false;
    uint8_t header[6] = {tag}; size_t n = 1;
    if (length < 128) header[n++] = (uint8_t)length;
    else {
        size_t k = length > 65535 ? 3 : length > 255 ? 2 : 1;
        header[n++] = (uint8_t)(0x80 | k);
        for (size_t i = k; i; i--) header[n++] = (uint8_t)(length >> (8 * (i - 1)));
    }
    if (!buf_reserve(out, n + length)) return false;
    memcpy(out->b + out->n, header, n); out->n += n;
    if (length) memcpy(out->b + out->n, bytes, length);
    out->n += length;
    return true;
}

static bool p12_wrap(cs_buf *out, uint8_t tag)
{
    cs_buf wrapped = {0};
    if (!p12_der(&wrapped, tag, out->b, out->n)) { buf_free(&wrapped); return false; }
    buf_free(out); *out = wrapped;
    return true;
}

static bool p12_append(cs_buf *out, p12_view bytes)
{
    if (!buf_reserve(out, bytes.n)) return false;
    if (bytes.n) memcpy(out->b + out->n, bytes.p, bytes.n);
    out->n += bytes.n;
    return true;
}

static bool p12_integer(cs_buf *out, unsigned n)
{
    uint8_t value[5]; size_t offset = sizeof(value);
    do { value[--offset] = (uint8_t)n; n >>= 8; } while (n);
    if (value[offset] & 0x80) value[--offset] = 0;
    return p12_der(out, 2, value + offset, sizeof(value) - offset);
}

static bool p12_crypt(bool encrypt, CCAlgorithm cipher, const uint8_t *key, size_t key_len,
                      const uint8_t *iv, p12_view input, cs_buf *output)
{
    size_t block = cipher == kCCAlgorithmAES ? 16 : 8, written = 0;
    if (input.n > 8 * 1024 * 1024 || !buf_reserve(output, input.n + block)) return false;
    CCCryptorStatus status = CCCrypt(encrypt ? kCCEncrypt : kCCDecrypt, cipher, kCCOptionPKCS7Padding,
                                    key, key_len, iv, input.p, input.n, output->b, output->cap, &written);
    if (status != kCCSuccess) { p12_secret_free(output); return false; }
    output->n = written;
    return true;
}

static bool p12_legacy_keys(p12_view salt, unsigned iterations, const char *password, uint8_t key[24], uint8_t iv[8])
{
    CFDataRef bmp = pkcs12_bmp(password);
    bool ok = pkcs12_key_gen(CFDataGetBytePtr(bmp), (size_t)CFDataGetLength(bmp), salt.p, salt.n, 1,
                             (int)iterations, key, 24, 20, 64)
           && pkcs12_key_gen(CFDataGetBytePtr(bmp), (size_t)CFDataGetLength(bmp), salt.p, salt.n, 2,
                             (int)iterations, iv, 8, 20, 64);
    p12_wipe(CFDataGetMutableBytePtr((CFMutableDataRef)bmp), (size_t)CFDataGetLength(bmp));
    CFRelease(bmp);
    return ok;
}

static bool p12_decrypt(p12_view algorithm, p12_view ciphertext, const char *password, cs_buf *plain)
{
    p12_view oid, params, salt, iv, kdf, kdf_alg, encryption, prf;
    uint8_t key[32] = {0}, legacy_iv[8] = {0}; size_t key_len = 0;
    unsigned iterations, explicit_key_len = 0; CCAlgorithm cipher = kCCAlgorithmAES;
    bool ok = false;
    if (!p12_take(&algorithm, 6, &oid) || !p12_take(&algorithm, 0x30, &params) || algorithm.n) goto done;
    if (oid_eq(oid.p, oid.n, p12_des_oid, sizeof(p12_des_oid))) {
        if (!p12_take(&params, 4, &salt) || !p12_count(&params, &iterations) || params.n) goto done;
        if (!p12_legacy_keys(salt, iterations, password, key, legacy_iv)) goto done;
        ok = p12_crypt(false, kCCAlgorithm3DES, key, 24, legacy_iv, ciphertext, plain);
        goto done;
    }
    if (!oid_eq(oid.p, oid.n, p12_pbes2_oid, sizeof(p12_pbes2_oid))
        || !p12_take(&params, 0x30, &kdf) || !p12_take(&params, 0x30, &encryption) || params.n
        || !p12_oid(&kdf, p12_pbkdf2_oid, sizeof(p12_pbkdf2_oid)) || !p12_take(&kdf, 0x30, &kdf_alg) || kdf.n
        || !p12_take(&kdf_alg, 4, &salt) || !p12_count(&kdf_alg, &iterations)) goto done;
    CCPseudoRandomAlgorithm hash = kCCPRFHmacAlgSHA1;
    if (kdf_alg.n && kdf_alg.p[0] == 2 && !p12_count(&kdf_alg, &explicit_key_len)) goto done;
    if (kdf_alg.n) {
        static const uint8_t prefix[] = {0x2a,0x86,0x48,0x86,0xf7,0x0d,2};
        if (!p12_take(&kdf_alg, 0x30, &prf) || kdf_alg.n || !p12_take(&prf, 6, &oid)
            || oid.n != sizeof(prefix) + 1 || memcmp(oid.p, prefix, sizeof(prefix))) goto done;
        switch (oid.p[sizeof(prefix)]) {
            case 7: hash = kCCPRFHmacAlgSHA1; break;
            case 8: hash = kCCPRFHmacAlgSHA224; break;
            case 9: hash = kCCPRFHmacAlgSHA256; break;
            case 10: hash = kCCPRFHmacAlgSHA384; break;
            case 11: hash = kCCPRFHmacAlgSHA512; break;
            default: goto done;
        }
        if (prf.n) { p12_view null; if (!p12_take(&prf, 5, &null) || null.n || prf.n) goto done; }
    }
    static const uint8_t aes_prefix[] = {0x60,0x86,0x48,1,0x65,3,4,1};
    static const uint8_t des_cbc[] = {0x2a,0x86,0x48,0x86,0xf7,0x0d,3,7};
    if (!p12_take(&encryption, 6, &oid) || !p12_take(&encryption, 4, &iv) || encryption.n) goto done;
    if (oid.n == sizeof(aes_prefix) + 1 && !memcmp(oid.p, aes_prefix, sizeof(aes_prefix))) {
        switch (oid.p[sizeof(aes_prefix)]) { case 2: key_len = 16; break; case 22: key_len = 24; break; case 42: key_len = 32; break; default: goto done; }
        if (iv.n != 16) goto done;
    } else if (oid_eq(oid.p, oid.n, des_cbc, sizeof(des_cbc))) {
        cipher = kCCAlgorithm3DES; key_len = 24; if (iv.n != 8) goto done;
    } else goto done;
    if (explicit_key_len && explicit_key_len != key_len) goto done;
    if (CCKeyDerivationPBKDF(kCCPBKDF2, password, strlen(password), salt.p, salt.n, hash, iterations, key, key_len) != kCCSuccess) goto done;
    ok = p12_crypt(false, cipher, key, key_len, iv.p, ciphertext, plain);
done:
    p12_wipe(key, sizeof(key)); p12_wipe(legacy_iv, sizeof(legacy_iv));
    return ok;
}

static bool p12_encrypt_key(p12_view pkcs8, const char *password, cs_buf *encrypted)
{
    uint8_t salt[16], key[24] = {0}, iv[8] = {0};
    cs_buf params = {0}, algorithm = {0}, cipher = {0};
    bool ok = SecRandomCopyBytes(kSecRandomDefault, sizeof(salt), salt) == errSecSuccess
           && p12_legacy_keys((p12_view){salt, sizeof(salt)}, 2048, password, key, iv)
           && p12_crypt(true, kCCAlgorithm3DES, key, sizeof(key), iv, pkcs8, &cipher)
           && p12_der(&params, 4, salt, sizeof(salt)) && p12_integer(&params, 2048) && p12_wrap(&params, 0x30)
           && p12_der(&algorithm, 6, p12_des_oid, sizeof(p12_des_oid)) && p12_append(&algorithm, (p12_view){params.b, params.n})
           && p12_wrap(&algorithm, 0x30) && p12_append(encrypted, (p12_view){algorithm.b, algorithm.n})
           && p12_der(encrypted, 4, cipher.b, cipher.n) && p12_wrap(encrypted, 0x30);
    p12_wipe(key, sizeof(key)); p12_wipe(iv, sizeof(iv));
    buf_free(&params); buf_free(&algorithm); buf_free(&cipher);
    return ok;
}

static bool p12_attributes(p12_view attributes, cs_buf *out)
{
    if (!attributes.n) return true;
    p12_view values, attr, oid, set, id;
    if (!p12_take(&attributes, 0x31, &values) || attributes.n) return false;
    while (values.n) {
        if (!p12_take(&values, 0x30, &attr) || !p12_take(&attr, 6, &oid) || !p12_take(&attr, 0x31, &set) || attr.n) return false;
        if (oid_eq(oid.p, oid.n, p12_local_id_oid, sizeof(p12_local_id_oid))) {
            if (out->n || !p12_take(&set, 4, &id) || set.n) return false;
            /* Rebuild the SET containing the ID, with no unrelated bag attributes. */
            cs_buf entry = {0}, ids = {0};
            bool ok = p12_der(&ids, 4, id.p, id.n) && p12_wrap(&ids, 0x31)
                   && p12_der(&entry, 6, p12_local_id_oid, sizeof(p12_local_id_oid))
                   && p12_append(&entry, (p12_view){ids.b, ids.n}) && p12_wrap(&entry, 0x30)
                   && p12_append(out, (p12_view){entry.b, entry.n});
            buf_free(&entry); buf_free(&ids); if (!ok) return false;
        }
    }
    return !out->n || p12_wrap(out, 0x31);
}

static bool p12_safe(p12_view encoded, const char *password, cs_buf *bags, unsigned *keys, unsigned *certs, unsigned depth)
{
    p12_view safe, bag, oid, value;
    if (depth > 4 || !p12_take(&encoded, 0x30, &safe) || encoded.n) return false;
    while (safe.n) {
        cs_buf plain = {0}, replacement = {0}, attrs = {0}, rebuilt = {0}; bool ok = false;
        if (!p12_take(&safe, 0x30, &bag) || !p12_take(&bag, 6, &oid)
            || oid.n != sizeof(p12_bag_oid) + 1 || memcmp(oid.p, p12_bag_oid, sizeof(p12_bag_oid))
            || !p12_take(&bag, 0xa0, &value)) goto end_bag;
        unsigned kind = oid.p[sizeof(p12_bag_oid)];
        if (kind == 6) { ok = !bag.n && p12_safe(value, password, bags, keys, certs, depth + 1); goto end_bag; }
        if (kind == 1 || kind == 2) {
            if (++*keys != 1) goto end_bag; /* Explicitly support a single signing identity. */
            p12_view pkcs8 = value;
            if (kind == 2) {
                p12_view wrapped, algorithm, ciphertext;
                if (!p12_take(&value, 0x30, &wrapped) || value.n || !p12_take(&wrapped, 0x30, &algorithm)
                    || !p12_take(&wrapped, 4, &ciphertext) || wrapped.n || !p12_decrypt(algorithm, ciphertext, password, &plain)) goto end_bag;
                pkcs8 = (p12_view){plain.b, plain.n};
            }
            p12_view checked = pkcs8, sequence;
            if (!p12_take(&checked, 0x30, &sequence) || checked.n || !p12_encrypt_key(pkcs8, password, &replacement)) goto end_bag;
            kind = 2;
        } else if (kind == 3) {
            if (++*certs > 128 || !p12_append(&replacement, value)) goto end_bag;
        } else goto end_bag;
        uint8_t bag_oid[sizeof(p12_bag_oid) + 1]; memcpy(bag_oid, p12_bag_oid, sizeof(p12_bag_oid)); bag_oid[sizeof(p12_bag_oid)] = (uint8_t)kind;
        ok = p12_attributes(bag, &attrs) && p12_der(&rebuilt, 6, bag_oid, sizeof(bag_oid))
          && p12_der(&rebuilt, 0xa0, replacement.b, replacement.n) && p12_append(&rebuilt, (p12_view){attrs.b, attrs.n})
          && p12_wrap(&rebuilt, 0x30) && p12_append(bags, (p12_view){rebuilt.b, rebuilt.n});
end_bag:
        p12_secret_free(&plain); buf_free(&replacement); buf_free(&attrs); buf_free(&rebuilt);
        if (!ok) return false;
    }
    return true;
}

bool husk_p12_copy_compatible(const uint8_t *p12, size_t length, const char *password,
                              uint8_t **output, size_t *output_length)
{
    if (!output || !output_length) return false;
    *output = NULL; *output_length = 0;
    if (!p12 || length > 8 * 1024 * 1024 || pkcs12_mac_check(p12, length, password) != 1) return false;
    password = password ? password : "";
    p12_view input = {p12, length}, pfx, version, auth, explicit, octets, infos;
    cs_buf bags = {0}, safe = {0}, content = {0}, result = {0}, mac = {0}, digest_info = {0};
    unsigned keys = 0, certs = 0; bool ok = false;
    if (!p12_take(&input, 0x30, &pfx) || input.n || !p12_take(&pfx, 2, &version)
        || version.n != 1 || version.p[0] != 3 || !p12_take(&pfx, 0x30, &auth)
        || !p12_oid(&auth, p12_data_oid, sizeof(p12_data_oid)) || !p12_take(&auth, 0xa0, &explicit) || auth.n
        || !p12_take(&explicit, 4, &octets) || explicit.n || !p12_take(&octets, 0x30, &infos) || octets.n) goto finish;
    while (infos.n) {
        p12_view info, oid, wrapped; cs_buf plain = {0}; bool decoded = false;
        if (!p12_take(&infos, 0x30, &info) || !p12_take(&info, 6, &oid) || !p12_take(&info, 0xa0, &wrapped) || info.n) goto finish;
        if (oid_eq(oid.p, oid.n, p12_data_oid, sizeof(p12_data_oid))) {
            p12_view contents;
            decoded = p12_take(&wrapped, 4, &contents) && !wrapped.n && p12_safe(contents, password, &bags, &keys, &certs, 0);
        } else if (oid_eq(oid.p, oid.n, p12_encrypted_oid, sizeof(p12_encrypted_oid))) {
            p12_view encrypted, v, info_data, algorithm, ciphertext;
            decoded = p12_take(&wrapped, 0x30, &encrypted) && !wrapped.n && p12_take(&encrypted, 2, &v)
                   && v.n == 1 && v.p[0] == 0 && p12_take(&encrypted, 0x30, &info_data) && !encrypted.n
                   && p12_oid(&info_data, p12_data_oid, sizeof(p12_data_oid)) && p12_take(&info_data, 0x30, &algorithm)
                   && p12_take(&info_data, 0x80, &ciphertext) && !info_data.n && p12_decrypt(algorithm, ciphertext, password, &plain)
                   && p12_safe((p12_view){plain.b, plain.n}, password, &bags, &keys, &certs, 0);
        }
        p12_secret_free(&plain); if (!decoded) goto finish;
    }
    if (keys != 1 || !certs || !p12_wrap(&bags, 0x30) || !p12_der(&safe, 4, bags.b, bags.n) || !p12_wrap(&safe, 0xa0)
        || !p12_der(&content, 6, p12_data_oid, sizeof(p12_data_oid)) || !p12_append(&content, (p12_view){safe.b, safe.n})
        || !p12_wrap(&content, 0x30) || !p12_wrap(&content, 0x30)) goto finish;
    buf_free(&safe);
    uint8_t salt[16], key[20] = {0}, digest[20];
    if (SecRandomCopyBytes(kSecRandomDefault, sizeof(salt), salt) != errSecSuccess) goto finish;
    CFDataRef bmp = pkcs12_bmp(password);
    bool derived = pkcs12_key_gen(CFDataGetBytePtr(bmp), (size_t)CFDataGetLength(bmp), salt, sizeof(salt), 3, 2048, key, 20, 20, 64);
    p12_wipe(CFDataGetMutableBytePtr((CFMutableDataRef)bmp), (size_t)CFDataGetLength(bmp)); CFRelease(bmp);
    if (!derived) { p12_wipe(key, sizeof(key)); goto finish; }
    CCHmac(kCCHmacAlgSHA1, key, sizeof(key), content.b, content.n, digest); p12_wipe(key, sizeof(key));
    static const uint8_t sha1_algorithm[] = {0x30,9,6,5,0x2b,0x0e,3,2,0x1a,5,0};
    ok = p12_append(&digest_info, (p12_view){sha1_algorithm, sizeof(sha1_algorithm)}) && p12_der(&digest_info, 4, digest, sizeof(digest))
      && p12_wrap(&digest_info, 0x30) && p12_append(&mac, (p12_view){digest_info.b, digest_info.n})
      && p12_der(&mac, 4, salt, sizeof(salt)) && p12_integer(&mac, 2048) && p12_wrap(&mac, 0x30)
      && p12_der(&safe, 4, content.b, content.n) && p12_wrap(&safe, 0xa0);
    buf_free(&content);
    ok = ok && p12_der(&content, 6, p12_data_oid, sizeof(p12_data_oid)) && p12_append(&content, (p12_view){safe.b, safe.n})
      && p12_wrap(&content, 0x30) && p12_integer(&result, 3) && p12_append(&result, (p12_view){content.b, content.n})
      && p12_append(&result, (p12_view){mac.b, mac.n}) && p12_wrap(&result, 0x30)
      && pkcs12_mac_check(result.b, result.n, password) == 1;
    if (ok) { *output = result.b; *output_length = result.n; result = (cs_buf){0}; }
finish:
    buf_free(&bags); buf_free(&safe); buf_free(&content); buf_free(&result); buf_free(&mac); buf_free(&digest_info);
    return ok;
}
