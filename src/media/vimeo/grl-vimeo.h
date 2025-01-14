/*
 * Copyright (C) 2010 Igalia S.L.
 *
 * Contact: Iago Toral Quiroga <itoral@igalia.com>
 *
 * Authors: Joaquim Rocha <jrocha@igalia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef _GRL_VIMEO_SOURCE_H_
#define _GRL_VIMEO_SOURCE_H_

#include <grilo.h>

#define GRL_VIMEO_SOURCE_TYPE                  \
  (grl_vimeo_source_get_type ())

#define GRL_VIMEO_SOURCE(obj)                          \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                   \
                               GRL_VIMEO_SOURCE_TYPE,  \
                               GrlVimeoSource))

#define GRL_IS_VIMEO_SOURCE(obj)                       \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                   \
                               GRL_VIMEO_SOURCE_TYPE))

#define GRL_VIMEO_SOURCE_CLASS(klass)                  \
  (G_TYPE_CHECK_CLASS_CAST((klass),                     \
                           GRL_VIMEO_SOURCE_TYPE,      \
                           GrlVimeoSourceClass))

#define GRL_IS_VIMEO_SOURCE_CLASS(klass)               \
  (G_TYPE_CHECK_CLASS_TYPE((klass),                     \
                           GRL_VIMEO_SOURCE_TYPE))

#define GRL_VIMEO_SOURCE_GET_CLASS(obj)                \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                    \
                              GRL_VIMEO_SOURCE_TYPE,   \
                              GrlVimeoSourceClass))

typedef struct _GrlVimeoSource        GrlVimeoSource;
typedef struct _GrlVimeoSourcePrivate GrlVimeoSourcePrivate;

struct _GrlVimeoSource {

  GrlMediaSource parent;

  /*< private >*/
  GrlVimeoSourcePrivate *priv;
};

typedef struct _GrlVimeoSourceClass GrlVimeoSourceClass;

struct _GrlVimeoSourceClass {

  GrlMediaSourceClass parent_class;

};

GType grl_vimeo_source_get_type (void);

#endif /* _GRL_VIMEO_SOURCE_H_ */
