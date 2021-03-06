/*
 * Wayland Support
 *
 * Copyright (C) 2012 Intel Corporation
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef META_WAYLAND_SEAT_H
#define META_WAYLAND_SEAT_H

#include <wayland-server.h>
#include <clutter/clutter.h>

#include "meta-wayland-types.h"
#include "meta-wayland-pointer.h"
#include "meta-wayland-keyboard.h"
#include "meta-wayland-touch.h"
#include "meta-wayland-data-device.h"

struct _MetaWaylandSeat
{
  struct wl_list base_resource_list;
  struct wl_display *wl_display;

  MetaWaylandPointer pointer;
  MetaWaylandKeyboard keyboard;
  MetaWaylandTouch touch;
  MetaWaylandDataDevice data_device;

  guint capabilities;
};

void meta_wayland_seat_init (MetaWaylandCompositor *compositor);

void meta_wayland_seat_free (MetaWaylandSeat *seat);

void meta_wayland_seat_update (MetaWaylandSeat    *seat,
                               const ClutterEvent *event);

gboolean meta_wayland_seat_handle_event (MetaWaylandSeat *seat,
                                         const ClutterEvent *event);

void meta_wayland_seat_set_input_focus (MetaWaylandSeat    *seat,
                                        MetaWaylandSurface *surface);

void meta_wayland_seat_repick (MetaWaylandSeat *seat);
void meta_wayland_seat_update_cursor_surface (MetaWaylandSeat *seat);

gboolean meta_wayland_seat_get_grab_info (MetaWaylandSeat    *seat,
					  MetaWaylandSurface *surface,
					  uint32_t            serial,
					  gfloat             *x,
					  gfloat             *y);

#endif /* META_WAYLAND_SEAT_H */
