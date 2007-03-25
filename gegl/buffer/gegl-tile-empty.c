/* This file is part of GEGL.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Copyright 2006 Øyvind Kolås <pippin@gimp.org>
 */
#include <glib.h>
#include <glib-object.h>
#include <string.h>

#include "gegl-tile-trait.h"
#include "gegl-tile-empty.h"

G_DEFINE_TYPE (GeglTileEmpty, gegl_tile_empty, GEGL_TYPE_TILE_TRAIT)
static GObjectClass * parent_class = NULL;
enum
{
  PROP_0,
  PROP_BACKEND
};

static void
finalize (GObject *object)
{
  GeglTileEmpty *empty = GEGL_TILE_EMPTY (object);

  if (empty->tile)
    g_object_unref (empty->tile);
  (*G_OBJECT_CLASS (parent_class)->finalize)(object);
}

static GeglTile *
get_tile (GeglTileStore *gegl_tile_store,
          gint           x,
          gint           y,
          gint           z)
{
  GeglTileStore *source = GEGL_TILE_TRAIT (gegl_tile_store)->source;
  GeglTileEmpty *empty  = GEGL_TILE_EMPTY (gegl_tile_store);
  GeglTile      *tile   = NULL;

  if (source)
    tile = gegl_tile_store_get_tile (source, x, y, z);
  if (tile != NULL)
    return tile;

  tile = gegl_tile_dup (empty->tile);

  return tile;
}


static void
get_property (GObject    *gobject,
              guint       property_id,
              GValue     *value,
              GParamSpec *pspec)
{
  GeglTileEmpty *empty = GEGL_TILE_EMPTY (gobject);

  switch (property_id)
    {
      case PROP_BACKEND:
        g_value_set_object (value, empty->backend);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, property_id, pspec);
        break;
    }
}

static void
set_property (GObject      *gobject,
              guint         property_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  GeglTileEmpty *empty = GEGL_TILE_EMPTY (gobject);

  switch (property_id)
    {
      case PROP_BACKEND:
        empty->backend = g_value_get_object (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, property_id, pspec);
        break;
    }
}

static GObject *
constructor (GType                  type,
             guint                  n_params,
             GObjectConstructParam *params)
{
  GObject       *object;
  GeglTileEmpty *empty;
  gint           tile_width;
  gint           tile_height;
  gint           tile_size;

  object = G_OBJECT_CLASS (parent_class)->constructor (type, n_params, params);
  empty  = GEGL_TILE_EMPTY (object);

  g_assert (empty->backend);
  g_object_get (empty->backend, "tile-width", &tile_width,
                "tile-height", &tile_height,
                "tile-size", &tile_size,
                NULL);
  /* FIXME: need babl format here */
  empty->tile = gegl_tile_new (tile_size);
  memset (gegl_tile_get_data (empty->tile), 0x00, tile_size);

  return object;
}


static void
gegl_tile_empty_class_init (GeglTileEmptyClass *klass)
{
  GObjectClass       *gobject_class         = G_OBJECT_CLASS (klass);
  GeglTileStoreClass *gegl_tile_store_class = GEGL_TILE_STORE_CLASS (klass);

  gegl_tile_store_class->get_tile = get_tile;
  gobject_class->set_property     = set_property;
  gobject_class->get_property     = get_property;
  gobject_class->constructor      = constructor;

  g_object_class_install_property (gobject_class, PROP_BACKEND,
                                   g_param_spec_object ("backend",
                                                        "backend",
                                                        "backend for this tilestore (needed for tile size data)",
                                                        G_TYPE_OBJECT,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  gobject_class->finalize = finalize;
  parent_class            = g_type_class_peek_parent (klass);
}

static void
gegl_tile_empty_init (GeglTileEmpty *self)
{
}
