/*
 * This file is part of MPlayer.
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

#ifndef MPLAYER_MPLAYER_H
#define MPLAYER_MPLAYER_H

#include <stdlib.h>

#include "core/mp_msg.h"

struct MPContext;
struct MPOpts;
struct subtitle;

void set_osd_subtitle(struct MPContext *mpctx, struct subtitle *subs);

struct mp_resolve_result {
    char *url;
    char *title;
};

struct mp_resolve_result *mp_resolve_quvi(const char *url, struct MPOpts *opts);

#endif /* MPLAYER_MPLAYER_H */
