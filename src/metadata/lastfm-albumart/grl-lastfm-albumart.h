/*
 * Copyright (C) 2010 Igalia S.L.
 *
 * Contact: Iago Toral Quiroga <itoral@igalia.com>
 *
 * Authors: Juan A. Suarez Romero <jasuarez@igalia.com>
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

#ifndef _GRL_LASTFM_ALBUMART_SOURCE_H_
#define _GRL_LASTFM_ALBUMART_SOURCE_H_

#include <grilo.h>

#define GRL_LASTFM_ALBUMART_SOURCE_TYPE         \
  (grl_lastfm_albumart_source_get_type ())

#define GRL_LASTFM_ALBUMART_SOURCE(obj)                         \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                           \
                               GRL_LASTFM_ALBUMART_SOURCE_TYPE, \
                               GrlLastfmAlbumartSource))

#define GRL_IS_LASTFM_ALBUMART_SOURCE(obj)                              \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                                   \
                               GRL_LASTFM_ALBUMART_SOURCE_TYPE))

#define GRL_LASTFM_ALBUMART_SOURCE_CLASS(klass)                 \
  (G_TYPE_CHECK_CLASS_CAST((klass),                             \
                           GRL_LASTFM_ALBUMART_SOURCE_TYPE,     \
                           GrlLastfmAlbumartSourceClass))

#define GRL_IS_LASTFM_ALBUMART_SOURCE_CLASS(klass)              \
  (G_TYPE_CHECK_CLASS_TYPE((klass),                             \
                           GRL_LASTFM_ALBUMART_SOURCE_TYPE))

#define GRL_LASTFM_ALBUMART_SOURCE_GET_CLASS(obj)                       \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                                    \
                              GRL_LASTFM_ALBUMART_SOURCE_TYPE,          \
                              GrlLastfmAlbumartSourceClass))

typedef struct _GrlLastfmAlbumartSource GrlLastfmAlbumartSource;

struct _GrlLastfmAlbumartSource {

  GrlMetadataSource parent;

};

typedef struct _GrlLastfmAlbumartSourceClass GrlLastfmAlbumartSourceClass;

struct _GrlLastfmAlbumartSourceClass {

  GrlMetadataSourceClass parent_class;

};

GType grl_lastfm_albumart_source_get_type (void);

#endif /* _GRL_LASTFM_ALBUMART_SOURCE_H_ */
