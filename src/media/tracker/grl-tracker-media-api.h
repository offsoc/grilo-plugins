/*
 * Copyright (C) 2011 Igalia S.L.
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

#ifndef _GRL_TRACKER_MEDIA_API_H_
#define _GRL_TRACKER_MEDIA_API_H_

#include "grl-tracker-media.h"

/**/

void grl_tracker_media_init_requests (void);

const GList *grl_tracker_media_writable_keys (GrlMetadataSource *source);

void grl_tracker_media_query (GrlMediaSource *source,
                              GrlMediaSourceQuerySpec *qs);

void grl_tracker_media_metadata (GrlMediaSource *source,
                                 GrlMediaSourceMetadataSpec *ms);

void grl_tracker_media_set_metadata (GrlMetadataSource *source,
                                     GrlMetadataSourceSetMetadataSpec *sms);

void grl_tracker_media_cancel (GrlMetadataSource *source, guint operation_id);

void grl_tracker_media_search (GrlMediaSource *source,
                               GrlMediaSourceSearchSpec *ss);

void grl_tracker_media_browse (GrlMediaSource *source,
                               GrlMediaSourceBrowseSpec *bs);

gboolean grl_tracker_media_change_start (GrlMediaSource *source,
                                         GError **error);

gboolean grl_tracker_media_change_stop (GrlMediaSource *source,
                                        GError **error);

#endif /* _GRL_TRACKER_MEDIA_API_H_ */
