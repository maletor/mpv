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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "core/mp_msg.h"
#include "core/options.h"

#include "video/img_format.h"
#include "video/mp_image.h"
#include "vf.h"

#include "core/m_option.h"
#include "core/m_struct.h"

static const struct vf_priv_s {
    int crop_w,crop_h;
    int crop_x,crop_y;
} vf_priv_dflt = {
  -1,-1,
  -1,-1
};

//===========================================================================//

static int config(struct vf_instance *vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt)
{
    // calculate the missing parameters:
    if(vf->priv->crop_w<=0 || vf->priv->crop_w>width) vf->priv->crop_w=width;
    if(vf->priv->crop_h<=0 || vf->priv->crop_h>height) vf->priv->crop_h=height;
    if(vf->priv->crop_x<0) vf->priv->crop_x=(width-vf->priv->crop_w)/2;
    if(vf->priv->crop_y<0) vf->priv->crop_y=(height-vf->priv->crop_h)/2;
    // rounding:

    struct mp_imgfmt_desc fmt = mp_imgfmt_get_desc(outfmt);

    vf->priv->crop_x = MP_ALIGN_DOWN(vf->priv->crop_x, fmt.align_x);
    vf->priv->crop_y = MP_ALIGN_DOWN(vf->priv->crop_y, fmt.align_y);

    // check:
    if(vf->priv->crop_w+vf->priv->crop_x>width ||
       vf->priv->crop_h+vf->priv->crop_y>height){
	mp_tmsg(MSGT_VFILTER, MSGL_WARN, "[CROP] Bad position/width/height - cropped area outside of the original!\n");
	return 0;
    }
    vf_rescale_dsize(&d_width, &d_height, width, height,
                     vf->priv->crop_w, vf->priv->crop_h);
    return vf_next_config(vf,vf->priv->crop_w,vf->priv->crop_h,d_width,d_height,flags,outfmt);
}

static struct mp_image *filter(struct vf_instance *vf, struct mp_image *mpi)
{
    mp_image_crop(mpi, vf->priv->crop_x, vf->priv->crop_y,
                  vf->priv->crop_x + vf->priv->crop_w,
                  vf->priv->crop_y + vf->priv->crop_h);
    return mpi;
}

static int query_format(struct vf_instance *vf, unsigned int fmt)
{
    if (!IMGFMT_IS_HWACCEL(fmt))
        return vf_next_query_format(vf, fmt);
    return 0;
}

static int vf_open(vf_instance_t *vf, char *args){
    vf->config=config;
    vf->filter=filter;
    vf->query_format=query_format;
    mp_msg(MSGT_VFILTER, MSGL_INFO, "Crop: %d x %d, %d ; %d\n",
    vf->priv->crop_w,
    vf->priv->crop_h,
    vf->priv->crop_x,
    vf->priv->crop_y);
    return 1;
}

#define ST_OFF(f) M_ST_OFF(struct vf_priv_s,f)
static const m_option_t vf_opts_fields[] = {
  {"w", ST_OFF(crop_w), CONF_TYPE_INT, M_OPT_MIN,0 ,0, NULL},
  {"h", ST_OFF(crop_h), CONF_TYPE_INT, M_OPT_MIN,0 ,0, NULL},
  {"x", ST_OFF(crop_x), CONF_TYPE_INT, M_OPT_MIN,-1 ,0, NULL},
  {"y", ST_OFF(crop_y), CONF_TYPE_INT, M_OPT_MIN,-1 ,0, NULL},
  { NULL, NULL, 0, 0, 0, 0,  NULL }
};

static const m_struct_t vf_opts = {
  "crop",
  sizeof(struct vf_priv_s),
  &vf_priv_dflt,
  vf_opts_fields
};

const vf_info_t vf_info_crop = {
    "cropping",
    "crop",
    "A'rpi",
    "",
    vf_open,
    &vf_opts
};

//===========================================================================//
