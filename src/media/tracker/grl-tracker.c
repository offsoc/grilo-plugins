/*
 * Copyright (C) 2011 Igalia S.L.
 * Copyright (C) 2011 Intel Corporation.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <grilo.h>
#include <string.h>
#include <tracker-sparql.h>

#include "grl-tracker.h"
#include "grl-tracker-media.h"
#include "grl-tracker-media-api.h"
#include "grl-tracker-media-notif.h"
#include "grl-tracker-metadata.h"
#include "grl-tracker-request-queue.h"
#include "grl-tracker-utils.h"

/* --------- Logging  -------- */

#define GRL_LOG_DOMAIN_DEFAULT tracker_general_log_domain
GRL_LOG_DOMAIN_STATIC(tracker_general_log_domain);

/* ------- Definitions ------- */

#define TRACKER_FOLDER_CLASS_REQUEST           \
  "SELECT ?urn WHERE "                         \
  "{ "                                         \
  "?urn a rdfs:Class . "                       \
  "FILTER(fn:ends-with(?urn,\"nfo#Folder\")) " \
  "}"

#define TRACKER_UPNP_CLASS_REQUEST                      \
  "SELECT ?urn WHERE "                                  \
  "{ "                                                  \
  "?urn a rdfs:Class . "                                \
  "FILTER(fn:ends-with(?urn,\"upnp#UPnPDataObject\")) " \
  "}"

#define TRACKER_NOTIFY_FOLDER_UPDATE            \
  "INSERT "                                     \
  "{ "                                          \
  "<%s> tracker:notify true "                   \
  "}"

/* --- Other --- */

gboolean grl_tracker_plugin_init (GrlPluginRegistry *registry,
                                  const GrlPluginInfo *plugin,
                                  GList *configs);

/* ===================== Globals  ================= */

TrackerSparqlConnection *grl_tracker_connection = NULL;
const GrlPluginInfo *grl_tracker_plugin;
gboolean grl_tracker_upnp_present = FALSE;
GrlTrackerQueue *grl_tracker_queue = NULL;

/* tracker plugin config */
gboolean grl_tracker_per_device_source = FALSE;
gboolean grl_tracker_browse_filesystem = FALSE;
gboolean grl_tracker_show_documents    = FALSE;

/* =================== Tracker Plugin  =============== */

static void
init_sources (void)
{
  grl_tracker_setup_key_mappings ();

  grl_tracker_queue = grl_tracker_queue_new ();

  if (grl_tracker_connection != NULL) {
    grl_tracker_media_dbus_start_watch ();

    grl_tracker_metadata_source_init ();
    grl_tracker_media_sources_init ();
  }
}

static void
tracker_update_folder_class_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      data)
{
  init_sources ();
}

static void
tracker_get_folder_class_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer       data)
{
  TrackerSparqlCursor  *cursor;

  GRL_DEBUG ("%s", __FUNCTION__);

  cursor = tracker_sparql_connection_query_finish (grl_tracker_connection,
                                                   result, NULL);

  if (!cursor) {
    init_sources ();
    return;
  }

  if (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
    gchar *update = g_strdup_printf (TRACKER_NOTIFY_FOLDER_UPDATE,
                                     tracker_sparql_cursor_get_string (cursor,
                                                                       0,
                                                                       NULL));

    GRL_DEBUG ("\tupdate query: '%s'", update);

    tracker_sparql_connection_update_async (grl_tracker_connection,
                                            update,
                                            G_PRIORITY_DEFAULT,
                                            NULL,
                                            tracker_update_folder_class_cb,
                                            NULL);

    g_free (update);
  }

  g_object_unref (cursor);
}

static void
tracker_get_upnp_class_cb (GObject      *object,
                           GAsyncResult *result,
                           gpointer      data)
{
  GError *error = NULL;
  TrackerSparqlCursor *cursor;

  GRL_DEBUG ("%s", G_STRFUNC);

  cursor = tracker_sparql_connection_query_finish (grl_tracker_connection,
                                                   result, &error);
  if (error) {
    GRL_WARNING ("Could not execute sparql query for upnp class: %s",
                 error->message);
    g_error_free (error);
  } else {
    if (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
      GRL_DEBUG ("\tUPnP ontology present");
      grl_tracker_upnp_present = TRUE;
    }
  }

  if (cursor)
    g_object_unref (cursor);

  if (grl_tracker_browse_filesystem)
    tracker_sparql_connection_query_async (grl_tracker_connection,
                                           TRACKER_FOLDER_CLASS_REQUEST,
                                           NULL,
                                           tracker_get_folder_class_cb,
                                           NULL);
  else
    init_sources ();
}

static void
tracker_get_connection_cb (GObject             *object,
                           GAsyncResult        *res,
                           const GrlPluginInfo *plugin)
{
  GError *error = NULL;
  /* GrlTrackerMedia *source; */

  GRL_DEBUG ("%s", __FUNCTION__);

  grl_tracker_connection = tracker_sparql_connection_get_finish (res, &error);

  if (error) {
    GRL_INFO ("Could not get connection to Tracker: %s", error->message);
    g_error_free (error);
    return;
  }

  GRL_DEBUG ("\trequest : '%s'", TRACKER_UPNP_CLASS_REQUEST);

  tracker_sparql_connection_query_async (grl_tracker_connection,
                                         TRACKER_UPNP_CLASS_REQUEST,
                                         NULL,
                                         tracker_get_upnp_class_cb,
                                         NULL);
}

gboolean
grl_tracker_plugin_init (GrlPluginRegistry *registry,
                         const GrlPluginInfo *plugin,
                         GList *configs)
{
  GrlConfig *config;
  gint config_count;

  GRL_LOG_DOMAIN_INIT (tracker_general_log_domain, "tracker-general");
  grl_tracker_media_init_notifs ();
  grl_tracker_media_init_requests ();
  grl_tracker_metadata_init_requests ();

  grl_tracker_plugin = plugin;

  if (!configs) {
    GRL_INFO ("\tConfiguration not provided! Using default configuration.");
  } else {
    config_count = g_list_length (configs);
    if (config_count > 1) {
      GRL_INFO ("\tProvided %i configs, but will only use one", config_count);
    }

    config = GRL_CONFIG (configs->data);

    grl_tracker_per_device_source =
      grl_config_get_boolean (config, "per-device-source");
    grl_tracker_browse_filesystem =
      grl_config_get_boolean (config, "browse-filesystem");
    grl_tracker_show_documents =
      grl_config_get_boolean (config, "show-documents");
  }

  tracker_sparql_connection_get_async (NULL,
                                       (GAsyncReadyCallback) tracker_get_connection_cb,
                                       (gpointer) plugin);
  return TRUE;
}

GRL_PLUGIN_REGISTER (grl_tracker_plugin_init,
                     NULL,
                     GRL_TRACKER_PLUGIN_ID);
