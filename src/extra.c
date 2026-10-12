/* Kast Manager 2.x — extra tools (Tejas Khanna, GPL-3.0-or-later).
 * Journal, connections, sensors, startup, disk usage, users,
 * devices, modules, cron, battery. */
#include <string.h>
#include <ifaddrs.h>
#include <arpa/inet.h>

#include "kast.h"

/* ---------- shared bits ---------- */

GtkWidget *
kast_tool_shell (const char *title, GtkWidget **list_out)
{
  GtkWidget *scroll = gtk_scrolled_window_new ();
  gtk_widget_set_hexpand (scroll, TRUE);
  gtk_widget_set_vexpand (scroll, TRUE);

  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_widget_set_margin_top (box, 20);
  gtk_widget_set_margin_bottom (box, 24);
  gtk_widget_set_margin_start (box, 24);
  gtk_widget_set_margin_end (box, 24);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scroll), box);

  GtkWidget *t = gtk_label_new (title);
  gtk_widget_add_css_class (t, "kast-neon-title");
  gtk_widget_set_halign (t, GTK_ALIGN_START);
  gtk_box_append (GTK_BOX (box), t);

  GtkWidget *card = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_add_css_class (card, "card");
  gtk_box_append (GTK_BOX (box), card);

  GtkWidget *list = gtk_list_box_new ();
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (list), GTK_SELECTION_NONE);
  gtk_box_append (GTK_BOX (card), list);
  *list_out = list;
  return scroll;
}

void
kast_tool_row (GtkWidget *list, const char *a, const char *b)
{
  GtkWidget *row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_set_margin_top (row, 3);
  gtk_widget_set_margin_bottom (row, 3);
  gtk_widget_set_margin_start (row, 10);
  gtk_widget_set_margin_end (row, 10);
  GtkWidget *la = gtk_label_new (a ? a : "");
  gtk_widget_set_halign (la, GTK_ALIGN_START);
  gtk_widget_set_hexpand (la, TRUE);
  gtk_label_set_ellipsize (GTK_LABEL (la), PANGO_ELLIPSIZE_MIDDLE);
  gtk_label_set_selectable (GTK_LABEL (la), TRUE);
  gtk_box_append (GTK_BOX (row), la);
  if (b)
    {
      GtkWidget *lb = gtk_label_new (b);
      gtk_widget_add_css_class (lb, "dim-label");
      gtk_widget_set_halign (lb, GTK_ALIGN_END);
      gtk_label_set_ellipsize (GTK_LABEL (lb), PANGO_ELLIPSIZE_START);
      gtk_box_append (GTK_BOX (row), lb);
    }
  gtk_list_box_append (GTK_LIST_BOX (list), row);
}

void
kast_tool_clear (GtkWidget *list)
{
  GtkWidget *child = gtk_widget_get_first_child (list);
  while (child)
    {
      GtkWidget *next = gtk_widget_get_next_sibling (child);
      gtk_list_box_remove (GTK_LIST_BOX (list), child);
      child = next;
    }
}

gboolean
kast_have_tool (const char *name)
{
  char *p = g_find_program_in_path (name);
  gboolean ok = p != NULL;
  g_free (p);
  return ok;
}

/* ---------- 1. journal ---------- */

typedef struct {
  GtkWidget *list;
  gboolean errors_only;
} KastJournal;

static void
journal_fill (KastJournal *j)
{
  kast_tool_clear (j->list);
  if (!kast_have_tool ("journalctl"))
    {
      kast_tool_row (j->list, "journalctl not found", NULL);
      return;
    }
  const char *pri = j->errors_only ? "-p" : "-p";
  const char *lvl = j->errors_only ? "3" : "6";
  char *argv[] = { "journalctl", (char *) pri, (char *) lvl, "-b",
                   "--no-pager", "-n", "120", "-o", "short", NULL };
  char *out = NULL;
  if (!g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                     &out, NULL, NULL, NULL) || !out)
    {
      kast_tool_row (j->list, "Cannot read journal (try as your own logs only)", NULL);
      return;
    }
  char **lines = g_strsplit (out, "\n", -1);
  int n = 0;
  for (int i = 0; lines[i]; i++)
    {
      if (!*g_strstrip (lines[i]))
        continue;
      kast_tool_row (j->list, lines[i], NULL);
      n++;
    }
  if (!n)
    kast_tool_row (j->list, "Journal is empty", NULL);
  g_strfreev (lines);
  g_free (out);
}

static void
on_journal_filter (GtkButton *btn, gpointer data)
{
  KastJournal *j = data;
  j->errors_only = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (btn), "kast-err"));
  journal_fill (j);
}

GtkWidget *
kast_journal_page (KastUi *ui)
{
  (void) ui;
  KastJournal *j = g_new0 (KastJournal, 1);
  j->errors_only = TRUE;
  GtkWidget *list = NULL;
  GtkWidget *page = kast_tool_shell ("System journal", &list);
  j->list = list;

  GtkWidget *box = gtk_widget_get_parent (gtk_widget_get_parent (list));
  GtkWidget *row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  GtkWidget *b1 = gtk_button_new_with_label ("Errors");
  GtkWidget *b2 = gtk_button_new_with_label ("All info+");
  g_object_set_data (G_OBJECT (b1), "kast-err", GINT_TO_POINTER (1));
  g_object_set_data (G_OBJECT (b2), "kast-err", GINT_TO_POINTER (0));
  g_signal_connect (b1, "clicked", G_CALLBACK (on_journal_filter), j);
  g_signal_connect (b2, "clicked", G_CALLBACK (on_journal_filter), j);
  gtk_box_append (GTK_BOX (row), b1);
  gtk_box_append (GTK_BOX (row), b2);
  gtk_box_append (GTK_BOX (box), row);
  gtk_box_reorder_child_after (GTK_BOX (box), row, gtk_widget_get_first_child (box));

  journal_fill (j);
  g_object_set_data_full (G_OBJECT (page), "kast-j", j, g_free);
  return page;
}

/* ---------- 2. network connections ---------- */

static void
hexip_port (const char *hex, char *ip_out, gsize iplen, int *port_out)
{
  char addr[65] = { 0 }, port[9] = { 0 };
  const char *colon = strchr (hex, ':');
  g_strlcpy (addr, hex, colon ? (gsize)(colon - hex) + 1 : sizeof addr);
  if (colon)
    g_strlcpy (port, colon + 1, sizeof port);
  *port_out = (int) strtol (port, NULL, 16);
  if (strlen (addr) <= 8)
    {
      /* IPv4 little-endian hex */
      guint32 v = (guint32) strtoul (addr, NULL, 16);
      g_snprintf (ip_out, iplen, "%u.%u.%u.%u",
                  v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF, (v >> 24) & 0xFF);
    }
  else
    {
      /* IPv6: 4 x 32-bit words, each little-endian */
      guint32 w[4] = { 0, 0, 0, 0 };
      for (int i = 0; i < 4 && i * 8 < (int) strlen (addr); i++)
        {
          char tmp[9] = { 0 };
          g_strlcpy (tmp, addr + i * 8, 9);
          w[i] = (guint32) strtoul (tmp, NULL, 16);
        }
      struct in6_addr a6;
      for (int i = 0; i < 4; i++)
        {
          a6.s6_addr[i * 4 + 0] = w[i] & 0xFF;
          a6.s6_addr[i * 4 + 1] = (w[i] >> 8) & 0xFF;
          a6.s6_addr[i * 4 + 2] = (w[i] >> 16) & 0xFF;
          a6.s6_addr[i * 4 + 3] = (w[i] >> 24) & 0xFF;
        }
      inet_ntop (AF_INET6, &a6, ip_out, iplen);
    }
}

static char *
inode_proc (const char *inode)
{
  char *want = g_strdup_printf ("socket:[%s]", inode);
  GDir *proc = g_dir_open ("/proc", 0, NULL);
  char *found = NULL;
  if (proc)
    {
      const char *name;
      while (!found && (name = g_dir_read_name (proc)) != NULL)
        {
          char *end = NULL;
          strtol (name, &end, 10);
          if (!end || *end)
            continue;
          char *fdd = g_strdup_printf ("/proc/%s/fd", name);
          GDir *fd = g_dir_open (fdd, 0, NULL);
          if (fd)
            {
              const char *fdn;
              while (!found && (fdn = g_dir_read_name (fd)) != NULL)
                {
                  char *linkp = g_build_filename (fdd, fdn, NULL);
                  char *target = g_file_read_link (linkp, NULL);
                  if (target && !g_strcmp0 (target, want))
                    {
                      char *cmdp = g_strdup_printf ("/proc/%s/comm", name);
                      char *comm = NULL;
                      if (g_file_get_contents (cmdp, &comm, NULL, NULL))
                        {
                          found = g_strdup_printf ("%s (%s)", g_strstrip (comm), name);
                          g_free (comm);
                        }
                      else
                        found = g_strdup_printf ("pid %s", name);
                      g_free (cmdp);
                    }
                  g_free (target);
                  g_free (linkp);
                }
              g_dir_close (fd);
            }
          g_free (fdd);
        }
      g_dir_close (proc);
    }
  g_free (want);
  return found ? found : g_strdup ("—");
}

static void
conns_file (GtkWidget *list, const char *path, const char *proto)
{
  static const char *states[] = { "", "ESTABLISHED", "SYN_SENT", "SYN_RECV",
      "FIN_WAIT1", "FIN_WAIT2", "TIME_WAIT", "CLOSE", "CLOSE_WAIT",
      "LAST_ACK", "LISTEN", "CLOSING", "NEW_SYN_RECV" };
  char *text = NULL;
  if (!g_file_get_contents (path, &text, NULL, NULL))
    return;
  char **lines = g_strsplit (text, "\n", -1);
  for (int i = 1; lines[i]; i++)
    {
      char local[80] = { 0 }, rem[80] = { 0 }, st[8] = { 0 }, inode[32] = { 0 };
      char **tok = g_strsplit_set (g_strstrip (lines[i]), " \t", -1);
      /* squeeze empties */
      char *c[16];
      int n = 0;
      for (int j = 0; tok[j] && n < 16; j++)
        if (*tok[j])
          c[n++] = tok[j];
      if (n < 10)
        {
          g_strfreev (tok);
          continue;
        }
      char lip[64], rip[64];
      int lport = 0, rport = 0;
      hexip_port (c[1], lip, sizeof lip, &lport);
      hexip_port (c[2], rip, sizeof rip, &rport);
      int sti = (int) strtol (c[3], NULL, 16);
      g_strlcpy (inode, c[9], sizeof inode);
      const char *stname = (sti >= 1 && sti <= 12) ? states[sti] : "?";
      char *proc = inode_proc (inode);
      char *left = g_strdup_printf ("%s %s:%d → %s:%d", proto, lip, lport, rip, rport);
      char *right = g_strdup_printf ("%s · %s", stname, proc);
      kast_tool_row (list, left, right);
      g_free (left); g_free (right); g_free (proc);
      g_strfreev (tok);
    }
  g_strfreev (lines);
  g_free (text);
}

static void
conns_fill (GtkWidget *list)
{
  kast_tool_clear (list);
  conns_file (list, "/proc/net/tcp", "tcp");
  conns_file (list, "/proc/net/tcp6", "tcp6");
  conns_file (list, "/proc/net/udp", "udp");
  conns_file (list, "/proc/net/udp6", "udp6");
}

static void
on_conns_refresh (GtkButton *, gpointer data)
{
  conns_fill (data);
}

GtkWidget *
kast_conns_page (KastUi *ui)
{
  (void) ui;
  GtkWidget *list = NULL;
  GtkWidget *page = kast_tool_shell ("Network connections", &list);
  GtkWidget *box = gtk_widget_get_parent (gtk_widget_get_parent (list));
  GtkWidget *row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  GtkWidget *b = gtk_button_new_with_label ("Refresh");
  g_signal_connect (b, "clicked", G_CALLBACK (on_conns_refresh), list);
  gtk_box_append (GTK_BOX (row), b);
  gtk_box_append (GTK_BOX (box), row);
  gtk_box_reorder_child_after (GTK_BOX (box), row, gtk_widget_get_first_child (box));
  conns_fill (list);
  return page;
}

/* ---------- 3. sensors ---------- */

static void
sensors_fill (GtkWidget *list)
{
  kast_tool_clear (list);
  GDir *hw = g_dir_open ("/sys/class/hwmon", 0, NULL);
  int n = 0;
  if (hw)
    {
      const char *h;
      while ((h = g_dir_read_name (hw)) != NULL)
        {
          char *base = g_build_filename ("/sys/class/hwmon", h, NULL);
          char *namep = g_build_filename (base, "name", NULL);
          char *chip = NULL;
          g_file_get_contents (namep, &chip, NULL, NULL);
          g_free (namep);
          for (int i = 1; i <= 16; i++)
            {
              char *tp = g_strdup_printf ("%s/temp%d_input", base, i);
              char *tv = NULL;
              if (g_file_get_contents (tp, &tv, NULL, NULL))
                {
                  char *lp = g_strdup_printf ("%s/temp%d_label", base, i);
                  char *lab = NULL;
                  g_file_get_contents (lp, &lab, NULL, NULL);
                  g_free (lp);
                  double c = atol (g_strstrip (tv)) / 1000.0;
                  char *left = g_strdup_printf ("%s %s", chip ? g_strstrip (chip) : h,
                                               lab ? g_strstrip (lab) : "");
                  char *right = g_strdup_printf ("%.1f °C", c);
                  kast_tool_row (list, left, right);
                  g_free (left); g_free (right);
                  g_free (lab);
                  n++;
                }
              g_free (tp);
              g_free (tv);
              char *fp = g_strdup_printf ("%s/fan%d_input", base, i);
              char *fv = NULL;
              if (g_file_get_contents (fp, &fv, NULL, NULL))
                {
                  char *left = g_strdup_printf ("%s fan%d",
                      chip ? g_strstrip (chip) : h, i);
                  char *right = g_strdup_printf ("%s RPM", g_strstrip (fv));
                  kast_tool_row (list, left, right);
                  g_free (left); g_free (right);
                  n++;
                }
              g_free (fp);
              g_free (fv);
            }
          g_free (chip);
          g_free (base);
        }
      g_dir_close (hw);
    }
  /* fallback: thermal zones */
  if (!n)
    {
      GDir *tz = g_dir_open ("/sys/class/thermal", 0, NULL);
      if (tz)
        {
          const char *z;
          while ((z = g_dir_read_name (tz)) != NULL)
            {
              if (!g_str_has_prefix (z, "thermal_zone"))
                continue;
              char *tp = g_strdup_printf ("/sys/class/thermal/%s/temp", z);
              char *typ = g_strdup_printf ("/sys/class/thermal/%s/type", z);
              char *tv = NULL, *tn = NULL;
              if (g_file_get_contents (tp, &tv, NULL, NULL))
                {
                  g_file_get_contents (typ, &tn, NULL, NULL);
                  char *right = g_strdup_printf ("%.1f °C", atol (g_strstrip (tv)) / 1000.0);
                  kast_tool_row (list, tn ? g_strstrip (tn) : z, right);
                  g_free (right);
                  n++;
                }
              g_free (tp); g_free (typ); g_free (tv); g_free (tn);
            }
          g_dir_close (tz);
        }
    }
  if (!n)
    kast_tool_row (list, "No sensors found", NULL);
}

static void
on_sensors_refresh (GtkButton *, gpointer data)
{
  sensors_fill (data);
}

GtkWidget *
kast_sensors_page (KastUi *ui)
{
  (void) ui;
  GtkWidget *list = NULL;
  GtkWidget *page = kast_tool_shell ("Temperatures and fans", &list);
  GtkWidget *box = gtk_widget_get_parent (gtk_widget_get_parent (list));
  GtkWidget *row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  GtkWidget *b = gtk_button_new_with_label ("Refresh");
  g_signal_connect (b, "clicked", G_CALLBACK (on_sensors_refresh), list);
  gtk_box_append (GTK_BOX (row), b);
  gtk_box_append (GTK_BOX (box), row);
  gtk_box_reorder_child_after (GTK_BOX (box), row, gtk_widget_get_first_child (box));
  sensors_fill (list);
  return page;
}

/* ---------- 4. startup analyzer ---------- */

GtkWidget *
kast_startup_page (KastUi *ui)
{
  (void) ui;
  GtkWidget *list = NULL;
  GtkWidget *page = kast_tool_shell ("Startup analyzer", &list);
  if (!kast_have_tool ("systemd-analyze"))
    {
      kast_tool_row (list, "systemd-analyze not found", NULL);
      return page;
    }
  char *out = NULL;
  char *argv[] = { "systemd-analyze", "blame", NULL };
  if (g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                    &out, NULL, NULL, NULL) && out)
    {
      char **lines = g_strsplit (out, "\n", -1);
      int n = 0;
      for (int i = 0; lines[i] && n < 25; i++)
        {
          if (!*g_strstrip (lines[i]))
            continue;
          /* "12.884s foo.service" */
          char *line = g_strstrip (lines[i]);
          char *sp = strchr (line, ' ');
          if (!sp)
            continue;
          *sp = '\0';
          kast_tool_row (list, sp + 1, line);
          n++;
        }
      g_strfreev (lines);
    }
  g_free (out);
  return page;
}

/* ---------- 5. disk usage ---------- */

typedef struct {
  GtkWidget *list;
  GtkWidget *status;
  GThread *thread;
  GMutex lock;
  gboolean done;
  char *result;
} KastDu;

static int
du_cmp (gconstpointer a, gconstpointer b, gpointer _unused)
{
  (void) _unused;
  guint64 xa = g_ascii_strtoull (*(char * const *) a, NULL, 10);
  guint64 xb = g_ascii_strtoull (*(char * const *) b, NULL, 10);
  return (xa < xb) - (xa > xb);
}

static gpointer
du_thread (gpointer data)
{
  KastDu *d = data;
  char *out = NULL;
  char *home = g_strdup (g_get_home_dir ());
  char *argv[] = { "du", "-x", "-d", "2", home, NULL };
  if (g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                    &out, NULL, NULL, NULL) && out)
    {
      /* sort desc by size, keep top 30 */
      char **lines = g_strsplit (out, "\n", -1);
      GPtrArray *rows = g_ptr_array_new_with_free_func (g_free);
      for (int i = 0; lines[i]; i++)
        {
          if (!*g_strstrip (lines[i]))
            continue;
          char *tab = strchr (lines[i], '\t');
          if (!tab)
            continue;
          *tab = '\0';
          g_ptr_array_add (rows, g_strdup_printf ("%llu\t%s",
              (unsigned long long) g_ascii_strtoull (lines[i], NULL, 10), tab + 1));
        }
      g_ptr_array_sort_with_data (rows, du_cmp, NULL);
      GString *res = g_string_new (NULL);
      for (guint i = 0; i < MIN (30, rows->len); i++)
        {
          char *row = g_ptr_array_index (rows, i);
          char *tab = strchr (row, '\t');
          guint64 kb = g_ascii_strtoull (row, NULL, 10);
          char *sz = kast_bytes (kb * 1024);
          g_string_append_printf (res, "%s\t%s\n", sz, tab + 1);
          g_free (sz);
        }
      g_mutex_lock (&d->lock);
      d->result = g_string_free (res, FALSE);
      d->done = TRUE;
      g_mutex_unlock (&d->lock);
      g_ptr_array_free (rows, TRUE);
      g_strfreev (lines);
    }
  else
    {
      g_mutex_lock (&d->lock);
      d->result = g_strdup ("");
      d->done = TRUE;
      g_mutex_unlock (&d->lock);
    }
  g_free (home);
  g_free (out);
  return NULL;
}

static gboolean
du_poll (gpointer data)
{
  KastDu *d = data;
  if (!g_mutex_trylock (&d->lock))
    return G_SOURCE_CONTINUE;
  gboolean done = d->done;
  g_mutex_unlock (&d->lock);
  if (!done)
    return G_SOURCE_CONTINUE;
  g_thread_join (d->thread);
  kast_tool_clear (d->list);
  if (d->result && *d->result)
    {
      char **lines = g_strsplit (d->result, "\n", -1);
      for (int i = 0; lines[i]; i++)
        {
          if (!*lines[i])
            continue;
          char *tab = strchr (lines[i], '\t');
          if (!tab)
            continue;
          *tab = '\0';
          kast_tool_row (d->list, tab + 1, lines[i]);
        }
      g_strfreev (lines);
    }
  else
    kast_tool_row (d->list, "Nothing measured (du missing?)", NULL);
  gtk_label_set_text (GTK_LABEL (d->status), "Top 30 folders under home.");
  g_free (d->result);
  g_mutex_clear (&d->lock);
  g_free (d);
  return G_SOURCE_REMOVE;
}

static void
on_du_scan (GtkButton *, gpointer data)
{
  GtkWidget **pack = data; /* { list, status } */
  KastDu *d = g_new0 (KastDu, 1);
  d->list = pack[0];
  d->status = pack[1];
  g_mutex_init (&d->lock);
  kast_tool_clear (d->list);
  kast_tool_row (d->list, "Scanning home folder…", NULL);
  d->thread = g_thread_new ("kast-du", du_thread, d);
  g_timeout_add (500, du_poll, d);
}

GtkWidget *
kast_usage_page (KastUi *ui)
{
  (void) ui;
  GtkWidget *list = NULL;
  GtkWidget *page = kast_tool_shell ("Disk usage (home)", &list);
  GtkWidget *box = gtk_widget_get_parent (gtk_widget_get_parent (list));
  GtkWidget *row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  GtkWidget *b = gtk_button_new_with_label ("Scan now");
  GtkWidget *status = gtk_label_new ("Press Scan now (can take a while).");
  gtk_widget_add_css_class (status, "dim-label");
  gtk_widget_set_halign (status, GTK_ALIGN_START);
  gtk_widget_set_hexpand (status, TRUE);
  static GtkWidget *pack[2];
  pack[0] = list;
  pack[1] = status;
  g_signal_connect (b, "clicked", G_CALLBACK (on_du_scan), pack);
  gtk_box_append (GTK_BOX (row), b);
  gtk_box_append (GTK_BOX (row), status);
  gtk_box_append (GTK_BOX (box), row);
  gtk_box_reorder_child_after (GTK_BOX (box), row, gtk_widget_get_first_child (box));
  return page;
}

/* ---------- 6. users ---------- */

GtkWidget *
kast_users_page (KastUi *ui)
{
  (void) ui;
  GtkWidget *list = NULL;
  GtkWidget *page = kast_tool_shell ("Users and sessions", &list);

  char *text = NULL;
  if (g_file_get_contents ("/etc/passwd", &text, NULL, NULL))
    {
      char **lines = g_strsplit (text, "\n", -1);
      for (int i = 0; lines[i]; i++)
        {
          if (!*lines[i] || lines[i][0] == '#')
            continue;
          /* name:x:uid:gid:gecos:home:shell */
          char **f = g_strsplit (lines[i], ":", 7);
          if (g_strv_length (f) >= 7)
            {
              char *right = g_strdup_printf ("uid %s · %s", f[2], f[6]);
              kast_tool_row (list, f[0], right);
              g_free (right);
            }
          g_strfreev (f);
        }
      g_strfreev (lines);
      g_free (text);
    }

  if (kast_have_tool ("loginctl"))
    {
      char *out = NULL;
      char *argv[] = { "loginctl", "list-sessions", "--no-legend", "--no-pager", NULL };
      if (g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                        &out, NULL, NULL, NULL) && out && *g_strstrip (out))
        {
          kast_tool_row (list, "— active sessions —", NULL);
          char **lines = g_strsplit (out, "\n", -1);
          for (int i = 0; lines[i]; i++)
            {
              if (!*g_strstrip (lines[i]))
                continue;
              kast_tool_row (list, g_strstrip (lines[i]), NULL);
            }
          g_strfreev (lines);
        }
      g_free (out);
    }
  return page;
}

/* ---------- 7. devices ---------- */

GtkWidget *
kast_devices_page (KastUi *ui)
{
  (void) ui;
  GtkWidget *list = NULL;
  GtkWidget *page = kast_tool_shell ("PCI and USB devices", &list);
  if (kast_have_tool ("lspci"))
    {
      char *out = NULL;
      char *argv[] = { "lspci", "-mm", NULL };
      if (g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                        &out, NULL, NULL, NULL) && out)
        {
          kast_tool_row (list, "— PCI —", NULL);
          char **lines = g_strsplit (out, "\n", -1);
          for (int i = 0; lines[i]; i++)
            {
              if (!*g_strstrip (lines[i]))
                continue;
              kast_tool_row (list, g_strstrip (lines[i]), NULL);
            }
          g_strfreev (lines);
        }
      g_free (out);
    }
  if (kast_have_tool ("lsusb"))
    {
      char *out = NULL;
      char *argv[] = { "lsusb", NULL };
      if (g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                        &out, NULL, NULL, NULL) && out)
        {
          kast_tool_row (list, "— USB —", NULL);
          char **lines = g_strsplit (out, "\n", -1);
          for (int i = 0; lines[i]; i++)
            {
              if (!*g_strstrip (lines[i]))
                continue;
              kast_tool_row (list, g_strstrip (lines[i]), NULL);
            }
          g_strfreev (lines);
        }
      g_free (out);
    }
  return page;
}

/* ---------- 8. kernel modules ---------- */

GtkWidget *
kast_modules_page (KastUi *ui)
{
  (void) ui;
  GtkWidget *list = NULL;
  GtkWidget *page = kast_tool_shell ("Kernel modules", &list);
  char *text = NULL;
  if (g_file_get_contents ("/proc/modules", &text, NULL, NULL))
    {
      char **lines = g_strsplit (text, "\n", -1);
      for (int i = 0; lines[i]; i++)
        {
          if (!*lines[i])
            continue;
          /* name size used by state addr */
          char **tok = g_strsplit_set (lines[i], " \t", 6);
          char *c[6] = { NULL };
          int n = 0;
          for (int j = 0; tok[j] && n < 6; j++)
            if (*tok[j])
              c[n++] = tok[j];
          if (n >= 3)
            {
              char *right = g_strdup_printf ("%s bytes · used by %s", c[1], c[2]);
              kast_tool_row (list, c[0], right);
              g_free (right);
            }
          g_strfreev (tok);
        }
      g_strfreev (lines);
      g_free (text);
    }
  else
    kast_tool_row (list, "Cannot read /proc/modules", NULL);
  return page;
}

/* ---------- 9. cron ---------- */

GtkWidget *
kast_cron_page (KastUi *ui)
{
  (void) ui;
  GtkWidget *list = NULL;
  GtkWidget *page = kast_tool_shell ("Scheduled tasks (cron)", &list);
  if (kast_have_tool ("crontab"))
    {
      char *out = NULL, *err = NULL;
      char *argv[] = { "crontab", "-l", NULL };
      if (g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                        &out, &err, NULL, NULL) && out && *g_strstrip (out))
        {
          kast_tool_row (list, "— my crontab —", NULL);
          char **lines = g_strsplit (out, "\n", -1);
          for (int i = 0; lines[i]; i++)
            {
              if (!*g_strstrip (lines[i]) || lines[i][0] == '#')
                continue;
              kast_tool_row (list, g_strstrip (lines[i]), NULL);
            }
          g_strfreev (lines);
        }
      else
        kast_tool_row (list, "No personal crontab", NULL);
      g_free (out);
      g_free (err);
    }
  const char *dirs[] = { "/etc/cron.d", "/etc/cron.daily", "/etc/cron.hourly",
                         "/etc/cron.weekly", "/etc/cron.monthly", NULL };
  for (int d = 0; dirs[d]; d++)
    {
      GDir *dir = g_dir_open (dirs[d], 0, NULL);
      if (!dir)
        continue;
      kast_tool_row (list, dirs[d], NULL);
      const char *name;
      while ((name = g_dir_read_name (dir)) != NULL)
        kast_tool_row (list, name, NULL);
      g_dir_close (dir);
    }
  return page;
}

/* ---------- 10. battery ---------- */

static void
batt_kv (GtkWidget *list, const char *dir, const char *file, const char *label,
         const char *unit, double div)
{
  char *p = g_build_filename ("/sys/class/power_supply", dir, file, NULL);
  char *v = NULL;
  if (g_file_get_contents (p, &v, NULL, NULL))
    {
      double num = atol (g_strstrip (v)) / div;
      char *right = g_strdup_printf ("%.1f %s", num, unit);
      kast_tool_row (list, label, right);
      g_free (right);
    }
  g_free (p);
  g_free (v);
}

static void
batt_txt (GtkWidget *list, const char *dir, const char *file, const char *label)
{
  char *p = g_build_filename ("/sys/class/power_supply", dir, file, NULL);
  char *v = NULL;
  if (g_file_get_contents (p, &v, NULL, NULL))
    kast_tool_row (list, label, g_strstrip (v));
  g_free (p);
  g_free (v);
}

GtkWidget *
kast_battery_page (KastUi *ui)
{
  (void) ui;
  GtkWidget *list = NULL;
  GtkWidget *page = kast_tool_shell ("Battery details", &list);
  GDir *d = g_dir_open ("/sys/class/power_supply", 0, NULL);
  int n = 0;
  if (d)
    {
      const char *name;
      while ((name = g_dir_read_name (d)) != NULL)
        {
          char *typep = g_build_filename ("/sys/class/power_supply", name, "type", NULL);
          char *type = NULL;
          g_file_get_contents (typep, &type, NULL, NULL);
          g_free (typep);
          gboolean is_batt = type && strstr (type, "Battery");
          g_free (type);
          if (!is_batt)
            continue;
          n++;
          kast_tool_row (list, name, NULL);
          batt_kv (list, name, "capacity", "Charge", "%", 1.0);
          batt_txt (list, name, "status", "Status");
          batt_txt (list, name, "health", "Health");
          batt_txt (list, name, "technology", "Technology");
          batt_kv (list, name, "voltage_now", "Voltage", "V", 1000000.0);
          batt_kv (list, name, "current_now", "Current", "A", 1000000.0);
          batt_kv (list, name, "power_now", "Power draw", "W", 1000000.0);
          batt_kv (list, name, "energy_full", "Full charge", "Wh", 1000000.0);
          batt_kv (list, name, "energy_full_design", "Design charge", "Wh", 1000000.0);
          batt_kv (list, name, "charge_full", "Full charge", "Ah", 1000000.0);
          batt_kv (list, name, "charge_full_design", "Design charge", "Ah", 1000000.0);
          batt_txt (list, name, "manufacturer", "Manufacturer");
          batt_txt (list, name, "model_name", "Model");
        }
      g_dir_close (d);
    }
  if (!n)
    kast_tool_row (list, "No battery found", NULL);
  return page;
}
