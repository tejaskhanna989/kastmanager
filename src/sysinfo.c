/* Kast Manager 2.x — system info page (Tejas Khanna, GPL-3.0-or-later). */
#include <sys/utsname.h>
#include <string.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <glibtop/mem.h>
#include <glibtop/sysinfo.h>

#include "kast.h"

static void
info_row (GtkBox *box, const char *key, const char *value)
{
  GtkWidget *row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  GtkWidget *k = gtk_label_new (key);
  gtk_widget_add_css_class (k, "dim-label");
  gtk_widget_set_halign (k, GTK_ALIGN_START);
  gtk_widget_set_size_request (k, 150, -1);
  gtk_box_append (GTK_BOX (row), k);
  GtkWidget *v = gtk_label_new (value ? value : "n/a");
  gtk_widget_set_halign (v, GTK_ALIGN_START);
  gtk_widget_set_hexpand (v, TRUE);
  gtk_label_set_selectable (GTK_LABEL (v), TRUE);
  gtk_label_set_ellipsize (GTK_LABEL (v), PANGO_ELLIPSIZE_END);
  gtk_box_append (GTK_BOX (row), v);
  gtk_box_append (box, row);
}

static char *
cpu_model (void)
{
  const glibtop_sysinfo *info = glibtop_get_sysinfo ();
  if (info && info->ncpu > 0 && info->cpuinfo[0].values)
    {
      const char *model = g_hash_table_lookup (info->cpuinfo[0].values, "model name");
      if (model && *model)
        return g_strdup (model);
    }
  /* fallback: /proc/cpuinfo */
  char *text = NULL;
  if (g_file_get_contents ("/proc/cpuinfo", &text, NULL, NULL))
    {
      char **lines = g_strsplit (text, "\n", -1);
      for (int i = 0; lines[i]; i++)
        {
          if (g_str_has_prefix (lines[i], "model name"))
            {
              char *c = strchr (lines[i], ':');
              char *r = c ? g_strdup (g_strstrip (c + 1)) : g_strdup ("Unknown CPU");
              g_strfreev (lines);
              g_free (text);
              return r;
            }
        }
      g_strfreev (lines);
      g_free (text);
    }
  return g_strdup ("Unknown CPU");
}

GtkWidget *
kast_sysinfo_page (KastUi *ui)
{
  (void) ui;
  GtkWidget *scroll = gtk_scrolled_window_new ();
  gtk_widget_set_hexpand (scroll, TRUE);
  gtk_widget_set_vexpand (scroll, TRUE);

  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_widget_set_margin_top (box, 20);
  gtk_widget_set_margin_bottom (box, 24);
  gtk_widget_set_margin_start (box, 24);
  gtk_widget_set_margin_end (box, 24);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scroll), box);

  GtkWidget *title = gtk_label_new ("System info");
  gtk_widget_add_css_class (title, "kast-neon-title");
  gtk_widget_set_halign (title, GTK_ALIGN_START);
  gtk_box_append (GTK_BOX (box), title);

  GtkWidget *card = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  gtk_widget_add_css_class (card, "card");
  gtk_box_append (GTK_BOX (box), card);

  char *host = g_strdup (g_get_host_name ());
  struct utsname un;
  uname (&un);
  char *kernel = g_strdup_printf ("%s %s %s", un.sysname, un.release, un.machine);
  char *cpu = cpu_model ();
  char *cores = g_strdup_printf ("%u logical processors", g_get_num_processors ());

  glibtop_mem mem;
  glibtop_get_mem (&mem);
  char *mt = kast_bytes (mem.total);
  char *ram = g_strdup_printf ("%s total", mt);
  g_free (mt);

  const char *de = g_getenv ("XDG_CURRENT_DESKTOP");
  const char *sess = g_getenv ("XDG_SESSION_TYPE");
  char *desk = g_strdup_printf ("%s (%s session)",
      de && *de ? de : "Unknown desktop", sess && *sess ? sess : "?");
  char *up = kast_uptime_str ();
  char *uptime = g_strdup_printf ("%s uptime", up);

  /* boot time via systemd-analyze, best effort:
     "Startup finished in 3.5s (kernel) + 12.8s (userspace) = ..." */
  char *boottime = g_strdup ("n/a");
  {
    char *out = NULL;
    char *argv[] = { "systemd-analyze", "time", NULL };
    if (g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                      &out, NULL, NULL, NULL) && out)
      {
        char *plus = strstr (out, "+");
        if (plus)
          {
            plus++;
            while (*plus == ' ')
              plus++;
            char *end = plus;
            while (*end && *end != ' ' && *end != '\n')
              end++;
            if (end > plus)
              {
                g_free (boottime);
                boottime = g_strndup (plus, end - plus);
              }
          }
        g_free (out);
      }
  }

  /* IP addresses */
  GString *ips = g_string_new (NULL);
  {
    struct ifaddrs *list = NULL;
    if (getifaddrs (&list) == 0)
      {
        for (struct ifaddrs *ifa = list; ifa; ifa = ifa->ifa_next)
          {
            if (!ifa->ifa_addr || !g_strcmp0 (ifa->ifa_name, "lo"))
              continue;
            int fam = ifa->ifa_addr->sa_family;
            if (fam != AF_INET && fam != AF_INET6)
              continue;
            char host[NI_MAXHOST] = { 0 };
            if (getnameinfo (ifa->ifa_addr,
                             fam == AF_INET ? sizeof (struct sockaddr_in)
                                            : sizeof (struct sockaddr_in6),
                             host, sizeof host, NULL, 0, NI_NUMERICHOST) == 0)
              {
                if (ips->len)
                  g_string_append (ips, ", ");
                g_string_append_printf (ips, "%s (%s)", host, ifa->ifa_name);
              }
          }
        freeifaddrs (list);
      }
  }

  info_row (GTK_BOX (card), "Hostname", host);
  info_row (GTK_BOX (card), "Kernel", kernel);
  info_row (GTK_BOX (card), "Processor", cpu);
  info_row (GTK_BOX (card), "Cores", cores);
  info_row (GTK_BOX (card), "Memory", ram);
  info_row (GTK_BOX (card), "Desktop", desk);
  info_row (GTK_BOX (card), "Uptime", uptime);
  info_row (GTK_BOX (card), "Boot time", boottime);
  info_row (GTK_BOX (card), "IP addresses", ips->len ? ips->str : "n/a");

  g_free (host); g_free (kernel); g_free (cpu); g_free (cores);
  g_free (ram); g_free (desk); g_free (up); g_free (uptime);
  g_free (boottime);
  g_string_free (ips, TRUE);
  return scroll;
}
