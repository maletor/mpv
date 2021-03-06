/*
 * Copyright (c) 2004 Michael Niedermayer <michaelni@gmx.at>
 * Copyright (c) 2013 Stefano Pigozzi <stefano.pigozzi@gmail.com>
 *
 * This file is part of mpv.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <libavutil/opt.h>
#include <libavutil/audioconvert.h>
#include <libavutil/common.h>
#include <libavutil/samplefmt.h>
#include <libavutil/mathematics.h>

#include "talloc.h"
#include "config.h"

#if defined(CONFIG_LIBAVRESAMPLE)
#include <libavresample/avresample.h>
#elif defined(CONFIG_LIBSWRESAMPLE)
#include <libswresample/swresample.h>
#define AVAudioResampleContext SwrContext
#define avresample_alloc_context swr_alloc
#define avresample_open swr_init
#define avresample_close(x) do { } while(0)
#define avresample_available(x) 0
#define avresample_convert(ctx, out, out_planesize, out_samples, in, in_planesize, in_samples) \
    swr_convert(ctx, out, out_samples, (const uint8_t**)(in), in_samples)
#else
#error "config.h broken"
#endif

#include "core/mp_msg.h"
#include "core/subopt-helper.h"
#include "audio/filter/af.h"

struct af_resample_opts {
    int filter_size;
    int phase_shift;
    int linear;
    double cutoff;

    int out_rate;
    int in_rate;
};

struct af_resample {
    struct AVAudioResampleContext *avrctx;
    struct af_resample_opts ctx;   // opts in the context
    struct af_resample_opts opts;  // opts requested by the user
};

#ifdef CONFIG_LIBAVRESAMPLE
static int get_delay(struct af_resample *s)
{
    return avresample_get_delay(s->avrctx);
}
#else
static int get_delay(struct af_resample *s)
{
    return swr_get_delay(s->avrctx, s->ctx.in_rate);
}
#endif

static double af_resample_default_cutoff(int filter_size)
{
    return FFMAX(1.0 - 6.5 / (filter_size + 8), 0.80);
}

static bool needs_lavrctx_reconfigure(struct af_resample *s,
                                      struct mp_audio *in,
                                      struct mp_audio *out)
{
    return s->ctx.out_rate    != out->rate ||
           s->ctx.in_rate     != in->rate ||
           s->ctx.filter_size != s->opts.filter_size ||
           s->ctx.phase_shift != s->opts.phase_shift ||
           s->ctx.linear      != s->opts.linear ||
           s->ctx.cutoff      != s->opts.cutoff;

}

#define ctx_opt_set_int(a,b) av_opt_set_int(s->avrctx, (a), (b), 0)
#define ctx_opt_set_dbl(a,b) av_opt_set_double(s->avrctx, (a), (b), 0)

static int control(struct af_instance *af, int cmd, void *arg)
{
    struct af_resample *s = (struct af_resample *) af->setup;
    struct mp_audio *in   = (struct mp_audio *) arg;
    struct mp_audio *out  = (struct mp_audio *) af->data;

    switch (cmd) {
    case AF_CONTROL_REINIT: {
        if ((out->rate == in->rate) || (out->rate == 0))
            return AF_DETACH;

        out->nch    = FFMIN(in->nch, AF_NCH);
        out->format = AF_FORMAT_S16_NE;
        out->bps    = 2;
        af->mul     = (double) out->rate / in->rate;
        af->delay   = out->nch * s->opts.filter_size / FFMIN(af->mul, 1);

        if (needs_lavrctx_reconfigure(s, in, out)) {
            if (s->avrctx)
                avresample_close(s->avrctx);

            s->ctx.out_rate    = out->rate;
            s->ctx.in_rate     = in->rate;
            s->ctx.filter_size = s->opts.filter_size;
            s->ctx.phase_shift = s->opts.phase_shift;
            s->ctx.linear      = s->opts.linear;
            s->ctx.cutoff      = s->opts.cutoff;

            int ch_layout = av_get_default_channel_layout(out->nch);

            ctx_opt_set_int("in_channel_layout",  ch_layout);
            ctx_opt_set_int("out_channel_layout", ch_layout);

            ctx_opt_set_int("in_sample_rate",     s->ctx.in_rate);
            ctx_opt_set_int("out_sample_rate",    s->ctx.out_rate);

            ctx_opt_set_int("in_sample_fmt",      AV_SAMPLE_FMT_S16);
            ctx_opt_set_int("out_sample_fmt",     AV_SAMPLE_FMT_S16);

            ctx_opt_set_int("filter_size",        s->ctx.filter_size);
            ctx_opt_set_int("phase_shift",        s->ctx.phase_shift);
            ctx_opt_set_int("linear_interp",      s->ctx.linear);

            ctx_opt_set_dbl("cutoff",             s->ctx.cutoff);

            if (avresample_open(s->avrctx) < 0) {
                mp_msg(MSGT_AFILTER, MSGL_ERR, "[lavrresample] Cannot open "
                       "Libavresample Context. \n");
                return AF_ERROR;
            }
        }

        int out_rate, test_output_res;
        // hack to make af_test_output ignore the samplerate change
        out_rate  = out->rate;
        out->rate = in->rate;
        test_output_res = af_test_output(af, in);
        out->rate = out_rate;
        return test_output_res;
    }
    case AF_CONTROL_COMMAND_LINE: {
        s->opts.cutoff = 0.0;

        const opt_t subopts[] = {
            {"srate",        OPT_ARG_INT,    &out->rate, NULL},
            {"filter_size",  OPT_ARG_INT,    &s->opts.filter_size, NULL},
            {"phase_shift",  OPT_ARG_INT,    &s->opts.phase_shift, NULL},
            {"linear",       OPT_ARG_BOOL,   &s->opts.linear, NULL},
            {"cutoff",       OPT_ARG_FLOAT,  &s->opts.cutoff, NULL},
            {0}
        };

        if (subopt_parse(arg, subopts) != 0) {
            mp_msg(MSGT_AFILTER, MSGL_ERR, "[lavrresample] Invalid option "
                   "specified.\n");
            return AF_ERROR;
        }

        if (s->opts.cutoff <= 0.0)
            s->opts.cutoff = af_resample_default_cutoff(s->opts.filter_size);
        return AF_OK;
    }
    case AF_CONTROL_RESAMPLE_RATE | AF_CONTROL_SET:
        out->rate = *(int *)arg;
        return AF_OK;
    }
    return AF_UNKNOWN;
}

#undef ctx_opt_set_int
#undef ctx_opt_set_dbl

static void uninit(struct af_instance *af)
{
    if (af->setup) {
        struct af_resample *s = af->setup;
        if (s->avrctx)
            avresample_close(s->avrctx);
        talloc_free(af->setup);
    }
}

static struct mp_audio *play(struct af_instance *af, struct mp_audio *data)
{
    struct af_resample *s = af->setup;
    struct mp_audio *in   = data;
    struct mp_audio *out  = af->data;


    int in_size     = data->len;
    int in_samples  = in_size / (data->bps * data->nch);
    int out_samples = avresample_available(s->avrctx) +
        av_rescale_rnd(get_delay(s) + in_samples,
                       s->ctx.out_rate, s->ctx.in_rate, AV_ROUND_UP);
    int out_size    = out->bps * out_samples * out->nch;

    if (talloc_get_size(out->audio) < out_size)
        out->audio = talloc_realloc_size(out, out->audio, out_size);

    af->delay = out->bps * av_rescale_rnd(get_delay(s),
                                          s->ctx.out_rate, s->ctx.in_rate,
                                          AV_ROUND_UP);

    out_samples = avresample_convert(s->avrctx,
            (uint8_t **) &out->audio, out_size, out_samples,
            (uint8_t **) &in->audio,  in_size,  in_samples);

    out_size  = out->bps * out_samples * out->nch;
    in->audio = out->audio;
    in->len   = out_size;
    in->rate  = s->ctx.out_rate;
    return data;
}

static int af_open(struct af_instance *af)
{
    struct af_resample *s = talloc_zero(NULL, struct af_resample);

    af->control = control;
    af->uninit  = uninit;
    af->play    = play;
    af->mul     = 1;
    af->data    = talloc_zero(s, struct mp_audio);

    af->data->rate   = 44100;

    int default_filter_size = 16;
    s->opts = (struct af_resample_opts) {
        .linear      = 0,
        .filter_size = default_filter_size,
        .cutoff      = af_resample_default_cutoff(default_filter_size),
        .phase_shift = 10,
    };

    s->avrctx = avresample_alloc_context();
    af->setup = s;

    if (s->avrctx) {
        return AF_OK;
    } else {
        mp_msg(MSGT_AFILTER, MSGL_ERR, "[lavrresample] Cannot initialize "
               "Libavresample Context. \n");
        uninit(af);
        return AF_ERROR;
    }
}

struct af_info af_info_lavrresample = {
    "Sample frequency conversion using libavresample",
    "lavrresample",
    "Stefano Pigozzi (based on Michael Niedermayer's lavcresample)",
    "",
    AF_FLAGS_REENTRANT,
    af_open
};
