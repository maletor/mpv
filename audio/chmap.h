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

#ifndef MP_CHMAP_H
#define MP_CHMAP_H

#include <inttypes.h>
#include <stdbool.h>
#include "core/bstr.h"

#define MP_NUM_CHANNELS 8

// Speaker a channel can be assigned to.
// This corresponds to WAVEFORMATEXTENSIBLE channel mask bit indexes.
// E.g. channel_mask = (1 << MP_SPEAKER_ID_FRONT_LEFT) | ...
enum {
    // Official WAVEFORMATEXTENSIBLE
    MP_SPEAKER_ID_FRONT_LEFT = 0,
    MP_SPEAKER_ID_FRONT_RIGHT,
    MP_SPEAKER_ID_FRONT_CENTER,
    MP_SPEAKER_ID_LOW_FREQUENCY,
    MP_SPEAKER_ID_BACK_LEFT,
    MP_SPEAKER_ID_BACK_RIGHT,
    MP_SPEAKER_ID_FRONT_LEFT_OF_CENTER,
    MP_SPEAKER_ID_FRONT_RIGHT_OF_CENTER,
    MP_SPEAKER_ID_BACK_CENTER,
    MP_SPEAKER_ID_SIDE_LEFT,
    MP_SPEAKER_ID_SIDE_RIGHT,
    MP_SPEAKER_ID_TOP_CENTER,
    MP_SPEAKER_ID_TOP_FRONT_LEFT,
    MP_SPEAKER_ID_TOP_FRONT_CENTER,
    MP_SPEAKER_ID_TOP_FRONT_RIGHT,
    MP_SPEAKER_ID_TOP_BACK_LEFT,
    MP_SPEAKER_ID_TOP_BACK_CENTER,
    MP_SPEAKER_ID_TOP_BACK_RIGHT,
    // Inofficial/libav* extensions
    MP_SPEAKER_ID_STEREO_LEFT = 29, // stereo downmix special speakers
    MP_SPEAKER_ID_STEREO_RIGHT,
    MP_SPEAKER_ID_WIDE_LEFT,
    MP_SPEAKER_ID_WIDE_RIGHT,
    MP_SPEAKER_ID_SURROUND_DIRECT_LEFT,
    MP_SPEAKER_ID_SURROUND_DIRECT_RIGHT,
    MP_SPEAKER_ID_LOW_FREQUENCY_2,

    // Including the unassigned IDs in between. This is not a valid ID anymore.
    MP_SPEAKER_ID_COUNT = 64
};

struct mp_chmap {
    uint8_t num; // number of channels
    // Given a channel n, speaker[n] is the speaker ID driven by that channel.
    // Entries after speaker[num - 1] are undefined.
    uint8_t speaker[MP_NUM_CHANNELS];
};

#define MP_CHMAP_INIT_STEREO {2, {MP_SPEAKER_ID_FRONT_LEFT,     \
                                  MP_SPEAKER_ID_FRONT_RIGHT}}

bool mp_chmap_is_valid(const struct mp_chmap *src);
bool mp_chmap_is_empty(const struct mp_chmap *src);
bool mp_chmap_equals(const struct mp_chmap *a, const struct mp_chmap *b);

bool mp_chmap_is_stereo(const struct mp_chmap *src);

void mp_chmap_from_channels(struct mp_chmap *dst, int num_channels);

uint64_t mp_chmap_to_lavc(const struct mp_chmap *src);
uint64_t mp_chmap_to_lavc_unchecked(const struct mp_chmap *src);
void mp_chmap_from_lavc(struct mp_chmap *dst, uint64_t src);

bool mp_chmap_is_lavc(const struct mp_chmap *src);
void mp_chmap_reorder_to_lavc(struct mp_chmap *map);

void mp_chmap_reorder_to_alsa(struct mp_chmap *map);

void mp_chmap_get_reorder(int dst[MP_NUM_CHANNELS], const struct mp_chmap *from,
                          const struct mp_chmap *to);

char *mp_chmap_to_str(const struct mp_chmap *src);
bool mp_chmap_from_str(struct mp_chmap *dst, bstr src);

// Use these to avoid chaos in case lavc's definition should diverge from MS.
#define mp_chmap_to_waveext mp_chmap_to_lavc
#define mp_chmap_from_waveext mp_chmap_from_lavc
#define mp_chmap_is_waveext mp_chmap_is_lavc
#define mp_chmap_reorder_to_waveext mp_chmap_reorder_to_lavc

#endif
