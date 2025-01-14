/*
 * Copyright (C) 2011 Intel Corporation.
 *
 * Contact: Iago Toral Quiroga <itoral@igalia.com>
 *
 * Authors: Lionel Landwerlin <lionel.g.landwerlin@linux.intel.com>
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

#ifndef _GRL_TRACKER_MEDIA_CACHE_H_
#define _GRL_TRACKER_MEDIA_CACHE_H_

#include "grl-tracker-media.h"

typedef struct _GrlTrackerCache GrlTrackerCache;

GrlTrackerCache *grl_tracker_media_cache_new (gsize size);

void grl_tracker_media_cache_free (GrlTrackerCache *cache);

void grl_tracker_media_cache_add_item (GrlTrackerCache *cache,
                                       guint id,
                                       GrlTrackerMedia *source);
void grl_tracker_media_cache_del_source (GrlTrackerCache *cache,
                                         GrlTrackerMedia *source);

GrlTrackerMedia *grl_tracker_media_cache_get_source (GrlTrackerCache *cache,
                                                     guint id);

#endif /* _GRL_TRACKER_MEDIA_CACHE_H_ */
