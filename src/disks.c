/* Kast Manager 2.x — file systems page (Tejas Khanna, GPL-3.0-or-later). */
#include <mntent.h>
#include <sys/statvfs.h>
#include <string.h>

#include "kast.h"

typedef struct {
  KastUi *ui;
  GtkBox *list;
} KastDisks;

static gboolean
skip_fs (const char *type, const char *dir)
{
  static const char *skip_types[] = {
    "squashfs", "overlay", "overlayfs", "nsfs", "cgroup", "cgroup2",
    "proc", "sysfs", "devpts", "mqueue", "shm", "fuse.portal",
    "fuse.gvfsd-fuse", "tracefs", "debugfs", "securityfs", "configfs",
    "fusectl", "binfmt_misc", "autofs", "sunrpc", NULL,
  };
  for (int i = 0; skip_types[i]; i++)
    if (!g_strcmp0 (type, skip_types[i]))
      return TRUE;
  if (g_str_has_prefix (dir, "/tmp/.mount_"))
    return TRUE;
  if (g_str_has_prefix (dir, "/run/snap"))
    return TRUE;
  return FALSE;
}

static GtkWidget *
disk_row (const char *dev, const char *dir, const char *type,
          guint64 total, guint64 used)
{
  GtkWidget *row = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_add_css_class (row, "card");

  GtkWidget *top = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  GtkWidget *dev_l = gtk_label_new (dev);
  gtk_widget_set_halign (dev_l, GTK_ALIGN_START);
  gtk_widget_set_hexpand (dev_l, TRUE);
  gtk_widget_add_css_class (dev_l, "dash-value");
  PangoAttrList *attrs = pango_attr_list_new ();
  pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
  gtk_label_set_attributes (GTK_LABEL (dev_l), attrs);
  pango_attr_list_unref (attrs);
  gtk_box_append (GTK_BOX (top), dev_l);

  char *pct_txt = g_strdup_printf ("%.0f%%", total ? 100.0 * (double) used / (double) total : 0.0);
  GtkWidget *pct_l = gtk_label_new (pct_txt);
  gtk_widget_add_css_class (pct_l, "dash-accent-load");
  g_free (pct_txt);
  gtk_box_append (GTK_BOX (top), pct_l);
  gtk_box_append (GTK_BOX (row), top);

  char *u = kast_bytes (used);
  char *t = kast_bytes (total);
  char *sub = g_strdup_printf ("%s · %s · %s of %s", dir, type, u, t);
  GtkWidget *sub_l = gtk_label_new (sub);
  gtk_widget_add_css_class (sub_l, "dim-label");
  gtk_widget_set_halign (sub_l, GTK_ALIGN_START);
  gtk_label_set_ellipsize (GTK_LABEL (sub_l), PANGO_ELLIPSIZE_MIDDLE);
  g_free (u); g_free (t); g_free (sub);
  gtk_box_append (GTK_BOX (row), sub_l);

  GtkWidget *bar = gtk_level_bar_new_for_interval (0.0, 100.0);
  if (total)
    gtk_level_bar_set_value (GTK_LEVEL_BAR (bar), 100.0 * (double) used / (double) total);
  gtk_box_append (GTK_BOX (row), bar);
  return row;
}

static void
refresh_disks (KastDisks *k)
{
  GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (k->list));
  while (child)
    {
      GtkWidget *next = gtk_widget_get_next_sibling (child);
      gtk_box_remove (k->list, child);
      child = next;
    }

  FILE *mtab = setmntent ("/proc/mounts", "r");
  if (!mtab)
    return;
  struct mntent *ent;
  while ((ent = getmntent (mtab)) != NULL)
    {
      if (skip_fs (ent->mnt_type, ent->mnt_dir))
        continue;
      struct statvfs vfs;
      if (statvfs (ent->mnt_dir, &vfs) != 0 || vfs.f_blocks == 0)
        continue;
      guint64 total = (guint64) vfs.f_blocks * vfs.f_frsize;
      guint64 free = (guint64) vfs.f_bfree * vfs.f_frsize;
      gtk_box_append (k->list,
          disk_row (ent->mnt_fsname, ent->mnt_dir, ent->mnt_type, total, total - free));
    }
  endmntent (mtab);
}

static gboolean
disks_tick (gpointer data)
{
  KastDisks *k = data;
  const char *page = adw_view_stack_get_visible_child_name (k->ui->stack);
  if (g_strcmp0 (page, KAST_PAGE_DISKS) == 0)
    refresh_disks (k);
  return G_SOURCE_CONTINUE;
}

GtkWidget *
kast_disks_page (KastUi *ui)
{
  KastDisks *k = g_new0 (KastDisks, 1);
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

  GtkWidget *title = gtk_label_new ("File systems");
  gtk_widget_add_css_class (title, "kast-neon-title");
  gtk_widget_set_halign (title, GTK_ALIGN_START);
  gtk_box_append (GTK_BOX (box), title);

  k->list = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 12));
  gtk_box_append (GTK_BOX (box), GTK_WIDGET (k->list));

  refresh_disks (k);
  g_timeout_add_seconds (5, disks_tick, k);
  g_object_set_data_full (G_OBJECT (scroll), "kast-disks", k, g_free);
  return scroll;
}
