#ifndef __META_BLUR_FACTORY_PRIVATE_H__
#define __META_BLUR_FACTORY_PRIVATE_H__

#include <cairo.h>
#include <clutter/clutter.h>
#include "meta-window-shape.h"
#include <meta/meta-shadow-factory.h>

typedef struct _MetaBlur MetaBlur;
typedef struct _MetaBlurClass MetaBlurClass;

struct _MetaBlurClass {
  
};

#define META_TYPE_BLUR                  (meta_blur_get_type ())
#define META_BLUR(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_BLUR, MetaBlur))
#define META_IS_BLUR(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_TYPE_BLUR))
#define META_BLUR_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), META_TYPE_BLUR, MetaBlurClass))
#define META_IS_BLUR_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), META_TYPE_BLUR))
#define META_BLUR_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), META_TYPE_BLUR, MetaBlurClass))

GType meta_blur_get_type(void);
MetaBlur * meta_blur_new(void);

void        meta_blur_paint       (MetaBlur           *shadow,
                                     int                    window_x,
                                     int                    window_y,
                                     int                    window_width,
                                     int                    window_height,
                                     guint8                 opacity,
                                     cairo_region_t        *clip,
                                     gboolean               clip_strictly);
#endif