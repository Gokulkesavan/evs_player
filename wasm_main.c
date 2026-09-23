/* wasm_main.c -----------------------------------------------------------------
 * Drop-in main() for the 3GPP TS 26.443 EVS reference decoder, WASM-friendly.
 *
 * Adds a "-mime" mode that reads the EVS MIME storage format produced by
 * index.html (buildEvsMimeFile). Also exports a programmatic entry point
 * evs_mime_decode(in_path, out_path, fs_hz) for ccall().
 *
 * HOW TO WIRE IT IN
 * -----------------
 *  1. In lib_dec/evs_dec.c, rename the existing entry point:
 *         int main(int argc, char** argv)  ->  int evs_ref_main(int argc, char** argv)
 *     (add a prototype in that file or a shared header).
 *
 *  2. Add this file to the build (see build.sh).
 *
 *  3. Fix the four EVS_* macros below to match the exact API names in your
 *     TS 26.443 release. Every release I've seen exposes *some* form of
 *     open / process / close; the shapes are stable, only the names drift.
 * -----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define EXPORT
#endif

/* ---- ADAPT THESE FOUR LINES to your TS 26.443 release ----------------------*/
extern void* EVS_DEC_Open (int sampling_rate_hz);                                  /* TODO */
extern int   EVS_DEC_Process(void* st, const uint8_t* bs, int bs_len,
                             int16_t* pcm_out, int* nsamp_out, int bfi);           /* TODO */
extern void  EVS_DEC_Close (void* st);                                             /* TODO */
/* ---------------------------------------------------------------------------*/

/* Fallback: the original reference CLI (so `EVS_dec file.in file.out` still works) */
extern int evs_ref_main(int argc, char** argv);

/* Frame sizes in bytes = ceil(bits/8). Indexed by ToC rate_idx. */
static const int PRIMARY_BYTES[16] = {
    7, 18, 20, 24, 33, 41, 61, 80, 120, 160, 240, 320, /* 0..11  (2.8 .. 128 kbps) */
    6,                                                   /* 12     SID 2.4 kbps       */
    0,                                                   /* 13     reserved           */
    0,                                                   /* 14     SPEECH_LOST        */
    0                                                    /* 15     NO_DATA            */
};
static const int AMRWBIO_BYTES[16] = {
    17, 23, 32, 36, 40, 46, 50, 58, 60, /* 0..8  (6.60 .. 23.85 kbps) */
    5,                                   /* 9     SID                  */
    0, 0, 0, 0,                          /* 10..13 reserved            */
    0,                                   /* 14    SPEECH_LOST          */
    0                                    /* 15    NO_DATA              */
};

static const uint8_t MIME_MAGIC[12] = {
    0x23,0x21,0x45,0x56,0x53,0x5F,0x4D,0x43,0x31,0x2E,0x30,0x0A /* "#!EVS_MC1.0\n" */
};

static uint32_t be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] <<  8) |  (uint32_t)p[3];
}

/* Core decoder loop. Returns 0 on success. */
static int mime_decode(const char* in_path, const char* out_path, int fs_hz)
{
    FILE* fin  = fopen(in_path,  "rb");
    FILE* fout = fopen(out_path, "wb");
    if (!fin || !fout) {
        if (fin)  fclose(fin);
        if (fout) fclose(fout);
        fprintf(stderr, "mime_decode: cannot open in=%s out=%s\n", in_path, out_path);
        return -1;
    }

    uint8_t hdr[16];
    if (fread(hdr, 1, 16, fin) != 16 || memcmp(hdr, MIME_MAGIC, 12) != 0) {
        fprintf(stderr, "mime_decode: bad MIME header\n");
        fclose(fin); fclose(fout); return -1;
    }
    uint32_t channels = be32(hdr + 12);
    if (channels != 1) {
        fprintf(stderr, "mime_decode: %u channels not supported (mono only)\n", channels);
        fclose(fin); fclose(fout); return -1;
    }

    void* ctx = EVS_DEC_Open(fs_hz);
    if (!ctx) { fclose(fin); fclose(fout); return -1; }

    /* 20 ms frames; max 48 kHz mono -> 960 samples. */
    int16_t pcm[1920];
    uint8_t frame[512];

    for (;;) {
        int toc = fgetc(fin);
        if (toc == EOF) break;

        int mode = (toc >> 5) & 1;
        int idx  = toc & 0x0F;
        int nb   = (mode == 0) ? PRIMARY_BYTES[idx] : AMRWBIO_BYTES[idx];

        int bfi = (idx == 13 || idx == 14 || idx == 15);   /* reserved | lost | no_data */

        if (nb > 0 && fread(frame, 1, nb, fin) != (size_t)nb) break;

        int nsamp = 0;
        EVS_DEC_Process(ctx, nb > 0 ? frame : NULL, nb, pcm, &nsamp, bfi);

        if (nsamp > 0) fwrite(pcm, 2, (size_t)nsamp, fout);   /* s16le */
    }

    EVS_DEC_Close(ctx);
    fclose(fin);
    fclose(fout);
    return 0;
}

/* ---- Programmatic entry point (preferred over callMain from JS) ----------- */
EXPORT int evs_mime_decode(const char* in_path, const char* out_path, int fs_hz)
{
    return mime_decode(in_path, out_path, fs_hz);
}

/* ---- main() -------------------------------------------------------------- */
EXPORT int main(int argc, char** argv)
{
    if (argc >= 5 && strcmp(argv[1], "-mime") == 0) {
        int fs_khz = atoi(argv[2]);
        int rc = mime_decode(argv[3], argv[4], fs_khz * 1000);
#ifdef __EMSCRIPTEN__
        /* Keep the runtime alive so callMain/ccall can be invoked again. */
        emscripten_exit_with_live_runtime();
#endif
        return rc;
    }
    /* Anything else: hand off to the untouched reference CLI. */
    return evs_ref_main(argc, argv);
}