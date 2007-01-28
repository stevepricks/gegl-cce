/* This file is part of GEGL
 *
 * GEGL is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * GEGL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GEGL; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Copyright 2003 Calvin Williamson
 */

#ifndef __GEGL_UTILS_H__
#define __GEGL_UTILS_H__

G_BEGIN_DECLS

void        gegl_rectangle_set          (GeglRectangle *r,
                                         gint           x,
                                         gint           y,
                                         guint          w,
                                         guint          h);
gboolean    gegl_rectangle_equal        (GeglRectangle *r,
                                         GeglRectangle *s);
gboolean    gegl_rectangle_equal_coords (GeglRectangle *r,
                                         gint           x,
                                         gint           y,
                                         gint           w,
                                         gint           h);
void        gegl_rectangle_copy         (GeglRectangle *to,
                                         GeglRectangle *from);
void        gegl_rectangle_bounding_box (GeglRectangle *dest,
                                         GeglRectangle *src1,
                                         GeglRectangle *src2);
gboolean    gegl_rectangle_intersect    (GeglRectangle *dest,
                                         GeglRectangle *src1,
                                         GeglRectangle *src2);
gboolean    gegl_rectangle_contains     (GeglRectangle *r,
                                         GeglRectangle *s);

void        gegl_log_debug              (gchar    *file,
                                         gint      line,
                                         gchar    *function,
                                         gchar    *format,
                                         ...);
void        gegl_log_info               (gchar    *file,
                                         gint      line,
                                         gchar    *function,
                                         gchar    *format,
                                         ...);
void        gegl_log_message            (gchar    *file,
                                         gint      line,
                                         gchar    *function,
                                         gchar    *format,
                                         ...);
void        gegl_log_direct             (gchar    *format,
                                         ...);
GType       gegl_rectangle_get_type     (void) G_GNUC_CONST;

#ifndef __GEGL_H__
#define     GEGL_TYPE_RECTANGLE            (gegl_rectangle_get_type ())
#endif

#define GEGL_FLOAT_EPSILON            (1e-5)
#define GEGL_FLOAT_IS_ZERO(value)     (_gegl_float_epsilon_zero ((value)))
#define GEGL_FLOAT_EQUAL(v1, v2)      (_gegl_float_epsilon_equal ((v1), (v2)))

#define INT_MULT(a,b,t)  ((t) = (a) * (b) + 0x80, ((((t) >> 8) + (t)) >> 8))

inline gint _gegl_float_epsilon_zero  (float     value);
inline gint _gegl_float_epsilon_equal (float     v1,
                                       float     v2);


G_END_DECLS

#endif /* __GEGL_UTILS_H__ */
