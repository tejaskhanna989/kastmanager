/* Kast Manager 2.x — full controls: power, services, autostart
 * (Tejas Khanna, GPL-3.0-or-later). */
#include <string.h>

#include "kast.h"

typedef struct {
  KastUi *ui;
  GtkListBox *svc_list;
  GtkLabel *status;
} KastControls;

/* ---- power ---- */

static void
login1_call (const char *method)
{
  GError *err = NULL;
  GDBusConnection *bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &err);
  if (!bus)
    {
      g_warning ("system bus: %s", err ? err->message : "?");
      g_clear_error (&err);
      return;
    }
  GVariant *ret = g_dbus_connection_call_sync (bus,
      "org.freedesktop.login1", "/org/freedesktop/login1",
      "org.freedesktop.login1.Manager", method,
      g_variant_new ("(b)", TRUE),
      NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &err);
  if (!ret)
    {
      g_warning ("%s failed: %s", method, err ? err->message : "?");
      g_clear_error (&err);
    }
  else
    g_variant_unref (ret);
  g_object_unref (bus);
}

static void
on_power_response (GtkDialog *dlg, int resp, gpointer data)
{
  char *method = data;
  if (resp == GTK_RESPONSE_OK)
    login1_call (method);
  g_free (method);
  gtk_window_destroy (GTK_WINDOW (dlg));
}

static void
power_ask (KastControls *k, const char *heading, const char *method)
{
  GtkWidget *dlg = gtk_message_dialog_new (GTK_WINDOW (k->ui->window),
      GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE, "%s", heading);
  gtk_dialog_add_button (GTK_DIALOG (dlg), "Cancel", GTK_RESPONSE_CANCEL);
  GtkWidget *ok = gtk_dialog_add_button (GTK_DIALOG (dlg), heading, GTK_RESPONSE_OK);
  gtk_widget_add_css_class (ok, "destructive-action");
  gtk_dialog_set_default_response (GTK_DIALOG (dlg), GTK_RESPONSE_CANCEL);
  g_signal_connect (dlg, "response", G_CALLBACK (on_power_response), g_strdup (method));
  g_signal_connect_swapped (dlg, "response", G_CALLBACK (gtk_window_destroy), dlg);
  /* destroy runs after on_power_response (connected first) */
  gtk_window_present (GTK_WINDOW (dlg));
}

static void
power_simple (const char *cmd)
{
  char *argv[] = { "loginctl", (char *) cmd, NULL };
  g_spawn_async (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
}

static void
on_shutdown (GtkButton *, gpointer data) { power_ask (data, "Shut down?", "PowerOff"); }
static void
on_reboot (GtkButton *, gpointer data) { power_ask (data, "Restart?", "Reboot"); }
static void
on_suspend (GtkButton *, gpointer data) { login1_call ("Suspend"); }
static void
on_lock (GtkButton *, gpointer data) { power_simple ("lock-session"); }
static void
on_logout (GtkButton *, gpointer data)
{
  KastControls *k = data;
  const char *sess = g_getenv ("XDG_SESSION_ID");
  if (sess && *sess)
    {
      char *argv[] = { "loginctl", "terminate-session", (char *) sess, NULL };
      g_spawn_async (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
    }
  else
    gtk_label_set_text (k->status, "No session ID for logout");
}

/* ---- services ---- */

static void
svc_run (KastControls *k, const char *verb, const char *unit)
{
  char *pidinfo = g_strdup_printf ("%s %s", verb, unit);
  char *argv[] = { "pkexec", "systemctl", (char *) verb, (char *) unit, NULL };
  GError *err = NULL;
  g_spawn_async (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &err);
  if (err)
    {
      gtk_label_set_text (k->status, err->message);
      g_error_free (err);
    }
  else
    {
      char *msg = g_strdup_printf ("%s requested, refreshing…", pidinfo);
      gtk_label_set_text (k->status, msg);
      g_free (msg);
    }
  g_free (pidinfo);
}

static const char *
svc_selected (KastControls *k)
{
  GtkListBoxRow *row = gtk_list_box_get_selected_row (k->svc_list);
  if (!row)
    return NULL;
  return g_object_get_data (G_OBJECT (row), "kast-unit");
}

static void
on_svc_start (GtkButton *, gpointer data)
{
  KastControls *k = data;
  const char *u = svc_selected (k);
  if (u)
    svc_run (k, "start", u);
}
static void
on_svc_stop (GtkButton *, gpointer data)
{
  KastControls *k = data;
  const char *u = svc_selected (k);
  if (u)
    svc_run (k, "stop", u);
}
static void
on_svc_restart (GtkButton *, gpointer data)
{
  KastControls *k = data;
  const char *u = svc_selected (k);
  if (u)
    svc_run (k, "restart", u);
}

static void
refresh_services (KastControls *k)
{
  GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (k->svc_list));
  while (child)
    {
      GtkWidget *next = gtk_widget_get_next_sibling (child);
      gtk_list_box_remove (k->svc_list, child);
      child = next;
    }

  char *out = NULL;
  char *argv[] = { "systemctl", "list-units", "--type=service", "--all",
                   "--no-legend", "--plain", NULL };
  GError *err = NULL;
  if (!g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                     &out, NULL, NULL, &err))
    {
      gtk_label_set_text (k->status, err ? err->message : "systemctl failed");
      g_clear_error (&err);
      return;
    }

  char **lines = g_strsplit (out ? out : "", "\n", -1);
  for (int i = 0; lines[i]; i++)
    {
      char *line = g_strstrip (lines[i]);
      if (!*line)
        continue;
      /* UNIT LOAD ACTIVE SUB DESCRIPTION… */
      char **tok = g_strsplit_set (line, " \t", 6);
      int n = g_strv_length (tok);
      if (n < 5)
        {
          g_strfreev (tok);
          continue;
        }
      /* collapse: first 4 non-empty tokens, rest is description */
      char *cols[5] = { NULL, NULL, NULL, NULL, NULL };
      int c = 0;
      for (int j = 0; tok[j] && c < 4; j++)
        {
          if (*tok[j])
            cols[c++] = tok[j];
        }
      if (c < 4)
        {
          g_strfreev (tok);
          continue;
        }
      const char *desc = line + strlen (cols[0]);
      /* description = remainder after 4th token; approximate */
      GtkWidget *row = gtk_list_box_row_new ();
      g_object_set_data_full (G_OBJECT (row), "kast-unit", g_strdup (cols[0]), g_free);

      GtkWidget *grid = gtk_grid_new ();
      gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
      gtk_widget_set_margin_top (grid, 5);
      gtk_widget_set_margin_bottom (grid, 5);
      gtk_widget_set_margin_start (grid, 10);
      gtk_widget_set_margin_end (grid, 10);
      gtk_widget_set_hexpand (grid, TRUE);
      gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), grid);

      GtkWidget *un = gtk_label_new (cols[0]);
      gtk_widget_set_halign (un, GTK_ALIGN_START);
      gtk_widget_set_hexpand (un, TRUE);
      gtk_label_set_ellipsize (GTK_LABEL (un), PANGO_ELLIPSIZE_END);
      gtk_grid_attach (GTK_GRID (grid), un, 0, 0, 1, 1);

      GtkWidget *st = gtk_label_new (cols[2]);
      gtk_widget_set_size_request (st, 80, -1);
      if (!g_strcmp0 (cols[2], "active"))
        gtk_widget_add_css_class (st, "dash-accent-swap");
      else if (!g_strcmp0 (cols[2], "failed"))
        gtk_widget_add_css_class (st, "dash-accent-uptime");
      else
        gtk_widget_add_css_class (st, "dim-label");
      gtk_grid_attach (GTK_GRID (grid), st, 1, 0, 1, 1);

      GtkWidget *sb = gtk_label_new (cols[3]);
      gtk_widget_add_css_class (sb, "dim-label");
      gtk_widget_set_size_request (sb, 90, -1);
      gtk_grid_attach (GTK_GRID (grid), sb, 2, 0, 1, 1);

      gtk_list_box_append (k->svc_list, row);
      (void) desc;
      g_strfreev (tok);
    }
  g_strfreev (lines);
  g_free (out);
  gtk_label_set_text (k->status, "");
}

static void
on_svc_refresh (GtkButton *, gpointer data) { refresh_services (data); }

/* ---- autostart ---- */

static gboolean on_autostart_toggle (GtkSwitch *, gboolean, gpointer);

static void
refresh_autostart (GtkBox *box)
{
  GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (box));
  while (child)
    {
      GtkWidget *next = gtk_widget_get_next_sibling (child);
      gtk_box_remove (box, child);
      child = next;
    }

  char *dir = g_build_filename (g_get_user_config_dir (), "autostart", NULL);
  GDir *d = g_dir_open (dir, 0, NULL);
  if (!d)
    {
      GtkWidget *l = gtk_label_new ("No autostart entries (~/.config/autostart).");
      gtk_widget_add_css_class (l, "dim-label");
      gtk_widget_set_halign (l, GTK_ALIGN_START);
      gtk_box_append (box, l);
      g_free (dir);
      return;
    }

  const char *name;
  while ((name = g_dir_read_name (d)) != NULL)
    {
      if (!g_str_has_suffix (name, ".desktop"))
        continue;
      char *path = g_build_filename (dir, name, NULL);
      GKeyFile *kf = g_key_file_new ();
      GError *err = NULL;
      g_key_file_load_from_file (kf, path, G_KEY_FILE_NONE, &err);
      if (err)
        {
          g_clear_error (&err);
          g_key_file_free (kf);
          g_free (path);
          continue;
        }
      char *disp = g_key_file_get_string (kf, "Desktop Entry", "Name", NULL);
      if (!disp)
        disp = g_strdup (name);
      gboolean hidden = g_key_file_get_boolean (kf, "Desktop Entry", "Hidden", NULL);

      GtkWidget *row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
      GtkWidget *lab = gtk_label_new (disp);
      gtk_widget_set_halign (lab, GTK_ALIGN_START);
      gtk_widget_set_hexpand (lab, TRUE);
      gtk_label_set_ellipsize (GTK_LABEL (lab), PANGO_ELLIPSIZE_END);
      gtk_box_append (GTK_BOX (row), lab);

      GtkWidget *sw = gtk_switch_new ();
      gtk_switch_set_active (GTK_SWITCH (sw), !hidden);
      g_object_set_data_full (G_OBJECT (sw), "kast-path", path, g_free);
      g_signal_connect (sw, "state-set", G_CALLBACK (on_autostart_toggle),
                        g_object_get_data (G_OBJECT (sw), "kast-path"));
      gtk_box_append (GTK_BOX (row), sw);
      gtk_box_append (box, row);

      g_free (disp);
      g_key_file_free (kf);
    }
  g_dir_close (d);
  g_free (dir);
}

static gboolean
on_autostart_toggle (GtkSwitch *, gboolean state, gpointer data)
{
  char *path = data;
  GKeyFile *kf = g_key_file_new ();
  GError *err = NULL;
  g_key_file_load_from_file (kf, path, G_KEY_FILE_KEEP_COMMENTS, &err);
  if (!err)
    {
      g_key_file_set_boolean (kf, "Desktop Entry", "Hidden", !state);
      g_key_file_save_to_file (kf, path, &err);
    }
  if (err)
    {
      g_warning ("autostart save: %s", err->message);
      g_error_free (err);
    }
  g_key_file_free (kf);
  return FALSE; /* let the switch flip */
}

GtkWidget *
kast_controls_page (KastUi *ui)
{
  KastControls *k = g_new0 (KastControls, 1);
  k->ui = ui;

  GtkWidget *scroll = gtk_scrolled_window_new ();
  gtk_widget_set_hexpand (scroll, TRUE);
  gtk_widget_set_vexpand (scroll, TRUE);

  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_widget_set_margin_top (box, 20);
  gtk_widget_set_margin_bottom (box, 24);
  gtk_widget_set_margin_start (box, 24);
  gtk_widget_set_margin_end (box, 24);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scroll), box);

  GtkWidget *title = gtk_label_new ("Full controls");
  gtk_widget_add_css_class (title, "kast-neon-title");
  gtk_widget_set_halign (title, GTK_ALIGN_START);
  gtk_box_append (GTK_BOX (box), title);

  /* power */
  GtkWidget *pcard = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_add_css_class (pcard, "card");
  GtkWidget *pt = gtk_label_new ("Power");
  PangoAttrList *pa = pango_attr_list_new ();
  pango_attr_list_insert (pa, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
  gtk_label_set_attributes (GTK_LABEL (pt), pa);
  pango_attr_list_unref (pa);
  gtk_widget_set_halign (pt, GTK_ALIGN_START);
  gtk_box_append (GTK_BOX (pcard), pt);
  GtkWidget *prow = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  const char *plabels[] = { "Shut down", "Restart", "Suspend", "Lock", "Log out" };
  GCallback pfuncs[] = { G_CALLBACK (on_shutdown), G_CALLBACK (on_reboot),
      G_CALLBACK (on_suspend), G_CALLBACK (on_lock), G_CALLBACK (on_logout) };
  for (int i = 0; i < 5; i++)
    {
      GtkWidget *b = gtk_button_new_with_label (plabels[i]);
      if (i < 2)
        gtk_widget_add_css_class (b, "destructive-action");
      g_signal_connect (b, "clicked", pfuncs[i], k);
      gtk_box_append (GTK_BOX (prow), b);
    }
  gtk_box_append (GTK_BOX (pcard), prow);
  gtk_box_append (GTK_BOX (box), pcard);

  /* services */
  GtkWidget *scard = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_add_css_class (scard, "card");
  GtkWidget *shead = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  GtkWidget *st = gtk_label_new ("Services (systemd, changes ask for password)");
  gtk_widget_set_halign (st, GTK_ALIGN_START);
  gtk_widget_set_hexpand (st, TRUE);
  PangoAttrList *sa = pango_attr_list_new ();
  pango_attr_list_insert (sa, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
  gtk_label_set_attributes (GTK_LABEL (st), sa);
  pango_attr_list_unref (sa);
  gtk_box_append (GTK_BOX (shead), st);
  GtkWidget *sref = gtk_button_new_with_label ("Refresh");
  g_signal_connect (sref, "clicked", G_CALLBACK (on_svc_refresh), k);
  gtk_box_append (GTK_BOX (shead), sref);
  gtk_box_append (GTK_BOX (scard), shead);

  GtkWidget *sscroll = gtk_scrolled_window_new ();
  gtk_widget_set_size_request (sscroll, -1, 260);
  gtk_widget_set_vexpand (sscroll, FALSE);
  gtk_box_append (GTK_BOX (scard), sscroll);
  k->svc_list = GTK_LIST_BOX (gtk_list_box_new ());
  gtk_list_box_set_selection_mode (k->svc_list, GTK_SELECTION_SINGLE);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sscroll), GTK_WIDGET (k->svc_list));
  gtk_box_append (GTK_BOX (box), scard);

  GtkWidget *srow = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  GtkWidget *bstart = gtk_button_new_with_label ("Start");
  GtkWidget *bstop = gtk_button_new_with_label ("Stop");
  GtkWidget *brestart = gtk_button_new_with_label ("Restart");
  gtk_widget_add_css_class (bstop, "destructive-action");
  g_signal_connect (bstart, "clicked", G_CALLBACK (on_svc_start), k);
  g_signal_connect (bstop, "clicked", G_CALLBACK (on_svc_stop), k);
  g_signal_connect (brestart, "clicked", G_CALLBACK (on_svc_restart), k);
  gtk_box_append (GTK_BOX (srow), bstart);
  gtk_box_append (GTK_BOX (srow), bstop);
  gtk_box_append (GTK_BOX (srow), brestart);
  gtk_box_append (GTK_BOX (scard), srow);

  k->status = GTK_LABEL (gtk_label_new (""));
  gtk_widget_add_css_class (GTK_WIDGET (k->status), "dim-label");
  gtk_widget_set_halign (GTK_WIDGET (k->status), GTK_ALIGN_START);
  gtk_box_append (GTK_BOX (scard), GTK_WIDGET (k->status));

  /* autostart */
  GtkWidget *acard = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_add_css_class (acard, "card");
  GtkWidget *at = gtk_label_new ("Start automatically");
  PangoAttrList *aa = pango_attr_list_new ();
  pango_attr_list_insert (aa, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
  gtk_label_set_attributes (GTK_LABEL (at), aa);
  pango_attr_list_unref (aa);
  gtk_widget_set_halign (at, GTK_ALIGN_START);
  gtk_box_append (GTK_BOX (acard), at);
  GtkWidget *alist = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_box_append (GTK_BOX (acard), alist);
  refresh_autostart (GTK_BOX (alist));
  gtk_box_append (GTK_BOX (box), acard);

  refresh_services (k);
  g_object_set_data_full (G_OBJECT (scroll), "kast-controls", k, g_free);
  return scroll;
}
