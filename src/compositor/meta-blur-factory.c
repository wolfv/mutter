
#include <config.h>
#include <math.h>
#include <string.h>

#include "cogl-utils.h"
#include "meta-blur-factory.h"
#include "region-utils.h"


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

//"

struct _MetaBlur
{
  int ref_count;

  CoglTexture *texture;
  gint radius;
  gfloat sigma;

  CoglPipeline *pipeline;

  gboolean vertical_texture_dirty;
  CoglHandle horizontal_texture;
  CoglHandle vertical_texture;
  CoglHandle vertical_fbo;

  CoglPipeline *horizontal_pipeline;
  gint horizontal_pixel_step_uniform;
  gint horizontal_factors_uniform;
  
  CoglPipeline *vertical_pipeline;
  gint vertical_pixel_step_uniform;
  gint vertical_factors_uniform;

  /* The outer order is the distance the shadow extends outside the window
   * shape; the inner border is the unscaled portion inside the window
   * shape */

  guint tex_width;
  guint tex_height;

  guint scale_width : 1;
  guint scale_height : 1;
  gint pixel_step_uniform;
  CoglSnippet * snippet;

};

enum
{
PROP_0,
PROP_SIGMA,
PROP_LAST
};

MetaBlur * 
meta_blur_new(void) {
  return g_object_new (META_TYPE_BLUR, NULL);
}

static void meta_blur_init(MetaBlur * klass) {
    clutter_blur_effect_set_sigma_real (klass, 0.84089642f);
};
static void meta_blur_class_init(MetaBlurClass * klass) {};


G_DEFINE_TYPE (MetaBlur, meta_blur, G_TYPE_OBJECT);

static void
make_blur (MetaBlur *self)
{
  ClutterBackend *backend = clutter_get_default_backend ();
  CoglContext *ctx = clutter_backend_get_cogl_context (backend);
  self->pipeline = cogl_pipeline_new (ctx);

  CoglSnippet * snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_TEXTURE_LOOKUP,
                              box_blur_glsl_declarations,
                              NULL);
  cogl_snippet_set_replace (snippet, box_blur_glsl_shader);
  cogl_pipeline_add_layer_snippet (self->pipeline, 0, snippet);
  cogl_object_unref (snippet);

  cogl_pipeline_set_layer_null_texture (self->pipeline,
                                        0, /* layer number */
                                        COGL_TEXTURE_TYPE_2D);
}

static CoglPipeline *
get_blur_pipeline(MetaBlur * self, int radius) {
  CoglSnippet *snippet;
  GString *source = g_string_new (NULL);
  CoglContext *ctx =
    clutter_backend_get_cogl_context (clutter_get_default_backend ());
  int i;

  g_string_append_printf (source,
                          "uniform vec2 pixel_step;\n"
                          "uniform float factors[%i];\n",
                          radius * 2 + 1);

  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_TEXTURE_LOOKUP,
                              source->str,
                              NULL /* post */);

  g_string_set_size (source, 0);

  CoglPipeline * pipeline = cogl_pipeline_new (ctx);
  cogl_pipeline_set_layer_null_texture (pipeline,
                                        0, /* layer_num */
                                        COGL_TEXTURE_TYPE_2D);
  cogl_pipeline_set_layer_wrap_mode (pipeline,
                                     0, /* layer_num */
                                     COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE);
  cogl_pipeline_set_layer_filters (pipeline,
                                   0, /* layer_num */
                                   COGL_PIPELINE_FILTER_NEAREST,
                                   COGL_PIPELINE_FILTER_NEAREST);

  for (i = 0; i < radius * 2 + 1; i++) {
    g_string_append (source, "cogl_texel ");

    if (i == 0)
      g_string_append (source, "=");
    else
      g_string_append (source, "+=");

    g_string_append_printf (source,
                            " texture2D (cogl_sampler, "
                            "cogl_tex_coord.st");
    if (i != radius)
      g_string_append_printf (source,
                              " + pixel_step * %f",
                              (float) (i - radius));
    g_string_append_printf (source,
                            ") * factors[%i];\n",
                            i);
  }
  cogl_snippet_set_replace (snippet, source->str);

  g_string_free (source, TRUE);

  cogl_pipeline_add_layer_snippet (pipeline, 0, snippet);

  cogl_object_unref (snippet);
  return pipeline;
}


static void
update_horizontal_pipeline_texture (MetaBlur *self) {
  float pixel_step[2];
  cogl_pipeline_set_layer_texture (self->horizontal_pipeline,
    0, /* layer_num */
    self->horizontal_texture);
  pixel_step[0] = 1.0f / self->tex_width;
  pixel_step[1] = 0.0f;
  cogl_pipeline_set_uniform_float (self->horizontal_pipeline,
    self->horizontal_pixel_step_uniform,
    2, /* n_components */
    1, /* count */
    pixel_step);
}


static void
update_vertical_pipeline_texture (MetaBlur *self) {
  float pixel_step[2];
  cogl_pipeline_set_layer_texture (self->vertical_pipeline,
    0, /* layer_num */
    self->vertical_texture);
  pixel_step[0] = 0.0f;
  pixel_step[1] = 1.0f / self->tex_height;
  cogl_pipeline_set_uniform_float (self->vertical_pipeline,
    self->vertical_pixel_step_uniform,
    2, /* n_components */
    1, /* count */
    pixel_step);
}

void
meta_blur_post_paint(MetaBlur * self) {
  CoglHandle horizontal_texture;

  horizontal_texture =
    self->texture;

  if (horizontal_texture != self->horizontal_texture) {
    if (self->horizontal_texture)
      cogl_object_unref (self->horizontal_texture);
    self->horizontal_texture = cogl_object_ref (horizontal_texture);

    self->tex_width = cogl_texture_get_width (horizontal_texture);
    self->tex_height = cogl_texture_get_height (horizontal_texture);

    update_horizontal_pipeline_texture (self);

    if (self->vertical_texture == NULL ||
        self->tex_width != cogl_texture_get_width (self->vertical_texture) ||
        self->tex_height != cogl_texture_get_height (self->vertical_texture))
      {
        if (self->vertical_texture) {
            cogl_object_unref (self->vertical_texture);
            cogl_object_unref (self->vertical_fbo);
          }

        self->vertical_texture =
          cogl_texture_new_with_size (self->tex_width, self->tex_height,
                                      COGL_TEXTURE_NO_SLICING,
                                      COGL_PIXEL_FORMAT_RGBA_8888_PRE);
        self->vertical_fbo =
          cogl_offscreen_new_to_texture (self->vertical_texture);

        update_vertical_pipeline_texture (self);
      }
  }
  self->vertical_texture_dirty = TRUE;
}

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
  if(self->pipeline == NULL) {
    make_blur(self);
  }

  guchar * pixels;
  pixels = g_malloc0(window_width * window_height * 4);

  cogl_read_pixels(
    window_x, 
    window_y, 
    window_width, 
    window_height, 
    COGL_READ_PIXELS_COLOR_BUFFER,
    COGL_PIXEL_FORMAT_RGBA_8888, 
    (guchar *) pixels);
  int i = 0;
  for(i = 0; i < window_width * window_height * 4; i += 4) {
    pixels[i] = 0;
  }
  
  self->texture = cogl_texture_new_from_data(
    window_width, 
    window_height,
    COGL_TEXTURE_NONE,
    COGL_PIXEL_FORMAT_RGBA_8888,
    COGL_PIXEL_FORMAT_RGBA_8888,
    0,
    pixels
  );

  self->pixel_step_uniform =
    cogl_pipeline_get_uniform_location (self->pipeline, "pixel_step");

  gint tex_width = cogl_texture_get_width (self->texture);
  gint tex_height = cogl_texture_get_height (self->texture);

  self->tex_width = tex_width;
  self->tex_height = tex_height;

  if (self->vertical_texture_dirty) {
    cogl_framebuffer_draw_rectangle (self->vertical_fbo,
                                     self->horizontal_pipeline,
                                     -1.0f, 1.0f, 1.0f, -1.0f);

    self->vertical_texture_dirty = FALSE;
  }

  gint paint_opacity = 255;
  cogl_pipeline_set_color4ub (self->vertical_pipeline,
                              paint_opacity,
                              paint_opacity,
                              paint_opacity,
                              paint_opacity);

  cogl_framebuffer_draw_rectangle (cogl_get_draw_framebuffer (),
                                   self->vertical_pipeline,
                                   0, 0,
                                   self->tex_width ,self->tex_height);
}


static void
meta_blur_set_sigma_real (MetaBlur *self,
                          gfloat sigma)
{
  int radius;
  float *factors;
  float sum = 0.0f;
  int i;

  if (self->sigma == sigma)
    return;

  /* According to wikipedia, using a matrix with dimensions
     ⌈6σ⌉×⌈6σ⌉ gives good enough results in practice */
  radius = floorf (ceilf (6 * sigma) / 2.0f);

  if (self->horizontal_pipeline && radius != self->radius)
    {
      cogl_object_unref (self->horizontal_pipeline);
      self->horizontal_pipeline = NULL;
      cogl_object_unref (self->vertical_pipeline);
      self->vertical_pipeline = NULL;
    }

  if (self->horizontal_pipeline == NULL)
    {
      CoglPipeline *base_pipeline =
        get_blur_pipeline (self,
                           radius);

      self->horizontal_pipeline = cogl_pipeline_copy (base_pipeline);
      self->horizontal_pixel_step_uniform =
        cogl_pipeline_get_uniform_location (self->horizontal_pipeline,
                                            "pixel_step");
      self->horizontal_factors_uniform =
        cogl_pipeline_get_uniform_location (self->horizontal_pipeline,
                                            "factors");
      update_horizontal_pipeline_texture (self);

      self->vertical_pipeline = cogl_pipeline_copy (base_pipeline);
      self->vertical_pixel_step_uniform =
        cogl_pipeline_get_uniform_location (self->vertical_pipeline,
                                            "pixel_step");
      self->vertical_factors_uniform =
        cogl_pipeline_get_uniform_location (self->vertical_pipeline,
                                            "factors");
      update_vertical_pipeline_texture (self);

      /* To avoid needing to clear the vertical texture we going to
         disable blending in the horizontal pipeline and just fill it
         with the horizontal texture */
      cogl_pipeline_set_blend (self->horizontal_pipeline,
                               "RGBA = ADD (SRC_COLOR, 0)",
                               NULL);
    }

  factors = g_alloca (sizeof (float) * (radius * 2 + 1));
  /* Special case when the radius is zero to make it just draw the
     image normally */
  if (radius == 0)
    factors[0] = 1.0f;
  else
    {
      for (i = -radius; i <= radius; i++)
        factors[i + radius] = (powf (G_E, -(i * i) / (2.0f * sigma * sigma))
                               / sqrtf (2.0f * G_PI * sigma * sigma));
      /* Normalize all of the factors */
      for (i = -radius; i <= radius; i++)
        sum += factors[i + radius];
      for (i = -radius; i <= radius; i++)
        factors[i + radius] /= sum;
    }

  cogl_pipeline_set_uniform_float (self->horizontal_pipeline,
                                   self->horizontal_factors_uniform,
                                   1, /* n_components */
                                   radius * 2 + 1,
                                   factors);
  cogl_pipeline_set_uniform_float (self->vertical_pipeline,
                                   self->vertical_factors_uniform,
                                   1, /* n_components */
                                   radius * 2 + 1,
                                   factors);

  self->sigma = sigma;
  self->radius = radius;

  self->vertical_texture_dirty = TRUE;
}