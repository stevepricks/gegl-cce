/* This file is part of GEGL
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
 * Copyright 2006 Øyvind Kolås
 */


#include "config.h"

#include <glib-object.h>

#include "gegl.h"
#include "gegl-debug.h"
#include "gegl-operation-point-composer.h"
#include "gegl-operation-context.h"
#include "gegl-config.h"
#include "gegl-types-internal.h"
#include "gegl-buffer-private.h"
#include "gegl-tile-storage.h"
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

#include "opencl/gegl-cl.h"
#include "gegl-buffer-cl-iterator.h"

typedef struct ThreadData
{
  GeglOperationPointComposerClass *klass;
  GeglOperation                   *operation;
  guchar                          *input;
  guchar                          *aux;
  guchar                          *output;
  gint                            *pending;
  gint                            *started;
  gint                             level;
  gboolean                         success;
  GeglRectangle                    roi;

  guchar                          *in_tmp;
  guchar                          *aux_tmp;
  guchar                          *output_tmp;
  const Babl *input_fish;
  const Babl *aux_fish;
  const Babl *output_fish;
} ThreadData;

static void thread_process (gpointer thread_data, gpointer unused)
{
  ThreadData *data = thread_data;

  guchar *input = data->input;
  guchar *aux = data->aux;
  guchar *output = data->output;
  glong samples = data->roi.width * data->roi.height;

  if (data->input_fish && input)
    {
      babl_process (data->input_fish, data->input, data->in_tmp, samples);
      input = data->in_tmp;
    }
  if (data->aux_fish && aux)
    {
      babl_process (data->aux_fish, data->aux, data->aux_tmp, samples);
      aux = data->aux_tmp;
    }
  if (data->output_fish)
    output = data->output_tmp;

  if (!data->klass->process (data->operation,
                       input, aux,
                       output, samples,
                       &data->roi, data->level))
    data->success = FALSE;

  if (data->output_fish)
    babl_process (data->output_fish, data->output_tmp, data->output, samples);

  g_atomic_int_add (data->pending, -1);
}

static GThreadPool *thread_pool (void)
{
  static GThreadPool *pool = NULL;
  if (!pool)
    {
      pool =  g_thread_pool_new (thread_process, NULL, gegl_config_threads (),
                                 FALSE, NULL);
    }
  return pool;
}

static gboolean
gegl_operation_composer_process (GeglOperation        *operation,
                                 GeglOperationContext *context,
                                 const gchar          *output_prop,
                                 const GeglRectangle  *result,
                                 gint                  level)
{
  GeglOperationComposerClass *klass   = GEGL_OPERATION_COMPOSER_GET_CLASS (operation);
  GeglBuffer                 *input;
  GeglBuffer                 *aux;
  GeglBuffer                 *output;
  gboolean                    success = FALSE;

  GeglRectangle scaled_result = *result;
  if (level)
  {
    scaled_result.x >>= level;
    scaled_result.y >>= level;
    scaled_result.width >>= level;
    scaled_result.height >>= level;
    result = &scaled_result;
  }

  if (strcmp (output_prop, "output"))
    {
      g_warning ("requested processing of %s pad on a composer", output_prop);
      return FALSE;
    }

  if (result->width == 0 || result->height == 0)
  {
    output = gegl_operation_context_get_target (context, "output");
    return TRUE;
  }

  input  = gegl_operation_context_get_source (context, "input");
  output = gegl_operation_context_get_output_maybe_in_place (operation,
                                                             context,
                                                             input,
                                                             result);

  aux   = gegl_operation_context_get_source (context, "aux");

  /* A composer with a NULL aux, can still be valid, the
   * subclass has to handle it.
   */
  if (input != NULL ||
      aux != NULL)
    {
      success = klass->process (operation, input, aux, output, result, level);

      g_clear_object (&input);
      g_clear_object (&aux);
    }
  else
    {
      g_warning ("%s received NULL input, aux, and aux2",
                 gegl_node_get_operation (operation->node));
    }

  return success;
}

static gboolean gegl_operation_point_composer_process
                              (GeglOperation       *operation,
                               GeglBuffer          *input,
                               GeglBuffer          *aux,
                               GeglBuffer          *output,
                               const GeglRectangle *result,
                               gint                 level);

G_DEFINE_TYPE (GeglOperationPointComposer, gegl_operation_point_composer, GEGL_TYPE_OPERATION_COMPOSER)

/*static gboolean
gegl_operation_point_composer_cl_process (GeglOperation       *operation,
                                          GeglBuffer          *input,
                                          GeglBuffer          *aux,
                                          GeglBuffer          *output,
                                          const GeglRectangle *result,
                                          gint                 level)
{
  const Babl *in_format  = gegl_operation_get_format (operation, "input");
  const Babl *out_format = gegl_operation_get_format (operation, "output");

  GeglOperationClass *operation_class = GEGL_OPERATION_GET_CLASS (operation);
  GeglOperationPointComposerClass *point_composer_class = GEGL_OPERATION_POINT_COMPOSER_GET_CLASS (operation);

  GeglBufferClIterator *iter = NULL;

  cl_int cl_err = 0;
  gboolean err;

  GEGL_NOTE (GEGL_DEBUG_OPENCL, "GEGL_OPERATION_POINT_COMPOSER: %s", operation_class->name);

  //Process
  iter = gegl_buffer_cl_iterator_new (output, result, out_format, GEGL_CL_BUFFER_WRITE);

  gegl_buffer_cl_iterator_add (iter, input, result, in_format, GEGL_CL_BUFFER_READ, GEGL_ABYSS_NONE);

  if (aux)
    {
      const Babl *aux_format = gegl_operation_get_format (operation, "aux");
      gegl_buffer_cl_iterator_add (iter, aux, result, aux_format, GEGL_CL_BUFFER_READ, GEGL_ABYSS_NONE);
    }

  while (gegl_buffer_cl_iterator_next (iter, &err))
    {
      if (err)
        return FALSE;

      if (point_composer_class->cl_process)
        {
          err = point_composer_class->cl_process (operation,
                                                  iter->tex[1],
                                                  (aux) ? iter->tex[2] : NULL,
                                                  iter->tex[0],
                                                  iter->size[0],
                                                  &iter->roi[0],
                                                  level);
          if (err)
            {
              GEGL_NOTE (GEGL_DEBUG_OPENCL, "Error: %s", operation_class->name);
              gegl_buffer_cl_iterator_stop (iter);
              return FALSE;
            }
        }
      else if (operation_class->cl_data)
        {
          gint p = 0;
          GeglClRunData *cl_data = operation_class->cl_data;

          cl_err = gegl_clSetKernelArg (cl_data->kernel[0], p++, sizeof(cl_mem), (void*)&iter->tex[1]);
          CL_CHECK;
          cl_err = gegl_clSetKernelArg (cl_data->kernel[0], p++, sizeof(cl_mem), (aux) ? (void*)&iter->tex[2] : NULL);
          CL_CHECK;
          cl_err = gegl_clSetKernelArg (cl_data->kernel[0], p++, sizeof(cl_mem), (void*)&iter->tex[0]);
          CL_CHECK;

          gegl_operation_cl_set_kernel_args (operation, cl_data->kernel[0], &p, &cl_err);
          CL_CHECK;

          cl_err = gegl_clEnqueueNDRangeKernel (gegl_cl_get_command_queue (),
                                                cl_data->kernel[0], 1,
                                                NULL, &iter->size[0], NULL,
                                                0, NULL, NULL);
          CL_CHECK;
        }
      else
        {
          g_warning ("OpenCL support enabled, but no way to execute");
          gegl_buffer_cl_iterator_stop (iter);
          return FALSE;
        }
    }

  return TRUE;

error:
  GEGL_NOTE (GEGL_DEBUG_OPENCL, "Error: %s", gegl_cl_errstring (cl_err));
  if (iter)
    gegl_buffer_cl_iterator_stop (iter);
  return FALSE;
}*/

static void prepare (GeglOperation *operation)
{
  const Babl *format = gegl_babl_rgba_linear_float ();
  gegl_operation_set_format (operation, "input", format);
  gegl_operation_set_format (operation, "aux", format);
  gegl_operation_set_format (operation, "output", format);
}

static void
gegl_operation_point_composer_class_init (GeglOperationPointComposerClass *klass)
{
  GeglOperationClass          *operation_class = GEGL_OPERATION_CLASS (klass);
  GeglOperationComposerClass *composer_class  = GEGL_OPERATION_COMPOSER_CLASS (klass);

  composer_class->process = gegl_operation_point_composer_process;
  operation_class->process = gegl_operation_composer_process;
  operation_class->prepare = prepare;
  operation_class->no_cache =TRUE;
  operation_class->want_in_place = TRUE;
  operation_class->threaded = TRUE;
}

static void
gegl_operation_point_composer_init (GeglOperationPointComposer *self)
{

}

static gboolean
gegl_operation_point_composer_process (GeglOperation       *operation,
                                       GeglBuffer          *input,
                                       GeglBuffer          *aux,
                                       GeglBuffer          *output,
                                       const GeglRectangle *result,
                                       gint                 level)
{
  GeglOperationClass *operation_class = GEGL_OPERATION_GET_CLASS (operation);
  GeglOperationPointComposerClass *point_composer_class = GEGL_OPERATION_POINT_COMPOSER_GET_CLASS (operation);
  const Babl *in_format   = gegl_operation_get_format (operation, "input");
  const Babl *aux_format  = gegl_operation_get_format (operation, "aux");
  const Babl *out_format  = gegl_operation_get_format (operation, "output");


  if ((result->width > 0) && (result->height > 0))
    {
      const Babl *in_buf_format  = input?gegl_buffer_get_format(input):NULL;
      const Babl *aux_buf_format = aux?gegl_buffer_get_format(aux):NULL;
      const Babl *output_buf_format = output?gegl_buffer_get_format(output):NULL;

/*      if (gegl_operation_use_opencl (operation) && (operation_class->cl_data || point_composer_class->cl_process))
        {
          if (gegl_operation_point_composer_cl_process (operation, input, aux, output, result, level))
              return TRUE;
        }*/

      if (gegl_operation_use_threading (operation, result) && result->height > 1)
      {
        gint threads = gegl_config_threads ();
        GThreadPool *pool = thread_pool ();
        ThreadData thread_data[GEGL_MAX_THREADS];
        GeglBufferIterator *i = gegl_buffer_iterator_new (output, result, level, output_buf_format,
                                                          GEGL_ACCESS_WRITE, GEGL_ABYSS_NONE);
        gint foo = 0, read = 0;

        gint in_bpp = input?babl_format_get_bytes_per_pixel (in_format):0;
        gint aux_bpp = aux?babl_format_get_bytes_per_pixel (aux_format):0;
        gint out_bpp = babl_format_get_bytes_per_pixel (out_format);

        gint in_buf_bpp = input?babl_format_get_bytes_per_pixel (in_buf_format):0;
        gint aux_buf_bpp = aux?babl_format_get_bytes_per_pixel (aux_buf_format):0;
        gint out_buf_bpp = babl_format_get_bytes_per_pixel (output_buf_format);
        gint temp_id = 0;

        if (input)
        {
          read = gegl_buffer_iterator_add (i, input, result, level, in_buf_format,
                                           GEGL_ACCESS_READ, GEGL_ABYSS_NONE);
          for (gint j = 0; j < threads; j ++)
          {
            if (in_buf_format != in_format)
            {
              thread_data[j].input_fish = babl_fish (in_buf_format, in_format);
              thread_data[j].in_tmp = gegl_temp_buffer (temp_id++, in_bpp * output->tile_storage->tile_width * output->tile_storage->tile_height);
            }
            else
            {
              thread_data[j].input_fish = NULL;
            }
          }
        }
        else
          for (gint j = 0; j < threads; j ++)
            thread_data[j].input_fish = NULL;
        if (aux)
        {
          foo = gegl_buffer_iterator_add (i, aux, result, level, aux_buf_format,
                                          GEGL_ACCESS_READ, GEGL_ABYSS_NONE);
          for (gint j = 0; j < threads; j ++)
          {
            if (aux_buf_format != aux_format)
            {
              thread_data[j].aux_fish = babl_fish (aux_buf_format, aux_format);
              thread_data[j].aux_tmp = gegl_temp_buffer (temp_id++, aux_bpp * output->tile_storage->tile_width * output->tile_storage->tile_height);
            }
            else
            {
              thread_data[j].aux_fish = NULL;
            }
          }
        }
        else
        {
          for (gint j = 0; j < threads; j ++)
            thread_data[j].aux_fish = NULL;
        }

        for (gint j = 0; j < threads; j ++)
        {
          if (output_buf_format != gegl_buffer_get_format (output))
          {
            thread_data[j].output_fish = babl_fish (out_format, output_buf_format);
            thread_data[j].output_tmp = gegl_temp_buffer (temp_id++, out_bpp * output->tile_storage->tile_width * output->tile_storage->tile_height);
          }
          else
          {
            thread_data[j].output_fish = NULL;
          }
        }

        while (gegl_buffer_iterator_next (i))
          {
            gint threads = gegl_config_threads ();
            gint pending;
            gint bit;

            if (i->roi[0].height < threads)
            {
              threads = i->roi[0].height;
            }

            bit = i->roi[0].height / threads;
            pending = threads;

            for (gint j = 0; j < threads; j++)
            {
              thread_data[j].roi.x = (i->roi[0]).x;
              thread_data[j].roi.width = (i->roi[0]).width;
              thread_data[j].roi.y = (i->roi[0]).y + bit * j;
              thread_data[j].roi.height = bit;
            }
            thread_data[threads-1].roi.height = i->roi[0].height - (bit * (threads-1));

            for (gint j = 0; j < threads; j++)
            {
              thread_data[j].klass = point_composer_class;
              thread_data[j].operation = operation;
              thread_data[j].input = input?((guchar*)i->data[read]) + (bit * j * i->roi[0].width * in_buf_bpp):NULL;
              thread_data[j].aux = aux?((guchar*)i->data[foo]) + (bit * j * i->roi[0].width * aux_buf_bpp):NULL;
              thread_data[j].output = ((guchar*)i->data[0]) + (bit * j * i->roi[0].width * out_buf_bpp);
              thread_data[j].pending = &pending;
              thread_data[j].level = level;
              thread_data[j].success = TRUE;
            }
            pending = threads;

            for (gint j = 1; j < threads; j++)
              g_thread_pool_push (pool, &thread_data[j], NULL);
            thread_process (&thread_data[0], NULL);

            while (g_atomic_int_get (&pending)) {};
          }

        return TRUE;
      }
      else
      {
        GeglBufferIterator *i = gegl_buffer_iterator_new (output, result, level, out_format, GEGL_ACCESS_WRITE, GEGL_ABYSS_NONE);
        gint foo = 0, read = 0;

        if (input)
          read = gegl_buffer_iterator_add (i, input, result, level, in_format, GEGL_ACCESS_READ, GEGL_ABYSS_NONE);
        if (aux)
          foo = gegl_buffer_iterator_add (i, aux, result, level, aux_format, GEGL_ACCESS_READ, GEGL_ABYSS_NONE);

        while (gegl_buffer_iterator_next (i))
          {
            point_composer_class->process (operation, input?i->data[read]:NULL,
                                                      aux?i->data[foo]:NULL,
                                                      i->data[0], i->length, &(i->roi[0]), level);
          }
        return TRUE;
      }
    }
  return TRUE;
}
