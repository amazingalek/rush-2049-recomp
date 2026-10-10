// The game's speech decoder: GSM 06.10 full-rate decoding stages, hand-written MIPS assembly at
// 0x800277D0-0x80027DD0 that N64Recomp can't translate (it reads the PC with bltzal and keeps tables inline).
// These are C translations of that assembly, not of the GSM specification: they keep its 32-bit arithmetic,
// which skips some of the specification's saturation, so the output matches the console's.
//
// The decoder is dead code in this ROM: every path into it starts at a function nothing references (by jal, by a
// lui/addiu pair or as a pointer in ROM data). It is a linked-in speech library the game never uses; the translation
// keeps the island from needing a stub, and logs "[gsm] first speech frame" if a path turns up after all.
//
// Decoder state (the a0 argument of the long-term and short-term stages), byte offsets:
//   +0x000  drp history, 160 s16; drp[0..39] (the newest samples) start at +0x0F0
//   +0x140  short-term filter v[0..7], 32-bit words
//   +0x164  de-emphasis state msr, s16
//   +0x166  previous lag nrp, s16
//   +0x168  LARpp, two sets of 8 s16 (+0x168 and +0x178)
//   +0x188  which LARpp set is current, s16 (0: +0x168, else +0x178)

#include <cstdint>
#include <cstdio>

#include "recomp.h"

namespace {

int32_t mult_r(int32_t a, int32_t b) {
    // mult + mflo (low 32 bits of the product), then +0x4000 and an arithmetic shift by 15.
    uint32_t product = (uint32_t)((int64_t)a * (int64_t)b);
    return (int32_t)(product + 0x4000) >> 15;
}

int16_t load_h(uint8_t* rdram, uint32_t addr) {
    return MEM_H(0, (gpr)(int32_t)addr);
}

void store_h(uint8_t* rdram, uint32_t addr, int32_t value) {
    MEM_H(0, (gpr)(int32_t)addr) = (int16_t)value;
}

// LARp -> rp (GSM 06.10 5.2.9.2), as 0x80027D7C computes it.
int32_t larp_to_rp(int32_t larp) {
    bool negative = larp < 0;
    int32_t temp = negative ? -larp : larp;
    if (temp < 11059) {
        temp <<= 1;
    }
    else if (temp < 20070) {
        temp += 11059;
    }
    else {
        temp = (temp >> 2) + 26112;
    }
    if (temp > 0x7FFF) {
        temp = 0x7FFF;
    }
    return (int16_t)(negative ? -temp : temp);
}

} // namespace

// APCM inverse quantization and RPE grid positioning. a0 = xmaxc, a1 = Mc, a2 = xMc[13] (s16, replaced by the
// dequantized xMp), a3 = erp[40] (s16 out).
extern "C" void func_800277D0(uint8_t* rdram, recomp_context* ctx) {
    static bool logged = false;
    if (!logged) {
        logged = true;
        fprintf(stderr, "[gsm] first speech frame\n");
    }
    int32_t xmaxc = (int32_t)ctx->r4;
    int32_t mc = (int32_t)ctx->r5;
    uint32_t xmc = (uint32_t)ctx->r6;
    uint32_t erp = (uint32_t)ctx->r7;

    int32_t exp, mant;
    if (xmaxc - 7 > 0) {
        exp = (xmaxc - 8) >> 3;
        mant = xmaxc & 7;
    }
    else {
        // Inline table at 0x800278BC, indexed by xmaxc 0..7: exponent in the high nibble, mantissa in the low 3 bits.
        static const int8_t small[8] = { (int8_t)0xC7, (int8_t)0xD7, (int8_t)0xE3, (int8_t)0xE7,
                                         (int8_t)0xF1, (int8_t)0xF3, (int8_t)0xF5, (int8_t)0xF7 };
        int32_t entry = small[xmaxc & 7];
        exp = entry >> 4;
        mant = entry & 7;
    }
    int32_t fac = (mant << 11) + 0x47FF;
    int32_t shift = 6 - exp;
    uint32_t round = 1u << ((shift - 1) & 31);
    for (int i = 0; i < 13; i++) {
        int32_t temp = (load_h(rdram, xmc + i * 2) << 13) - 0x7000;
        temp = mult_r(temp, fac);
        temp = (int32_t)((uint32_t)temp + round) >> (shift & 31);
        store_h(rdram, xmc + i * 2, temp);
    }

    for (int i = 0; i < 40; i++) {
        store_h(rdram, erp + i * 2, 0);
    }
    for (int i = 0; i < 13; i++) {
        store_h(rdram, erp + (uint32_t)mc * 2 + i * 6, load_h(rdram, xmc + i * 2));
    }
}

// Long-term synthesis filtering. a0 = decoder state, a1 = Ncr, a2 = bcr, a3 = erp[40]. Writes drp[0..39].
extern "C" void func_800278C4(uint8_t* rdram, recomp_context* ctx) {
    uint32_t state = (uint32_t)ctx->r4;
    int32_t ncr = (int32_t)ctx->r5;
    int32_t bcr = (int32_t)ctx->r6;
    uint32_t erp = (uint32_t)ctx->r7;

    int32_t nr = load_h(rdram, state + 0x166);
    if (ncr - 40 >= 0 && ncr - 120 <= 0) {
        nr = ncr;
    }
    store_h(rdram, state + 0x166, nr);

    // QLB, inline at 0x80027978.
    static const uint16_t qlb[4] = { 0x0CCD, 0x2CCD, 0x5333, 0x7FFF };
    int32_t brp = qlb[bcr & 3];

    // Shift the history first: drp[-120..-1] = drp[-80..39].
    for (int i = 0; i < 120; i++) {
        store_h(rdram, state + i * 2, (uint16_t)load_h(rdram, state + 0x50 + i * 2));
    }
    for (int k = 0; k < 40; k++) {
        int32_t drpp = mult_r(load_h(rdram, state + 0xF0 + (uint32_t)(k - nr) * 2), brp);
        store_h(rdram, state + 0xF0 + k * 2, drpp + load_h(rdram, erp + k * 2));
    }
}

// Short-term synthesis filtering and de-emphasis over a 160-sample frame. a0 = decoder state, a1 = wt[160] (s16 in),
// a2 = sr[160] (s16 out).
extern "C" void func_80027B44(uint8_t* rdram, recomp_context* ctx) {
    uint32_t state = (uint32_t)ctx->r4;
    uint32_t wt = (uint32_t)ctx->r5;
    uint32_t sr = (uint32_t)ctx->r6;

    int32_t v[8];
    for (int i = 0; i < 8; i++) {
        v[i] = MEM_W(0, (gpr)(int32_t)(state + 0x140 + i * 4));
    }
    int32_t msr = load_h(rdram, state + 0x164);

    bool second_set = load_h(rdram, state + 0x188) != 0;
    uint32_t cur = state + (second_set ? 0x178 : 0x168);
    uint32_t prev = state + (second_set ? 0x168 : 0x178);

    struct Section {
        int start, count;
    };
    static const Section sections[4] = { { 0, 13 }, { 13, 14 }, { 27, 13 }, { 40, 120 } };
    for (int s = 0; s < 4; s++) {
        int32_t rp[8];
        for (int i = 0; i < 8; i++) {
            int32_t c = load_h(rdram, cur + i * 2);
            int32_t p = load_h(rdram, prev + i * 2);
            int32_t larp;
            switch (s) {
            case 0: larp = (c >> 2) + (p >> 1) + ((p >> 1) >> 1); break;
            case 1: larp = (c >> 1) + (p >> 1); break;
            case 2: larp = ((c >> 1) >> 1) + ((p >> 2) + (c >> 1)); break;
            default: larp = c; break;
            }
            rp[i] = larp_to_rp(larp);
        }

        for (int k = sections[s].start; k < sections[s].start + sections[s].count; k++) {
            int32_t sri = load_h(rdram, wt + k * 2);
            // i = 7: v[8] is never used, so only the input update.
            sri = (int32_t)((uint32_t)sri - (uint32_t)mult_r(v[7], rp[7]));
            for (int i = 6; i >= 0; i--) {
                sri = (int32_t)((uint32_t)sri - (uint32_t)mult_r(v[i], rp[i]));
                v[i + 1] = (int32_t)((uint32_t)mult_r(rp[i], sri) + (uint32_t)v[i]);
            }
            v[0] = sri;

            // De-emphasis; msr stays a full register until it is stored at the end.
            int32_t out = (int32_t)((uint32_t)sri + (uint32_t)mult_r(0x6E14, msr));
            msr = out;
            out = (int32_t)((uint32_t)out << 1);
            int32_t overflow = ((out >> 15) + 1) >> 1;
            if (overflow != 0) {
                out = overflow < 0 ? -0x8000 : 0x7FFF;
            }
            store_h(rdram, sr + k * 2, out & 0xFFF8);
        }
    }

    for (int i = 0; i < 8; i++) {
        MEM_W(0, (gpr)(int32_t)(state + 0x140 + i * 4)) = v[i];
    }
    store_h(rdram, state + 0x164, msr);
}
