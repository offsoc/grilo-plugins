/*
 * Copyright (C) 2010, 2011 Igalia S.L.
 * Copyright (C) 2011 Intel Corporation.
 *
 * Contact: Iago Toral Quiroga <itoral@igalia.com>
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
#include <net/grl-net.h>
#include <gdata/gdata.h>
#include <quvi/quvi.h>
#include <string.h>

#include "grl-youtube.h"

enum {
  PROP_0,
  PROP_SERVICE,
};

#define GRL_YOUTUBE_SOURCE_GET_PRIVATE(object)            \
  (G_TYPE_INSTANCE_GET_PRIVATE((object),                  \
                               GRL_YOUTUBE_SOURCE_TYPE,   \
                               GrlYoutubeSourcePriv))

/* --------- Logging  -------- */

#define GRL_LOG_DOMAIN_DEFAULT youtube_log_domain
GRL_LOG_DOMAIN_STATIC(youtube_log_domain);

/* ----- Root categories ---- */

#define YOUTUBE_ROOT_NAME       "Youtube"

#define ROOT_DIR_FEEDS_INDEX      0
#define ROOT_DIR_CATEGORIES_INDEX 1

#define YOUTUBE_FEEDS_ID        "standard-feeds"
#define YOUTUBE_FEEDS_NAME      "Standard feeds"

#define YOUTUBE_CATEGORIES_ID   "categories"
#define YOUTUBE_CATEGORIES_NAME "Categories"
#define YOUTUBE_CATEGORIES_URL  "http://gdata.youtube.com/schemas/2007/categories.cat"

/* ----- Feeds categories ---- */

#define YOUTUBE_TOP_RATED_ID         (YOUTUBE_FEEDS_ID "/0")
#define YOUTUBE_TOP_RATED_NAME       "Top Rated"

#define YOUTUBE_TOP_FAVS_ID          (YOUTUBE_FEEDS_ID "/1")
#define YOUTUBE_TOP_FAVS_NAME        "Top Favorites"

#define YOUTUBE_MOST_VIEWED_ID       (YOUTUBE_FEEDS_ID "/2")
#define YOUTUBE_MOST_VIEWED_NAME     "Most Viewed"

#define YOUTUBE_MOST_POPULAR_ID      (YOUTUBE_FEEDS_ID "/3")
#define YOUTUBE_MOST_POPULAR_NAME    "Most Popular"

#define YOUTUBE_MOST_RECENT_ID       (YOUTUBE_FEEDS_ID "/4")
#define YOUTUBE_MOST_RECENT_NAME     "Most Recent"

#define YOUTUBE_MOST_DISCUSSED_ID    (YOUTUBE_FEEDS_ID "/5")
#define YOUTUBE_MOST_DISCUSSED_NAME  "Most Discussed"

#define YOUTUBE_MOST_LINKED_ID       (YOUTUBE_FEEDS_ID "/6")
#define YOUTUBE_MOST_LINKED_NAME     "Most Linked"

#define YOUTUBE_MOST_RESPONDED_ID    (YOUTUBE_FEEDS_ID "/7")
#define YOUTUBE_MOST_RESPONDED_NAME  "Most Responded"

#define YOUTUBE_FEATURED_ID          (YOUTUBE_FEEDS_ID "/8")
#define YOUTUBE_FEATURED_NAME        "Recently Featured"

#define YOUTUBE_MOBILE_ID            (YOUTUBE_FEEDS_ID "/9")
#define YOUTUBE_MOBILE_NAME          "Watch On Mobile"

/* --- Other --- */

#define YOUTUBE_MAX_CHUNK       50

#define YOUTUBE_VIDEO_INFO_URL  "http://www.youtube.com/get_video_info?video_id=%s"
#define YOUTUBE_VIDEO_URL       "http://www.youtube.com/get_video?video_id=%s&t=%s&asv="
#define YOUTUBE_CATEGORY_URL    "http://gdata.youtube.com/feeds/api/videos/-/%s?&start-index=%s&max-results=%s"
#define YOUTUBE_WATCH_URL       "http://www.youtube.com/watch?v="

#define YOUTUBE_VIDEO_MIME      "application/x-shockwave-flash"
#define YOUTUBE_SITE_URL        "www.youtube.com"


/* --- Plugin information --- */

#define PLUGIN_ID   YOUTUBE_PLUGIN_ID

#define SOURCE_ID   "grl-youtube"
#define SOURCE_NAME "Youtube"
#define SOURCE_DESC "A source for browsing and searching Youtube videos"

/* --- Data types --- */

typedef void (*AsyncReadCbFunc) (gchar *data, gpointer user_data);

typedef void (*BuildMediaFromEntryCbFunc) (GrlMedia *media, gpointer user_data);

typedef struct {
  gchar *id;
  gchar *name;
  guint count;
} CategoryInfo;

typedef struct {
  GrlMediaSource *source;
  GCancellable *cancellable;
  guint operation_id;
  const gchar *container_id;
  GList *keys;
  GrlMetadataResolutionFlags flags;
  guint skip;
  guint count;
  GrlMediaSourceResultCb callback;
  gpointer user_data;
  guint error_code;
  CategoryInfo *category_info;
  guint emitted;
  guint matches;
  guint ref_count;
} OperationSpec;

typedef struct {
  AsyncReadCbFunc callback;
  gchar *url;
  gpointer user_data;
} AsyncReadCb;

typedef struct {
  GrlMedia *media;
  GCancellable *cancellable;
  BuildMediaFromEntryCbFunc callback;
  gpointer user_data;
} SetMediaUrlAsyncReadCb;

typedef enum {
  YOUTUBE_MEDIA_TYPE_ROOT,
  YOUTUBE_MEDIA_TYPE_FEEDS,
  YOUTUBE_MEDIA_TYPE_CATEGORIES,
  YOUTUBE_MEDIA_TYPE_FEED,
  YOUTUBE_MEDIA_TYPE_CATEGORY,
  YOUTUBE_MEDIA_TYPE_VIDEO,
} YoutubeMediaType;

struct _GrlYoutubeSourcePriv {
  GDataService *service;
  quvi_t quvi_handle;

  GrlNetWc *wc;
};

#define YOUTUBE_CLIENT_ID "grilo"

static GrlYoutubeSource *grl_youtube_source_new (const gchar *api_key,
						 const gchar *client_id);

static void grl_youtube_source_set_property (GObject *object,
                                             guint propid,
                                             const GValue *value,
                                             GParamSpec *pspec);
static void grl_youtube_source_finalize (GObject *object);

gboolean grl_youtube_plugin_init (GrlPluginRegistry *registry,
                                  const GrlPluginInfo *plugin,
                                  GList *configs);

static const GList *grl_youtube_source_supported_keys (GrlMetadataSource *source);

static const GList *grl_youtube_source_slow_keys (GrlMetadataSource *source);

static void grl_youtube_source_search (GrlMediaSource *source,
                                       GrlMediaSourceSearchSpec *ss);

static void grl_youtube_source_browse (GrlMediaSource *source,
                                       GrlMediaSourceBrowseSpec *bs);

static void grl_youtube_source_metadata (GrlMediaSource *source,
                                         GrlMediaSourceMetadataSpec *ms);

static gboolean grl_youtube_test_media_from_uri (GrlMediaSource *source,
						 const gchar *uri);

static void grl_youtube_get_media_from_uri (GrlMediaSource *source,
					    GrlMediaSourceMediaFromUriSpec *mfus);

static void grl_youtube_source_cancel (GrlMetadataSource *source,
                                       guint operation_id);

static void produce_from_directory (CategoryInfo *dir, gint dir_size, OperationSpec *os);

/* ==================== Global Data  ================= */

guint root_dir_size = 2;
CategoryInfo root_dir[] = {
  {YOUTUBE_FEEDS_ID,      YOUTUBE_FEEDS_NAME,      10},
  {YOUTUBE_CATEGORIES_ID, YOUTUBE_CATEGORIES_NAME, -1},
  {NULL, NULL, 0}
};

CategoryInfo feeds_dir[] = {
  {YOUTUBE_TOP_RATED_ID,      YOUTUBE_TOP_RATED_NAME,       -1},
  {YOUTUBE_TOP_FAVS_ID,       YOUTUBE_TOP_FAVS_NAME,        -1},
  {YOUTUBE_MOST_VIEWED_ID,    YOUTUBE_MOST_VIEWED_NAME,     -1},
  {YOUTUBE_MOST_POPULAR_ID,   YOUTUBE_MOST_POPULAR_NAME,    -1},
  {YOUTUBE_MOST_RECENT_ID,    YOUTUBE_MOST_RECENT_NAME,     -1},
  {YOUTUBE_MOST_DISCUSSED_ID, YOUTUBE_MOST_DISCUSSED_NAME,  -1},
  {YOUTUBE_MOST_LINKED_ID,    YOUTUBE_MOST_LINKED_NAME,     -1},
  {YOUTUBE_MOST_RESPONDED_ID, YOUTUBE_MOST_RESPONDED_NAME,  -1},
  {YOUTUBE_FEATURED_ID,       YOUTUBE_FEATURED_NAME,        -1},
  {YOUTUBE_MOBILE_ID,         YOUTUBE_MOBILE_NAME,          -1},
  {NULL, NULL, 0}
};

CategoryInfo *categories_dir = NULL;

static GrlYoutubeSource *ytsrc = NULL;

/* =================== Youtube Plugin  =============== */

gboolean
grl_youtube_plugin_init (GrlPluginRegistry *registry,
                         const GrlPluginInfo *plugin,
                         GList *configs)
{
  gchar *api_key;
  GrlConfig *config;
  gint config_count;
  GrlYoutubeSource *source;

  GRL_LOG_DOMAIN_INIT (youtube_log_domain, "youtube");

  GRL_DEBUG ("youtube_plugin_init");

  if (!configs) {
    GRL_INFO ("Configuration not provided! Plugin not loaded");
    return FALSE;
  }

  config_count = g_list_length (configs);
  if (config_count > 1) {
    GRL_INFO ("Provided %d configs, but will only use one", config_count);
  }

  config = GRL_CONFIG (configs->data);
  api_key = grl_config_get_api_key (config);
  if (!api_key) {
    GRL_INFO ("Missing API Key, cannot load plugin");
    return FALSE;
  }

#if !GLIB_CHECK_VERSION(2,32,0)
  /* libgdata needs this */
  if (!g_thread_supported()) {
    g_thread_init (NULL);
  }
#endif

  source = grl_youtube_source_new (api_key, YOUTUBE_CLIENT_ID);

  grl_plugin_registry_register_source (registry,
                                       plugin,
                                       GRL_MEDIA_PLUGIN (source),
                                       NULL);

  g_free (api_key);

  return TRUE;
}

GRL_PLUGIN_REGISTER (grl_youtube_plugin_init,
                     NULL,
                     PLUGIN_ID);

/* ================== Youtube GObject ================ */

G_DEFINE_TYPE (GrlYoutubeSource, grl_youtube_source, GRL_TYPE_MEDIA_SOURCE);

static GrlYoutubeSource *
grl_youtube_source_new (const gchar *api_key, const gchar *client_id)
{
  GrlYoutubeSource *source;
  GDataYouTubeService *service;

  GRL_DEBUG ("grl_youtube_source_new");

#ifdef HAVE_LIBGDATA_0_9
  service = gdata_youtube_service_new (api_key, NULL);
#else /* HAVE_LIBGDATA_0_9 */
  service = gdata_youtube_service_new (api_key, client_id);
#endif /* !HAVE_LIBGDATA_0_9 */
  if (!service) {
    GRL_WARNING ("Failed to initialize gdata service");
    return NULL;
  }

  /* Use auto-split mode because Youtube fails for queries
     that request more than YOUTUBE_MAX_CHUNK results */
  source = GRL_YOUTUBE_SOURCE (g_object_new (GRL_YOUTUBE_SOURCE_TYPE,
					     "source-id", SOURCE_ID,
					     "source-name", SOURCE_NAME,
					     "source-desc", SOURCE_DESC,
					     "auto-split-threshold",
					     YOUTUBE_MAX_CHUNK,
                                             "yt-service", service,
					     NULL));

  /* Set up quvi */
  if (quvi_init (&(source->priv->quvi_handle)) != QUVI_OK) {
    source->priv->quvi_handle = NULL;
  } else {
    quvi_setopt (source->priv->quvi_handle, QUVIOPT_FORMAT, "mp4_360p");
    quvi_setopt (source->priv->quvi_handle, QUVIOPT_NOVERIFY);
  }

  ytsrc = source;

  return source;
}

static void
grl_youtube_source_class_init (GrlYoutubeSourceClass * klass)
{
  GrlMediaSourceClass *source_class = GRL_MEDIA_SOURCE_CLASS (klass);
  GrlMetadataSourceClass *metadata_class = GRL_METADATA_SOURCE_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  source_class->search = grl_youtube_source_search;
  source_class->browse = grl_youtube_source_browse;
  source_class->metadata = grl_youtube_source_metadata;
  source_class->test_media_from_uri = grl_youtube_test_media_from_uri;
  source_class->media_from_uri = grl_youtube_get_media_from_uri;
  metadata_class->supported_keys = grl_youtube_source_supported_keys;
  metadata_class->slow_keys = grl_youtube_source_slow_keys;
  metadata_class->cancel = grl_youtube_source_cancel;
  gobject_class->set_property = grl_youtube_source_set_property;
  gobject_class->finalize = grl_youtube_source_finalize;

  g_object_class_install_property (gobject_class,
                                   PROP_SERVICE,
                                   g_param_spec_object ("yt-service",
                                                        "youtube-service",
                                                        "gdata youtube service object",
                                                        GDATA_TYPE_YOUTUBE_SERVICE,
                                                        G_PARAM_WRITABLE
                                                        | G_PARAM_CONSTRUCT_ONLY
                                                        | G_PARAM_STATIC_NAME));

  g_type_class_add_private (klass, sizeof (GrlYoutubeSourcePriv));
}

static void
grl_youtube_source_init (GrlYoutubeSource *source)
{
  source->priv = GRL_YOUTUBE_SOURCE_GET_PRIVATE (source);
}

static void
grl_youtube_source_set_property (GObject *object,
                                 guint propid,
                                 const GValue *value,
                                 GParamSpec *pspec)

{
  switch (propid) {
  case PROP_SERVICE: {
    GrlYoutubeSource *self;
    self = GRL_YOUTUBE_SOURCE (object);
    self->priv->service = g_value_get_object (value);
    break;
  }
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, propid, pspec);
  }
}

static void
grl_youtube_source_finalize (GObject *object)
{
  GrlYoutubeSource *self;

  self = GRL_YOUTUBE_SOURCE (object);

  if (self->priv->wc)
    g_object_unref (self->priv->wc);

  if (self->priv->service)
    g_object_unref (self->priv->service);

  if (self->priv->quvi_handle)
    quvi_close (&(self->priv->quvi_handle));

  G_OBJECT_CLASS (grl_youtube_source_parent_class)->finalize (object);
}

/* ======================= Utilities ==================== */

static void
release_operation_data (GrlMetadataSource *source,
                        guint operation_id)
{
  GCancellable *cancellable = grl_operation_get_data (operation_id);

  if (cancellable) {
    g_object_unref (cancellable);
  }
}

static OperationSpec *
operation_spec_new (void)
{
  OperationSpec *os;

  GRL_DEBUG ("Allocating new spec");

  os =  g_slice_new0 (OperationSpec);
  os->ref_count = 1;

  return os;
}

static void
operation_spec_unref (OperationSpec *os)
{
  os->ref_count--;
  if (os->ref_count == 0) {
    if (os->cancellable) {
      g_object_unref (os->cancellable);
    }
    g_slice_free (OperationSpec, os);
    GRL_DEBUG ("freeing spec");
  }
}

static void
operation_spec_ref (OperationSpec *os)
{
  GRL_DEBUG ("Reffing spec");
  os->ref_count++;
}

inline static GrlNetWc *
get_wc (void)
{
  if (ytsrc && !ytsrc->priv->wc)
    ytsrc->priv->wc = grl_net_wc_new ();

  return ytsrc->priv->wc;
}

static void
read_done_cb (GObject *source_object,
              GAsyncResult *res,
              gpointer user_data)
{
  AsyncReadCb *arc = (AsyncReadCb *) user_data;
  GError *wc_error = NULL;
  gchar *content = NULL;

  grl_net_wc_request_finish (GRL_NET_WC (source_object),
                         res,
                         &content,
                         NULL,
                         &wc_error);
  if (wc_error) {
    if (wc_error->code != GRL_NET_WC_ERROR_CANCELLED) {
      GRL_WARNING ("Failed to open '%s': %s", arc->url, wc_error->message);
    }
    arc->callback (NULL, arc->user_data);
    g_error_free (wc_error);
  } else {
    arc->callback (content, arc->user_data);
  }
  g_free (arc->url);
  g_slice_free (AsyncReadCb, arc);
}

static void
read_url_async (const gchar *url,
                GCancellable *cancellable,
                AsyncReadCbFunc callback,
                gpointer user_data)
{
  AsyncReadCb *arc;

  arc = g_slice_new0 (AsyncReadCb);
  arc->url = g_strdup (url);
  arc->callback = callback;
  arc->user_data = user_data;

  GRL_DEBUG ("Opening async '%s'", url);
  grl_net_wc_request_async (get_wc (),
                        url,
                        cancellable,
                        read_done_cb,
                        arc);
}

static void
build_media_from_entry (GrlYoutubeSource *source,
                        GrlMedia *content,
                        GDataEntry *entry,
                        GCancellable *cancellable,
                        const GList *keys,
                        BuildMediaFromEntryCbFunc callback,
                        gpointer user_data)
{
  GDataYouTubeVideo *video;
  GDataMediaThumbnail *thumbnail;
  GrlMedia *media;
  GList *iter;
  quvi_media_t v;
  QUVIcode rc;
  gchar *url;

  if (!content) {
    media = grl_media_video_new ();
  } else {
    media = content;
  }

  video = GDATA_YOUTUBE_VIDEO (entry);

  /* Make sure we set the media id in any case */
  if (!grl_media_get_id (media)) {
    grl_media_set_id (media, gdata_youtube_video_get_video_id (video));
  }

  iter = (GList *) keys;
  while (iter) {
    if (iter->data == GRL_METADATA_KEY_TITLE) {
      grl_media_set_title (media, gdata_entry_get_title (entry));
    } else if (iter->data == GRL_METADATA_KEY_DESCRIPTION) {
      grl_media_set_description (media,
				 gdata_youtube_video_get_description (video));
    } else if (iter->data == GRL_METADATA_KEY_THUMBNAIL) {
      GList *thumb_list;
      thumb_list = gdata_youtube_video_get_thumbnails (video);
      while (thumb_list) {
        thumbnail = GDATA_MEDIA_THUMBNAIL (thumb_list->data);
        grl_media_add_thumbnail (media,
                                 gdata_media_thumbnail_get_uri (thumbnail));
        thumb_list = g_list_next (thumb_list);
      }
    } else if (iter->data == GRL_METADATA_KEY_DATE) {
      GTimeVal date;
      gchar *date_str;
      gint64 published = gdata_entry_get_published (entry);
      date.tv_sec = (glong) published;
      date.tv_usec = 0;
      if (date.tv_sec != 0 || date.tv_usec != 0) {
        date_str = g_time_val_to_iso8601 (&date);
        grl_media_set_date (media, date_str);
        g_free (date_str);
      }
    } else if (iter->data == GRL_METADATA_KEY_DURATION) {
      grl_media_set_duration (media, gdata_youtube_video_get_duration (video));
    } else if (iter->data == GRL_METADATA_KEY_MIME) {
      grl_media_set_mime (media, YOUTUBE_VIDEO_MIME);
    } else if (iter->data == GRL_METADATA_KEY_SITE) {
      grl_media_set_site (media, gdata_youtube_video_get_player_uri (video));
    } else if (iter->data == GRL_METADATA_KEY_EXTERNAL_URL) {
      grl_media_set_external_url (media,
				  gdata_youtube_video_get_player_uri (video));
    } else if (iter->data == GRL_METADATA_KEY_RATING) {
      gdouble average;
      gdata_youtube_video_get_rating (video, NULL, NULL, NULL, &average);
      grl_media_set_rating (media, average, 5.00);
    } else if (iter->data == GRL_METADATA_KEY_URL && source->priv->quvi_handle) {
      rc = quvi_parse (source->priv->quvi_handle,
                       (char *) gdata_youtube_video_get_player_uri (video),
                       &v);
      if (rc == QUVI_OK) {
        rc = quvi_getprop (v, QUVIPROP_MEDIAURL, &url);
        if (rc == QUVI_OK) {
          grl_media_set_url (media, url);
        }
        quvi_parse_close (&v);
      }
    } else if (iter->data == GRL_METADATA_KEY_EXTERNAL_PLAYER) {
      GDataYouTubeContent *youtube_content;
      youtube_content =
	gdata_youtube_video_look_up_content (video,
					     "application/x-shockwave-flash");
      if (youtube_content != NULL) {
        const gchar *uri =
          gdata_media_content_get_uri (GDATA_MEDIA_CONTENT (youtube_content));
	grl_media_set_external_player (media, uri);
      }
    }
    iter = g_list_next (iter);
  }

  callback (media, user_data);
}

static void
parse_categories (xmlDocPtr doc, xmlNodePtr node, OperationSpec *os)
{
  guint total = 0;
  GList *all = NULL, *iter;
  CategoryInfo *cat_info;
  gchar *id;
  guint index = 0;

  GRL_DEBUG ("parse_categories");

  while (node) {
    cat_info = g_slice_new (CategoryInfo);
    id = (gchar *) xmlGetProp (node, (xmlChar *) "term");
    cat_info->id = g_strconcat (YOUTUBE_CATEGORIES_ID, "/", id, NULL);
    cat_info->name = (gchar *) xmlGetProp (node, (xmlChar *) "label");
    all = g_list_prepend (all, cat_info);
    g_free (id);
    node = node->next;
    total++;
    GRL_DEBUG ("  Found category: '%d - %s'", index++, cat_info->name);
  }

  if (all) {
    root_dir[ROOT_DIR_CATEGORIES_INDEX].count = total;
    categories_dir = g_new0 (CategoryInfo, total + 1);
    iter = all;
    do {
      cat_info = (CategoryInfo *) iter->data;
      categories_dir[total - 1].id = cat_info->id ;
      categories_dir[total - 1].name = cat_info->name;
      categories_dir[total - 1].count = -1;
      total--;
      g_slice_free (CategoryInfo, cat_info);
      iter = g_list_next (iter);
    } while (iter);
    g_list_free (all);

    produce_from_directory (categories_dir,
                            root_dir[ROOT_DIR_CATEGORIES_INDEX].count,
                            os);
  }
}

static void
build_categories_directory_read_cb (gchar *xmldata, gpointer user_data)
{
  xmlDocPtr doc;
  xmlNodePtr node;

  if (!xmldata) {
    g_critical ("Failed to build category directory (1)");
    return;
  }

  doc = xmlReadMemory (xmldata, strlen (xmldata), NULL, NULL,
                       XML_PARSE_RECOVER | XML_PARSE_NOBLANKS);
  if (!doc) {
    g_critical ("Failed to build category directory (2)");
    goto free_resources;
  }

  node = xmlDocGetRootElement (doc);
  if (!node) {
    g_critical ("Failed to build category directory (3)");
    goto free_resources;
  }

  if (xmlStrcmp (node->name, (const xmlChar *) "categories")) {
    g_critical ("Failed to build category directory (4)");
    goto free_resources;
  }

  node = node->xmlChildrenNode;
  if (!node) {
    g_critical ("Failed to build category directory (5)");
    goto free_resources;
  }

  parse_categories (doc, node, user_data);

 free_resources:
  xmlFreeDoc (doc);
}

static gint
get_feed_type_from_id (const gchar *feed_id)
{
  gchar *tmp;
  gchar *test;
  gint feed_type;

  tmp = g_strrstr (feed_id, "/");
  if (!tmp) {
    return -1;
  }
  tmp++;

  feed_type = strtol (tmp, &test, 10);
  if (*test != '\0') {
    return -1;
  }

  return feed_type;
}

static const gchar *
get_category_term_from_id (const gchar *category_id)
{
  gchar *term;
  term = g_strrstr (category_id, "/");
  if (!term) {
    return NULL;
  }
  return ++term;
}

static gint
get_category_index_from_id (const gchar *category_id)
{
  gint i;

  for (i=0; i<root_dir[ROOT_DIR_CATEGORIES_INDEX].count; i++) {
    if (!strcmp (categories_dir[i].id, category_id)) {
      return i;
    }
  }
  return -1;
}

static void
build_media_from_entry_metadata_cb (GrlMedia *media, gpointer user_data)
{
  GrlMediaSourceMetadataSpec *ms = (GrlMediaSourceMetadataSpec *) user_data;
  release_operation_data (GRL_METADATA_SOURCE (ms->source), ms->metadata_id);
  ms->callback (ms->source, ms->metadata_id, media, ms->user_data, NULL);
}

static void
build_media_from_entry_search_cb (GrlMedia *media, gpointer user_data)
{
  /*
   * TODO: Async resolution of URL messes (or could mess) with the sorting,
   * If we want to ensure a particular sorting or implement sorting
   * mechanisms we should add code to handle that here so we emit items in
   * the right order and not just when we got the URL resolved (would
   * damage response time though).
   */
  OperationSpec *os = (OperationSpec *) user_data;
  guint remaining;

  if (g_cancellable_is_cancelled (os->cancellable)) {
    GRL_DEBUG ("%s: cancelled", __FUNCTION__);
    return;
  }

  if (os->emitted < os->count) {
    remaining = os->count - os->emitted - 1;
    os->callback (os->source,
		  os->operation_id,
		  media,
		  remaining,
		  os->user_data,
		  NULL);
    if (remaining == 0) {
      GRL_DEBUG ("Unreffing spec in build_media_from_entry_search_cb");
      operation_spec_unref (os);
    } else {
      os->emitted++;
    }
  }
}

static void
build_category_directory (OperationSpec *os)
{
  GRL_DEBUG (__FUNCTION__);

  read_url_async (YOUTUBE_CATEGORIES_URL,
                  NULL,
                  build_categories_directory_read_cb,
                  os);
}

static void
metadata_cb (GObject *object,
	     GAsyncResult *result,
	     gpointer user_data)
{
  GError *error = NULL;
  GrlYoutubeSource *source;
  GDataEntry *video;
  GDataService *service;
  GrlMediaSourceMetadataSpec *ms = (GrlMediaSourceMetadataSpec *) user_data;

  GRL_DEBUG ("metadata_cb");

  source = GRL_YOUTUBE_SOURCE (ms->source);
  service = GDATA_SERVICE (source->priv->service);

  video = gdata_service_query_single_entry_finish (service, result, &error);

  if (error) {
    release_operation_data (GRL_METADATA_SOURCE (ms->source), ms->metadata_id);
    error->code = GRL_CORE_ERROR_METADATA_FAILED;
    ms->callback (ms->source, ms->metadata_id, ms->media, ms->user_data, error);
    g_error_free (error);
  } else {
    build_media_from_entry (GRL_YOUTUBE_SOURCE (ms->source),
                            ms->media,
                            video,
                            grl_operation_get_data (ms->metadata_id),
                            ms->keys,
			    build_media_from_entry_metadata_cb,
                            ms);
  }

  if (video) {
    g_object_unref (video);
  }
}

static void
search_progress_cb (GDataEntry *entry,
		    guint index,
		    guint count,
		    gpointer user_data)
{
  OperationSpec *os = (OperationSpec *) user_data;

  /* Check if operation has been cancelled */
  if (g_cancellable_is_cancelled (os->cancellable)) {
    GRL_DEBUG ("%s: cancelled (%u, %u)", __FUNCTION__, index, count);
    build_media_from_entry_search_cb (NULL, os);
    return;
  }

  if (index < count) {
    /* Keep track of the items we got here. Due to the asynchronous
     * nature of build_media_from_entry(), when search_cb is invoked
     * we have to check if we got as many results as we requested or
     * not, and handle that situation properly */
    os->matches++;
    build_media_from_entry (GRL_YOUTUBE_SOURCE (os->source),
                            NULL,
                            entry,
                            os->cancellable,
                            os->keys,
                            build_media_from_entry_search_cb,
                            os);
  } else {
    GRL_WARNING ("Invalid index/count received grom libgdata, ignoring result");
  }

  /* The entry will be freed when freeing the feed in search_cb */
}

static void
search_cb (GObject *object, GAsyncResult *result, OperationSpec *os)
{
  GDataFeed *feed;
  GError *error = NULL;
  gboolean need_extra_unref = FALSE;
  GrlYoutubeSource *source = GRL_YOUTUBE_SOURCE (os->source);

  GRL_DEBUG ("search_cb");

  /* Check if operation was cancelled */
  if (g_cancellable_is_cancelled (os->cancellable)) {
    GRL_DEBUG ("Search operation has been cancelled");
    os->callback (os->source, os->operation_id, NULL, 0, os->user_data, NULL);
    operation_spec_unref (os);
    /* Look for OPERATION_SPEC_REF_RATIONALE for details on the reason for this
     * extra unref */
    operation_spec_unref (os);
    return;
  }

  feed = gdata_service_query_finish (source->priv->service, result, &error);
  if (!error && feed) {
    /* If we are browsing a category, update the count for it */
    if (os->category_info) {
      os->category_info->count = gdata_feed_get_total_results (feed);
    }

    /* Check if we got as many results as we requested */
    if (os->matches < os->count) {
      os->count = os->matches;
      /* In case we are resolving URLs asynchronously, from now on
       * results will be sent with appropriate remaining, but it can
       * also be the case that we have sent all the results already
       * and the last one was sent with remaining>0, in that case
       * we should send a finishing message now. */
      if (os->emitted == os->count) {
	GRL_DEBUG ("sending finishing message");
	os->callback (os->source, os->operation_id,
		      NULL, 0, os->user_data, NULL);
	need_extra_unref = TRUE;
      }
    }
  } else {
    if (!error) {
      error = g_error_new (GRL_CORE_ERROR,
			   os->error_code,
			   "Failed to obtain feed from Youtube");
    } else {
      error->code = os->error_code;
    }
    os->callback (os->source, os->operation_id, NULL, 0, os->user_data, error);
    g_error_free (error);
    need_extra_unref = TRUE;
  }

  if (feed)
    g_object_unref (feed);

  GRL_DEBUG ("Unreffing spec in search_cb");
  operation_spec_unref (os);
  if (need_extra_unref) {
    /* We did not free the spec in the emission callback, do it here */
    GRL_DEBUG ("need extra spec unref in search_cb");
    operation_spec_unref (os);
  }
}

static gboolean
is_category_container (const gchar *container_id)
{
  return g_str_has_prefix (container_id, YOUTUBE_CATEGORIES_ID "/");
}

static gboolean
is_feeds_container (const gchar *container_id)
{
  return g_str_has_prefix (container_id, YOUTUBE_FEEDS_ID "/");
}

static YoutubeMediaType
classify_media_id (const gchar *media_id)
{
  if (!media_id) {
    return YOUTUBE_MEDIA_TYPE_ROOT;
  } else if (!strcmp (media_id, YOUTUBE_FEEDS_ID)) {
    return YOUTUBE_MEDIA_TYPE_FEEDS;
  } else if (!strcmp (media_id, YOUTUBE_CATEGORIES_ID)) {
    return YOUTUBE_MEDIA_TYPE_CATEGORIES;
  } else if (is_category_container (media_id)) {
    return YOUTUBE_MEDIA_TYPE_CATEGORY;
  } else if (is_feeds_container (media_id)) {
    return YOUTUBE_MEDIA_TYPE_FEED;
  } else {
    return YOUTUBE_MEDIA_TYPE_VIDEO;
  }
}

static void
set_category_childcount (GDataService *service,
			 GrlMediaBox *content,
                         CategoryInfo *dir,
                         guint index)
{
  gint childcount;
  gboolean set_childcount = TRUE;
  const gchar *container_id;

  container_id = grl_media_get_id (GRL_MEDIA (content));

  if (dir == NULL) {
    /* Special case: we want childcount of root category */
    childcount = root_dir_size;
  } else if (!strcmp (dir[index].id, YOUTUBE_FEEDS_ID)) {
    childcount = root_dir[ROOT_DIR_FEEDS_INDEX].count;
  } else if (!strcmp (dir[index].id, YOUTUBE_CATEGORIES_ID)) {
    childcount = root_dir[ROOT_DIR_CATEGORIES_INDEX].count;
  } else if (is_feeds_container (container_id)) {
    gint feed_index = get_feed_type_from_id (container_id);
    if (feed_index >= 0) {
      childcount = feeds_dir[feed_index].count;
    } else {
      set_childcount = FALSE;
    }
  } else if (is_category_container (container_id)) {
    gint cat_index = get_category_index_from_id (container_id);
    if (cat_index >= 0) {
      childcount = categories_dir[cat_index].count;
    } else {
      set_childcount = FALSE;
    }
  } else {
    set_childcount = FALSE;
  }

  if (set_childcount) {
    grl_media_box_set_childcount (content, childcount);
  }
}

static GrlMedia *
produce_container_from_directory (GDataService *service,
				  GrlMedia *media,
				  CategoryInfo *dir,
				  guint index)
{
  GrlMedia *content;

  if (!media) {
    /* Create mode */
    content = grl_media_box_new ();
  } else {
    /* Update mode */
    content = media;
  }

  if (!dir) {
    grl_media_set_id (content, NULL);
    grl_media_set_title (content, YOUTUBE_ROOT_NAME);
  } else {
    grl_media_set_id (content, dir[index].id);
    grl_media_set_title (content, dir[index].name);
  }
  grl_media_set_site (content, YOUTUBE_SITE_URL);
  set_category_childcount (service, GRL_MEDIA_BOX (content), dir, index);

  return content;
}

static void
produce_from_directory (CategoryInfo *dir, gint dir_size, OperationSpec *os)
{
  guint index, remaining;

  GRL_DEBUG ("produce_from_directory");

  /* Youtube's first index is 1, but the directories start at 0 */
  os->skip--;

  if (os->skip >= dir_size) {
    /* No results */
    os->callback (os->source,
		  os->operation_id,
		  NULL,
		  0,
		  os->user_data,
		  NULL);
    operation_spec_unref (os);
  } else {
    index = os->skip;
    remaining = MIN (dir_size - os->skip, os->count);

    do {
      GDataService *service = GRL_YOUTUBE_SOURCE (os->source)->priv->service;

      GrlMedia *content =
	produce_container_from_directory (service, NULL, dir, index);

      remaining--;
      index++;

      os->callback (os->source,
		    os->operation_id,
		    content,
		    remaining,
		    os->user_data,
		    NULL);

      if (remaining == 0) {
	operation_spec_unref (os);
      }
    } while (remaining > 0);
  }
}

static void
produce_from_feed (OperationSpec *os)
{
  GError *error = NULL;
  gint feed_type;
  GDataQuery *query;
  GDataService *service;

  feed_type = get_feed_type_from_id (os->container_id);

  if (feed_type < 0) {
    error = g_error_new (GRL_CORE_ERROR,
			 GRL_CORE_ERROR_BROWSE_FAILED,
			 "Invalid feed id: %s", os->container_id);
    os->callback (os->source,
		  os->operation_id,
		  NULL,
		  0,
		  os->user_data,
		  error);
    g_error_free (error);
    operation_spec_unref (os);
    return;
  }
  /* OPERATION_SPEC_REF_RATIONALE
   * Depending on wether the URL has been requested, metadata resolution
   * for each item in the result set may or may not be asynchronous.
   * We cannot free the spec in search_cb because that may be called
   * before the asynchronous URL resolution is finished, and we cannot
   * do it in build_media_from_entry_search_cb either, because in the
   * synchronous case (when we do not request URL) search_cb will
   * be invoked after it.
   * Thus, the solution is to increase the reference count here and
   * have both places unreffing the spec, that way, no matter which
   * is invoked last, the spec will be freed only once. */
  operation_spec_ref (os);

  os->cancellable = g_cancellable_new ();
  grl_operation_set_data (os->operation_id, os->cancellable);

  service = GRL_YOUTUBE_SOURCE (os->source)->priv->service;
  query = gdata_query_new_with_limits (NULL , os->skip, os->count);
  os->category_info = &feeds_dir[feed_type];

#ifdef HAVE_LIBGDATA_0_9
  gdata_youtube_service_query_standard_feed_async (GDATA_YOUTUBE_SERVICE (service),
                                                   feed_type,
                                                   query,
                                                   os->cancellable,
                                                   search_progress_cb,
                                                   os,
                                                   NULL,
                                                   (GAsyncReadyCallback) search_cb,
                                                   os);
#else /* HAVE_LIBGDATA_0_9 */
  gdata_youtube_service_query_standard_feed_async (GDATA_YOUTUBE_SERVICE (service),
                                                   feed_type,
                                                   query,
                                                   os->cancellable,
                                                   search_progress_cb,
                                                   os,
                                                   (GAsyncReadyCallback) search_cb,
                                                   os);
#endif /* !HAVE_LIBGDATA_0_9 */

  g_object_unref (query);
}

static void
produce_from_category (OperationSpec *os)
{
  GError *error = NULL;
  GDataQuery *query;
  GDataService *service;
  const gchar *category_term;
  gint category_index;

  category_term = get_category_term_from_id (os->container_id);
  category_index = get_category_index_from_id (os->container_id);

  if (!category_term) {
    error = g_error_new (GRL_CORE_ERROR,
			 GRL_CORE_ERROR_BROWSE_FAILED,
			 "Invalid category id: %s", os->container_id);
    os->callback (os->source,
		  os->operation_id,
		  NULL,
		  0,
		  os->user_data,
		  error);
    g_error_free (error);
    operation_spec_unref (os);
    return;
  }

  /* Look for OPERATION_SPEC_REF_RATIONALE for details */
  operation_spec_ref (os);

  service = GRL_YOUTUBE_SOURCE (os->source)->priv->service;
  query = gdata_query_new_with_limits (NULL , os->skip, os->count);
  os->category_info = &categories_dir[category_index];
  gdata_query_set_categories (query, category_term);

#ifdef HAVE_LIBGDATA_0_9
  gdata_youtube_service_query_videos_async (GDATA_YOUTUBE_SERVICE (service),
                                            query,
                                            NULL,
                                            search_progress_cb,
                                            os,
                                            NULL,
                                            (GAsyncReadyCallback) search_cb,
                                            os);
#else /* HAVE_LIBGDATA_0_9 */
  gdata_youtube_service_query_videos_async (GDATA_YOUTUBE_SERVICE (service),
					    query,
					    NULL,
					    search_progress_cb,
					    os,
					    (GAsyncReadyCallback) search_cb,
					    os);
#endif /* !HAVE_LIBGDATA_0_9 */

  g_object_unref (query);
}

static gchar *
get_video_id_from_url (const gchar *url)
{
  gchar *marker, *end, *video_id;

  if (url == NULL) {
    return NULL;
  }

  marker = strstr (url, YOUTUBE_WATCH_URL);
  if (!marker) {
    return NULL;
  }

  marker += strlen (YOUTUBE_WATCH_URL);

  end = marker;
  while (*end != '\0' && *end != '&') {
    end++;
  }

  video_id = g_strndup (marker, end - marker);

  return video_id;
}

static void
build_media_from_entry_media_from_uri_cb (GrlMedia *media, gpointer user_data)
{
  GrlMediaSourceMediaFromUriSpec *mfus =
    (GrlMediaSourceMediaFromUriSpec *) user_data;

  release_operation_data (GRL_METADATA_SOURCE (mfus->source), mfus->media_from_uri_id);
  mfus->callback (mfus->source, mfus->media_from_uri_id, media, mfus->user_data, NULL);
}

static void
media_from_uri_cb (GObject *object, GAsyncResult *result, gpointer user_data)
{
  GError *error = NULL;
  GrlYoutubeSource *source;
  GDataEntry *video;
  GDataService *service;
  GrlMediaSourceMediaFromUriSpec *mfus =
    (GrlMediaSourceMediaFromUriSpec *) user_data;

  source = GRL_YOUTUBE_SOURCE (mfus->source);
  service = GDATA_SERVICE (source->priv->service);

  video = gdata_service_query_single_entry_finish (service, result, &error);

  if (error) {
    error->code = GRL_CORE_ERROR_MEDIA_FROM_URI_FAILED;
    release_operation_data (GRL_METADATA_SOURCE (mfus->source), mfus->media_from_uri_id);
    mfus->callback (mfus->source, mfus->media_from_uri_id, NULL, mfus->user_data, error);
    g_error_free (error);
  } else {
    build_media_from_entry (GRL_YOUTUBE_SOURCE (mfus->source),
                            NULL,
                            video,
                            grl_operation_get_data (mfus->media_from_uri_id),
                            mfus->keys,
			    build_media_from_entry_media_from_uri_cb,
			    mfus);
  }

  if (video) {
    g_object_unref (video);
  }
}

/* ================== API Implementation ================ */

static const GList *
grl_youtube_source_supported_keys (GrlMetadataSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_ID,
                                      GRL_METADATA_KEY_TITLE,
                                      GRL_METADATA_KEY_URL,
				      GRL_METADATA_KEY_EXTERNAL_URL,
                                      GRL_METADATA_KEY_DESCRIPTION,
                                      GRL_METADATA_KEY_DURATION,
                                      GRL_METADATA_KEY_DATE,
                                      GRL_METADATA_KEY_THUMBNAIL,
                                      GRL_METADATA_KEY_MIME,
                                      GRL_METADATA_KEY_CHILDCOUNT,
                                      GRL_METADATA_KEY_SITE,
                                      GRL_METADATA_KEY_RATING,
				      GRL_METADATA_KEY_EXTERNAL_PLAYER,
                                      NULL);
  }
  return keys;
}

static const GList *
grl_youtube_source_slow_keys (GrlMetadataSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_URL,
                                      NULL);
  }
  return keys;
}

static void
grl_youtube_source_search (GrlMediaSource *source,
                           GrlMediaSourceSearchSpec *ss)
{
  OperationSpec *os;
  GDataQuery *query;

  GRL_DEBUG ("grl_youtube_source_search (%u, %u)", ss->skip, ss->count);

  os = operation_spec_new ();
  os->source = source;
  os->cancellable = g_cancellable_new ();
  os->operation_id = ss->search_id;
  os->keys = ss->keys;
  os->skip = ss->skip + 1;
  os->count = ss->count;
  os->callback = ss->callback;
  os->user_data = ss->user_data;
  os->error_code = GRL_CORE_ERROR_SEARCH_FAILED;

  /* Look for OPERATION_SPEC_REF_RATIONALE for details */
  operation_spec_ref (os);

  grl_operation_set_data (ss->search_id, os->cancellable);

  query = gdata_query_new_with_limits (ss->text, os->skip, os->count);

#ifdef HAVE_LIBGDATA_0_9
  gdata_youtube_service_query_videos_async (GDATA_YOUTUBE_SERVICE (GRL_YOUTUBE_SOURCE (source)->priv->service),
                                            query,
                                            os->cancellable,
                                            search_progress_cb,
                                            os,
                                            NULL,
                                            (GAsyncReadyCallback) search_cb,
                                            os);
#else /* HAVE_LIBGDATA_0_9 */
  gdata_youtube_service_query_videos_async (GDATA_YOUTUBE_SERVICE (GRL_YOUTUBE_SOURCE (source)->priv->service),
					    query,
					    os->cancellable,
					    search_progress_cb,
					    os,
					    (GAsyncReadyCallback) search_cb,
					    os);
#endif /* !HAVE_LIBGDATA_0_9 */

  g_object_unref (query);
}

static void
grl_youtube_source_browse (GrlMediaSource *source,
                           GrlMediaSourceBrowseSpec *bs)
{
  OperationSpec *os;
  const gchar *container_id;

  GRL_DEBUG ("grl_youtube_source_browse: %s", grl_media_get_id (bs->container));

  container_id = grl_media_get_id (bs->container);

  os = operation_spec_new ();
  os->source = bs->source;
  os->operation_id = bs->browse_id;
  os->container_id = container_id;
  os->keys = bs->keys;
  os->flags = bs->flags;
  os->skip = bs->skip + 1;
  os->count = bs->count;
  os->callback = bs->callback;
  os->user_data = bs->user_data;
  os->error_code = GRL_CORE_ERROR_BROWSE_FAILED;

  switch (classify_media_id (container_id))
    {
    case YOUTUBE_MEDIA_TYPE_ROOT:
      produce_from_directory (root_dir, root_dir_size, os);
      break;
    case YOUTUBE_MEDIA_TYPE_FEEDS:
      produce_from_directory (feeds_dir,
			      root_dir[ROOT_DIR_FEEDS_INDEX].count, os);
      break;
    case YOUTUBE_MEDIA_TYPE_CATEGORIES:
      if (!categories_dir) {
        build_category_directory (os);
      } else {
        produce_from_directory (categories_dir,
                                root_dir[ROOT_DIR_CATEGORIES_INDEX].count,
                                os);
      }
      break;
    case YOUTUBE_MEDIA_TYPE_FEED:
      produce_from_feed (os);
      break;
    case YOUTUBE_MEDIA_TYPE_CATEGORY:
      produce_from_category (os);
      break;
    case YOUTUBE_MEDIA_TYPE_VIDEO:
    default:
      g_assert_not_reached ();
      break;
    }
}

static void
grl_youtube_source_metadata (GrlMediaSource *source,
                             GrlMediaSourceMetadataSpec *ms)
{
  YoutubeMediaType media_type;
  const gchar *id;
  GCancellable *cancellable;
  GDataService *service;
  GError *error = NULL;
  GrlMedia *media = NULL;

  GRL_DEBUG ("grl_youtube_source_metadata");

  id = grl_media_get_id (ms->media);
  media_type = classify_media_id (id);
  service = GRL_YOUTUBE_SOURCE (source)->priv->service;

  switch (media_type) {
  case YOUTUBE_MEDIA_TYPE_ROOT:
    media = produce_container_from_directory (service, ms->media, NULL, 0);
    break;
  case YOUTUBE_MEDIA_TYPE_FEEDS:
    media = produce_container_from_directory (service, ms->media, root_dir, 0);
    break;
  case YOUTUBE_MEDIA_TYPE_CATEGORIES:
    media = produce_container_from_directory (service, ms->media, root_dir, 1);
    break;
  case YOUTUBE_MEDIA_TYPE_FEED:
    {
      gint index = get_feed_type_from_id (id);
      if (index >= 0) {
	media = produce_container_from_directory (service, ms->media, feeds_dir,
						  index);
      } else {
	error = g_error_new (GRL_CORE_ERROR,
			     GRL_CORE_ERROR_METADATA_FAILED,
			     "Invalid feed id");
      }
    }
    break;
  case YOUTUBE_MEDIA_TYPE_CATEGORY:
    {
      gint index = get_category_index_from_id (id);
      if (index >= 0) {
	media = produce_container_from_directory (service, ms->media,
						  categories_dir, index);
      } else {
	error = g_error_new (GRL_CORE_ERROR,
			     GRL_CORE_ERROR_METADATA_FAILED,
			     "Invalid category id");
      }
    }
    break;
  case YOUTUBE_MEDIA_TYPE_VIDEO:
  default:
    cancellable = g_cancellable_new ();
    grl_operation_set_data (ms->metadata_id, cancellable);
    gchar *entryid = g_strconcat ("tag:youtube.com,2008:video:", id, NULL);

#ifdef HAVE_LIBGDATA_0_9
      gdata_service_query_single_entry_async (service,
                                              NULL,
                                              entryid,
                                              NULL,
                                              GDATA_TYPE_YOUTUBE_VIDEO,
                                              cancellable,
                                              metadata_cb,
                                              ms);
#else /* HAVE_LIBGDATA_0_9 */
      gdata_service_query_single_entry_async (service,
                                              entryid,
                                              NULL,
                                              GDATA_TYPE_YOUTUBE_VIDEO,
                                              cancellable,
                                              metadata_cb,
                                              ms);
#endif /* !HAVE_LIBGDATA_0_9 */

      g_free (entryid);
      break;
  }

  if (error) {
    ms->callback (ms->source, ms->metadata_id, ms->media, ms->user_data, error);
    g_error_free (error);
  } else if (media) {
    ms->callback (ms->source, ms->metadata_id, ms->media, ms->user_data, NULL);
  }
}

static gboolean
grl_youtube_test_media_from_uri (GrlMediaSource *source, const gchar *uri)
{
  gchar *video_id;
  gboolean ok;

  GRL_DEBUG ("grl_youtube_test_media_from_uri");

  video_id = get_video_id_from_url (uri);
  ok = (video_id != NULL);
  g_free (video_id);
  return ok;
}

static void
grl_youtube_get_media_from_uri (GrlMediaSource *source,
				 GrlMediaSourceMediaFromUriSpec *mfus)
{
  gchar *video_id;
  GError *error;
  GCancellable *cancellable;
  GDataService *service;
  gchar *entry_id;

  GRL_DEBUG ("grl_youtube_get_media_from_uri");

  video_id = get_video_id_from_url (mfus->uri);
  if (video_id == NULL) {
    error = g_error_new (GRL_CORE_ERROR,
			 GRL_CORE_ERROR_MEDIA_FROM_URI_FAILED,
			 "Cannot create media from '%s'", mfus->uri);
    mfus->callback (source, mfus->media_from_uri_id, NULL, mfus->user_data, error);
    g_error_free (error);
    return;
  }

  service = GRL_YOUTUBE_SOURCE (source)->priv->service;

  cancellable = g_cancellable_new ();
  grl_operation_set_data (mfus->media_from_uri_id, cancellable);
  entry_id = g_strconcat ("tag:youtube.com,2008:video:", video_id, NULL);

#ifdef HAVE_LIBGDATA_0_9
  gdata_service_query_single_entry_async (service,
                                          NULL,
                                          entry_id,
                                          NULL,
                                          GDATA_TYPE_YOUTUBE_VIDEO,
                                          cancellable,
                                          media_from_uri_cb,
                                          mfus);
#else /* HAVE_LIBGDATA_0_9 */
  gdata_service_query_single_entry_async (service,
					  entry_id,
					  NULL,
					  GDATA_TYPE_YOUTUBE_VIDEO,
					  cancellable,
					  media_from_uri_cb,
					  mfus);
#endif /* !HAVE_LIBGDATA_0_9 */

  g_free (entry_id);
}

static void
grl_youtube_source_cancel (GrlMetadataSource *source,
                           guint operation_id)
{
  GCancellable *cancellable;

  GRL_DEBUG (__FUNCTION__);

  cancellable = G_CANCELLABLE (grl_operation_get_data (operation_id));

  if (cancellable) {
    g_cancellable_cancel (cancellable);
  }
}
