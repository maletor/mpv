/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <assert.h>

#include <libavutil/channel_layout.h>

#include "chmap.h"

// Names taken from libavutil/channel_layout.c (Not accessible by API.)
static const char *speaker_names[MP_SPEAKER_ID_COUNT] = {
    [MP_SPEAKER_ID_FRONT_LEFT]              = "fl",
    [MP_SPEAKER_ID_FRONT_RIGHT]             = "fr",
    [MP_SPEAKER_ID_FRONT_CENTER]            = "fc",
    [MP_SPEAKER_ID_LOW_FREQUENCY]           = "lfe",
    [MP_SPEAKER_ID_BACK_LEFT]               = "bl",
    [MP_SPEAKER_ID_BACK_RIGHT]              = "br",
    [MP_SPEAKER_ID_FRONT_LEFT_OF_CENTER]    = "flc",
    [MP_SPEAKER_ID_FRONT_RIGHT_OF_CENTER]   = "frc",
    [MP_SPEAKER_ID_BACK_CENTER]             = "bc",
    [MP_SPEAKER_ID_SIDE_LEFT]               = "sl",
    [MP_SPEAKER_ID_SIDE_RIGHT]              = "sr",
    [MP_SPEAKER_ID_TOP_CENTER]              = "tc",
    [MP_SPEAKER_ID_TOP_FRONT_LEFT]          = "tfl",
    [MP_SPEAKER_ID_TOP_FRONT_CENTER]        = "tfc",
    [MP_SPEAKER_ID_TOP_FRONT_RIGHT]         = "tfr",
    [MP_SPEAKER_ID_TOP_BACK_LEFT]           = "tbl",
    [MP_SPEAKER_ID_TOP_BACK_CENTER]         = "tbc",
    [MP_SPEAKER_ID_TOP_BACK_RIGHT]          = "tbr",
    [MP_SPEAKER_ID_STEREO_LEFT]             = "dl",
    [MP_SPEAKER_ID_STEREO_RIGHT]            = "dr",
    [MP_SPEAKER_ID_WIDE_LEFT]               = "wl",
    [MP_SPEAKER_ID_WIDE_RIGHT]              = "wr",
    [MP_SPEAKER_ID_SURROUND_DIRECT_LEFT]    = "sdl",
    [MP_SPEAKER_ID_SURROUND_DIRECT_RIGHT]   = "sdr",
    [MP_SPEAKER_ID_LOW_FREQUENCY_2]         = "lfe2",
};

// Names taken from libavutil/channel_layout.c (Not accessible by API.)
// Channel order corresponds to lavc/waveex, except for the alsa entries.
static const char *std_layout_names[][2] = {
    {"empty",           ""}, // not in lavc
    {"mono",            "fc"},
    {"stereo",          "fl-fr"},
    {"2.1",             "fl-fr-lfe"},
    {"3.0",             "fl-fr-fc"},
    {"3.0(back)",       "fl-fr-bc"},
    {"4.0",             "fl-fr-fc-bc"},
    {"quad",            "fl-fr-bl-br"},
    {"quad(side)",      "fl-fr-sl-sr"},
    {"3.1",             "fl-fr-fc-lfe"},
    {"5.0",             "fl-fr-fc-bl-br"},
    {"5.0(alsa)",       "fl-fr-bl-br-fc"}, // not in lavc
    {"5.0(side)",       "fl-fr-fc-sl-sr"},
    {"4.1",             "fl-fr-fc-lfe-bc"},
    {"5.1",             "fl-fr-fc-lfe-bl-br"},
    {"5.1(alsa)",       "fl-fr-bl-br-fc-lfe"}, // not in lavc
    {"5.1(side)",       "fl-fr-fc-lfe-sl-sr"},
    {"6.0",             "fl-fr-fc-bc-sl-sr"},
    {"6.0(front)",      "fl-fr-flc-frc-sl-sr"},
    {"hexagonal",       "fl-fr-fc-bl-br-bc"},
    {"6.1",             "fl-fr-fc-lfe-bc-sl-sr"},
    {"6.1",             "fl-fr-fc-lfe-bl-br-bc"},
    {"6.1(front)",      "fl-fr-lfe-flc-frc-sl-sr"},
    {"7.0",             "fl-fr-fc-bl-br-sl-sr"},
    {"7.0(front)",      "fl-fr-fc-flc-frc-sl-sr"},
    {"7.1",             "fl-fr-fc-lfe-bl-br-sl-sr"},
    {"7.1(alsa)",       "fl-fr-bl-br-fc-lfe-sl-sr"}, // not in lavc
    {"7.1(wide)",       "fl-fr-fc-lfe-bl-br-flc-frc"},
    {"7.1(wide-side)",  "fl-fr-fc-lfe-flc-frc-sl-sr"},
    {"octagonal",       "fl-fr-fc-bl-br-bc-sl-sr"},
    {"downmix",         "dl-dr"},
    {0}
};

// Returns true if speakers are mapped uniquely, and there's at least 1 channel.
bool mp_chmap_is_valid(const struct mp_chmap *src)
{
    if (mp_chmap_is_empty(src))
        return false;
    bool speaker_mapped[MP_SPEAKER_ID_COUNT] = {0};
    for (int n = 0; n < src->num; n++) {
        int speaker = src->speaker[n];
        if (speaker >= MP_SPEAKER_ID_COUNT || speaker_mapped[speaker])
            return false;
        speaker_mapped[speaker] = true;
    }
    return true;
}

bool mp_chmap_is_empty(const struct mp_chmap *src)
{
    return src->num == 0;
}

// Note: empty channel maps compare as equal. Invalid ones can equal too.
bool mp_chmap_equals(const struct mp_chmap *a, const struct mp_chmap *b)
{
    if (a->num != b->num)
        return false;
    for (int n = 0; n < a->num; n++) {
        if (a->speaker[n] != b->speaker[n])
            return false;
    }
    return true;
}

bool mp_chmap_is_stereo(const struct mp_chmap *src)
{
    static const struct mp_chmap stereo = MP_CHMAP_INIT_STEREO;
    return mp_chmap_equals(src, &stereo);
}

// Set *dst to a standard layout with the given number of channels.
// If the number of channels is invalid, an invalid map is set, and
// mp_chmap_is_valid(dst) will return false.
void mp_chmap_from_channels(struct mp_chmap *dst, int num_channels)
{
    mp_chmap_from_lavc(dst, av_get_default_channel_layout(num_channels));
}

// Return the ffmpeg/libav channel layout as in <libavutil/channel_layout.h>.
// Warning: this ignores the order of the channels, and will return a channel
//          mask even if the order is different from libavcodec's.
uint64_t mp_chmap_to_lavc_unchecked(const struct mp_chmap *src)
{
    uint64_t mask = 0;
    for (int n = 0; n < src->num; n++)
        mask |= 1ULL << src->speaker[n];
    return mask;
}

// Return the ffmpeg/libav channel layout as in <libavutil/channel_layout.h>.
// Returns 0 if the channel order doesn't match lavc's or if it's invalid.
uint64_t mp_chmap_to_lavc(const struct mp_chmap *src)
{
    if (!mp_chmap_is_lavc(src))
        return 0;
    return mp_chmap_to_lavc_unchecked(src);
}

// Set channel map from the ffmpeg/libav channel layout as in
// <libavutil/channel_layout.h>.
// If the number of channels exceed MP_NUM_CHANNELS, set dst to empty.
void mp_chmap_from_lavc(struct mp_chmap *dst, uint64_t src)
{
    dst->num = 0;
    for (int n = 0; n < MP_SPEAKER_ID_COUNT; n++) {
        if (src & (1ULL << n)) {
            if (dst->num >= MP_NUM_CHANNELS) {
                dst->num = 0;
                return;
            }
            dst->speaker[dst->num] = n;
            dst->num++;
        }
    }
}

bool mp_chmap_is_lavc(const struct mp_chmap *src)
{
    if (!mp_chmap_is_valid(src))
        return false;
    // lavc's channel layout is a bit mask, and channels are always ordered
    // from LSB to MSB speaker bits, so speaker IDs have to increase.
    // We statically guarantee that there are less than 64 speaker IDs.
    assert(MP_SPEAKER_ID_COUNT <= 64);
    assert(src->num > 0);
    for (int n = 1; n < src->num; n++) {
        if (src->speaker[n - 1] >= src->speaker[n])
            return false;
    }
    return true;
}

void mp_chmap_reorder_to_lavc(struct mp_chmap *map)
{
    if (!mp_chmap_is_valid(map))
        return;
    uint64_t mask = mp_chmap_to_lavc_unchecked(map);
    mp_chmap_from_lavc(map, mask);
}

// For every channel n, map it to reorder[n].
static void mp_chmap_reorder(struct mp_chmap *map, int *reorder)
{
    struct mp_chmap new_map = *map;
    for (int n = 0; n < map->num; n++)
        new_map.speaker[n] = map->speaker[reorder[n]];
    *map = new_map;
}

void mp_chmap_reorder_to_alsa(struct mp_chmap *map)
{
    mp_chmap_reorder_to_lavc(map);
    // The channel order differs from lavc for 5, 6 and 8.
    // I don't know about channels > 8.
    if (map->num == 5) {
        // Note: ao_alsa.c claims it does not support 5 channels, even though
        //       reorder_ch.h defined a channel layout for it. It's not sure
        //       if this remapping works, or if it makes any sense.
        // Assuming 5.0
        //       0   1   2   3   4
        // lavc: L   R   C   Ls  Rs
        // alsa: L   R   Ls  Rs  C
        mp_chmap_reorder(map, (int[]) {0, 1, 3, 4, 2});
    } else if (map->num == 6) {
        // Assuming 5.1
        //       0   1   2   3   4   5
        // lavc: L   R   C   LFE Ls  Rs
        // alsa: L   R   Ls  Rs  C   LFE
        mp_chmap_reorder(map, (int[]) {0, 1, 4, 5, 2, 3});
    } else if (map->num == 8) {
        // Assuming 7.1
        //       0   1   2   3   4   5   6   7
        // lavc: L   R   C   LFE Ls  Rs  Rls Rrs
        // alsa: L   R   Ls  Rs  C   LFE Rls Rrs
        mp_chmap_reorder(map, (int[]) {0, 1, 4, 5, 2, 3, 6, 7});
    }
}

// Get reordering array for from->to reordering. from->to must have the same set
// of speakers (i.e. same number and speaker IDs, just different order). Then,
// for each speaker n, dst[n] will be set such that:
//      to->speaker[dst[n]] = from->speaker[n]
// (dst[n] gives the source channel for destination channel n)
void mp_chmap_get_reorder(int dst[MP_NUM_CHANNELS], const struct mp_chmap *from,
                          const struct mp_chmap *to)
{
    // Same set of speakers required
    assert(mp_chmap_to_lavc_unchecked(from) == mp_chmap_to_lavc_unchecked(to));
    for (int n = 0; n < from->num; n++) {
        int src = from->speaker[n];
        dst[n] = -1;
        for (int i = 0; i < to->num; i++) {
            if (src == to->speaker[i]) {
                dst[n] = i;
                break;
            }
        }
        assert(dst[n] != -1);
    }
    for (int n = 0; n < from->num; n++)
        assert(to->speaker[dst[n]] == from->speaker[n]);
}

// Returns something like "fl-fr-fc". If there's a standard layout in lavc
// order, return that, e.g. "3.0" instead of "fl-fr-fc".
// Unassigned but valid speakers get names like "sp28".
char *mp_chmap_to_str(const struct mp_chmap *src)
{
    char *res = talloc_strdup(NULL, "");

    for (int n = 0; n < src->num; n++) {
        int sp = src->speaker[n];
        const char *s = sp < MP_SPEAKER_ID_COUNT ? speaker_names[sp] : NULL;
        char buf[10];
        if (!s) {
            snprintf(buf, sizeof(buf), "sp%d", sp);
            s = buf;
        }
        res = talloc_asprintf_append_buffer(res, "%s%s", n > 0 ? "-" : "", s);
    }

    // To standard layout name
    for (int n = 0; std_layout_names[n][0]; n++) {
        if (res && strcmp(res, std_layout_names[n][1]) == 0) {
            talloc_free(res);
            res = talloc_strdup(NULL, std_layout_names[n][0]);
            break;
        }
    }

    return res;
}

// If src can be parsed as channel map (as produced by mp_chmap_to_str()),
// return true and set *dst. Otherwise, return false and don't change *dst.
// Note: call mp_chmap_is_valid() to test whether the returned map is valid
//       the map could be empty, or contain multiply mapped channels
bool mp_chmap_from_str(struct mp_chmap *dst, bstr src)
{
    // Single number corresponds to mp_chmap_from_channels()
    if (src.len > 0) {
        bstr rest;
        long long count = bstrtoll(src, &rest, 10);
        if (rest.len == 0) {
            struct mp_chmap res;
            mp_chmap_from_channels(&res, count);
            if (mp_chmap_is_valid(&res)) {
                *dst = res;
                return true;
            }
        }
    }

    // From standard layout name
    for (int n = 0; std_layout_names[n][0]; n++) {
        if (bstrcasecmp0(src, std_layout_names[n][0]) == 0) {
            src = bstr0(std_layout_names[n][1]);
            break;
        }
    }

    // Explicit speaker list (separated by "-")
    struct mp_chmap res = {0};
    while (src.len) {
        bstr s;
        bstr_split_tok(src, "-", &s, &src);
        int speaker = -1;
        for (int n = 0; n < MP_SPEAKER_ID_COUNT; n++) {
            const char *name = speaker_names[n];
            if (name && bstrcasecmp0(s, name) == 0) {
                speaker = n;
                break;
            }
        }
        if (speaker < 0) {
            if (s.len > 2 && bstrcasecmp0(bstr_splice(s, 0, 2), "sp") == 0) {
                s = bstr_cut(s, 2);
                long long sp = bstrtoll(s, &s, 0);
                if (s.len == 0 && sp >= 0 && sp < MP_SPEAKER_ID_COUNT)
                    speaker = sp;
            }
            if (speaker < 0)
                return false;
        }
        if (res.num >= MP_NUM_CHANNELS)
            return false;
        res.speaker[res.num] = speaker;
        res.num++;
    }

    *dst = res;
    return true;
}
