/*
 * Copyright (C) 2010, 2011 Igalia S.L.
 * Copyright (C) 2011 Intel Corporation.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <grilo.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "grl-vimeo.h"
#include "gvimeo.h"

#define GRL_VIMEO_SOURCE_GET_PRIVATE(object)                           \
  (G_TYPE_INSTANCE_GET_PRIVATE((object),                                \
                               GRL_VIMEO_SOURCE_TYPE,                  \
                               GrlVimeoSourcePrivate))

/* --------- Logging  -------- */

#define GRL_LOG_DOMAIN_DEFAULT vimeo_log_domain
GRL_LOG_DOMAIN_STATIC(vimeo_log_domain);

/* --- Plugin information --- */

#define PLUGIN_ID   VIMEO_PLUGIN_ID

#define SOURCE_ID   "grl-vimeo"
#define SOURCE_NAME "Vimeo"
#define SOURCE_DESC "A source for browsing and searching Vimeo videos"

typedef struct {
  GrlMediaSourceSearchSpec *ss;
  gint offset;
  gint page;
} SearchData;

struct _GrlVimeoSourcePrivate {
  GVimeo *vimeo;
};

static GrlVimeoSource *grl_vimeo_source_new (void);

gboolean grl_vimeo_plugin_init (GrlPluginRegistry *registry,
				const GrlPluginInfo *plugin,
				GList *configs);

static const GList *grl_vimeo_source_supported_keys (GrlMetadataSource *source);

static void grl_vimeo_source_metadata (GrlMediaSource *source,
				       GrlMediaSourceMetadataSpec *ss);

static void grl_vimeo_source_search (GrlMediaSource *source,
				     GrlMediaSourceSearchSpec *ss);

/* =================== Vimeo Plugin  =============== */

gboolean
grl_vimeo_plugin_init (GrlPluginRegistry *registry,
                        const GrlPluginInfo *plugin,
                        GList *configs)
{
  gchar *vimeo_key;
  gchar *vimeo_secret;
  GrlConfig *config;
  gint config_count;
  gboolean init_result = FALSE;
  GrlVimeoSource *source;

  GRL_LOG_DOMAIN_INIT (vimeo_log_domain, "vimeo");

  GRL_DEBUG ("vimeo_plugin_init");

#if !GLIB_CHECK_VERSION(2,32,0)
  if (!g_thread_supported ()) {
    g_thread_init (NULL);
  }
#endif

  if (!configs) {
    GRL_INFO ("Configuration not provided! Plugin not loaded");
    return FALSE;
  }

  config_count = g_list_length (configs);
  if (config_count > 1) {
    GRL_INFO ("Provided %d configs, but will only use one", config_count);
  }

  config = GRL_CONFIG (configs->data);

  vimeo_key = grl_config_get_api_key (config);
  vimeo_secret = grl_config_get_api_secret (config);

  if (!vimeo_key || !vimeo_secret) {
    GRL_INFO ("Required API key or secret configuration not provided."
              " Plugin not loaded");
    goto go_out;
  }

  source = grl_vimeo_source_new ();
  source->priv->vimeo = g_vimeo_new (vimeo_key, vimeo_secret);

  grl_plugin_registry_register_source (registry,
                                       plugin,
                                       GRL_MEDIA_PLUGIN (source),
                                       NULL);
  init_result = TRUE;

 go_out:

  if (vimeo_key != NULL)
    g_free (vimeo_key);
  if (vimeo_secret != NULL)
    g_free (vimeo_secret);

  return init_result;
}

GRL_PLUGIN_REGISTER (grl_vimeo_plugin_init,
                     NULL,
                     PLUGIN_ID);

/* ================== Vimeo GObject ================ */

static GrlVimeoSource *
grl_vimeo_source_new (void)
{
  GRL_DEBUG ("grl_vimeo_source_new");

  return g_object_new (GRL_VIMEO_SOURCE_TYPE,
                       "source-id", SOURCE_ID,
                       "source-name", SOURCE_NAME,
                       "source-desc", SOURCE_DESC,
                       NULL);
}

static void
grl_vimeo_source_class_init (GrlVimeoSourceClass * klass)
{
  GrlMediaSourceClass *source_class = GRL_MEDIA_SOURCE_CLASS (klass);
  GrlMetadataSourceClass *metadata_class = GRL_METADATA_SOURCE_CLASS (klass);

  source_class->metadata = grl_vimeo_source_metadata;
  source_class->search = grl_vimeo_source_search;
  metadata_class->supported_keys = grl_vimeo_source_supported_keys;

  g_type_class_add_private (klass, sizeof (GrlVimeoSourcePrivate));
}

static void
grl_vimeo_source_init (GrlVimeoSource *source)
{
  source->priv = GRL_VIMEO_SOURCE_GET_PRIVATE (source);
}

G_DEFINE_TYPE (GrlVimeoSource, grl_vimeo_source, GRL_TYPE_MEDIA_SOURCE);

/* ======================= Utilities ==================== */

static gint
str_to_gint (gchar *str)
{
  gint number;

  errno = 0;
  number = (gint) g_ascii_strtod (str, NULL);
  if (errno == 0)
  {
    return number;
  }
  return 0;
}

static gchar *
str_to_iso8601 (gchar *str)
{
  gchar **date;
  gchar *iso8601_date;

  date = g_strsplit (str, " ", -1);
  if (date[0]) {
    if (date[1]) {
      iso8601_date = g_strdup_printf ("%sT%sZ", date[0], date[1]);
    } else {
      iso8601_date = g_strdup_printf ("%sT", date[0]);
    }
  } else {
    iso8601_date = NULL;
  }
  g_strfreev (date);

  return iso8601_date;
}


static void
update_media (GrlMedia *media, GHashTable *video)
{
  gchar *str;

  str = g_hash_table_lookup (video, VIMEO_VIDEO_ID);
  if (str)
  {
    grl_media_set_id (media, str);
  }

  str = g_hash_table_lookup (video, VIMEO_VIDEO_TITLE);
  if (str)
  {
    grl_media_set_title (media, str);
  }

  str = g_hash_table_lookup (video, VIMEO_VIDEO_DESCRIPTION);
  if (str)
  {
    grl_media_set_description (media, str);
  }

  str = g_hash_table_lookup (video, VIMEO_VIDEO_DURATION);
  if (str)
  {
    grl_media_set_duration (media, str_to_gint (str));
  }

  str = g_hash_table_lookup (video, VIMEO_VIDEO_OWNER_NAME);
  if (str)
  {
    grl_media_set_author (media, str);
  }

  str = g_hash_table_lookup (video, VIMEO_VIDEO_UPLOAD_DATE);
  if (str)
  {
    gchar *date = str_to_iso8601(str);
    if (date) {
      grl_media_set_date (media, date);
      g_free (date);
    }
  }

  str = g_hash_table_lookup (video, VIMEO_VIDEO_THUMBNAIL);
  if (str)
  {
    grl_media_set_thumbnail (media, str);
  }

  str = g_hash_table_lookup (video, VIMEO_VIDEO_WIDTH);
  if (str)
  {
    grl_media_video_set_width (GRL_MEDIA_VIDEO (media), str_to_gint (str));
  }

  str = g_hash_table_lookup (video, VIMEO_VIDEO_HEIGHT);
  if (str)
  {
    grl_media_video_set_height (GRL_MEDIA_VIDEO (media), str_to_gint (str));
  }
}


static void
search_cb (GVimeo *vimeo, GList *video_list, gpointer user_data)
{
  GrlMedia *media = NULL;
  SearchData *sd = (SearchData *) user_data;
  gint count = sd->ss->count;
  gchar *media_type;

  /* Go to offset element */
  video_list = g_list_nth (video_list, sd->offset);

  /* No more elements can be sent */
  if (!video_list) {
    sd->ss->callback (sd->ss->source,
                      sd->ss->search_id,
                      NULL,
                      0,
                      sd->ss->user_data,
                      NULL);
    g_slice_free (SearchData, sd);
    return;
  }

  while (video_list && count)
  {
    media_type = g_hash_table_lookup (video_list->data, "title");
    if (media_type) {
      media = grl_media_video_new ();
    }

    if (media)
    {
      update_media (media, video_list->data);
      sd->ss->callback (sd->ss->source,
			sd->ss->search_id,
			media,
			sd->ss->count == 1? 0: -1,
			sd->ss->user_data,
			NULL);
    }
    video_list = g_list_next (video_list);

    if (--count)
      sd->ss->count = count;

    media = NULL;
  }

  /* Get more elements */
  if (count)
  {
    sd->offset = 0;
    sd->page++;
    g_vimeo_videos_search (vimeo, sd->ss->text, sd->page, search_cb, sd);
  }
  else
  {
    g_slice_free (SearchData, sd);
  }
}

static void
video_get_play_url_cb (gchar *url, gpointer user_data)
{
  GrlMediaSourceMetadataSpec *ms = (GrlMediaSourceMetadataSpec *) user_data;

  if (url)
  {
    grl_media_set_url (ms->media, url);
  }

  ms->callback (ms->source, ms->metadata_id, ms->media, ms->user_data, NULL);
}

/* ================== API Implementation ================ */

static const GList *
grl_vimeo_source_supported_keys (GrlMetadataSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_ID,
				      GRL_METADATA_KEY_TITLE,
				      GRL_METADATA_KEY_DESCRIPTION,
				      GRL_METADATA_KEY_URL,
				      GRL_METADATA_KEY_AUTHOR,
                                      GRL_METADATA_KEY_DATE,
				      GRL_METADATA_KEY_THUMBNAIL,
				      GRL_METADATA_KEY_DURATION,
				      GRL_METADATA_KEY_WIDTH,
				      GRL_METADATA_KEY_HEIGHT,
				      NULL);
  }
  return keys;
}

static void
grl_vimeo_source_metadata (GrlMediaSource *source,
			   GrlMediaSourceMetadataSpec *ms)
{
  gint id;
  const gchar *id_str;

  if (!ms->media || (id_str = grl_media_get_id (ms->media)) == NULL)
  {
    ms->callback (ms->source, ms->metadata_id, ms->media, ms->user_data, NULL);
    return;
  }

  errno = 0;
  id = (gint) g_ascii_strtod (id_str, NULL);
  if (errno != 0)
  {
    return;
  }

  g_vimeo_video_get_play_url (GRL_VIMEO_SOURCE (source)->priv->vimeo,
			      id,
			      video_get_play_url_cb,
			      ms);
}

static void
grl_vimeo_source_search (GrlMediaSource *source,
			 GrlMediaSourceSearchSpec *ss)
{
  SearchData *sd;
  GError *error;
  gint per_page;
  GVimeo *vimeo = GRL_VIMEO_SOURCE (source)->priv->vimeo;

  if (!ss->text) {
    /* Vimeo does not support searching all */
    error =
      g_error_new_literal (GRL_CORE_ERROR,
                           GRL_CORE_ERROR_SEARCH_NULL_UNSUPPORTED,
                           "Unable to execute search: non NULL search text is required");
    ss->callback (ss->source, ss->search_id, NULL, 0, ss->user_data, error);
    g_error_free (error);
    return;
  }

  /* Compute items per page and page offset */
  per_page = CLAMP (1 + ss->skip + ss->count, 0, 100);
  g_vimeo_set_per_page (vimeo, per_page);

  sd = g_slice_new (SearchData);
  sd->page = 1 + (ss->skip / per_page);
  sd->offset = ss->skip % per_page;
  sd->ss = ss;

  g_vimeo_videos_search (vimeo, ss->text, sd->page, search_cb, sd);
}
