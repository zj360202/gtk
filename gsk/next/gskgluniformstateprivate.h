/* gskgluniformstateprivate.h
 *
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef GSK_GL_UNIFORM_STATE_PRIVATE_H
#define GSK_GL_UNIFORM_STATE_PRIVATE_H

#include "gskgltypesprivate.h"

G_BEGIN_DECLS

typedef struct { float v0; } Uniform1f;
typedef struct { float v0; float v1; } Uniform2f;
typedef struct { float v0; float v1; float v2; } Uniform3f;
typedef struct { float v0; float v1; float v2; float v3; } Uniform4f;

typedef struct { int v0; } Uniform1i;
typedef struct { int v0; int v1; } Uniform2i;
typedef struct { int v0; int v1; int v2; } Uniform3i;
typedef struct { int v0; int v1; int v2; int v3; } Uniform4i;

typedef struct { guint v0; } Uniform1ui;

#define GSK_GL_UNIFORM_ARRAY_BITS  5
#define GSK_GL_UNIFORM_FORMAT_BITS 5
#define GSK_GL_UNIFORM_OFFSET_BITS 21

typedef struct _GskGLUniformInfo
{
  guint initial     : 1;
  guint format      : GSK_GL_UNIFORM_FORMAT_BITS;
  guint array_count : GSK_GL_UNIFORM_ARRAY_BITS;
  guint offset      : GSK_GL_UNIFORM_OFFSET_BITS;
} GskGLUniformInfo;

G_STATIC_ASSERT (sizeof (GskGLUniformInfo) == 4);

typedef struct _GskGLUniformInfoElement
{
  GskGLUniformInfo info;
  guint stamp;
} GskGLUniformInfoElement;

G_STATIC_ASSERT (sizeof (GskGLUniformInfoElement) == 8);

typedef struct _GskGLUniformProgram
{
  guint program_id;
  guint n_uniforms : 12;
  guint has_attachments : 1;

  /* To avoid walking our 1:1 array of location->uniform slots, we have
   * a sparse index that allows us to skip the empty zones.
   */
  guint *sparse;
  guint n_sparse;

  /* Uniforms are provided inline at the end of structure to avoid
   * an extra dereference.
   */
  GskGLUniformInfoElement uniforms[0];
} GskGLUniformProgram;

typedef struct _GskGLUniformState
{
  GHashTable *programs;
  guint8 *values_buf;
  guint values_pos;
  guint values_len;
} GskGLUniformState;

/**
 * GskGLUniformStateCallback:
 * @info: a pointer to the information about the uniform
 * @location: the location of the uniform within the GPU program.
 * @user_data: closure data for the callback
 *
 * This callback can be used to snapshot state of a program which
 * is useful when batching commands so that the state may be compared
 * with future evocations of the program.
 */
typedef void (*GskGLUniformStateCallback) (const GskGLUniformInfo *info,
                                           guint                   location,
                                           gpointer                user_data);

typedef enum _GskGLUniformKind
{
  GSK_GL_UNIFORM_FORMAT_1F = 1,
  GSK_GL_UNIFORM_FORMAT_2F,
  GSK_GL_UNIFORM_FORMAT_3F,
  GSK_GL_UNIFORM_FORMAT_4F,

  GSK_GL_UNIFORM_FORMAT_1FV,
  GSK_GL_UNIFORM_FORMAT_2FV,
  GSK_GL_UNIFORM_FORMAT_3FV,
  GSK_GL_UNIFORM_FORMAT_4FV,

  GSK_GL_UNIFORM_FORMAT_1I,
  GSK_GL_UNIFORM_FORMAT_2I,
  GSK_GL_UNIFORM_FORMAT_3I,
  GSK_GL_UNIFORM_FORMAT_4I,

  GSK_GL_UNIFORM_FORMAT_1UI,

  GSK_GL_UNIFORM_FORMAT_TEXTURE,

  GSK_GL_UNIFORM_FORMAT_MATRIX,
  GSK_GL_UNIFORM_FORMAT_ROUNDED_RECT,
  GSK_GL_UNIFORM_FORMAT_COLOR,

  GSK_GL_UNIFORM_FORMAT_LAST
} GskGLUniformFormat;

G_STATIC_ASSERT (GSK_GL_UNIFORM_FORMAT_LAST < (1 << GSK_GL_UNIFORM_FORMAT_BITS));

GskGLUniformState   *gsk_gl_uniform_state_new         (void);
GskGLUniformState   *gsk_gl_uniform_state_ref         (GskGLUniformState         *state);
void                 gsk_gl_uniform_state_unref       (GskGLUniformState         *state);
GskGLUniformProgram *gsk_gl_uniform_state_get_program (GskGLUniformState         *state,
                                                       guint                      program,
                                                       guint                      n_uniforms);
void                 gsk_gl_uniform_state_end_frame   (GskGLUniformState         *state);
gsize                gsk_gl_uniform_format_size       (GskGLUniformFormat         format);
gpointer             gsk_gl_uniform_state_init_value  (GskGLUniformState         *state,
                                                       GskGLUniformProgram       *program,
                                                       GskGLUniformFormat         format,
                                                       guint                      array_count,
                                                       guint                      location,
                                                       GskGLUniformInfoElement  **infoptr);

#define GSK_GL_UNIFORM_VALUE(base, offset) ((gpointer)((base) + ((offset) * 4)))
#define gsk_gl_uniform_state_get_uniform_data(state,offset) GSK_GL_UNIFORM_VALUE((state)->values_buf, offset)
#define gsk_gl_uniform_state_snapshot(state, program_info, callback, user_data) \
  G_STMT_START {                                                                \
    for (guint z = 0; z < program_info->n_sparse; z++)                          \
      {                                                                         \
        guint location = program_info->sparse[z];                               \
        GskGLUniformInfoElement *info = &program_info->uniforms[location];      \
                                                                                \
        g_assert (location < GL_MAX_UNIFORM_LOCATIONS);                         \
        g_assert (location < program_info->n_uniforms);                         \
                                                                                \
        if (info->info.format > 0)                                              \
          callback (&info->info, location, user_data);                          \
      }                                                                         \
  } G_STMT_END

static inline gpointer
gsk_gl_uniform_state_get_value (GskGLUniformState        *state,
                                GskGLUniformProgram      *program,
                                GskGLUniformFormat        format,
                                guint                     array_count,
                                guint                     location,
                                guint                     stamp,
                                GskGLUniformInfoElement **infoptr)
{
  GskGLUniformInfoElement *info;

  if (location == (guint)-1)
    return NULL;

  /* If the stamp is the same, then we can ignore the request
   * and short-circuit as early as possible. This requires the
   * caller to increment their private stamp when they change
   * internal state.
   *
   * This is generally used for the shared uniforms like projection,
   * modelview, clip, etc to avoid so many comparisons which cost
   * considerable CPU.
   */
  info = &program->uniforms[location];
  if (stamp != 0 && stamp == info->stamp)
    return NULL;

  if G_LIKELY (format == info->info.format && array_count <= info->info.array_count)
    {
      *infoptr = info;
      return GSK_GL_UNIFORM_VALUE (state->values_buf, info->info.offset);
    }

  return gsk_gl_uniform_state_init_value (state, program, format, array_count, location, infoptr);
}

static inline guint
gsk_gl_uniform_state_align (guint current_pos,
                            guint size)
{
  guint align = size > 8 ? 16 : (size > 4 ? 8 : 4);
  guint masked = current_pos & (align - 1);

  g_assert (size > 0);
  g_assert (align == 4 || align == 8 || align == 16);
  g_assert (masked < align);

  return align - masked;
}

static inline gpointer
gsk_gl_uniform_state_realloc (GskGLUniformState *state,
                              guint              size,
                              guint             *offset)
{
  guint padding = gsk_gl_uniform_state_align (state->values_pos, size);

  if G_UNLIKELY (state->values_len - padding - size < state->values_pos)
    {
      state->values_len *= 2;
      state->values_buf = g_realloc (state->values_buf, state->values_len);
    }

  /* offsets are in slots of 4 to use fewer bits */
  g_assert ((state->values_pos + padding) % 4 == 0);
  *offset = (state->values_pos + padding) / 4;
  state->values_pos += padding + size;

  return GSK_GL_UNIFORM_VALUE (state->values_buf, *offset);
}

#define GSK_GL_UNIFORM_STATE_REPLACE(info, u, type, count)                                \
  G_STMT_START {                                                                          \
    if ((info)->info.initial && count == (info)->info.array_count)                        \
      {                                                                                   \
        u = GSK_GL_UNIFORM_VALUE (state->values_buf, (info)->info.offset);                \
      }                                                                                   \
    else                                                                                  \
      {                                                                                   \
        guint offset;                                                                     \
        u = gsk_gl_uniform_state_realloc (state, sizeof(type) * MAX (1, count), &offset); \
        g_assert (offset < (1 << GSK_GL_UNIFORM_OFFSET_BITS));                            \
        (info)->info.offset = offset;                                                     \
        /* We might have increased array length */                                        \
        (info)->info.array_count = count;                                                 \
      }                                                                                   \
  } G_STMT_END

static inline void
gsk_gl_uniform_info_changed (GskGLUniformInfoElement *info,
                             guint                    location,
                             guint                    stamp)
{
  info->stamp = stamp;
  info->info.initial = FALSE;
}

static inline void
gsk_gl_uniform_state_set1f (GskGLUniformState   *state,
                            GskGLUniformProgram *program,
                            guint                location,
                            guint                stamp,
                            float                value0)
{
  Uniform1f *u;
  GskGLUniformInfoElement *info;

  g_assert (state != NULL);
  g_assert (program != 0);

  if ((u = gsk_gl_uniform_state_get_value (state, program, GSK_GL_UNIFORM_FORMAT_1F, 1, location, stamp, &info)))
    {
      GSK_GL_UNIFORM_STATE_REPLACE (info, u, Uniform1f , 1);
      u->v0 = value0;
      gsk_gl_uniform_info_changed (info, location, stamp);
    }
}

static inline void
gsk_gl_uniform_state_set2f (GskGLUniformState   *state,
                            GskGLUniformProgram *program,
                            guint                location,
                            guint                stamp,
                            float                value0,
                            float                value1)
{
  Uniform2f *u;
  GskGLUniformInfoElement *info;

  g_assert (state != NULL);
  g_assert (program != NULL);

  if ((u = gsk_gl_uniform_state_get_value (state, program, GSK_GL_UNIFORM_FORMAT_2F, 1, location, stamp, &info)))
    {
      GSK_GL_UNIFORM_STATE_REPLACE (info, u, Uniform2f, 1);
      u->v0 = value0;
      u->v1 = value1;
      gsk_gl_uniform_info_changed (info, location, stamp);
    }
}

static inline void
gsk_gl_uniform_state_set3f (GskGLUniformState   *state,
                            GskGLUniformProgram *program,
                            guint                location,
                            guint                stamp,
                            float                value0,
                            float                value1,
                            float                value2)
{
  Uniform3f *u;
  GskGLUniformInfoElement *info;

  g_assert (state != NULL);
  g_assert (program != NULL);

  if ((u = gsk_gl_uniform_state_get_value (state, program, GSK_GL_UNIFORM_FORMAT_3F, 1, location, stamp, &info)))
    {
      GSK_GL_UNIFORM_STATE_REPLACE (info, u, Uniform3f, 1);
      u->v0 = value0;
      u->v1 = value1;
      u->v2 = value2;
      gsk_gl_uniform_info_changed (info, location, stamp);
    }
}

static inline void
gsk_gl_uniform_state_set4f (GskGLUniformState   *state,
                            GskGLUniformProgram *program,
                            guint                location,
                            guint                stamp,
                            float                value0,
                            float                value1,
                            float                value2,
                            float                value3)
{
  Uniform4f *u;
  GskGLUniformInfoElement *info;

  g_assert (state != NULL);
  g_assert (program != NULL);

  if ((u = gsk_gl_uniform_state_get_value (state, program, GSK_GL_UNIFORM_FORMAT_4F, 1, location, stamp, &info)))
    {
      GSK_GL_UNIFORM_STATE_REPLACE (info, u, Uniform4f, 1);
      u->v0 = value0;
      u->v1 = value1;
      u->v2 = value2;
      u->v3 = value3;
      gsk_gl_uniform_info_changed (info, location, stamp);
    }
}

static inline void
gsk_gl_uniform_state_set1ui (GskGLUniformState   *state,
                             GskGLUniformProgram *program,
                             guint                location,
                             guint                stamp,
                             guint                value0)
{
  Uniform1ui *u;
  GskGLUniformInfoElement *info;

  g_assert (state != NULL);
  g_assert (program != NULL);

  if ((u = gsk_gl_uniform_state_get_value (state, program, GSK_GL_UNIFORM_FORMAT_1UI, 1, location, stamp, &info)))
    {
      GSK_GL_UNIFORM_STATE_REPLACE (info, u, Uniform1ui, 1);
      u->v0 = value0;
      gsk_gl_uniform_info_changed (info, location, stamp);
    }
}

static inline void
gsk_gl_uniform_state_set1i (GskGLUniformState   *state,
                            GskGLUniformProgram *program,
                            guint                location,
                            guint                stamp,
                            int                  value0)
{
  Uniform1i *u;
  GskGLUniformInfoElement *info;

  g_assert (state != NULL);
  g_assert (program != NULL);

  if ((u = gsk_gl_uniform_state_get_value (state, program, GSK_GL_UNIFORM_FORMAT_1I, 1, location, stamp, &info)))
    {
      GSK_GL_UNIFORM_STATE_REPLACE (info, u, Uniform1i, 1);
      u->v0 = value0;
      gsk_gl_uniform_info_changed (info, location, stamp);
    }
}

static inline void
gsk_gl_uniform_state_set2i (GskGLUniformState   *state,
                            GskGLUniformProgram *program,
                            guint                location,
                            guint                stamp,
                            int                  value0,
                            int                  value1)
{
  Uniform2i *u;
  GskGLUniformInfoElement *info;

  g_assert (state != NULL);
  g_assert (program != NULL);

  if ((u = gsk_gl_uniform_state_get_value (state, program, GSK_GL_UNIFORM_FORMAT_2I, 1, location, stamp, &info)))
    {
      GSK_GL_UNIFORM_STATE_REPLACE (info, u, Uniform2i, 1);
      u->v0 = value0;
      u->v1 = value1;
      gsk_gl_uniform_info_changed (info, location, stamp);
    }
}

static inline void
gsk_gl_uniform_state_set3i (GskGLUniformState   *state,
                            GskGLUniformProgram *program,
                            guint                location,
                            guint                stamp,
                            int                  value0,
                            int                  value1,
                            int                  value2)
{
  Uniform3i *u;
  GskGLUniformInfoElement *info;

  g_assert (state != NULL);
  g_assert (program != NULL);

  if ((u = gsk_gl_uniform_state_get_value (state, program, GSK_GL_UNIFORM_FORMAT_3I, 1, location, stamp, &info)))
    {
      GSK_GL_UNIFORM_STATE_REPLACE (info, u, Uniform3i, 1);
      u->v0 = value0;
      u->v1 = value1;
      u->v2 = value2;
      gsk_gl_uniform_info_changed (info, location, stamp);
    }
}

static inline void
gsk_gl_uniform_state_set4i (GskGLUniformState   *state,
                            GskGLUniformProgram *program,
                            guint                location,
                            guint                stamp,
                            int                  value0,
                            int                  value1,
                            int                  value2,
                            int                  value3)
{
  Uniform4i *u;
  GskGLUniformInfoElement *info;

  g_assert (state != NULL);
  g_assert (program != NULL);

  if ((u = gsk_gl_uniform_state_get_value (state, program, GSK_GL_UNIFORM_FORMAT_4I, 1, location, stamp, &info)))
    {
      GSK_GL_UNIFORM_STATE_REPLACE (info, u, Uniform4i, 1);
      u->v0 = value0;
      u->v1 = value1;
      u->v2 = value2;
      u->v3 = value3;
      gsk_gl_uniform_info_changed (info, location, stamp);
    }
}

static inline void
gsk_gl_uniform_state_set_rounded_rect (GskGLUniformState    *state,
                                       GskGLUniformProgram  *program,
                                       guint                 location,
                                       guint                 stamp,
                                       const GskRoundedRect *rounded_rect)
{
  GskRoundedRect *u;
  GskGLUniformInfoElement *info;

  g_assert (state != NULL);
  g_assert (program != NULL);
  g_assert (rounded_rect != NULL);

  if ((u = gsk_gl_uniform_state_get_value (state, program, GSK_GL_UNIFORM_FORMAT_ROUNDED_RECT, 1, location, stamp, &info)))
    {
      GSK_GL_UNIFORM_STATE_REPLACE (info, u, GskRoundedRect, 1);
      memcpy (u, rounded_rect, sizeof *rounded_rect);
      gsk_gl_uniform_info_changed (info, location, stamp);
    }
}

static inline void
gsk_gl_uniform_state_set_matrix (GskGLUniformState       *state,
                                 GskGLUniformProgram     *program,
                                 guint                    location,
                                 guint                    stamp,
                                 const graphene_matrix_t *matrix)
{
  graphene_matrix_t *u;
  GskGLUniformInfoElement *info;

  g_assert (state != NULL);
  g_assert (program != NULL);
  g_assert (matrix != NULL);

  if ((u = gsk_gl_uniform_state_get_value (state, program, GSK_GL_UNIFORM_FORMAT_MATRIX, 1, location, stamp, &info)))
    {
      GSK_GL_UNIFORM_STATE_REPLACE (info, u, graphene_matrix_t, 1);
      memcpy (u, matrix, sizeof *matrix);
      gsk_gl_uniform_info_changed (info, location, stamp);
    }
}

/**
 * gsk_gl_uniform_state_set_texture:
 * @state: a #GskGLUniformState
 * @program: the program id
 * @location: the location of the texture
 * @texture_slot: a texturing slot such as GL_TEXTURE0
 *
 * Sets the uniform expecting a texture to @texture_slot. This API
 * expects a texture slot such as GL_TEXTURE0 to reduce chances of
 * miss-use by the caller.
 *
 * The value stored to the uniform is in the form of 0 for GL_TEXTURE0,
 * 1 for GL_TEXTURE1, and so on.
 */
static inline void
gsk_gl_uniform_state_set_texture (GskGLUniformState   *state,
                                  GskGLUniformProgram *program,
                                  guint                location,
                                  guint                stamp,
                                  guint                texture_slot)
{
  GskGLUniformInfoElement *info;
  guint *u;

  g_assert (texture_slot >= GL_TEXTURE0);
  g_assert (texture_slot < GL_TEXTURE16);

  texture_slot -= GL_TEXTURE0;

  if ((u = gsk_gl_uniform_state_get_value (state, program, GSK_GL_UNIFORM_FORMAT_TEXTURE, 1, location, stamp, &info)))
    {
      GSK_GL_UNIFORM_STATE_REPLACE (info, u, guint, 1);
      *u = texture_slot;
      gsk_gl_uniform_info_changed (info, location, stamp);
    }
}

/**
 * gsk_gl_uniform_state_set_color:
 * @state: a #GskGLUniformState
 * @program: a program id > 0
 * @location: the uniform location
 * @color: a color to set or %NULL for transparent
 *
 * Sets a uniform to the color described by @color. This is a convenience
 * function to allow callers to avoid having to translate colors to floats
 * in other portions of the renderer.
 */
static inline void
gsk_gl_uniform_state_set_color (GskGLUniformState   *state,
                                GskGLUniformProgram *program,
                                guint                location,
                                guint                stamp,
                                const GdkRGBA       *color)
{
  static const GdkRGBA transparent = {0};
  GskGLUniformInfoElement *info;
  GdkRGBA *u;

  g_assert (state != NULL);
  g_assert (program != NULL);

  if ((u = gsk_gl_uniform_state_get_value (state, program, GSK_GL_UNIFORM_FORMAT_COLOR, 1, location, stamp, &info)))
    {
      if (color == NULL)
        color = &transparent;

      GSK_GL_UNIFORM_STATE_REPLACE (info, u, GdkRGBA, 1);
      memcpy (u, color, sizeof *color);
      gsk_gl_uniform_info_changed (info, location, stamp);
    }
}

static inline void
gsk_gl_uniform_state_set1fv (GskGLUniformState   *state,
                             GskGLUniformProgram *program,
                             guint                location,
                             guint                stamp,
                             guint                count,
                             const float         *value)
{
  Uniform1f *u;
  GskGLUniformInfoElement *info;

  g_assert (state != NULL);
  g_assert (program != NULL);
  g_assert (count > 0);

  if ((u = gsk_gl_uniform_state_get_value (state, program, GSK_GL_UNIFORM_FORMAT_1FV, count, location, stamp, &info)))
    {
      GSK_GL_UNIFORM_STATE_REPLACE (info, u, Uniform1f, count);
      memcpy (u, value, sizeof (Uniform1f) * count);
      gsk_gl_uniform_info_changed (info, location, stamp);
    }
}

static inline void
gsk_gl_uniform_state_set2fv (GskGLUniformState   *state,
                             GskGLUniformProgram *program,
                             guint                location,
                             guint                stamp,
                             guint                count,
                             const float         *value)
{
  Uniform2f *u;
  GskGLUniformInfoElement *info;

  g_assert (state != NULL);
  g_assert (program != NULL);
  g_assert (count > 0);

  if ((u = gsk_gl_uniform_state_get_value (state, program, GSK_GL_UNIFORM_FORMAT_2FV, count, location, stamp, &info)))
    {
      GSK_GL_UNIFORM_STATE_REPLACE (info, u, Uniform2f, count);
      memcpy (u, value, sizeof (Uniform2f) * count);
      gsk_gl_uniform_info_changed (info, location, stamp);
    }
}

static inline void
gsk_gl_uniform_state_set3fv (GskGLUniformState   *state,
                             GskGLUniformProgram *program,
                             guint                location,
                             guint                stamp,
                             guint                count,
                             const float         *value)
{
  Uniform3f *u;
  GskGLUniformInfoElement *info;

  g_assert (state != NULL);
  g_assert (program != NULL);
  g_assert (count > 0);

  if ((u = gsk_gl_uniform_state_get_value (state, program, GSK_GL_UNIFORM_FORMAT_3FV, count, location, stamp, &info)))
    {
      GSK_GL_UNIFORM_STATE_REPLACE (info, u, Uniform3f, count);
      memcpy (u, value, sizeof (Uniform3f) * count);
      gsk_gl_uniform_info_changed (info, location, stamp);
    }
}

static inline void
gsk_gl_uniform_state_set4fv (GskGLUniformState   *state,
                             GskGLUniformProgram *program,
                             guint                location,
                             guint                stamp,
                             guint                count,
                             const float         *value)
{
  Uniform4f *u;
  GskGLUniformInfoElement *info;

  g_assert (state != NULL);
  g_assert (program != NULL);
  g_assert (count > 0);

  if ((u = gsk_gl_uniform_state_get_value (state, program, GSK_GL_UNIFORM_FORMAT_4FV, count, location, stamp, &info)))
    {
      GSK_GL_UNIFORM_STATE_REPLACE (info, u, Uniform4f, count);
      memcpy (u, value, sizeof (Uniform4f) * count);
      gsk_gl_uniform_info_changed (info, location, stamp);
    }
}

G_END_DECLS

#endif /* GSK_GL_UNIFORM_STATE_PRIVATE_H */
