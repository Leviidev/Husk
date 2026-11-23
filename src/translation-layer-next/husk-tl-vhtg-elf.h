// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef HUSK_TL_VHTG_ELF_H
#define HUSK_TL_VHTG_ELF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define TL_ELF_MAX_LOADS 16u

typedef struct {
    uint64_t vaddr;
    uint64_t offset;
    uint64_t filesz;
    uint64_t memsz;
    uint32_t flags;
} tl_elf_load_t;

typedef struct {
    uint64_t anchor_va;
    uint64_t render_va;
} tl_vhtg_dyn_offsets_t;

typedef struct {
    uintptr_t str_anchor_off;
    uintptr_t str_topleft_off;
    uintptr_t str_charinfo_off;
    uintptr_t bss_safe_l_off;
    uintptr_t bss_safe_gen_off;
    uintptr_t bss_gamescene_off;
} tl_vhpt_dyn_offsets_t;

static inline uint16_t tl_elf_u16(const uint8_t *p) {
    uint16_t v = 0;
    memcpy(&v, p, sizeof(v));
    return v;
}

static inline uint32_t tl_elf_u32(const uint8_t *p) {
    uint32_t v = 0;
    memcpy(&v, p, sizeof(v));
    return v;
}

static inline uint64_t tl_elf_u64(const uint8_t *p) {
    uint64_t v = 0;
    memcpy(&v, p, sizeof(v));
    return v;
}

static inline bool tl_elf_check_hdr(const uint8_t *buf, size_t len) {
    if (!buf || len < 64u) return false;
    if (buf[0] != 0x7fu || buf[1] != 'E' || buf[2] != 'L' || buf[3] != 'F') return false;
    if (buf[4] != 2u || buf[5] != 1u) return false;
    return tl_elf_u16(buf + 16u) == 3u && tl_elf_u16(buf + 18u) == 183u;
}

static inline bool tl_elf_parse_loads(const uint8_t *buf, size_t len,
                                      tl_elf_load_t *loads, size_t max_loads, size_t *out_n) {
    if (!tl_elf_check_hdr(buf, len) || !loads || !out_n) return false;
    uint64_t phoff = tl_elf_u64(buf + 32u);
    uint16_t phent = tl_elf_u16(buf + 54u);
    uint16_t phnum = tl_elf_u16(buf + 56u);
    if (phent != 56u || phnum == 0u || phnum > 128u || phoff > len) return false;
    if ((uint64_t)phnum * phent > len - phoff) return false;
    size_t n = 0;
    for (uint16_t i = 0; i < phnum; ++i) {
        const uint8_t *ph = buf + phoff + (size_t)i * 56u;
        if (tl_elf_u32(ph) != 1u) continue;
        tl_elf_load_t seg;
        seg.flags = tl_elf_u32(ph + 4u);
        seg.offset = tl_elf_u64(ph + 8u);
        seg.vaddr = tl_elf_u64(ph + 16u);
        seg.filesz = tl_elf_u64(ph + 32u);
        seg.memsz = tl_elf_u64(ph + 40u);
        if (seg.filesz > seg.memsz || seg.offset > len || seg.filesz > len - seg.offset) return false;
        if (n < max_loads) loads[n++] = seg;
    }
    *out_n = n;
    return n > 0;
}

static inline bool tl_elf_va_to_off(const tl_elf_load_t *loads, size_t nloads,
                                    uint64_t va, size_t size, size_t *out_off) {
    for (size_t i = 0; i < nloads; ++i) {
        const tl_elf_load_t *s = &loads[i];
        if (va >= s->vaddr && va - s->vaddr <= s->filesz && size <= s->filesz - (va - s->vaddr)) {
            *out_off = (size_t)(s->offset + (va - s->vaddr));
            return true;
        }
    }
    return false;
}

static inline bool tl_elf_rw_va_valid(const tl_elf_load_t *loads, size_t nloads,
                                      uint64_t va, size_t size) {
    for (size_t i = 0; i < nloads; ++i) {
        const tl_elf_load_t *s = &loads[i];
        if ((s->flags & 2u) == 0u || (s->flags & 1u) != 0u) continue;
        if (va >= s->vaddr && va - s->vaddr <= s->memsz && size <= s->memsz - (va - s->vaddr)) {
            return true;
        }
    }
    return false;
}

static inline bool tl_elf_find_section(const uint8_t *buf, size_t len, const char *sec_name,
                                       uint32_t want_type, uint64_t *out_va,
                                       size_t *out_off, size_t *out_sz,
                                       uint32_t *out_link, uint64_t *out_entsz) {
    if (!tl_elf_check_hdr(buf, len)) return false;
    uint64_t shoff = tl_elf_u64(buf + 40u);
    uint16_t shent = tl_elf_u16(buf + 58u);
    uint16_t shnum = tl_elf_u16(buf + 60u);
    uint16_t shstrndx = tl_elf_u16(buf + 62u);
    if (shent != 64u || shnum == 0u || shnum > 4096u || shoff == 0u || shoff > len) return false;
    if ((uint64_t)shnum * 64u > len - shoff) return false;
    if (sec_name && shstrndx >= shnum) return false;

    const uint8_t *shstr = NULL;
    size_t shstr_sz = 0;
    if (sec_name) {
        const uint8_t *ss = buf + shoff + (size_t)shstrndx * 64u;
        uint64_t ss_off = tl_elf_u64(ss + 24u);
        uint64_t ss_sz = tl_elf_u64(ss + 32u);
        if (ss_off > len || ss_sz > len - ss_off) return false;
        shstr = buf + ss_off;
        shstr_sz = (size_t)ss_sz;
    }

    for (uint16_t i = 0; i < shnum; ++i) {
        const uint8_t *sh = buf + shoff + (size_t)i * 64u;
        uint32_t stype = tl_elf_u32(sh + 4u);
        if (want_type != 0u && stype != want_type) continue;
        if (sec_name) {
            uint32_t nm = tl_elf_u32(sh);
            if (nm >= shstr_sz) continue;
            size_t avail = shstr_sz - nm;
            const void *nul = memchr(shstr + nm, '\0', avail);
            if (!nul || strcmp((const char *)(shstr + nm), sec_name) != 0) continue;
        }
        uint64_t addr = tl_elf_u64(sh + 16u);
        uint64_t off = tl_elf_u64(sh + 24u);
        uint64_t sz = tl_elf_u64(sh + 32u);
        if (off > len || sz > len - off) continue;
        if (out_va) *out_va = addr;
        if (out_off) *out_off = (size_t)off;
        if (out_sz) *out_sz = (size_t)sz;
        if (out_link) *out_link = tl_elf_u32(sh + 40u);
        if (out_entsz) *out_entsz = tl_elf_u64(sh + 56u);
        return true;
    }
    return false;
}

static inline bool tl_elf_find_dyn_syms(const uint8_t *buf, size_t len,
                                        const char *const *names, uint64_t *out_vas, size_t count) {
    if (!names || !out_vas || count == 0u) return false;
    for (size_t i = 0; i < count; ++i) out_vas[i] = 0u;

    size_t sym_off = 0, sym_sz = 0;
    uint32_t link = 0;
    uint64_t entsz = 0;
    if (!tl_elf_find_section(buf, len, NULL, 11u /* SHT_DYNSYM */, NULL, &sym_off, &sym_sz, &link, &entsz)) {
        return false;
    }
    if (entsz != 24u || sym_sz < 24u) return false;

    uint64_t shoff = tl_elf_u64(buf + 40u);
    uint16_t shnum = tl_elf_u16(buf + 60u);
    if (link >= shnum) return false;
    const uint8_t *str_sh = buf + shoff + (size_t)link * 64u;
    uint64_t str_off64 = tl_elf_u64(str_sh + 24u);
    uint64_t str_sz64 = tl_elf_u64(str_sh + 32u);
    if (str_off64 > len || str_sz64 > len - str_off64 || str_sz64 == 0u) return false;
    const char *strtab = (const char *)(buf + str_off64);
    size_t str_sz = (size_t)str_sz64;

    size_t remaining = count;
    for (size_t off = 0; off <= sym_sz - 24u && remaining > 0u; off += 24u) {
        const uint8_t *sym = buf + sym_off + off;
        uint64_t st_val = tl_elf_u64(sym + 8u);
        if (st_val == 0u) continue;
        uint32_t st_name = tl_elf_u32(sym);
        if (st_name >= str_sz) continue;
        if (!memchr(strtab + st_name, '\0', str_sz - st_name)) continue;
        const char *sname = strtab + st_name;
        for (size_t k = 0; k < count; ++k) {
            if (out_vas[k] == 0u && strcmp(sname, names[k]) == 0) {
                out_vas[k] = st_val;
                --remaining;
                break;
            }
        }
    }
    return remaining == 0u;
}

static inline bool tl_vhtg_resolve_dyn_offsets(const uint8_t *buf, size_t len,
                                               tl_vhtg_dyn_offsets_t *out) {
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    tl_elf_load_t loads[TL_ELF_MAX_LOADS];
    size_t nloads = 0;
    if (!tl_elf_parse_loads(buf, len, loads, TL_ELF_MAX_LOADS, &nloads)) return false;

    static const char *const k_vhtg_syms[6] = {
        "KT_SetUiAnchorSafeArea",
        "_Z17Wnd_RenderWindowsv",
        "g_UIManager",
        "_ZN9KUIManage10GetUIGroupEPKc",
        "_ZN8KUIGroup9GetWindowEPKc",
        "_ZN10KWndWindow11SetPositionEii",
    };
    uint64_t vas[6];
    if (!tl_elf_find_dyn_syms(buf, len, k_vhtg_syms, vas, 6u)) return false;
    out->anchor_va = vas[0];
    out->render_va = vas[1];
    return true;
}

static inline bool tl_elf_scan_cstr_range(const uint8_t *buf, uint64_t base_va,
                                          size_t off, size_t sz,
                                          const char *needle, size_t nlen,
                                          uint64_t *match_va, size_t *hits) {
    if (!needle || nlen == 0u || sz < nlen + 1u) return true;
    const uint8_t *seg = buf + off;
    size_t pos = 0;
    while (pos <= sz - (nlen + 1u)) {
        const uint8_t *found = (const uint8_t *)memchr(seg + pos, (unsigned char)needle[0], sz - pos - nlen);
        if (!found) break;
        size_t i = (size_t)(found - seg);
        if (i + nlen < sz && memcmp(seg + i, needle, nlen + 1u) == 0 && (i == 0u || seg[i - 1u] == '\0')) {
            *match_va = base_va + i;
            (*hits)++;
            if (*hits > 1u) return false;
        }
        pos = i + 1u;
    }
    return true;
}

static inline bool tl_elf_find_unique_cstr(const uint8_t *buf, size_t len,
                                           const tl_elf_load_t *loads, size_t nloads,
                                           const char *needle, uint64_t *out_va) {
    size_t nlen = strlen(needle);
    uint64_t va = 0;
    size_t off = 0, sz = 0, hits = 0;
    if (tl_elf_find_section(buf, len, ".rodata", 0u, &va, &off, &sz, NULL, NULL) && va != 0u) {
        if (!tl_elf_scan_cstr_range(buf, va, off, sz, needle, nlen, out_va, &hits)) return false;
        return hits == 1u;
    }
    for (size_t i = 0; i < nloads; ++i) {
        if ((loads[i].flags & 2u) != 0u) continue;
        if (!tl_elf_scan_cstr_range(buf, loads[i].vaddr, (size_t)loads[i].offset,
                                    (size_t)loads[i].filesz, needle, nlen, out_va, &hits)) {
            return false;
        }
    }
    return hits == 1u;
}

static inline bool tl_elf_decode_adrp_pair(const uint8_t *chunk, size_t pos, size_t limit,
                                           uint64_t chunk_va, size_t max_gap,
                                           uint64_t *out_target, bool *out_is_add) {
    if (pos + 8u > limit) return false;
    uint32_t w0 = tl_elf_u32(chunk + pos);
    if ((w0 & 0x9f000000u) != 0x90000000u) return false;
    uint32_t rd = w0 & 0x1fu;
    uint64_t imm21 = ((uint64_t)((w0 >> 5) & 0x7ffffu) << 2) | ((uint64_t)(w0 >> 29) & 0x3u);
    int64_t simm = (imm21 & 0x100000ull) ? (int64_t)(imm21 | ~0x1fffffull) : (int64_t)imm21;
    uint64_t pc = chunk_va + pos;
    uint64_t page = (pc & ~0xfffull) + (uint64_t)(simm * 4096);

    for (size_t j = 1; j <= max_gap && pos + j * 4u + 4u <= limit; ++j) {
        uint32_t w1 = tl_elf_u32(chunk + pos + j * 4u);
        uint32_t rn = (w1 >> 5) & 0x1fu;
        if (rn != rd) continue;
        if ((w1 & 0xff800000u) == 0x91000000u) {
            uint64_t imm12 = (w1 >> 10) & 0xfffu;
            if ((w1 >> 22) & 1u) imm12 <<= 12;
            *out_target = page + imm12;
            *out_is_add = true;
            return true;
        }
        if ((w1 & 0xffc00000u) == 0xf9400000u) {
            uint64_t imm12 = (w1 >> 10) & 0xfffu;
            *out_target = page + imm12 * 8u;
            *out_is_add = false;
            return true;
        }
    }
    return false;
}

static inline bool tl_vhpt_resolve_dyn_offsets(const uint8_t *buf, size_t len,
                                               tl_vhpt_dyn_offsets_t *out) {
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    tl_elf_load_t loads[TL_ELF_MAX_LOADS];
    size_t nloads = 0;
    if (!tl_elf_parse_loads(buf, len, loads, TL_ELF_MAX_LOADS, &nloads)) return false;

    static const char *const k_safe_sym[1] = {
        "Java_org_cocos2dx_cpp_AppActivity_nativeSetSafeArea",
    };
    uint64_t safe_fn_va = 0;
    if (!tl_elf_find_dyn_syms(buf, len, k_safe_sym, &safe_fn_va, 1u)) return false;

    size_t safe_fn_off = 0;
    size_t probe_bytes = 80u;
    if (!tl_elf_va_to_off(loads, nloads, safe_fn_va, probe_bytes, &safe_fn_off)) {
        probe_bytes = 24u;
        if (!tl_elf_va_to_off(loads, nloads, safe_fn_va, probe_bytes, &safe_fn_off)) return false;
    }

    uint64_t safe_l_va = 0;
    for (size_t pos = 0; pos + 8u <= probe_bytes; pos += 4u) {
        uint64_t tgt = 0;
        bool is_add = false;
        if (tl_elf_decode_adrp_pair(buf + safe_fn_off, pos, probe_bytes, safe_fn_va, 4u, &tgt, &is_add)
            && is_add && tl_elf_rw_va_valid(loads, nloads, tgt, 20u)) {
            safe_l_va = tgt;
            break;
        }
    }
    if (safe_l_va == 0u) return false;

    uint64_t anchor_va = 0, topleft_va = 0, charinfo_va = 0, scene_head_va = 0;
    if (!tl_elf_find_unique_cstr(buf, len, loads, nloads, "Anchor", &anchor_va)) return false;
    if (!tl_elf_find_unique_cstr(buf, len, loads, nloads, "TopLeft", &topleft_va)) return false;
    if (!tl_elf_find_unique_cstr(buf, len, loads, nloads, "ui_mobile/charinfo.ini", &charinfo_va)) return false;
    if (!tl_elf_find_unique_cstr(buf, len, loads, nloads, "scene.update.head", &scene_head_va)) return false;

    uint64_t gamescene_va = 0;
    uint64_t text_va = 0;
    size_t text_off = 0, text_sz = 0;
    size_t scan_segs = 1u;
    bool use_text_sec = tl_elf_find_section(buf, len, ".text", 0u, &text_va, &text_off, &text_sz, NULL, NULL)
                        && text_va != 0u && text_sz >= 8u;
    if (!use_text_sec) scan_segs = nloads;

    for (size_t s = 0; s < scan_segs && gamescene_va == 0u; ++s) {
        uint64_t seg_va = text_va;
        size_t seg_off = text_off, seg_sz = text_sz;
        if (!use_text_sec) {
            if ((loads[s].flags & 1u) == 0u || loads[s].filesz < 8u) continue;
            seg_va = loads[s].vaddr;
            seg_off = (size_t)loads[s].offset;
            seg_sz = (size_t)loads[s].filesz;
        }
        const uint8_t *chunk = buf + seg_off;
        for (size_t pos = 0; pos + 8u <= seg_sz; pos += 4u) {
            uint64_t tgt = 0;
            bool is_add = false;
            if (!tl_elf_decode_adrp_pair(chunk, pos, seg_sz, seg_va, 4u, &tgt, &is_add)
                || !is_add || tgt != scene_head_va) {
                continue;
            }
            size_t look_end = pos + 80u;
            if (look_end > seg_sz) look_end = seg_sz;
            for (size_t p2 = pos + 4u; p2 + 8u <= look_end; p2 += 4u) {
                uint64_t bss_tgt = 0;
                bool bss_add = false;
                if (tl_elf_decode_adrp_pair(chunk, p2, look_end, seg_va, 4u, &bss_tgt, &bss_add)
                    && !bss_add && tl_elf_rw_va_valid(loads, nloads, bss_tgt, 8u)
                    && !(bss_tgt >= safe_l_va && bss_tgt < safe_l_va + 0x20u)) {
                    gamescene_va = bss_tgt;
                    break;
                }
            }
            if (gamescene_va != 0u) break;
        }
    }
    if (gamescene_va == 0u) return false;

    out->str_anchor_off = (uintptr_t)anchor_va;
    out->str_topleft_off = (uintptr_t)topleft_va;
    out->str_charinfo_off = (uintptr_t)charinfo_va;
    out->bss_safe_l_off = (uintptr_t)safe_l_va;
    out->bss_safe_gen_off = (uintptr_t)(safe_l_va + 0x10u);
    out->bss_gamescene_off = (uintptr_t)gamescene_va;
    return true;
}

#endif
