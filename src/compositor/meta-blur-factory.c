/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright 2010 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:meta-shadow-factory
 * @title: MetaBlurFactory
 * @short_description: Create and cache shadow textures for abritrary window shapes
 */

#include <config.h>
#include <math.h>
#include <string.h>

#include "cogl-utils.h"
#include "meta-shadow-factory-private.h"
#include "region-utils.h"

typedef struct _MetaBlurCacheKey  MetaBlurCacheKey;
typedef struct _MetaBlurClassInfo MetaBlurClassInfo;

#define BLUR_PADDING    2
static const gchar *box_blur_glsl_declarations =
"uniform vec2 pixel_step;\n";
#define SAMPLE(offx, offy) \
  "cogl_texel += texture2D (cogl_sampler, cogl_tex_coord.st + pixel_step * " \
  "vec2 (" G_STRINGIFY (offx) ", " G_STRINGIFY (offy) "));\n"
static const gchar *box_blur_glsl_shader =
"  cogl_texel = texture2D (cogl_sampler, cogl_tex_coord.st);\n"
  SAMPLE (-1.0, -1.0)
  SAMPLE ( 0.0, -1.0)
  SAMPLE (+1.0, -1.0)
  SAMPLE (-1.0,  0.0)
  SAMPLE (+1.0,  0.0)
  SAMPLE (-1.0, +1.0)
  SAMPLE ( 0.0, +1.0)
  SAMPLE (+1.0, +1.0)
"  cogl_texel /= 9.0;\n";
#undef SAMPLE

static CoglPipeline * base_pipeline;
//"

struct _MetaBlur
{
  int ref_count;

  MetaBlurFactory *factory;
  MetaBlurCacheKey key;
  CoglTexture *texture;
  CoglPipeline *pipeline;

  /* The outer order is the distance the shadow extends outside the window
   * shape; the inner border is the unscaled portion inside the window
   * shape */

  guint scale_width : 1;
  guint scale_height : 1;
  gint blur_pixel_step_uniform;

};

struct _MetaBlurClassInfo
{
  const char *name; /* const so we can reuse for static definitions */
  MetaBlurParams focused;
  MetaBlurParams unfocused;
};

struct _MetaBlurFactory
{
  GObject parent_instance;

  /* MetaBlurCacheKey => MetaBlur; the shadows are not referenced
   * by the factory, they are simply removed from the table when freed */
  GHashTable *shadows;

  /* class name => MetaBlurClassInfo */
  GHashTable *shadow_classes;
};

struct _MetaBlurFactoryClass
{
  GObjectClass parent_class;
};

G_DEFINE_TYPE (MetaBlurFactory, meta_blur_factory, G_TYPE_OBJECT);

void
meta_blur_paint (MetaBlur          *self,
                   int             window_x,
                   int             window_y,
                   int             window_width,
                   int             window_height,
                   guint8          opacity,
                   cairo_region_t *clip,
                   gboolean        clip_strictly)
{
  float texture_width = cogl_texture_get_width (self->texture);
  float texture_height = cogl_texture_get_height (self->texture);
  int i, j;
  float src_x[4];
  float src_y[4];
  int dest_x[4];
  int dest_y[4];
  int n_x, n_y;

  if(self->pipeline == NULL) {
    make_blur(self);
  }


  cogl_read_pixels(
    window_x, 
    window_y, 
    window_width, 
    window_height, 
    COGL_READ_PIXELS_COLOR_BUFFER,
    COGL_PIXEL_FORMAT_RGBA_8888, 
    (guchar *) pixels);

  self->texture = cogl_texture_new_from_data(
    width, 
    height,
    COGL_TEXTURE_NONE,
    COGL_PIXEL_FORMAT_RGBA_8888,
    COGL_PIXEL_FORMAT_RGBA_8888,
    0,
    pixels
  );



  // self->texture = COGL_TEXTURE (cogl_texture_2d_new_from_data (ctx,
  //                                                              extents.width,
  //                                                              extents.height,
  //                                                              COGL_PIXEL_FORMAT_RGBA_8888,
  //                                                              buffer_width,
  //                                                              buffer_height,
  //                                                              NULL));


  gfloat pixel_step[2];

  pixel_step[0] = 1.0f / texture_width;
  pixel_step[1] = 1.0f / texture_width;

  cogl_pipeline_set_uniform_float (self->pipeline,
                                   self->blur_pixel_step_uniform,
                                   2, /* n_components */
                                   1, /* count */
                                   pixel_step);

  cogl_pipeline_set_layer_texture (self->pipeline, 0, texture);

  cogl_pipeline_set_color4ub (self->pipeline,
                              opacity, opacity, opacity, opacity);

  cogl_set_source (self->pipeline);

  cogl_rectangle (0, 0, texture_height, texture_width);
  // cogl_rectangle_with_texture_coords (window_x, window_y, );
}

static void
meta_blur_class_info_free (MetaBlurClassInfo *class_info)
{
  g_free ((char *)class_info->name);
  g_slice_free (MetaBlurClassInfo, class_info);
}

static void
meta_blur_factory_init (MetaBlurFactory *factory)
{
  factory->snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_TEXTURE_LOOKUP,
                          box_blur_glsl_declarations,
                          NULL);
  cogl_snippet_set_replace (factory->snippet, box_blur_glsl_shader);
  guint i;
}

static void
meta_blur_factory_finalize (GObject *object)
{
  MetaBlurFactory *factory = meta_blur_FACTORY (object);
  /* Detach from the shadows in the table so we won't try to
   * remove them when they're freed. */
  G_OBJECT_CLASS (meta_blur_factory_parent_class)->finalize (object);
}

static void
meta_blur_factory_class_init (MetaBlurFactoryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = meta_blur_factory_finalize;
}

MetaBlurFactory *
meta_blur_factory_new (void)
{
  return g_object_new (META_TYPE_BLUR_FACTORY, NULL);
}

/**
 * meta_blur_factory_get_default:
 *
 * Return value: (transfer none): the global singleton shadow factory
 */
MetaBlurFactory *
meta_blur_factory_get_default (void)
{
  static MetaBlurFactory *factory;

  if (factory == NULL)
    factory = meta_blur_factory_new ();

  return factory;
}

static void
make_blur (MetaBlur *self)
{
  ClutterBackend *backend = clutter_get_default_backend ();
  CoglContext *ctx = clutter_backend_get_cogl_context (backend);
  cairo_rectangle_int_t extents;
  guchar *buffer;
  int buffer_width;
  int buffer_height;
  int x_offset;
  int y_offset;
  int n_rectangles, j, k;

  cairo_region_get_extents (region, &extents);

  CoglContext *ctx =
    clutter_backend_get_cogl_context (clutter_get_default_backend ());

  self->pipeline = cogl_pipeline_new (ctx);
  cogl_pipeline_add_layer_snippet (self->pipeline, 0, self->snippet);

}