/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#ifndef META_COMPOSITOR_PRIVATE_H
#define META_COMPOSITOR_PRIVATE_H

#include <X11/extensions/Xfixes.h>

#include <meta/compositor.h>
#include <meta/display.h>
#include "meta-plugin-manager.h"
#include "meta-window-actor-private.h"
#include <clutter/clutter.h>

struct _MetaCompositor
{
  MetaDisplay    *display;

  guint           repaint_func_id;

  gint64          server_time_query_time;
  gint64          server_time_offset;

  guint           server_time_is_monotonic_time : 1;
  guint           no_mipmaps  : 1;

  ClutterActor          *stage, *window_group, *top_window_group;
  ClutterActor          *background_actor;
  GList                 *windows;
  Window                 output;

  CoglOnscreen          *onscreen;
  CoglFrameClosure      *frame_closure;

  /* Used for unredirecting fullscreen windows */
  guint                  disable_unredirect_count;
  MetaWindow            *unredirected_window;

  gint                   switch_workspace_in_progress;

  MetaPluginManager *plugin_mgr;

  gboolean frame_has_updated_xsurfaces;
};

/* Wait 2ms after vblank before starting to draw next frame */
#define META_SYNC_DELAY 2

void meta_switch_workspace_completed (MetaCompositor *compositor);

gboolean meta_begin_modal_for_plugin (MetaCompositor   *compositor,
                                      MetaPlugin       *plugin,
                                      MetaModalOptions  options,
                                      guint32           timestamp);
void     meta_end_modal_for_plugin   (MetaCompositor   *compositor,
                                      MetaPlugin       *plugin,
                                      guint32           timestamp);

gint64 meta_compositor_monotonic_time_to_server_time (MetaDisplay *display,
                                                      gint64       monotonic_time);

#endif /* META_COMPOSITOR_PRIVATE_H */
