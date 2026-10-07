/*
 * Copyright (C) 2008-2010 Nick Schermer <nick@xfce.org>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "tasklist-widget.h"
#include "tasklist.h"

#include "common/panel-private.h"
#include "common/panel-utils.h"
#include "common/panel-xfconf.h"

#include <libxfce4ui/libxfce4ui.h>
#include <gio/gdesktopappinfo.h>
#include <libintl.h>


#define HANDLE_SIZE (4)
#define MINIOS_(String) dgettext (MINIOS_GETTEXT_PACKAGE, String)

static const GtkTargetEntry tasklist_pin_targets[] = {
  { "text/uri-list", 0, 0 }
};


struct _TasklistPlugin
{
  XfcePanelPlugin __parent__;

  /* the tasklist widget */
  GtkWidget *tasklist;
  GtkWidget *handle;
};



static void
tasklist_plugin_construct (XfcePanelPlugin *panel_plugin);
static void
tasklist_plugin_mode_changed (XfcePanelPlugin *panel_plugin,
                              XfcePanelPluginMode mode);
static gboolean
tasklist_plugin_size_changed (XfcePanelPlugin *panel_plugin,
                              gint size);
static void
tasklist_plugin_nrows_changed (XfcePanelPlugin *panel_plugin,
                               guint nrows);
static void
tasklist_plugin_screen_position_changed (XfcePanelPlugin *panel_plugin,
                                         XfceScreenPosition position);
static void
tasklist_plugin_configure_plugin (XfcePanelPlugin *panel_plugin);
static gboolean
tasklist_plugin_handle_draw (GtkWidget *widget,
                             cairo_t *cr,
                             TasklistPlugin *plugin);
static void
tasklist_plugin_drag_data_received (GtkWidget *widget,
                                    GdkDragContext *context,
                                    gint x,
                                    gint y,
                                    GtkSelectionData *data,
                                    guint info,
                                    guint time,
                                    TasklistPlugin *plugin);
static void
tasklist_plugin_pinned_refresh (GtkWidget *listbox);
static void
tasklist_plugin_pinned_remove_clicked (GtkButton *button,
                                       GtkWidget *listbox);
static void
tasklist_plugin_pinned_move_clicked (GtkButton *button,
                                     GtkWidget *listbox);



/* define and register the plugin */
XFCE_PANEL_DEFINE_PLUGIN_RESIDENT (TasklistPlugin, tasklist_plugin)



static void
tasklist_plugin_class_init (TasklistPluginClass *klass)
{
  XfcePanelPluginClass *plugin_class;

  plugin_class = XFCE_PANEL_PLUGIN_CLASS (klass);
  plugin_class->construct = tasklist_plugin_construct;
  plugin_class->mode_changed = tasklist_plugin_mode_changed;
  plugin_class->size_changed = tasklist_plugin_size_changed;
  plugin_class->nrows_changed = tasklist_plugin_nrows_changed;
  plugin_class->screen_position_changed = tasklist_plugin_screen_position_changed;
  plugin_class->configure_plugin = tasklist_plugin_configure_plugin;
}



static void
tasklist_plugin_init (TasklistPlugin *plugin)
{
  GtkWidget *box;

  bindtextdomain (MINIOS_GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (MINIOS_GETTEXT_PACKAGE, "UTF-8");

  /* create widgets */
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_add (GTK_CONTAINER (plugin), box);
  g_object_bind_property (G_OBJECT (plugin), "orientation",
                          G_OBJECT (box), "orientation",
                          G_BINDING_SYNC_CREATE);
  gtk_widget_show (box);

  plugin->handle = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start (GTK_BOX (box), plugin->handle, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (plugin->handle), "draw",
                    G_CALLBACK (tasklist_plugin_handle_draw), plugin);
  gtk_widget_set_size_request (plugin->handle, 8, 8);
  gtk_widget_show (plugin->handle);

  plugin->tasklist = g_object_new (XFCE_TYPE_TASKLIST, NULL);
  gtk_box_pack_start (GTK_BOX (box), plugin->tasklist, TRUE, TRUE, 0);

  /* Accept Whisker .desktop drops on the entire expanded plugin area,
   * not only on existing task buttons. */
  gtk_drag_dest_set (GTK_WIDGET (plugin), GTK_DEST_DEFAULT_ALL,
                     tasklist_pin_targets, G_N_ELEMENTS (tasklist_pin_targets),
                     GDK_ACTION_COPY);
  g_signal_connect (G_OBJECT (plugin), "drag-data-received",
                    G_CALLBACK (tasklist_plugin_drag_data_received), plugin);

  g_object_bind_property (G_OBJECT (plugin->tasklist), "show-handle",
                          G_OBJECT (plugin->handle), "visible",
                          G_BINDING_SYNC_CREATE);
}



static void
tasklist_plugin_construct (XfcePanelPlugin *panel_plugin)
{
  TasklistPlugin *plugin = TASKLIST_PLUGIN (panel_plugin);
  const PanelProperty properties[] = {
    { "show-labels", G_TYPE_BOOLEAN },
    { "grouping", G_TYPE_BOOLEAN },
    { "include-all-workspaces", G_TYPE_BOOLEAN },
    { "include-all-monitors", G_TYPE_BOOLEAN },
    { "flat-buttons", G_TYPE_BOOLEAN },
    { "switch-workspace-on-unminimize", G_TYPE_BOOLEAN },
    { "show-only-minimized", G_TYPE_BOOLEAN },
    { "show-wireframes", G_TYPE_BOOLEAN },
    { "show-handle", G_TYPE_BOOLEAN },
    { "show-tooltips", G_TYPE_BOOLEAN },
    { "sort-order", G_TYPE_UINT },
    { "window-scrolling", G_TYPE_BOOLEAN },
    { "wrap-windows", G_TYPE_BOOLEAN },
    { "include-all-blinking", G_TYPE_BOOLEAN },
    { "middle-click", G_TYPE_UINT },
    { "label-decorations", G_TYPE_BOOLEAN },
    { "pinned-apps", G_TYPE_PTR_ARRAY },
    { NULL }
  };

  /* The taskbar is the flexible area of the panel: consume all space
   * left between the fixed launchers and the notification area. */
  xfce_panel_plugin_set_expand (panel_plugin, TRUE);

  /* show configure */
  xfce_panel_plugin_menu_show_configure (XFCE_PANEL_PLUGIN (plugin));

  /* bind all properties */
  panel_properties_bind (NULL, G_OBJECT (plugin->tasklist),
                         xfce_panel_plugin_get_property_base (panel_plugin),
                         properties, FALSE);

  /* show the tasklist */
  gtk_widget_show (plugin->tasklist);
}



static void
tasklist_plugin_mode_changed (XfcePanelPlugin *panel_plugin,
                              XfcePanelPluginMode mode)
{
  TasklistPlugin *plugin = TASKLIST_PLUGIN (panel_plugin);

  /* set the new tasklist mode */
  xfce_tasklist_set_mode (XFCE_TASKLIST (plugin->tasklist), mode);
}



static gboolean
tasklist_plugin_size_changed (XfcePanelPlugin *panel_plugin,
                              gint size)
{
  TasklistPlugin *plugin = TASKLIST_PLUGIN (panel_plugin);

  /* set the tasklist size */
  xfce_tasklist_set_size (XFCE_TASKLIST (plugin->tasklist), size);

  return TRUE;
}



static void
tasklist_plugin_nrows_changed (XfcePanelPlugin *panel_plugin,
                               guint nrows)
{
  TasklistPlugin *plugin = TASKLIST_PLUGIN (panel_plugin);

  /* set the tasklist nrows */
  xfce_tasklist_set_nrows (XFCE_TASKLIST (plugin->tasklist), nrows);
}



static void
tasklist_plugin_screen_position_changed (XfcePanelPlugin *panel_plugin,
                                         XfceScreenPosition position)
{
  TasklistPlugin *plugin = TASKLIST_PLUGIN (panel_plugin);

  /* update monitor geometry; this function is also triggered when
   * the panel is moved to another monitor during runtime */
  xfce_tasklist_update_monitor_geometry (XFCE_TASKLIST (plugin->tasklist));
}



static GPtrArray *
tasklist_plugin_pinned_get_ids (GtkWidget *tasklist)
{
  GPtrArray *values = NULL;
  GPtrArray *ids = g_ptr_array_new_with_free_func (g_free);

  g_object_get (tasklist, "pinned-apps", &values, NULL);
  if (values != NULL)
    {
      for (guint i = 0; i < values->len; i++)
        {
          GValue *value = g_ptr_array_index (values, i);
          if (G_VALUE_HOLDS_STRING (value) && g_value_get_string (value) != NULL)
            g_ptr_array_add (ids, g_strdup (g_value_get_string (value)));
        }
      g_ptr_array_unref (values);
    }

  return ids;
}


static void
tasklist_plugin_pinned_set_ids (GtkWidget *tasklist,
                                GPtrArray *ids)
{
  GPtrArray *values = g_ptr_array_new_with_free_func ((GDestroyNotify) g_free);

  for (guint i = 0; i < ids->len; i++)
    {
      GValue *value = g_new0 (GValue, 1);
      g_value_init (value, G_TYPE_STRING);
      g_value_set_string (value, g_ptr_array_index (ids, i));
      g_ptr_array_add (values, value);
    }

  g_object_set (tasklist, "pinned-apps", values, NULL);
  for (guint i = 0; i < values->len; i++)
    g_value_unset (g_ptr_array_index (values, i));
  g_ptr_array_unref (values);
}


static void
tasklist_plugin_pinned_refresh (GtkWidget *listbox)
{
  GtkWidget *tasklist = g_object_get_data (G_OBJECT (listbox), "tasklist");
  GList *children;
  GPtrArray *ids;

  children = gtk_container_get_children (GTK_CONTAINER (listbox));
  for (GList *li = children; li != NULL; li = li->next)
    gtk_widget_destroy (GTK_WIDGET (li->data));
  g_list_free (children);

  ids = tasklist_plugin_pinned_get_ids (tasklist);
  for (guint i = 0; i < ids->len; i++)
    {
      const gchar *desktop_id = g_ptr_array_index (ids, i);
      GDesktopAppInfo *app = g_desktop_app_info_new (desktop_id);
      GtkWidget *row = gtk_list_box_row_new ();
      GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
      GtkWidget *icon = gtk_image_new ();
      GtkWidget *labels = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      GtkWidget *name;
      GtkWidget *id_label;
      GtkWidget *up = gtk_button_new_from_icon_name ("go-up-symbolic", GTK_ICON_SIZE_BUTTON);
      GtkWidget *down = gtk_button_new_from_icon_name ("go-down-symbolic", GTK_ICON_SIZE_BUTTON);
      GtkWidget *remove = gtk_button_new_from_icon_name ("list-remove-symbolic", GTK_ICON_SIZE_BUTTON);
      const gchar *display_name = desktop_id;

      if (app != NULL)
        {
          GIcon *gicon = g_app_info_get_icon (G_APP_INFO (app));
          display_name = g_app_info_get_display_name (G_APP_INFO (app));
          if (gicon != NULL)
            gtk_image_set_from_gicon (GTK_IMAGE (icon), gicon, GTK_ICON_SIZE_BUTTON);
        }

      name = gtk_label_new (display_name);
      gtk_label_set_xalign (GTK_LABEL (name), 0.0);
      id_label = gtk_label_new (desktop_id);
      gtk_label_set_xalign (GTK_LABEL (id_label), 0.0);
      gtk_style_context_add_class (gtk_widget_get_style_context (id_label), "dim-label");

      gtk_box_pack_start (GTK_BOX (labels), name, FALSE, FALSE, 0);
      gtk_box_pack_start (GTK_BOX (labels), id_label, FALSE, FALSE, 0);
      gtk_box_pack_start (GTK_BOX (box), icon, FALSE, FALSE, 0);
      gtk_box_pack_start (GTK_BOX (box), labels, TRUE, TRUE, 0);
      gtk_box_pack_start (GTK_BOX (box), up, FALSE, FALSE, 0);
      gtk_box_pack_start (GTK_BOX (box), down, FALSE, FALSE, 0);
      gtk_box_pack_start (GTK_BOX (box), remove, FALSE, FALSE, 0);
      gtk_container_add (GTK_CONTAINER (row), box);
      gtk_container_add (GTK_CONTAINER (listbox), row);

      gtk_widget_set_sensitive (up, i > 0);
      gtk_widget_set_sensitive (down, i + 1 < ids->len);
      g_object_set_data_full (G_OBJECT (up), "desktop-id", g_strdup (desktop_id), g_free);
      g_object_set_data (G_OBJECT (up), "direction", GINT_TO_POINTER (-1));
      g_object_set_data_full (G_OBJECT (down), "desktop-id", g_strdup (desktop_id), g_free);
      g_object_set_data (G_OBJECT (down), "direction", GINT_TO_POINTER (1));
      g_object_set_data_full (G_OBJECT (remove), "desktop-id", g_strdup (desktop_id), g_free);
      g_signal_connect (up, "clicked", G_CALLBACK (tasklist_plugin_pinned_move_clicked), listbox);
      g_signal_connect (down, "clicked", G_CALLBACK (tasklist_plugin_pinned_move_clicked), listbox);
      g_signal_connect (remove, "clicked", G_CALLBACK (tasklist_plugin_pinned_remove_clicked), listbox);

      if (app != NULL)
        g_object_unref (app);
    }
  g_ptr_array_unref (ids);
  gtk_widget_show_all (listbox);
}


static void
tasklist_plugin_pinned_remove_clicked (GtkButton *button,
                                       GtkWidget *listbox)
{
  GtkWidget *tasklist = g_object_get_data (G_OBJECT (listbox), "tasklist");
  const gchar *desktop_id = g_object_get_data (G_OBJECT (button), "desktop-id");
  GPtrArray *ids = tasklist_plugin_pinned_get_ids (tasklist);

  for (guint i = 0; i < ids->len; i++)
    if (g_strcmp0 (g_ptr_array_index (ids, i), desktop_id) == 0)
      {
        g_ptr_array_remove_index (ids, i);
        break;
      }

  tasklist_plugin_pinned_set_ids (tasklist, ids);
  g_ptr_array_unref (ids);
  tasklist_plugin_pinned_refresh (listbox);
}


static void
tasklist_plugin_pinned_move_clicked (GtkButton *button,
                                     GtkWidget *listbox)
{
  GtkWidget *tasklist = g_object_get_data (G_OBJECT (listbox), "tasklist");
  const gchar *desktop_id = g_object_get_data (G_OBJECT (button), "desktop-id");
  gint direction = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "direction"));
  GPtrArray *ids = tasklist_plugin_pinned_get_ids (tasklist);

  for (guint i = 0; i < ids->len; i++)
    if (g_strcmp0 (g_ptr_array_index (ids, i), desktop_id) == 0)
      {
        gint target = (gint) i + direction;
        if (target >= 0 && target < (gint) ids->len)
          {
            gpointer tmp = ids->pdata[i];
            ids->pdata[i] = ids->pdata[target];
            ids->pdata[target] = tmp;
          }
        break;
      }

  tasklist_plugin_pinned_set_ids (tasklist, ids);
  g_ptr_array_unref (ids);
  tasklist_plugin_pinned_refresh (listbox);
}


static void
tasklist_plugin_drag_data_received (GtkWidget *widget,
                                    GdkDragContext *context,
                                    gint x,
                                    gint y,
                                    GtkSelectionData *data,
                                    guint info,
                                    guint time,
                                    TasklistPlugin *plugin)
{
  gchar **uris = gtk_selection_data_get_uris (data);
  gboolean success = FALSE;

  if (uris != NULL)
    success = xfce_tasklist_pin_uris (XFCE_TASKLIST (plugin->tasklist), uris);
  g_strfreev (uris);
  gtk_drag_finish (context, success, FALSE, time);
}



static void
tasklist_plugin_configure_plugin (XfcePanelPlugin *panel_plugin)
{
  TasklistPlugin *plugin = TASKLIST_PLUGIN (panel_plugin);
  GtkBuilder *builder;
  GObject *dialog;
  GObject *object;

  /* setup the dialog */
  builder = panel_utils_builder_new (panel_plugin, "/org/xfce/panel/tasklist-dialog.glade", &dialog);
  if (G_UNLIKELY (builder == NULL))
    return;

#define TASKLIST_DIALOG_BIND(name, property) \
  object = gtk_builder_get_object (builder, (name)); \
  panel_return_if_fail (G_IS_OBJECT (object)); \
  g_object_bind_property (G_OBJECT (plugin->tasklist), (name), \
                          G_OBJECT (object), (property), \
                          G_BINDING_BIDIRECTIONAL \
                            | G_BINDING_SYNC_CREATE);

#define TASKLIST_DIALOG_BIND_INV(name, property) \
  object = gtk_builder_get_object (builder, (name)); \
  panel_return_if_fail (G_IS_OBJECT (object)); \
  g_object_bind_property (G_OBJECT (plugin->tasklist), \
                          name, G_OBJECT (object), \
                          property, \
                          G_BINDING_BIDIRECTIONAL \
                            | G_BINDING_SYNC_CREATE \
                            | G_BINDING_INVERT_BOOLEAN);

  TASKLIST_DIALOG_BIND ("show-labels", "active")
  TASKLIST_DIALOG_BIND ("grouping", "active")
  TASKLIST_DIALOG_BIND ("include-all-workspaces", "active")
  TASKLIST_DIALOG_BIND ("include-all-monitors", "active")
  TASKLIST_DIALOG_BIND ("flat-buttons", "active")
  TASKLIST_DIALOG_BIND_INV ("switch-workspace-on-unminimize", "active")
  TASKLIST_DIALOG_BIND ("show-only-minimized", "active")
  TASKLIST_DIALOG_BIND ("show-wireframes", "active")
  TASKLIST_DIALOG_BIND ("show-handle", "active")
  TASKLIST_DIALOG_BIND ("show-tooltips", "active")
  TASKLIST_DIALOG_BIND ("sort-order", "active")
  TASKLIST_DIALOG_BIND ("window-scrolling", "active")
  TASKLIST_DIALOG_BIND ("middle-click", "active")

  if (!WINDOWING_IS_X11 ())
    {
      /* not functional in x11, so avoid confusion */
      object = gtk_builder_get_object (builder, "include-all-workspaces");
      gtk_widget_hide (GTK_WIDGET (object));
      object = gtk_builder_get_object (builder, "switch-workspace-on-unminimize");
      gtk_widget_hide (GTK_WIDGET (object));
      object = gtk_builder_get_object (builder, "show-wireframes");
      gtk_widget_hide (GTK_WIDGET (object));
    }

  {
    GtkWidget *vbox = GTK_WIDGET (gtk_builder_get_object (builder, "vbox1"));
    GtkWidget *behavior_box = GTK_WIDGET (gtk_builder_get_object (builder, "vbox2"));
    GtkWidget *expand = gtk_check_button_new_with_mnemonic (MINIOS_("_Fill all available panel space"));
    GtkWidget *frame = gtk_frame_new (NULL);
    GtkWidget *frame_label = gtk_label_new (NULL);
    GtkWidget *outer = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *scroller = gtk_scrolled_window_new (NULL, NULL);
    GtkWidget *listbox = gtk_list_box_new ();

    /* Keep new behavior controls inside the stock Behavior section. */
    gtk_box_pack_start (GTK_BOX (behavior_box), expand, FALSE, FALSE, 0);
    g_object_bind_property (G_OBJECT (panel_plugin), "expand",
                            G_OBJECT (expand), "active",
                            G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

    gtk_label_set_markup (GTK_LABEL (frame_label), MINIOS_("<b>Pinned applications</b>"));
    gtk_frame_set_label_widget (GTK_FRAME (frame), frame_label);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request (scroller, -1, 130);
    gtk_container_add (GTK_CONTAINER (scroller), listbox);
    gtk_box_pack_start (GTK_BOX (outer), scroller, TRUE, TRUE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (outer), 6);
    gtk_container_add (GTK_CONTAINER (frame), outer);
    gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);
    g_object_set_data (G_OBJECT (listbox), "tasklist", plugin->tasklist);
    tasklist_plugin_pinned_refresh (listbox);
  }

  gtk_widget_show_all (GTK_WIDGET (dialog));
}



static gboolean
tasklist_plugin_handle_draw (GtkWidget *widget,
                             cairo_t *cr,
                             TasklistPlugin *plugin)
{
  GtkAllocation allocation;
  GtkStyleContext *ctx;
  gdouble x, y;
  guint i;
  GdkRGBA fg_rgba;

  panel_return_val_if_fail (TASKLIST_IS_PLUGIN (plugin), FALSE);
  panel_return_val_if_fail (plugin->handle == widget, FALSE);

  if (!gtk_widget_is_drawable (widget))
    return FALSE;

  gtk_widget_get_allocation (widget, &allocation);
  ctx = gtk_widget_get_style_context (widget);

  gtk_style_context_get_color (ctx, gtk_widget_get_state_flags (widget), &fg_rgba);
  /* Tone down the foreground color a bit for the separators */
  fg_rgba.alpha = 0.5;
  gdk_cairo_set_source_rgba (cr, &fg_rgba);
  cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);

  x = (allocation.width - HANDLE_SIZE) / 2;
  y = (allocation.height - HANDLE_SIZE) / 2;
  cairo_set_line_width (cr, 1.0);
  /* draw the handle */
  for (i = 0; i < 3; i++)
    {
      if (xfce_panel_plugin_get_orientation (XFCE_PANEL_PLUGIN (plugin)) == GTK_ORIENTATION_HORIZONTAL)
        {
          cairo_move_to (cr, x, y + (i * HANDLE_SIZE) - (HANDLE_SIZE / 2));
          cairo_line_to (cr, x + HANDLE_SIZE, y + (i * HANDLE_SIZE) - (HANDLE_SIZE / 2));
        }
      else
        {
          cairo_move_to (cr, x + (i * HANDLE_SIZE) - (HANDLE_SIZE / 2), y);
          cairo_line_to (cr, x + (i * HANDLE_SIZE) - (HANDLE_SIZE / 2), y + HANDLE_SIZE);
        }
      cairo_stroke (cr);
    }

  return TRUE;
}
