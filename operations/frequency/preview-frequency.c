/* This file is a part of GEGL
 *
 * GEGL is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * GEGL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GEGL; if not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright 2008 Zhang Junbo  <zhangjb@svn.gnome.org>
 */

#ifdef GEGL_CHANT_PROPERTIES

/* no properties */

#else

#define GEGL_CHANT_TYPE_FILTER
#define GEGL_CHANT_C_FILE       "preview-frequency.c"

#include "gegl-chant.h"
#include "tools/display.c"
#include "tools/fourier.c"
#include "tools/component.c"
#include <fftw3.h>

static GeglRectangle
get_bounding_box (GeglOperation *operation)
{
  GeglRectangle *in_rect = gegl_operation_source_get_bounding_box (operation,
                                                                   "input");
  GeglRectangle  result  = *in_rect;

  result.width = 2*(result.width-1);
  return result;
}

static GeglRectangle
get_cached_region(GeglOperation *operation,
                  const GeglRectangle *roi)
{
  return get_bounding_box(operation);
}

static GeglRectangle
get_required_for_output(GeglOperation *operation,
                        const gchar *input_pad,
                        const GeglRectangle *roi)
{
  GeglRectangle result = *gegl_operation_source_get_bounding_box(operation,
                                                                 "input");
  return result;
}

static void
prepare(GeglOperation *operation)
{
  gegl_operation_set_format(operation, "input",
                            babl_format("frequency double"));
  gegl_operation_set_format(operation, "output", babl_format("RGBA double"));
}

static gboolean
process(GeglOperation *operation,
        GeglBuffer *input,
        GeglBuffer *output,
        const GeglRectangle *result)
{
  gint width = 2*(gegl_buffer_get_width(input)-1); /* width always refers to the
                                                      image's (not buffer's)
                                                      width. */
  gint height = gegl_buffer_get_height(input);
  gdouble *src_buf;
  gdouble *dst_buf;
  gdouble *tmp_src_buf;
  gdouble *tmp_dst_buf;
  glong i;
  
  src_buf = g_new0(gdouble, 4*2*height*FFT_HALF(width));
  dst_buf = g_new0(gdouble, 4*width*height);
  tmp_src_buf = g_new0(gdouble, 2*height*FFT_HALF(width));
  tmp_dst_buf = g_new0(gdouble, width*height);

  gegl_buffer_get(input, 1.0, NULL, babl_format("frequency double"),
                  (gdouble *)src_buf, GEGL_AUTO_ROWSTRIDE);
  decode((gdouble *)src_buf);
  for (i=0; i<3; i++)
    {
      get_rgba_component(src_buf, tmp_src_buf, i, 2*height*FFT_HALF(width));
      fre2img((fftw_complex *)tmp_src_buf, tmp_dst_buf, width, height);
      set_rgba_component(tmp_dst_buf, dst_buf, i, width*height);
    }
  for (i=0; i<height*width; i++)
    {
      dst_buf[i*4+3] = 1;
    }
  
  gegl_buffer_set(output, 
                  NULL, babl_format ("RGBA double"), (gdouble *)dst_buf,
                  GEGL_AUTO_ROWSTRIDE);   
  g_free(src_buf);
  g_free(dst_buf);
  g_free(tmp_src_buf);
  g_free(tmp_dst_buf);
  return TRUE;
}

static void
gegl_chant_class_init(GeglChantClass *klass)
{
  GeglOperationClass *operation_class;
  GeglOperationFilterClass *filter_class;

  operation_class = GEGL_OPERATION_CLASS(klass);
  filter_class = GEGL_OPERATION_FILTER_CLASS(klass);

  filter_class->process = process;
  operation_class->prepare = prepare;
  operation_class->get_bounding_box = get_bounding_box;
  operation_class->get_required_for_output= get_required_for_output;  
  operation_class->get_cached_region = get_cached_region;

  operation_class->name = "preview-frequency";
  operation_class->categories = "frequency";
  operation_class->description
    = "Convert the frequency buffer to a displayable RGBA image.";
}

#endif
