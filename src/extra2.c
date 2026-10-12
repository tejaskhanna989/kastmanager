/* Kast Manager 2.x — more tools batch 2 (Tejas Khanna, GPL-3.0-or-later).
 * Network, bluetooth, printers, audio, displays, input, power,
 * fonts, environment, trash, clipboard, shortcuts. */
#include <string.h>
#include <stdio.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <net/if.h>

#include "kast.h"


/* ---------- 11. network interfaces ---------- */

GtkWidget *
kast_netif_page (KastUi *ui)
{
  (void) ui;
  GtkWidget *list = NULL;
  GtkWidget *page = kast_tool_shell ("Network interfaces", &list);
  struct ifaddrs *addrs = NULL;
  if (getifaddrs (&addrs) == 0)
    {
      /* group addrs per interface */
      GHashTable *seen = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
      for (struct ifaddrs *ifa = addrs; ifa; ifa = ifa->ifa_next)
        {
          if (!ifa->ifa_addr)
            continue;
          int fam = ifa->ifa_addr->sa_family;
          if (fam != AF_INET && fam != AF_INET6 && fam != AF_PACKET)
            continue;
          if (!g_hash_table_contains (seen, ifa->ifa_name))
            {
              g_hash_table_add (seen, g_strdup (ifa->ifa_name));
              char *macp = g_strdup_printf ("/sys/class/net/%s/address", ifa->ifa_name);
              char *stp = g_strdup_printf ("/sys/class/net/%s/operstate", ifa->ifa_name);
              char *mac = NULL, *st = NULL;
              g_file_get_contents (macp, &mac, NULL, NULL);
              g_file_get_contents (stp, &st, NULL, NULL);
              char *right = g_strdup_printf ("%s · %s",
                  mac ? g_strstrip (mac) : "?",
                  st ? g_strstrip (st) : "?");
              kast_tool_row (list, ifa->ifa_name, right);
              g_free (right); g_free (mac); g_free (st);
              g_free (macp); g_free (stp);
            }
          if (fam == AF_INET || fam == AF_INET6)
            {
              char host[NI_MAXHOST] = { 0 };
              if (getnameinfo (ifa->ifa_addr,
                      fam == AF_INET ? sizeof (struct sockaddr_in)
                                     : sizeof (struct sockaddr_in6),
                      host, sizeof host, NULL, 0, NI_NUMERICHOST) == 0)
                {
                  char *left = g_strdup_printf ("  %s", ifa->ifa_name);
                  kast_tool_row (list, left, host);
                  g_free (left);
                }
            }
        }
      g_hash_table_destroy (seen);
      freeifaddrs (addrs);
    }
  else
    kast_tool_row (list, "Cannot list interfaces", NULL);
  return page;
}

/* ---------- 12. bluetooth ---------- */

GtkWidget *
kast_blue_page (KastUi *ui)
{
  (void) ui;
  GtkWidget *list = NULL;
  GtkWidget *page = kast_tool_shell ("Bluetooth", &list);
  if (!kast_have_tool ("bluetoothctl"))
    {
      kast_tool_row (list, "bluetoothctl not found", NULL);
      return page;
    }
  char *out = NULL;
  char *argv[] = { "bluetoothctl", "devices", NULL };
  if (g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                    &out, NULL, NULL, NULL) && out)
    {
      char **lines = g_strsplit (out, "\n", -1);
      int n = 0;
      for (int i = 0; lines[i]; i++)
        {
          char *line = g_strstrip (lines[i]);
          if (!*line || !g_str_has_prefix (line, "Device "))
            continue;
          /* "Device XX:XX:... Name..." */
          char *sp1 = strchr (line + 7, ' ');
          if (!sp1)
            continue;
          *sp1 = '\0';
          kast_tool_row (list, sp1 + 1, line + 7);
          n++;
        }
      if (!n)
        kast_tool_row (list, "No paired devices", NULL);
      g_strfreev (lines);
    }
  g_free (out);
  return page;
}

/* ---------- 13. printers ---------- */

GtkWidget *
kast_printers_page (KastUi *ui)
{
  (void) ui;
  GtkWidget *list = NULL;
  GtkWidget *page = kast_tool_shell ("Printers", &list);
  if (!kast_have_tool ("lpstat"))
    {
      kast_tool_row (list, "lpstat not found (install cups)", NULL);
      return page;
    }
  char *out = NULL;
  char *argv[] = { "lpstat", "-p", "-d", NULL };
  if (g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                    &out, NULL, NULL, NULL) && out)
    {
      char **lines = g_strsplit (out, "\n", -1);
      int n = 0;
      for (int i = 0; lines[i]; i++)
        {
          if (!*g_strstrip (lines[i]))
            continue;
          kast_tool_row (list, g_strstrip (lines[i]), NULL);
          n++;
        }
      if (!n)
        kast_tool_row (list, "No printers configured", NULL);
      g_strfreev (lines);
    }
  g_free (out);
  return page;
}

/* ---------- 14. audio ---------- */

GtkWidget *
kast_audio_page (KastUi *ui)
{
  (void) ui;
  GtkWidget *list = NULL;
  GtkWidget *page = kast_tool_shell ("Audio devices", &list);
  if (kast_have_tool ("pactl"))
    {
      for (int kind = 0; kind < 2; kind++)
        {
          char *out = NULL;
          char *argv[] = { "pactl", "list", kind ? "sources" : "sinks", NULL };
          if (!g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                             &out, NULL, NULL, NULL) || !out)
            continue;
          kast_tool_row (list, kind ? "— Sources —" : "— Sinks —", NULL);
          char *name = NULL, *desc = NULL, *state = NULL;
          char **lines = g_strsplit (out, "\n", -1);
          for (int i = 0; lines[i]; i++)
            {
              char *line = lines[i];
              while (*line == ' ' || *line == '\t')
                line++;
              if (g_str_has_prefix (line, "Name: "))
                {
                  if (name)
                    {
                      char *right = g_strdup_printf ("%s · %s",
                          state ? state : "?", desc ? desc : "");
                      kast_tool_row (list, name, right);
                      g_free (right);
                    }
                  g_free (name); g_free (desc); g_free (state);
                  name = g_strdup (g_strstrip (line + 6));
                  desc = state = NULL;
                }
              else if (g_str_has_prefix (line, "Description: "))
                {
                  g_free (desc);
                  desc = g_strdup (g_strstrip (line + 13));
                }
              else if (g_str_has_prefix (line, "State: "))
                {
                  g_free (state);
                  state = g_strdup (g_strstrip (line + 7));
                }
            }
          if (name)
            {
              char *right = g_strdup_printf ("%s · %s",
                  state ? state : "?", desc ? desc : "");
              kast_tool_row (list, name, right);
              g_free (right);
            }
          g_free (name); g_free (desc); g_free (state);
          g_strfreev (lines);
          g_free (out);
        }
    }
  else
    kast_tool_row (list, "Neither pactl nor wpctl found", NULL);
  return page;
}

/* ---------- 15. displays ---------- */

GtkWidget *
kast_displays_page (KastUi *ui)
{
  (void) ui;
  GtkWidget *list = NULL;
  GtkWidget *page = kast_tool_shell ("Displays", &list);
  GdkDisplay *dpy = gdk_display_get_default ();
  if (!dpy)
    {
      kast_tool_row (list, "No display", NULL);
      return page;
    }
  GListModel *mons = gdk_display_get_monitors (dpy);
  guint n = g_list_model_get_n_items (mons);
  for (guint i = 0; i < n; i++)
    {
      GdkMonitor *m = g_list_model_get_item (mons, i);
      GdkRectangle geo;
      gdk_monitor_get_geometry (m, &geo);
      int scale = gdk_monitor_get_scale_factor (m);
      int hz = 0;
#if GTK_CHECK_VERSION(4, 8, 0)
      hz = (int) (gdk_monitor_get_refresh_rate (m) / 1000);
#endif
      const char *manuf = gdk_monitor_get_manufacturer (m);
      const char *model = gdk_monitor_get_model (m);
      char *left = g_strdup_printf ("%s %s",
          manuf ? manuf : "Display", model ? model : "");
      char *right;
      if (hz > 0)
        right = g_strdup_printf ("%dx%d @ %dHz · ×%d", geo.width, geo.height, hz, scale);
      else
        right = g_strdup_printf ("%dx%d · ×%d", geo.width, geo.height, scale);
      kast_tool_row (list, left, right);
      g_free (left); g_free (right);
      g_object_unref (m);
    }
  if (!n)
    kast_tool_row (list, "No monitors found", NULL);
  return page;
}

/* ---------- 16. input devices ---------- */

GtkWidget *
kast_input_page (KastUi *ui)
{
  (void) ui;
  GtkWidget *list = NULL;
  GtkWidget *page = kast_tool_shell ("Input devices", &list);
  char *text = NULL;
  if (!g_file_get_contents ("/proc/bus/input/devices", &text, NULL, NULL))
    {
      kast_tool_row (list, "Cannot read input devices", NULL);
      return page;
    }
  char *name = NULL, *handlers = NULL;
  char **lines = g_strsplit (text, "\n", -1);
  for (int i = 0; lines[i]; i++)
    {
      if (g_str_has_prefix (lines[i], "N: Name="))
        {
          g_free (name);
          name = g_strdup (g_strstrip (lines[i] + 8));
          if (*name == '"')
            {
              memmove (name, name + 1, strlen (name));
              char *q = strrchr (name, '"');
              if (q)
                *q = '\0';
            }
        }
      else if (g_str_has_prefix (lines[i], "H: Handlers="))
        {
          g_free (handlers);
          handlers = g_strdup (g_strstrip (lines[i] + 12));
        }
      else if (!*lines[i] && name)
        {
          kast_tool_row (list, name, handlers);
          g_free (name); g_free (handlers);
          name = handlers = NULL;
        }
    }
  if (name)
    {
      kast_tool_row (list, name, handlers);
      g_free (name); g_free (handlers);
    }
  g_strfreev (lines);
  g_free (text);
  return page;
}

/* ---------- 17. power profiles ---------- */

static void
power_set_profile (const char *profile, GtkLabel *status)
{
  char *argv[] = { "powerprofilesctl", "set", (char *) profile, NULL };
  GError *err = NULL;
  if (!g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                     NULL, NULL, NULL, &err))
    {
      gtk_label_set_text (status, err ? err->message : "Failed");
      g_clear_error (&err);
    }
  else
    {
      char *t = g_strdup_printf ("Profile: %s", profile);
      gtk_label_set_text (status, t);
      g_free (t);
    }
}

static void
on_pp_set (GtkButton *btn, gpointer data)
{
  GtkLabel *status = data;
  power_set_profile (g_object_get_data (G_OBJECT (btn), "kast-prof"), status);
}

GtkWidget *
kast_power_page (KastUi *ui)
{
  (void) ui;
  GtkWidget *list = NULL;
  GtkWidget *page = kast_tool_shell ("Power profiles", &list);
  GtkWidget *box = gtk_widget_get_parent (gtk_widget_get_parent (list));
  if (!kast_have_tool ("powerprofilesctl"))
    {
      kast_tool_row (list, "powerprofilesctl not found", NULL);
      return page;
    }
  char *out = NULL;
  char *argv[] = { "powerprofilesctl", "get", NULL };
  if (g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                    &out, NULL, NULL, NULL) && out)
    {
      char *cur = g_strdup_printf ("Current: %s", g_strstrip (out));
      kast_tool_row (list, cur, NULL);
      g_free (cur);
    }
  g_free (out);

  GtkWidget *row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  const char *profs[] = { "performance", "balanced", "power-saver" };
  GtkWidget *status = gtk_label_new ("");
  for (int i = 0; i < 3; i++)
    {
      GtkWidget *b = gtk_button_new_with_label (profs[i]);
      g_object_set_data (G_OBJECT (b), "kast-prof", (gpointer) profs[i]);
      g_signal_connect (b, "clicked", G_CALLBACK (on_pp_set), status);
      gtk_box_append (GTK_BOX (row), b);
    }
  gtk_box_append (GTK_BOX (box), row);
  gtk_box_reorder_child_after (GTK_BOX (box), row, gtk_widget_get_first_child (box));
  gtk_widget_add_css_class (status, "dim-label");
  gtk_widget_set_halign (status, GTK_ALIGN_START);
  gtk_box_append (GTK_BOX (box), status);
  return page;
}

/* ---------- 18. fonts ---------- */

GtkWidget *
kast_fonts_page (KastUi *ui)
{
  (void) ui;
  GtkWidget *list = NULL;
  GtkWidget *page = kast_tool_shell ("Fonts", &list);
  if (!kast_have_tool ("fc-list"))
    {
      kast_tool_row (list, "fc-list not found", NULL);
      return page;
    }
  char *out = NULL;
  char *argv[] = { "fc-list", ":", "family", NULL };
  if (g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                    &out, NULL, NULL, NULL) && out)
    {
      GHashTable *seen = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
      char **lines = g_strsplit (out, "\n", -1);
      int n = 0;
      for (int i = 0; lines[i] && n < 300; i++)
        {
          char *fam = g_strstrip (lines[i]);
          char *comma = strchr (fam, ',');
          if (comma)
            *comma = '\0';
          fam = g_strstrip (fam);
          if (!*fam || g_hash_table_contains (seen, fam))
            continue;
          g_hash_table_add (seen, g_strdup (fam));
          kast_tool_row (list, fam, NULL);
          n++;
        }
      char *note = g_strdup_printf ("%d families shown", n);
      kast_tool_row (list, note, NULL);
      g_free (note);
      g_hash_table_destroy (seen);
      g_strfreev (lines);
    }
  g_free (out);
  return page;
}

/* ---------- 19. environment ---------- */

extern char **environ;

GtkWidget *
kast_env_page (KastUi *ui)
{
  (void) ui;
  GtkWidget *list = NULL;
  GtkWidget *page = kast_tool_shell ("Environment variables", &list);
  for (char **e = environ; e && *e; e++)
    {
      char *eq = strchr (*e, '=');
      if (!eq)
        continue;
      char *key = g_strndup (*e, eq - *e);
      kast_tool_row (list, key, eq + 1);
      g_free (key);
    }
  return page;
}

/* ---------- 20. trash ---------- */

static void
trash_fill (GtkWidget *list)
{
  kast_tool_clear (list);
  char *dir = g_build_filename (g_get_home_dir (), ".local/share/Trash/files", NULL);
  GDir *d = g_dir_open (dir, 0, NULL);
  int n = 0;
  if (d)
    {
      const char *name;
      while ((name = g_dir_read_name (d)) != NULL)
        {
          char *ip = g_build_filename (g_get_home_dir (),
              ".local/share/Trash/info", NULL);
          char *infon = g_strdup_printf ("%s.trashinfo", name);
          char *ipath = g_build_filename (ip, infon, NULL);
          char *orig = NULL;
          char *itxt = NULL;
          if (g_file_get_contents (ipath, &itxt, NULL, NULL))
            {
              char *p = strstr (itxt, "Path=");
              if (p)
                {
                  p += 5;
                  char *nl = strchr (p, '\n');
                  orig = g_strndup (p, nl ? (gsize)(nl - p) : strlen (p));
                }
              g_free (itxt);
            }
          GtkWidget *row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
          gtk_widget_set_margin_top (row, 3);
          gtk_widget_set_margin_bottom (row, 3);
          gtk_widget_set_margin_start (row, 10);
          gtk_widget_set_margin_end (row, 10);
          GtkWidget *la = gtk_label_new (name);
          gtk_widget_set_halign (la, GTK_ALIGN_START);
          gtk_widget_set_hexpand (la, TRUE);
          gtk_label_set_ellipsize (GTK_LABEL (la), PANGO_ELLIPSIZE_MIDDLE);
          gtk_box_append (GTK_BOX (row), la);
          if (orig)
            {
              GtkWidget *lb = gtk_label_new (orig);
              gtk_widget_add_css_class (lb, "dim-label");
              gtk_label_set_ellipsize (GTK_LABEL (lb), PANGO_ELLIPSIZE_START);
              gtk_box_append (GTK_BOX (row), lb);
              g_free (orig);
            }
          GtkWidget *lrow = gtk_list_box_row_new ();
          gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (lrow), row);
          g_object_set_data_full (G_OBJECT (lrow), "kast-name", g_strdup (name), g_free);
          gtk_list_box_append (GTK_LIST_BOX (list), lrow);
          n++;
          g_free (ip); g_free (infon); g_free (ipath);
        }
      g_dir_close (d);
    }
  g_free (dir);
  if (!n)
    kast_tool_row (list, "Trash is empty", NULL);
}

static void
on_trash_refresh (GtkButton *, gpointer data) { trash_fill (data); }

static void
on_trash_restore (GtkButton *, gpointer data)
{
  GtkWidget **pack = data; /* { list, status } */
  GtkListBoxRow *row = gtk_list_box_get_selected_row (GTK_LIST_BOX (pack[0]));
  if (!row)
    {
      gtk_label_set_text (GTK_LABEL (pack[1]), "Select an item first");
      return;
    }
  const char *name = g_object_get_data (G_OBJECT (row), "kast-name");
  char *ip = g_build_filename (g_get_home_dir (), ".local/share/Trash/info", NULL);
  char *infon = g_strdup_printf ("%s.trashinfo", name);
  char *ipath = g_build_filename (ip, infon, NULL);
  char *itxt = NULL;
  char *orig = NULL;
  if (g_file_get_contents (ipath, &itxt, NULL, NULL))
    {
      char *p = strstr (itxt, "Path=");
      if (p)
        {
          p += 5;
          char *nl = strchr (p, '\n');
          orig = g_strndup (p, nl ? (gsize)(nl - p) : strlen (p));
        }
      g_free (itxt);
    }
  if (orig)
    {
      char *src = g_build_filename (g_get_home_dir (), ".local/share/Trash/files", name, NULL);
      GFile *s = g_file_new_for_path (src);
      GFile *d = g_file_new_for_path (orig);
      GError *err = NULL;
      if (!g_file_move (s, d, G_FILE_COPY_NONE, NULL, NULL, NULL, &err))
        {
          gtk_label_set_text (GTK_LABEL (pack[1]), err ? err->message : "Cannot restore");
          g_clear_error (&err);
        }
      else
        {
          remove (ipath);
          trash_fill (pack[0]);
        }
      g_object_unref (s);
      g_object_unref (d);
      g_free (src);
      g_free (orig);
    }
  g_free (ip); g_free (infon); g_free (ipath);
}

static void
on_trash_empty (GtkButton *, gpointer data)
{
  GtkWidget **pack = data;
  const char *subs[] = { "files", "info" };
  for (int i = 0; i < 2; i++)
    {
      char *dir = g_build_filename (g_get_home_dir (), ".local/share/Trash", subs[i], NULL);
      GDir *d = g_dir_open (dir, 0, NULL);
      if (d)
        {
          const char *name;
          while ((name = g_dir_read_name (d)) != NULL)
            {
              char *p = g_build_filename (dir, name, NULL);
              /* recursive rm via GFile */
              GFile *f = g_file_new_for_path (p);
              GError *err = NULL;
              /* try trash-delete: enumerate children first */
              GFileEnumerator *en = g_file_enumerate_children (f, "standard::name",
                  G_FILE_QUERY_INFO_NONE, NULL, NULL);
              if (en)
                {
                  GFileInfo *info;
                  while ((info = g_file_enumerator_next_file (en, NULL, NULL)) != NULL)
                    {
                      GFile *c = g_file_get_child (f, g_file_info_get_name (info));
                      g_file_delete (c, NULL, NULL);
                      g_object_unref (c);
                      g_object_unref (info);
                    }
                  g_file_enumerator_close (en, NULL, NULL);
                  g_object_unref (en);
                }
              g_file_delete (f, NULL, &err);
              g_clear_error (&err);
              g_object_unref (f);
              g_free (p);
            }
          g_dir_close (d);
        }
      g_free (dir);
    }
  trash_fill (pack[0]);
  gtk_label_set_text (GTK_LABEL (pack[1]), "Trash emptied.");
}

GtkWidget *
kast_trash_page (KastUi *ui)
{
  (void) ui;
  GtkWidget *list = NULL;
  GtkWidget *page = kast_tool_shell ("Trash", &list);
  /* selectable rows */
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (gtk_widget_get_parent (list) ? list : list),
                                   GTK_SELECTION_SINGLE);
  GtkWidget *box = gtk_widget_get_parent (gtk_widget_get_parent (list));
  GtkWidget *row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  GtkWidget *bref = gtk_button_new_with_label ("Refresh");
  GtkWidget *brestore = gtk_button_new_with_label ("Restore");
  GtkWidget *bempty = gtk_button_new_with_label ("Empty trash");
  gtk_widget_add_css_class (bempty, "destructive-action");
  GtkWidget *status = gtk_label_new ("");
  gtk_widget_add_css_class (status, "dim-label");
  gtk_widget_set_halign (status, GTK_ALIGN_START);
  gtk_widget_set_hexpand (status, TRUE);
  static GtkWidget *pack[2];
  pack[0] = list;
  pack[1] = status;
  g_signal_connect (bref, "clicked", G_CALLBACK (on_trash_refresh), list);
  g_signal_connect (brestore, "clicked", G_CALLBACK (on_trash_restore), pack);
  g_signal_connect (bempty, "clicked", G_CALLBACK (on_trash_empty), pack);
  gtk_box_append (GTK_BOX (row), bref);
  gtk_box_append (GTK_BOX (row), brestore);
  gtk_box_append (GTK_BOX (row), bempty);
  gtk_box_append (GTK_BOX (row), status);
  gtk_box_append (GTK_BOX (box), row);
  gtk_box_reorder_child_after (GTK_BOX (box), row, gtk_widget_get_first_child (box));
  trash_fill (list);
  return page;
}

/* ---------- 21. clipboard history ---------- */

typedef struct {
  GtkWidget *list;
  GPtrArray *hist; /* char* */
  char *last;
} KastClip;

static void
clipboard_read_done (GObject *src, GAsyncResult *res, gpointer data)
{
  KastClip *c = data;
  GError *err = NULL;
  char *text = gdk_clipboard_read_text_finish (GDK_CLIPBOARD (src), res, &err);
  if (err)
    {
      g_clear_error (&err);
      return;
    }
  if (text && *text && (!c->last || g_strcmp0 (text, c->last)))
    {
      g_free (c->last);
      c->last = g_strdup (text);
      /* dedupe + cap */
      for (guint i = 0; i < c->hist->len; i++)
        {
          if (!g_strcmp0 (g_ptr_array_index (c->hist, i), text))
            {
              g_ptr_array_remove_index (c->hist, i);
              break;
            }
        }
      g_ptr_array_insert (c->hist, 0, g_strdup (text));
      while (c->hist->len > 20)
        g_ptr_array_remove_index (c->hist, 20);
      /* rebuild */
      GtkWidget *child = gtk_widget_get_first_child (c->list);
      while (child)
        {
          GtkWidget *next = gtk_widget_get_next_sibling (child);
          gtk_list_box_remove (GTK_LIST_BOX (c->list), child);
          child = next;
        }
      for (guint i = 0; i < c->hist->len; i++)
        {
          const char *t = g_ptr_array_index (c->hist, i);
          char *one = g_strstrip (g_strdup (t));
          g_strdelimit (one, "\n", ' ');
          char *disp = g_strndup (one, 120);
          GtkWidget *row = gtk_label_new (disp);
          gtk_widget_set_halign (row, GTK_ALIGN_START);
          gtk_label_set_ellipsize (GTK_LABEL (row), PANGO_ELLIPSIZE_END);
          gtk_widget_set_margin_start (row, 10);
          gtk_list_box_append (GTK_LIST_BOX (c->list), row);
          g_free (one); g_free (disp);
        }
    }
  g_free (text);
}

static gboolean
clip_tick (gpointer data)
{
  KastClip *c = data;
  GdkClipboard *cb = gdk_display_get_clipboard (gdk_display_get_default ());
  if (cb)
    gdk_clipboard_read_text_async (cb, NULL, clipboard_read_done, c);
  return G_SOURCE_CONTINUE;
}

static void
on_clip_row (GtkListBox *, GtkListBoxRow *row, gpointer data)
{
  (void) data;
  if (!row)
    return;
  GtkWidget *child = gtk_list_box_row_get_child (row);
  const char *t = gtk_label_get_text (GTK_LABEL (child));
  GdkClipboard *cb = gdk_display_get_clipboard (gdk_display_get_default ());
  if (cb && t)
    gdk_clipboard_set_text (cb, t);
}

GtkWidget *
kast_clipboard_page (KastUi *ui)
{
  (void) ui;
  GtkWidget *list = NULL;
  GtkWidget *page = kast_tool_shell ("Clipboard history", &list);
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (list), GTK_SELECTION_SINGLE);
  g_signal_connect (list, "row-activated", G_CALLBACK (on_clip_row), NULL);
  KastClip *c = g_new0 (KastClip, 1);
  c->list = list;
  c->hist = g_ptr_array_new_with_free_func (g_free);
  g_timeout_add_seconds (2, clip_tick, c);
  g_object_set_data_full (G_OBJECT (page), "kast-clip", c, g_free);
  GtkWidget *box = gtk_widget_get_parent (gtk_widget_get_parent (list));
  GtkWidget *hint = gtk_label_new ("Click an entry to copy it back. Latest 20 texts kept.");
  gtk_widget_add_css_class (hint, "dim-label");
  gtk_widget_set_halign (hint, GTK_ALIGN_START);
  gtk_box_append (GTK_BOX (box), hint);
  return page;
}

/* ---------- 22. shortcuts cheatsheet ---------- */

GtkWidget *
kast_shortcuts_page (KastUi *ui)
{
  (void) ui;
  GtkWidget *list = NULL;
  GtkWidget *page = kast_tool_shell ("Kast shortcuts", &list);
  kast_tool_row (list, "End process", "Select row, then End");
  kast_tool_row (list, "Kill process", "Select row, then Kill");
  kast_tool_row (list, "Kill process tree", "Select row, then Kill tree");
  kast_tool_row (list, "Change priority", "Select row, then Priority");
  kast_tool_row (list, "Search processes", "Type in the search box");
  kast_tool_row (list, "Sort table", "Click a column header");
  kast_tool_row (list, "Jump to page", "Click a dashboard card or sidebar");
  kast_tool_row (list, "Sidebar modes", "Controls → Appearance");
  kast_tool_row (list, "Mini widget", "Main menu → Mini widget");
  kast_tool_row (list, "Benchmark PC", "Rating → Run benchmark");
  kast_tool_row (list, "File properties", "Files → select, Properties");
  kast_tool_row (list, "Uninstall", "bash install.sh --uninstall");
  return page;
}
