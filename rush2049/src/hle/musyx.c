/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus-rsp-hle - musyx.c                                         *
 *   Mupen64Plus homepage: https://mupen64plus.org/                        *
 *   Copyright (C) 2013 Bobby Smiles                                       *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "arithmetics.h"
#include "audio.h"
#include "common.h"
#include "hle_external.h"
#include "hle_internal.h"
#include "memory.h"

/* various constants */
enum { SUBFRAME_SIZE = 192 };
enum { MAX_VOICES = 32 };

enum { SAMPLE_BUFFER_SIZE = 0x200 };


enum {
    SFD_VOICE_COUNT     = 0x0,
    SFD_SFX_INDEX       = 0x2,
    SFD_VOICE_BITMASK   = 0x4,
    SFD_STATE_PTR       = 0x8,
    SFD_SFX_PTR         = 0xc,
    SFD_VOICES          = 0x10,

    /* v2 only */
    SFD2_10_PTR         = 0x10,
    SFD2_14_BITMASK     = 0x14,
    SFD2_15_BITMASK     = 0x15,
    SFD2_16_BITMASK     = 0x16,
    SFD2_18_PTR         = 0x18,
    SFD2_1C_PTR         = 0x1c,
    SFD2_20_PTR         = 0x20,
    SFD2_24_PTR         = 0x24,
    SFD2_VOICES         = 0x28
};

enum {
    VOICE_ENV_BEGIN         = 0x00,
    VOICE_ENV_STEP          = 0x10,
    VOICE_PITCH_Q16         = 0x20,
    VOICE_PITCH_SHIFT       = 0x22,
    VOICE_CATSRC_0          = 0x24,
    VOICE_CATSRC_1          = 0x30,
    VOICE_ADPCM_FRAMES      = 0x3c,
    VOICE_SKIP_SAMPLES      = 0x3e,

    /* for PCM16 */
    VOICE_U16_40            = 0x40,
    VOICE_U16_42            = 0x42,

    /* for ADPCM */
    VOICE_ADPCM_TABLE_PTR   = 0x40,

    VOICE_INTERLEAVED_PTR   = 0x44,
    VOICE_END_POINT         = 0x48,
    VOICE_RESTART_POINT     = 0x4a,
    VOICE_U16_4C            = 0x4c,
    VOICE_U16_4E            = 0x4e,

    VOICE_SIZE              = 0x50
};

enum {
    CATSRC_PTR1     = 0x00,
    CATSRC_PTR2     = 0x04,
    CATSRC_SIZE1    = 0x08,
    CATSRC_SIZE2    = 0x0a
};

enum {
    STATE_LAST_SAMPLE   = 0x0,
    STATE_BASE_VOL      = 0x100,
    STATE_CC0           = 0x110,
    STATE_740_LAST4_V1  = 0x290,

    STATE_740_LAST4_V2  = 0x110
};

enum {
    SFX_CBUFFER_PTR     = 0x00,
    SFX_CBUFFER_LENGTH  = 0x04,
    SFX_TAP_COUNT       = 0x08,
    SFX_FIR4_HGAIN      = 0x0a,
    SFX_TAP_DELAYS      = 0x0c,
    SFX_TAP_GAINS       = 0x2c,
    SFX_U16_3C          = 0x3c,
    SFX_U16_3E          = 0x3e,
    SFX_FIR4_HCOEFFS    = 0x40
};


/* struct definition */
/* rush2049: a high-quality side mix. Each voice is rendered a second time at twice the mix rate, with a 4-tap
 * Kaiser-windowed sinc in place of RESAMPLE_LUT. Everything else that reaches the left/right output (the base
 * volumes, the delayed cc0 surround channel, the delay effect) is carried over from the original mix. The result
 * goes to musyx_hires_store() beside the subframe the game gets, and the audio output plays it in place of the
 * game's buffer. RDRAM and the game's own output stay exactly as the original mix leaves them. MusyX v1 only. */
enum { HIRES_SIZE = 2 * SUBFRAME_SIZE };
enum { HIRES_PHASES = 256 };

typedef struct musyx_hires {
    float left[HIRES_SIZE];
    float right[HIRES_SIZE];
    float cc0[HIRES_SIZE];            /* carried into the next subframe, like musyx_t's cc0 */

    int16_t cc0_check[SUBFRAME_SIZE]; /* the original mix's cc0 that the carry belongs to */
    float sfx_history[3];             /* the delay effect's last samples, for upsampling it */
} musyx_hires_t;

/* the RSP task thread is the only caller */
static musyx_hires_t hires_state;
static float hires_kernel[HIRES_PHASES + 1][4];

typedef struct {
    /* internal subframes */
    int16_t left[SUBFRAME_SIZE];
    int16_t right[SUBFRAME_SIZE];
    int16_t cc0[SUBFRAME_SIZE];
    int16_t e50[SUBFRAME_SIZE];

    /* internal subframes base volumes */
    int32_t base_vol[4];

    /* */
    int16_t subframe_740_last4[4];

    /* rush2049: the high-quality side mix, or NULL */
    musyx_hires_t *hires;
} musyx_t;

typedef void (*mix_sfx_with_main_subframes_t)(musyx_t *musyx, const int16_t *subframe,
                                              const uint16_t* gains);

/* helper functions prototypes */
static void load_base_vol(struct hle_t* hle, int32_t *base_vol, uint32_t address);
static void save_base_vol(struct hle_t* hle, const int32_t *base_vol, uint32_t address);
static void update_base_vol(struct hle_t* hle, int32_t *base_vol,
                            uint32_t voice_mask, uint32_t last_sample_ptr,
                            uint8_t mask_15, uint32_t ptr_24);

static void init_subframes_v1(musyx_t *musyx);
static void init_subframes_v2(musyx_t *musyx);

static uint32_t voice_stage(struct hle_t* hle, musyx_t *musyx,
                            uint32_t voice_ptr, uint32_t last_sample_ptr);

static void dma_cat8(struct hle_t* hle, uint8_t *dst, uint32_t catsrc_ptr);
static void dma_cat16(struct hle_t* hle, uint16_t *dst, uint32_t catsrc_ptr);

static void load_samples_PCM16(struct hle_t* hle, uint32_t voice_ptr, int16_t *samples,
                               unsigned *segbase, unsigned *offset);
static void load_samples_ADPCM(struct hle_t* hle, uint32_t voice_ptr, int16_t *samples,
                               unsigned *segbase, unsigned *offset);

static void adpcm_decode_frames(struct hle_t* hle,
                                int16_t *dst, const uint8_t *src,
                                const int16_t *table, uint8_t count,
                                uint8_t skip_samples);

static void adpcm_predict_frame(int16_t *dst, const uint8_t *src,
                                const uint8_t *nibbles,
                                unsigned int rshift);

static void mix_voice_samples(struct hle_t* hle, musyx_t *musyx,
                              uint32_t voice_ptr, const int16_t *samples,
                              unsigned segbase, unsigned offset, uint32_t last_sample_ptr);

static void sfx_stage(struct hle_t* hle,
                      mix_sfx_with_main_subframes_t mix_sfx_with_main_subframes,
                      musyx_t *musyx, uint32_t sfx_ptr, uint16_t idx);

static void mix_sfx_with_main_subframes_v1(musyx_t *musyx, const int16_t *subframe,
                                           const uint16_t* gains);
static void mix_sfx_with_main_subframes_v2(musyx_t *musyx, const int16_t *subframe,
                                           const uint16_t* gains);

static void mix_samples(int16_t *y, int16_t x, int16_t hgain);
static void mix_subframes(int16_t *y, const int16_t *x, int16_t hgain);
static void mix_fir4(int16_t *y, const int16_t *x, int16_t hgain, const int16_t *hcoeffs);


static void interleave_stage_v1(struct hle_t* hle, musyx_t *musyx,
                                uint32_t output_ptr);

static void interleave_stage_v2(struct hle_t* hle, musyx_t *musyx,
                                uint16_t mask_16, uint32_t ptr_18,
                                uint32_t ptr_1c, uint32_t output_ptr);

static void hires_begin_task(musyx_hires_t *hires, const int16_t *cc0);
static void init_hires_v1(musyx_t *musyx);
static void mix_voice_hires(musyx_hires_t *hires, const int16_t *samples, unsigned segbase,
                            const int16_t *sample, const int16_t *sample_end,
                            const int16_t *sample_restart, uint32_t pitch_accu, uint32_t pitch_step,
                            const int32_t *env, const int32_t *env_step);
static void mix_sfx_hires(musyx_hires_t *hires, const int16_t *subframe);
static void interleave_hires_v1(struct hle_t* hle, musyx_t *musyx, uint32_t output_ptr);

static int32_t dot4(const int16_t *x, const int16_t *y)
{
    size_t i;
    int32_t accu = 0;

    for (i = 0; i < 4; ++i)
        accu = clamp_s16(accu + (((int32_t)x[i] * (int32_t)y[i]) >> 15));

    return accu;
}

/**************************************************************************
 * MusyX v1 audio ucode
 **************************************************************************/
void musyx_v1_task(struct hle_t* hle)
{
    uint32_t sfd_ptr   = *dmem_u32(hle, TASK_DATA_PTR);
    uint32_t sfd_count = *dmem_u32(hle, TASK_DATA_SIZE);
    uint32_t state_ptr;
    musyx_t musyx;

    HleVerboseMessage(hle->user_defined,
                      "musyx_v1_task: *data=%x, #SF=%d",
                      sfd_ptr,
                      sfd_count);

    state_ptr = *dram_u32(hle, sfd_ptr + SFD_STATE_PTR);

    /* load initial state */
    load_base_vol(hle, musyx.base_vol, state_ptr + STATE_BASE_VOL);
    dram_load_u16(hle, (uint16_t *)musyx.cc0, state_ptr + STATE_CC0, SUBFRAME_SIZE);
    dram_load_u16(hle, (uint16_t *)musyx.subframe_740_last4, state_ptr + STATE_740_LAST4_V1,
             4);

    musyx.hires = &hires_state;
    hires_begin_task(musyx.hires, musyx.cc0);

    for (;;) {
        /* parse SFD structure */
        uint16_t sfx_index   = *dram_u16(hle, sfd_ptr + SFD_SFX_INDEX);
        uint32_t voice_mask  = *dram_u32(hle, sfd_ptr + SFD_VOICE_BITMASK);
        uint32_t sfx_ptr     = *dram_u32(hle, sfd_ptr + SFD_SFX_PTR);
        uint32_t voice_ptr       = sfd_ptr + SFD_VOICES;
        uint32_t last_sample_ptr = state_ptr + STATE_LAST_SAMPLE;
        uint32_t output_ptr;

        /* initialize internal subframes using updated base volumes */
        update_base_vol(hle, musyx.base_vol, voice_mask, last_sample_ptr, 0, 0);
        init_hires_v1(&musyx);
        init_subframes_v1(&musyx);

        /* active voices get mixed into L,R,cc0,e50 subframes (optional) */
        output_ptr = voice_stage(hle, &musyx, voice_ptr, last_sample_ptr);

        /* apply delay-based effects (optional) */
        sfx_stage(hle, mix_sfx_with_main_subframes_v1,
                  &musyx, sfx_ptr, sfx_index);

        /* emit interleaved L,R subframes */
        interleave_stage_v1(hle, &musyx, output_ptr);
        interleave_hires_v1(hle, &musyx, output_ptr);

        --sfd_count;
        if (sfd_count == 0)
            break;

        sfd_ptr += SFD_VOICES + MAX_VOICES * VOICE_SIZE;
        state_ptr = *dram_u32(hle, sfd_ptr + SFD_STATE_PTR);
    }

    /* writeback updated state */
    save_base_vol(hle, musyx.base_vol, state_ptr + STATE_BASE_VOL);
    dram_store_u16(hle, (uint16_t *)musyx.cc0, state_ptr + STATE_CC0, SUBFRAME_SIZE);
    dram_store_u16(hle, (uint16_t *)musyx.subframe_740_last4, state_ptr + STATE_740_LAST4_V1,
              4);
    memcpy(hires_state.cc0_check, musyx.cc0, sizeof(musyx.cc0));

    rsp_break(hle, SP_STATUS_TASKDONE);
}

/**************************************************************************
 * MusyX v2 audio ucode
 **************************************************************************/
void musyx_v2_task(struct hle_t* hle)
{
    uint32_t sfd_ptr   = *dmem_u32(hle, TASK_DATA_PTR);
    uint32_t sfd_count = *dmem_u32(hle, TASK_DATA_SIZE);
    musyx_t musyx;

    musyx.hires = NULL;

    HleVerboseMessage(hle->user_defined,
                      "musyx_v2_task: *data=%x, #SF=%d",
                      sfd_ptr,
                      sfd_count);

    for (;;) {
        /* parse SFD structure */
        uint16_t sfx_index       = *dram_u16(hle, sfd_ptr + SFD_SFX_INDEX);
        uint32_t voice_mask      = *dram_u32(hle, sfd_ptr + SFD_VOICE_BITMASK);
        uint32_t state_ptr       = *dram_u32(hle, sfd_ptr + SFD_STATE_PTR);
        uint32_t sfx_ptr         = *dram_u32(hle, sfd_ptr + SFD_SFX_PTR);
        uint32_t voice_ptr       = sfd_ptr + SFD2_VOICES;

        uint32_t ptr_10          = *dram_u32(hle, sfd_ptr + SFD2_10_PTR);
        uint8_t  mask_14         = *dram_u8 (hle, sfd_ptr + SFD2_14_BITMASK);
        uint8_t  mask_15         = *dram_u8 (hle, sfd_ptr + SFD2_15_BITMASK);
        uint16_t mask_16         = *dram_u16(hle, sfd_ptr + SFD2_16_BITMASK);
        uint32_t ptr_18          = *dram_u32(hle, sfd_ptr + SFD2_18_PTR);
        uint32_t ptr_1c          = *dram_u32(hle, sfd_ptr + SFD2_1C_PTR);
        uint32_t ptr_20          = *dram_u32(hle, sfd_ptr + SFD2_20_PTR);
        uint32_t ptr_24          = *dram_u32(hle, sfd_ptr + SFD2_24_PTR);

        uint32_t last_sample_ptr = state_ptr + STATE_LAST_SAMPLE;
        uint32_t output_ptr;

        /* load state */
        load_base_vol(hle, musyx.base_vol, state_ptr + STATE_BASE_VOL);
        dram_load_u16(hle, (uint16_t *)musyx.subframe_740_last4,
                state_ptr + STATE_740_LAST4_V2, 4);

        /* initialize internal subframes using updated base volumes */
        update_base_vol(hle, musyx.base_vol, voice_mask, last_sample_ptr, mask_15, ptr_24);
        init_subframes_v2(&musyx);

        if (ptr_10) {
            /* TODO */
            HleWarnMessage(hle->user_defined,
                           "ptr_10=%08x mask_14=%02x ptr_24=%08x",
                           ptr_10, mask_14, ptr_24);
        }

        /* active voices get mixed into L,R,cc0,e50 subframes (optional) */
        output_ptr = voice_stage(hle, &musyx, voice_ptr, last_sample_ptr);

        /* apply delay-based effects (optional) */
        sfx_stage(hle, mix_sfx_with_main_subframes_v2,
                  &musyx, sfx_ptr, sfx_index);

        dram_store_u16(hle, (uint16_t*)musyx.left,  output_ptr                  , SUBFRAME_SIZE);
        dram_store_u16(hle, (uint16_t*)musyx.right, output_ptr + 2*SUBFRAME_SIZE, SUBFRAME_SIZE);
        dram_store_u16(hle, (uint16_t*)musyx.cc0,   output_ptr + 4*SUBFRAME_SIZE, SUBFRAME_SIZE);

        /* store state */
        save_base_vol(hle, musyx.base_vol, state_ptr + STATE_BASE_VOL);
        dram_store_u16(hle, (uint16_t*)musyx.subframe_740_last4,
                state_ptr + STATE_740_LAST4_V2, 4);

        if (mask_16)
            interleave_stage_v2(hle, &musyx, mask_16, ptr_18, ptr_1c, ptr_20);

        --sfd_count;
        if (sfd_count == 0)
            break;

        sfd_ptr += SFD2_VOICES + MAX_VOICES * VOICE_SIZE;
    }

    rsp_break(hle, SP_STATUS_TASKDONE);
}





static void load_base_vol(struct hle_t* hle, int32_t *base_vol, uint32_t address)
{
    base_vol[0] = ((uint32_t)(*dram_u16(hle, address))     << 16) | (*dram_u16(hle, address +  8));
    base_vol[1] = ((uint32_t)(*dram_u16(hle, address + 2)) << 16) | (*dram_u16(hle, address + 10));
    base_vol[2] = ((uint32_t)(*dram_u16(hle, address + 4)) << 16) | (*dram_u16(hle, address + 12));
    base_vol[3] = ((uint32_t)(*dram_u16(hle, address + 6)) << 16) | (*dram_u16(hle, address + 14));
}

static void save_base_vol(struct hle_t* hle, const int32_t *base_vol, uint32_t address)
{
    unsigned k;

    for (k = 0; k < 4; ++k) {
        *dram_u16(hle, address) = (uint16_t)(base_vol[k] >> 16);
        address += 2;
    }

    for (k = 0; k < 4; ++k) {
        *dram_u16(hle, address) = (uint16_t)(base_vol[k]);
        address += 2;
    }
}

static void update_base_vol(struct hle_t* hle, int32_t *base_vol,
                            uint32_t voice_mask, uint32_t last_sample_ptr,
                            uint8_t mask_15, uint32_t ptr_24)
{
    unsigned i, k;
    uint32_t mask;

    HleVerboseMessage(hle->user_defined, "base_vol voice_mask = %08x", voice_mask);
    HleVerboseMessage(hle->user_defined,
                      "BEFORE: base_vol = %08x %08x %08x %08x",
                      base_vol[0], base_vol[1], base_vol[2], base_vol[3]);

    /* optim: skip voices contributions entirely if voice_mask is empty */
    if (voice_mask != 0) {
        for (i = 0, mask = 1; i < MAX_VOICES;
             ++i, mask <<= 1, last_sample_ptr += 8) {
            if ((voice_mask & mask) == 0)
                continue;

            for (k = 0; k < 4; ++k)
                base_vol[k] += (int16_t)*dram_u16(hle, last_sample_ptr + k * 2);
        }
    }

    /* optim: skip contributions entirely if mask_15 is empty */
    if (mask_15 != 0) {
        for(i = 0, mask = 1; i < 4;
                ++i, mask <<= 1, ptr_24 += 8) {
            if ((mask_15 & mask) == 0)
                continue;

            for(k = 0; k < 4; ++k)
                base_vol[k] += (int16_t)*dram_u16(hle, ptr_24 + k * 2);
        }
    }

    /* apply 3% decay */
    for (k = 0; k < 4; ++k)
        base_vol[k] = (base_vol[k] * 0x0000f850) >> 16;

    HleVerboseMessage(hle->user_defined,
                      "AFTER: base_vol = %08x %08x %08x %08x",
                      base_vol[0], base_vol[1], base_vol[2], base_vol[3]);
}




static void init_subframes_v1(musyx_t *musyx)
{
    unsigned i;

    int16_t base_cc0 = clamp_s16(musyx->base_vol[2]);
    int16_t base_e50 = clamp_s16(musyx->base_vol[3]);

    int16_t *left  = musyx->left;
    int16_t *right = musyx->right;
    int16_t *cc0   = musyx->cc0;
    int16_t *e50   = musyx->e50;

    for (i = 0; i < SUBFRAME_SIZE; ++i) {
        *(e50++)    = base_e50;
        *(left++)   = clamp_s16(*cc0 + base_cc0);
        *(right++)  = clamp_s16(-*cc0 - base_cc0);
        *(cc0++)    = 0;
    }
}

static void init_subframes_v2(musyx_t *musyx)
{
    unsigned i,k;
    int16_t values[4];
    int16_t* subframes[4];

    for(k = 0; k < 4; ++k)
        values[k] = clamp_s16(musyx->base_vol[k]);

    subframes[0] = musyx->left;
    subframes[1] = musyx->right;
    subframes[2] = musyx->cc0;
    subframes[3] = musyx->e50;

    for (i = 0; i < SUBFRAME_SIZE; ++i) {

        for(k = 0; k < 4; ++k)
            *(subframes[k]++) = values[k];
    }
}

/* Process voices, and returns interleaved subframe destination address */
static uint32_t voice_stage(struct hle_t* hle, musyx_t *musyx,
                            uint32_t voice_ptr, uint32_t last_sample_ptr)
{
    uint32_t output_ptr;
    int i = 0;

    /* voice stage can be skipped if first voice has no samples */
    if (*dram_u16(hle, voice_ptr + VOICE_CATSRC_0 + CATSRC_SIZE1) == 0) {
        HleVerboseMessage(hle->user_defined, "Skipping Voice stage");
        output_ptr = *dram_u32(hle, voice_ptr + VOICE_INTERLEAVED_PTR);
    } else {
        /* otherwise process voices until a non null output_ptr is encountered */
        for (;;) {
            /* load voice samples (PCM16 or APDCM) */
            int16_t samples[SAMPLE_BUFFER_SIZE];
            unsigned segbase;
            unsigned offset;

            HleVerboseMessage(hle->user_defined, "Processing Voice #%d", i);

            if (*dram_u8(hle, voice_ptr + VOICE_ADPCM_FRAMES) == 0)
                load_samples_PCM16(hle, voice_ptr, samples, &segbase, &offset);
            else
                load_samples_ADPCM(hle, voice_ptr, samples, &segbase, &offset);

            /* mix them with each internal subframes */
            mix_voice_samples(hle, musyx, voice_ptr, samples, segbase, offset,
                              last_sample_ptr + i * 8);

            /* check break condition */
            output_ptr = *dram_u32(hle, voice_ptr + VOICE_INTERLEAVED_PTR);
            if (output_ptr != 0)
                break;

            /* next voice */
            ++i;
            voice_ptr += VOICE_SIZE;
        }
    }

    return output_ptr;
}

static void dma_cat8(struct hle_t* hle, uint8_t *dst, uint32_t catsrc_ptr)
{
    uint32_t ptr1  = *dram_u32(hle, catsrc_ptr + CATSRC_PTR1);
    uint32_t ptr2  = *dram_u32(hle, catsrc_ptr + CATSRC_PTR2);
    uint16_t size1 = *dram_u16(hle, catsrc_ptr + CATSRC_SIZE1);
    uint16_t size2 = *dram_u16(hle, catsrc_ptr + CATSRC_SIZE2);

    size_t count1 = size1;
    size_t count2 = size2;

    HleVerboseMessage(hle->user_defined,
                      "dma_cat: %08x %08x %04x %04x",
                      ptr1,
                      ptr2,
                      size1,
                      size2);

    dram_load_u8(hle, dst, ptr1, count1);

    if (size2 == 0)
        return;

    dram_load_u8(hle, dst + count1, ptr2, count2);
}

static void dma_cat16(struct hle_t* hle, uint16_t *dst, uint32_t catsrc_ptr)
{
    uint32_t ptr1  = *dram_u32(hle, catsrc_ptr + CATSRC_PTR1);
    uint32_t ptr2  = *dram_u32(hle, catsrc_ptr + CATSRC_PTR2);
    uint16_t size1 = *dram_u16(hle, catsrc_ptr + CATSRC_SIZE1);
    uint16_t size2 = *dram_u16(hle, catsrc_ptr + CATSRC_SIZE2);

    size_t count1 = size1 >> 1;
    size_t count2 = size2 >> 1;

    HleVerboseMessage(hle->user_defined,
                      "dma_cat: %08x %08x %04x %04x",
                      ptr1,
                      ptr2,
                      size1,
                      size2);

    dram_load_u16(hle, dst, ptr1, count1);

    if (size2 == 0)
        return;

    dram_load_u16(hle, dst + count1, ptr2, count2);
}

static void load_samples_PCM16(struct hle_t* hle, uint32_t voice_ptr, int16_t *samples,
                               unsigned *segbase, unsigned *offset)
{

    uint8_t  u8_3e  = *dram_u8(hle, voice_ptr + VOICE_SKIP_SAMPLES);
    uint16_t u16_40 = *dram_u16(hle, voice_ptr + VOICE_U16_40);
    uint16_t u16_42 = *dram_u16(hle, voice_ptr + VOICE_U16_42);

    unsigned count = align(u16_40 + u8_3e, 4);

    HleVerboseMessage(hle->user_defined, "Format: PCM16");

    *segbase = SAMPLE_BUFFER_SIZE - count;
    *offset  = u8_3e;

    dma_cat16(hle, (uint16_t *)samples + *segbase, voice_ptr + VOICE_CATSRC_0);

    if (u16_42 != 0)
        dma_cat16(hle, (uint16_t *)samples, voice_ptr + VOICE_CATSRC_1);
}

static void load_samples_ADPCM(struct hle_t* hle, uint32_t voice_ptr, int16_t *samples,
                               unsigned *segbase, unsigned *offset)
{
    /* decompressed samples cannot exceed 0x400 bytes;
     * ADPCM has a compression ratio of 5/16 */
    uint8_t buffer[SAMPLE_BUFFER_SIZE * 2 * 5 / 16];
    int16_t adpcm_table[128];

    uint8_t u8_3c = *dram_u8(hle, voice_ptr + VOICE_ADPCM_FRAMES    );
    uint8_t u8_3d = *dram_u8(hle, voice_ptr + VOICE_ADPCM_FRAMES + 1);
    uint8_t u8_3e = *dram_u8(hle, voice_ptr + VOICE_SKIP_SAMPLES    );
    uint8_t u8_3f = *dram_u8(hle, voice_ptr + VOICE_SKIP_SAMPLES + 1);
    uint32_t adpcm_table_ptr = *dram_u32(hle, voice_ptr + VOICE_ADPCM_TABLE_PTR);
    unsigned count;

    HleVerboseMessage(hle->user_defined, "Format: ADPCM");

    HleVerboseMessage(hle->user_defined, "Loading ADPCM table: %08x", adpcm_table_ptr);
    dram_load_u16(hle, (uint16_t *)adpcm_table, adpcm_table_ptr, 128);

    count = u8_3c << 5;

    *segbase = SAMPLE_BUFFER_SIZE - count;
    *offset  = u8_3e & 0x1f;

    dma_cat8(hle, buffer, voice_ptr + VOICE_CATSRC_0);
    adpcm_decode_frames(hle, samples + *segbase, buffer, adpcm_table, u8_3c, u8_3e);

    if (u8_3d != 0) {
        dma_cat8(hle, buffer, voice_ptr + VOICE_CATSRC_1);
        adpcm_decode_frames(hle, samples, buffer, adpcm_table, u8_3d, u8_3f);
    }
}

static void adpcm_decode_frames(struct hle_t* hle,
                                int16_t *dst, const uint8_t *src,
                                const int16_t *table, uint8_t count,
                                uint8_t skip_samples)
{
    int16_t frame[32];
    const uint8_t *nibbles = src + 8;
    unsigned i;
    bool jump_gap = false;

    HleVerboseMessage(hle->user_defined,
                      "ADPCM decode: count=%d, skip=%d",
                      count, skip_samples);

    if (skip_samples >= 32) {
        jump_gap = true;
        nibbles += 16;
        src += 4;
    }

    for (i = 0; i < count; ++i) {
        uint8_t c2 = nibbles[0];

        const int16_t *book = (c2 & 0xf0) + table;
        unsigned int rshift = (c2 & 0x0f);

        adpcm_predict_frame(frame, src, nibbles, rshift);

        memcpy(dst, frame, 2 * sizeof(frame[0]));
        adpcm_compute_residuals(dst +  2, frame +  2, book, dst     , 6);
        adpcm_compute_residuals(dst +  8, frame +  8, book, dst +  6, 8);
        adpcm_compute_residuals(dst + 16, frame + 16, book, dst + 14, 8);
        adpcm_compute_residuals(dst + 24, frame + 24, book, dst + 22, 8);

        if (jump_gap) {
            nibbles += 8;
            src += 32;
        }

        jump_gap = !jump_gap;
        nibbles += 16;
        src += 4;
        dst += 32;
    }
}

static void adpcm_predict_frame(int16_t *dst, const uint8_t *src,
                                const uint8_t *nibbles,
                                unsigned int rshift)
{
    unsigned int i;

    *(dst++) = (src[0] << 8) | src[1];
    *(dst++) = (src[2] << 8) | src[3];

    for (i = 1; i < 16; ++i) {
        uint8_t byte = nibbles[i];

        *(dst++) = adpcm_predict_sample(byte, 0xf0,  8, rshift);
        *(dst++) = adpcm_predict_sample(byte, 0x0f, 12, rshift);
    }
}

static void mix_voice_samples(struct hle_t* hle, musyx_t *musyx,
                              uint32_t voice_ptr, const int16_t *samples,
                              unsigned segbase, unsigned offset, uint32_t last_sample_ptr)
{
    int i, k;

    /* parse VOICE structure */
    const uint16_t pitch_q16   = *dram_u16(hle, voice_ptr + VOICE_PITCH_Q16);
    const uint16_t pitch_shift = *dram_u16(hle, voice_ptr + VOICE_PITCH_SHIFT); /* Q4.12 */

    const uint16_t end_point     = *dram_u16(hle, voice_ptr + VOICE_END_POINT);
    const uint16_t restart_point = *dram_u16(hle, voice_ptr + VOICE_RESTART_POINT);

    const uint16_t u16_4e = *dram_u16(hle, voice_ptr + VOICE_U16_4E);

    /* init values and pointers */
    const int16_t       *sample         = samples + segbase + offset + u16_4e;
    const int16_t *const sample_end     = samples + segbase + end_point;
    const int16_t *const sample_restart = samples + (restart_point & 0x7fff) +
                                          (((restart_point & 0x8000) != 0) ? 0x000 : segbase);


    uint32_t pitch_accu = pitch_q16;
    uint32_t pitch_step = pitch_shift << 4;

    int32_t  v4_env[4];
    int32_t  v4_env_step[4];
    int16_t *v4_dst[4];
    int16_t  v4[4];

    dram_load_u32(hle, (uint32_t *)v4_env,      voice_ptr + VOICE_ENV_BEGIN, 4);
    dram_load_u32(hle, (uint32_t *)v4_env_step, voice_ptr + VOICE_ENV_STEP,  4);

    if (musyx->hires != NULL)
        mix_voice_hires(musyx->hires, samples, segbase, sample, sample_end, sample_restart,
                        pitch_accu, pitch_step, v4_env, v4_env_step);

    v4_dst[0] = musyx->left;
    v4_dst[1] = musyx->right;
    v4_dst[2] = musyx->cc0;
    v4_dst[3] = musyx->e50;

    HleVerboseMessage(hle->user_defined,
                      "Voice debug: segbase=%d"
                      "\tu16_4e=%04x\n"
                      "\tpitch: frac0=%04x shift=%04x\n"
                      "\tend_point=%04x restart_point=%04x\n"
                      "\tenv      = %08x %08x %08x %08x\n"
                      "\tenv_step = %08x %08x %08x %08x\n",
                      segbase,
                      u16_4e,
                      pitch_q16, pitch_shift,
                      end_point, restart_point,
                      v4_env[0],      v4_env[1],      v4_env[2],      v4_env[3],
                      v4_env_step[0], v4_env_step[1], v4_env_step[2], v4_env_step[3]);

    for (i = 0; i < SUBFRAME_SIZE; ++i) {
        /* update sample and lut pointers and then pitch_accu */
        const int16_t *lut = (RESAMPLE_LUT + ((pitch_accu & 0xfc00) >> 8));
        int dist;
        int16_t v;

        sample += (pitch_accu >> 16);
        pitch_accu &= 0xffff;
        pitch_accu += pitch_step;

        /* handle end/restart points */
        dist = sample - sample_end;
        if (dist >= 0)
            sample = sample_restart + dist;

        /* apply resample filter */
        v = clamp_s16(dot4(sample, lut));

        for (k = 0; k < 4; ++k) {
            /* envmix */
            int32_t accu = (v * (v4_env[k] >> 16)) >> 15;
            v4[k] = clamp_s16(accu);
            *(v4_dst[k]) = clamp_s16(accu + *(v4_dst[k]));

            /* update envelopes and dst pointers */
            ++(v4_dst[k]);
            v4_env[k] += v4_env_step[k];
        }
    }

    /* save last resampled sample */
    dram_store_u16(hle, (uint16_t *)v4, last_sample_ptr, 4);

    HleVerboseMessage(hle->user_defined,
                      "last_sample = %04x %04x %04x %04x",
                      v4[0], v4[1], v4[2], v4[3]);
}


static void sfx_stage(struct hle_t* hle, mix_sfx_with_main_subframes_t mix_sfx_with_main_subframes,
                      musyx_t *musyx, uint32_t sfx_ptr, uint16_t idx)
{
    unsigned int i;

    int16_t buffer[SUBFRAME_SIZE + 4];
    int16_t *subframe = buffer + 4;

    uint32_t tap_delays[8];
    int16_t tap_gains[8];
    int16_t fir4_hcoeffs[4];

    int16_t delayed[SUBFRAME_SIZE];
    int dpos, dlength;

    const uint32_t pos = idx * SUBFRAME_SIZE;

    uint32_t cbuffer_ptr;
    uint32_t cbuffer_length;
    uint16_t tap_count;
    int16_t fir4_hgain;
    uint16_t sfx_gains[2];

    HleVerboseMessage(hle->user_defined, "SFX: %08x, idx=%d", sfx_ptr, idx);

    if (sfx_ptr == 0) {
        if (musyx->hires != NULL)
            memset(musyx->hires->sfx_history, 0, sizeof(musyx->hires->sfx_history));
        return;
    }

    /* load sfx  parameters */
    cbuffer_ptr    = *dram_u32(hle, sfx_ptr + SFX_CBUFFER_PTR);
    cbuffer_length = *dram_u32(hle, sfx_ptr + SFX_CBUFFER_LENGTH);

    tap_count      = *dram_u16(hle, sfx_ptr + SFX_TAP_COUNT);

    dram_load_u32(hle, tap_delays, sfx_ptr + SFX_TAP_DELAYS, 8);
    dram_load_u16(hle, (uint16_t *)tap_gains,  sfx_ptr + SFX_TAP_GAINS,  8);

    fir4_hgain     = *dram_u16(hle, sfx_ptr + SFX_FIR4_HGAIN);
    dram_load_u16(hle, (uint16_t *)fir4_hcoeffs, sfx_ptr + SFX_FIR4_HCOEFFS, 4);

    sfx_gains[0]   = *dram_u16(hle, sfx_ptr + SFX_U16_3C);
    sfx_gains[1]   = *dram_u16(hle, sfx_ptr + SFX_U16_3E);

    HleVerboseMessage(hle->user_defined,
                      "cbuffer: ptr=%08x length=%x", cbuffer_ptr,
                      cbuffer_length);

    HleVerboseMessage(hle->user_defined,
                      "fir4: hgain=%04x hcoeff=%04x %04x %04x %04x",
                      fir4_hgain,
                      fir4_hcoeffs[0], fir4_hcoeffs[1], fir4_hcoeffs[2], fir4_hcoeffs[3]);

    HleVerboseMessage(hle->user_defined,
                      "tap count=%d\n"
                      "delays: %08x %08x %08x %08x %08x %08x %08x %08x\n"
                      "gains:  %04x %04x %04x %04x %04x %04x %04x %04x",
                      tap_count,
                      tap_delays[0], tap_delays[1], tap_delays[2], tap_delays[3],
                      tap_delays[4], tap_delays[5], tap_delays[6], tap_delays[7],
                      tap_gains[0], tap_gains[1], tap_gains[2], tap_gains[3],
                      tap_gains[4], tap_gains[5], tap_gains[6], tap_gains[7]);

    HleVerboseMessage(hle->user_defined, "sfx_gains=%04x %04x", sfx_gains[0], sfx_gains[1]);

    /* mix up to 8 delayed subframes */
    memset(subframe, 0, SUBFRAME_SIZE * sizeof(subframe[0]));
    for (i = 0; i < tap_count; ++i) {

        dpos = pos - tap_delays[i];
        if (dpos <= 0)
            dpos += cbuffer_length;
        dlength = SUBFRAME_SIZE;

        if ((uint32_t)(dpos + SUBFRAME_SIZE) > cbuffer_length) {
            dlength = cbuffer_length - dpos;
            dram_load_u16(hle, (uint16_t *)delayed + dlength, cbuffer_ptr, SUBFRAME_SIZE - dlength);
        }

        dram_load_u16(hle, (uint16_t *)delayed, cbuffer_ptr + dpos * 2, dlength);

        mix_subframes(subframe, delayed, tap_gains[i]);
    }

    /* add resulting subframe to main subframes */
    mix_sfx_with_main_subframes(musyx, subframe, sfx_gains);
    if (musyx->hires != NULL)
        mix_sfx_hires(musyx->hires, subframe);

    /* apply FIR4 filter and writeback filtered result */
    memcpy(buffer, musyx->subframe_740_last4, 4 * sizeof(int16_t));
    memcpy(musyx->subframe_740_last4, subframe + SUBFRAME_SIZE - 4, 4 * sizeof(int16_t));
    mix_fir4(musyx->e50, buffer + 1, fir4_hgain, fir4_hcoeffs);
    dram_store_u16(hle, (uint16_t *)musyx->e50, cbuffer_ptr + pos * 2, SUBFRAME_SIZE);
}

static void mix_sfx_with_main_subframes_v1(musyx_t *musyx, const int16_t *subframe,
                                           const uint16_t* UNUSED(gains))
{
    unsigned i;

    for (i = 0; i < SUBFRAME_SIZE; ++i) {
        int16_t v = subframe[i];
        musyx->left[i]  = clamp_s16(musyx->left[i]  + v);
        musyx->right[i] = clamp_s16(musyx->right[i] + v);
    }
}

static void mix_sfx_with_main_subframes_v2(musyx_t *musyx, const int16_t *subframe,
                                           const uint16_t* gains)
{
    unsigned i;

    for (i = 0; i < SUBFRAME_SIZE; ++i) {
        int16_t v = subframe[i];
        int16_t v1 = (int32_t)(v * gains[0]) >> 16;
        int16_t v2 = (int32_t)(v * gains[1]) >> 16;

        musyx->left[i]  = clamp_s16(musyx->left[i]  + v1);
        musyx->right[i] = clamp_s16(musyx->right[i] + v1);
        musyx->cc0[i]   = clamp_s16(musyx->cc0[i]   + v2);
    }
}

static void mix_samples(int16_t *y, int16_t x, int16_t hgain)
{
    *y = clamp_s16(*y + ((x * hgain + 0x4000) >> 15));
}

static void mix_subframes(int16_t *y, const int16_t *x, int16_t hgain)
{
    unsigned int i;

    for (i = 0; i < SUBFRAME_SIZE; ++i)
        mix_samples(&y[i], x[i], hgain);
}

static void mix_fir4(int16_t *y, const int16_t *x, int16_t hgain, const int16_t *hcoeffs)
{
    unsigned int i;
    int32_t h[4];

    h[0] = (hgain * hcoeffs[0]) >> 15;
    h[1] = (hgain * hcoeffs[1]) >> 15;
    h[2] = (hgain * hcoeffs[2]) >> 15;
    h[3] = (hgain * hcoeffs[3]) >> 15;

    for (i = 0; i < SUBFRAME_SIZE; ++i) {
        int32_t v = (h[0] * x[i] + h[1] * x[i + 1] + h[2] * x[i + 2] + h[3] * x[i + 3]) >> 15;
        y[i] = clamp_s16(y[i] + v);
    }
}

static void interleave_stage_v1(struct hle_t* hle, musyx_t *musyx, uint32_t output_ptr)
{
    size_t i;

    int16_t base_left;
    int16_t base_right;

    int16_t *left;
    int16_t *right;
    uint32_t *dst;

    HleVerboseMessage(hle->user_defined, "interleave: %08x", output_ptr);

    base_left  = clamp_s16(musyx->base_vol[0]);
    base_right = clamp_s16(musyx->base_vol[1]);

    left  = musyx->left;
    right = musyx->right;
    dst  = dram_u32(hle, output_ptr);

    for (i = 0; i < SUBFRAME_SIZE; ++i) {
        uint16_t l = clamp_s16(*(left++)  + base_left);
        uint16_t r = clamp_s16(*(right++) + base_right);

        *(dst++) = (l << 16) | r;
    }
}

static void interleave_stage_v2(struct hle_t* hle, musyx_t *musyx,
                                uint16_t mask_16, uint32_t ptr_18,
                                uint32_t ptr_1c, uint32_t output_ptr)
{
    unsigned i, k;
    int16_t subframe[SUBFRAME_SIZE];
    uint32_t *dst;
    uint16_t mask;

    HleVerboseMessage(hle->user_defined,
                      "mask_16=%04x ptr_18=%08x ptr_1c=%08x output_ptr=%08x",
                      mask_16, ptr_18, ptr_1c, output_ptr);

    /* compute L_total, R_total and update subframe @ptr_1c */
    memset(subframe, 0, SUBFRAME_SIZE*sizeof(subframe[0]));

    for(i = 0; i < SUBFRAME_SIZE; ++i) {
        int16_t v = *dram_u16(hle, ptr_1c + i*2);
        musyx->left[i] = v;
        musyx->right[i] = clamp_s16(-v);
    }

    for (k = 0, mask = 1; k < 8; ++k, mask <<= 1, ptr_18 += 8) {
        int16_t hgain;
        uint32_t address;

        if ((mask_16 & mask) == 0)
            continue;

        address = *dram_u32(hle, ptr_18);
        hgain   = *dram_u16(hle, ptr_18 + 4);

        for(i = 0; i < SUBFRAME_SIZE; ++i, address += 2) {
            mix_samples(&musyx->left[i],  *dram_u16(hle, address), hgain);
            mix_samples(&musyx->right[i], *dram_u16(hle, address + 2*SUBFRAME_SIZE), hgain);
            mix_samples(&subframe[i],     *dram_u16(hle, address + 4*SUBFRAME_SIZE), hgain);
        }
    }

    /* interleave L_total and R_total */
    dst = dram_u32(hle, output_ptr);
    for(i = 0; i < SUBFRAME_SIZE; ++i) {
        uint16_t l = musyx->left[i];
        uint16_t r = musyx->right[i];
        *(dst++) = (l << 16) | r;
    }

    /* writeback subframe @ptr_1c */
    dram_store_u16(hle, (uint16_t*)subframe, ptr_1c, SUBFRAME_SIZE);
}

/**************************************************************************
 * rush2049: high-quality side mix (see musyx_hires_t)
 **************************************************************************/
static double bessel_i0(double x)
{
    double sum = 1.0, term = 1.0;
    int k;

    for (k = 1; k < 32; ++k) {
        term *= (x / (2.0 * k)) * (x / (2.0 * k));
        sum += term;
    }

    return sum;
}

/* hires_kernel[p] weighs sample[0..3] for the point p/HIRES_PHASES of the way from sample[1] to sample[2], the
 * same taps RESAMPLE_LUT uses (the game loads no more than that). A Kaiser window with beta 3 keeps the passband
 * flatter than RESAMPLE_LUT (-2.5 dB against -6.5 dB at 0.4 of the sample rate) and rejects the images further
 * out better (-36 dB against -29 dB at 0.75). */
static void init_hires_kernel(void)
{
    const double pi = 3.14159265358979323846;
    const double beta = 3.0;
    unsigned p, k;

    for (p = 0; p <= HIRES_PHASES; ++p) {
        double c[4];
        double sum = 0.0;
        double f = (double)p / HIRES_PHASES;

        for (k = 0; k < 4; ++k) {
            double t = f + 1.0 - k;
            double w = 1.0 - (t / 2.0) * (t / 2.0);
            double sinc = (t == 0.0) ? 1.0 : sin(pi * t) / (pi * t);

            c[k] = (w > 0.0) ? sinc * bessel_i0(beta * sqrt(w)) / bessel_i0(beta) : 0.0;
            sum += c[k];
        }

        for (k = 0; k < 4; ++k)
            hires_kernel[p][k] = (float)(c[k] / sum);
    }
}

/* frac is Q16; the phases in between are interpolated linearly */
static float hires_interpolate(const int16_t *x, uint32_t frac)
{
    const float *a = hires_kernel[frac >> 8];
    const float *b = hires_kernel[(frac >> 8) + 1];
    float va = a[0] * x[0] + a[1] * x[1] + a[2] * x[2] + a[3] * x[3];
    float vb = b[0] * x[0] + b[1] * x[1] + b[2] * x[2] + b[3] * x[3];

    return va + (vb - va) * (float)(frac & 0xff) * (1.0f / 256.0f);
}

static int16_t hires_s16(float x)
{
    x = (x < -32768.0f) ? -32768.0f : x;
    x = (x >  32767.0f) ?  32767.0f : x;

    return (int16_t)lrintf(x);
}

static void hires_begin_task(musyx_hires_t *hires, const int16_t *cc0)
{
    static bool kernel_ready = false;
    unsigned i;

    if (!kernel_ready) {
        init_hires_kernel();
        kernel_ready = true;
    }

    /* The carried cc0 belongs to the previous task's cc0. If the game's state doesn't match it (the first task,
     * or a state the side mix never saw), rebuild the carry from the game's. */
    if (memcmp(hires->cc0_check, cc0, sizeof(hires->cc0_check)) == 0)
        return;

    for (i = 0; i < SUBFRAME_SIZE; ++i) {
        int16_t next = cc0[(i + 1 < SUBFRAME_SIZE) ? i + 1 : i];

        hires->cc0[2 * i]     = cc0[i];
        hires->cc0[2 * i + 1] = 0.5f * ((float)cc0[i] + (float)next);
    }
}

/* mirrors init_subframes_v1, before it clears cc0 */
static void init_hires_v1(musyx_t *musyx)
{
    musyx_hires_t *hires = musyx->hires;
    float base_cc0 = clamp_s16(musyx->base_vol[2]);
    unsigned i;

    for (i = 0; i < HIRES_SIZE; ++i) {
        float c = hires->cc0[i] + base_cc0;

        hires->left[i]  = c;
        hires->right[i] = -c;
        hires->cc0[i]   = 0.0f;
    }
}

/* The voice again at twice the rate. Output i is at time i/2 of the original's 192 outputs, so the even ones land on
 * the sample positions the original mix uses, and the envelopes ramp the same way. */
static void mix_voice_hires(musyx_hires_t *hires, const int16_t *samples, unsigned segbase,
                            const int16_t *sample, const int16_t *sample_end,
                            const int16_t *sample_restart, uint32_t pitch_accu, uint32_t pitch_step,
                            const int32_t *env, const int32_t *env_step)
{
    const uint32_t half_step = pitch_step >> 1; /* pitch_step is a multiple of 16, so this is exact */
    float *dst[3];
    int i, k;

    dst[0] = hires->left;
    dst[1] = hires->right;
    dst[2] = hires->cc0;

    for (i = 0; i < HIRES_SIZE; ++i) {
        uint32_t frac = pitch_accu & 0xffff;
        const int16_t *next = sample + (pitch_accu >> 16);
        int dist = next - sample_end;
        float v;

        if (dist >= 0)
            next = sample_restart + dist;

        /* The last output is half a step past the original's last one, which can need a sample the game did not
         * load. The main segment is loaded up to the end of the buffer; anywhere else, hold the position. */
        if (i == HIRES_SIZE - 1 && next != sample &&
            !(next >= samples + segbase && next + 3 < samples + SAMPLE_BUFFER_SIZE)) {
            next = sample;
            frac = 0xffff;
        }

        sample = next;
        pitch_accu = (pitch_accu & 0xffff) + half_step;
        v = hires_interpolate(sample, frac);

        for (k = 0; k < 3; ++k) {
            /* the envelope at time i/2: env + env_step * i / 2, then >> 16 as envmix does */
            int64_t e = ((int64_t)env[k] * 2 + (int64_t)env_step[k] * i) >> 17;

            dst[k][i] += v * (float)e * (1.0f / 32768.0f);
        }
    }
}

/* The delay effect's output at twice the rate, as mix_sfx_with_main_subframes_v1 adds it. It lags by two samples
 * of the original rate, so the odd outputs can be interpolated from samples on both sides; on a reverb that is
 * inaudible. */
static void mix_sfx_hires(musyx_hires_t *hires, const int16_t *subframe)
{
    const float *half = hires_kernel[HIRES_PHASES / 2];
    float x[SUBFRAME_SIZE + 3];
    unsigned i;

    memcpy(x, hires->sfx_history, sizeof(hires->sfx_history));
    for (i = 0; i < SUBFRAME_SIZE; ++i)
        x[i + 3] = subframe[i];
    memcpy(hires->sfx_history, x + SUBFRAME_SIZE, sizeof(hires->sfx_history));

    for (i = 0; i < SUBFRAME_SIZE; ++i) {
        float even = x[i + 1];
        float odd  = half[0] * x[i] + half[1] * x[i + 1] + half[2] * x[i + 2] + half[3] * x[i + 3];

        hires->left[2 * i]      += even;
        hires->right[2 * i]     += even;
        hires->left[2 * i + 1]  += odd;
        hires->right[2 * i + 1] += odd;
    }
}

/* mirrors interleave_stage_v1, into a host-side buffer */
static void interleave_hires_v1(struct hle_t* hle, musyx_t *musyx, uint32_t output_ptr)
{
    musyx_hires_t *hires = musyx->hires;
    float base_left  = clamp_s16(musyx->base_vol[0]);
    float base_right = clamp_s16(musyx->base_vol[1]);
    uint32_t out[HIRES_SIZE];
    unsigned i;

    for (i = 0; i < HIRES_SIZE; ++i) {
        uint16_t l = (uint16_t)hires_s16(hires->left[i]  + base_left);
        uint16_t r = (uint16_t)hires_s16(hires->right[i] + base_right);

        out[i] = ((uint32_t)l << 16) | r;
    }

    musyx_hires_store(dram_u32(hle, output_ptr), out);
}
