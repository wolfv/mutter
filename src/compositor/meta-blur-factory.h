#ifndef __META_BLUR_FACTORY_PRIVATE_H__
#define __META_BLUR_FACTORY_PRIVATE_H__

#include <cairo.h>
#include <clutter/clutter.h>
#include "meta-window-shape.h"
#include <meta/meta-shadow-factory.h>

typedef struct _MetaBlur MetaBlur;

void make_blur(MetaBlur * blur);

void        meta_blur_paint       (MetaBlur           *shadow,
                                     int                    window_x,
                                     int                    window_y,
                                     int                    window_width,
                                     int                    window_height,
                                     guint8                 opacity,
                                     cairo_region_t        *clip,
                                     gboolean               clip_strictly);
#endif /* META_BACKGROUND_H */
