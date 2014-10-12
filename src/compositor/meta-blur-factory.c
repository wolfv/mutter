
#include <config.h>
#include <math.h>
#include <string.h>

#include "cogl-utils.h"
#include "meta-blur-factory.h"
#include "region-utils.h"


#define BLUR_PADDING    2
static const gchar *box_blur_glsl_declarations =
"uniform vec2 pixel_step;\n";

struct _MetaBlur
{
  int ref_count;

  CoglTexture *texture;
  CoglPipeline *pipeline;

  /* The outer order is the distance the shadow extends outside the window
   * shape; the inner border is the unscaled portion inside the window
   * shape */

  guint scale_width : 1;
  guint scale_height : 1;
  gint pixel_step_uniform;
  CoglSnippet * snippet;

  CoglFramebuffer *buffer_fb;
  CoglTexture *blur_tex;
  CoglFramebuffer *buffer_fb2;
  CoglTexture *blur_tex2;

};

static void
get_shader(gint radius, GString * shader) {
    int i,j, total;
    printf("shader");
    g_string_append(shader, "cogl_texel = texture2D (cogl_sampler, cogl_tex_coord.st);\n");
    for(i = -radius + 1; i < radius; i++) {
        for(j = -radius + 1; j < radius; j++) {
            g_string_append_printf(shader,
                "cogl_texel += texture2D (cogl_sampler, cogl_tex_coord.st + pixel_step * " \
                "vec2 ( %d, %d));\n",
                i, j
            );
        }
    }
    total = (radius * 2 - 1) * (radius * 2 - 1);
    g_string_append_printf(shader,
        "cogl_texel /= %d;\n",
        total
    );
}

MetaBlur * 
meta_blur_new(void) {
  return g_object_new (META_TYPE_BLUR, NULL);
}

static void meta_blur_init(MetaBlur * klass) {};
static void meta_blur_class_init(MetaBlurClass * klass) {};


G_DEFINE_TYPE (MetaBlur, meta_blur, G_TYPE_OBJECT);

static void
make_blur (MetaBlur *self)
{
  ClutterBackend *backend = clutter_get_default_backend ();
  CoglContext *ctx = clutter_backend_get_cogl_context (backend);
  self->pipeline = cogl_pipeline_new (ctx);
  GString * shader;
  get_shader(6, shader);
  CoglSnippet * snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_TEXTURE_LOOKUP,
                              box_blur_glsl_declarations,
                              NULL);
  cogl_snippet_set_replace (snippet, shader);
  cogl_pipeline_add_layer_snippet (self->pipeline, 0, snippet);
  cogl_object_unref (snippet);

  cogl_pipeline_set_layer_null_texture (self->pipeline,
                                        0, /* layer number */
                                        COGL_TEXTURE_TYPE_2D);

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
  ClutterBackend *backend = clutter_get_default_backend ();
  CoglContext *ctx = clutter_backend_get_cogl_context (backend);
  
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
    COGL_PIXEL_FORMAT_RGB_888, 
    (guchar *) pixels);

  self->texture = cogl_texture_new_from_data(
    window_width, 
    window_height,
    COGL_TEXTURE_NONE,
    COGL_PIXEL_FORMAT_RGB_888,
    COGL_PIXEL_FORMAT_RGB_888,
    0,
    pixels
  );

  g_free(pixels);
  
  self->pixel_step_uniform =
    cogl_pipeline_get_uniform_location (self->pipeline, "pixel_step");

  gint tex_width = cogl_texture_get_width (self->texture);
  gint tex_height = cogl_texture_get_height (self->texture);

  self->blur_tex = cogl_texture_2d_new_with_size(ctx, tex_width / 10, tex_height / 10);
  self->buffer_fb = cogl_offscreen_new_with_texture(self->blur_tex);
  self->blur_tex2 = cogl_texture_2d_new_with_size(ctx, tex_width / 5, tex_height / 5);
  self->buffer_fb2 = cogl_offscreen_new_with_texture(self->blur_tex2);

  if(self->pixel_step_uniform > -1) {

      gfloat pixel_step[2];

      pixel_step[0] = 1.0f / (tex_width / 10);
      pixel_step[1] = 1.0f / (tex_height / 10);

      cogl_pipeline_set_uniform_float (self->pipeline,
                                       self->pixel_step_uniform,
                                       2, /* n_components */
                                       1, /* count */
                                       pixel_step);
    }

  cogl_pipeline_set_layer_texture (self->pipeline, 0, self->texture);

  cogl_framebuffer_draw_textured_rectangle(
      self->buffer_fb,
      self->pipeline,
      -1, 1, 1, -1,
      0, 0, 1, 1
  );
  if(self->pixel_step_uniform > -1) {

      gfloat pixel_step[2];

      pixel_step[0] = 1.0f / (tex_width / 5);
      pixel_step[1] = 1.0f / (tex_height / 5);

      cogl_pipeline_set_uniform_float (self->pipeline,
                                       self->pixel_step_uniform,
                                       2, /* n_components */
                                       1, /* count */
                                       pixel_step);
    }

  cogl_pipeline_set_layer_texture(self->pipeline, 0, self->blur_tex);
  cogl_framebuffer_draw_textured_rectangle(
      self->buffer_fb2,
      self->pipeline,
      -1, 1, 1, -1,
      0, 0, 1, 1
  );
  if(self->pixel_step_uniform > -1) {

      gfloat pixel_step[2];

      pixel_step[0] = 1.0f / (tex_width / 1);
      pixel_step[1] = 1.0f / (tex_height / 1);

      cogl_pipeline_set_uniform_float (self->pipeline,
                                       self->pixel_step_uniform,
                                       2, /* n_components */
                                       1, /* count */
                                       pixel_step);
  }

  cogl_pipeline_set_layer_texture(self->pipeline, 0, self->blur_tex2);


  cogl_pipeline_set_color4ub (self->pipeline,
                              opacity, opacity, opacity, opacity);

  // cogl_set_source (self->pipeline);
  cogl_push_source(self->pipeline);
  cogl_rectangle (0, 0, window_width, window_height);
  cogl_pop_source();
  g_free(self->texture);
  g_free(self->blur_tex);
  g_free(self->buffer_fb); 
  g_free(self->blur_tex2);
  g_free(self->buffer_fb2);
  // cogl_rectangle_with_texture_coords (window_x, window_y, );
}

