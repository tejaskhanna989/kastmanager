/* Kast Manager 2.x — minimal file management (Tejas Khanna, GPL-3.0-or-later).
 * Browse, open, new folder, trash. */
#include <string.h>

#include "kast.h"

typedef struct {
  KastUi *ui;
  GFile *dir;
  GtkListBox *list;
  GtkLabel *status;
  GtkEntry *path_entry;
} KastFiles;

static char *
file_desc (GFileInfo *info)
{
  GFileType type = g_file_info_get_file_type (info);
  if (type == G_FILE_TYPE_DIRECTORY)
    return g_strdup ("Folder");
  goffset size = g_file_info_get_size (info);
  char *sz = kast_bytes ((guint64) size);
  GDateTime *mt = g_file_info_get_modification_date_time (info);
  char *mts = mt ? g_date_time_format (mt, "%Y-%m-%d %H:%M") : g_strdup ("");
  char *desc = g_strdup_printf ("%s · %s", sz, mts);
  g_free (sz);
  g_free (mts);
  return desc;
}

static void files_navigate (KastFiles *k, GFile *dir);

static void
on_row_activated (GtkListBox *, GtkListBoxRow *row, gpointer data)
{
  KastFiles *k = data;
  GFile *f = g_object_get_data (G_OBJECT (row), "kast-file");
  GFileType type = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "kast-type"));
  if (type == G_FILE_TYPE_DIRECTORY)
    {
      files_navigate (k, f);
    }
  else
    {
      GError *err = NULL;
      char *uri = g_file_get_uri (f);
      g_app_info_launch_default_for_uri (uri, NULL, &err);
      if (err)
        {
          gtk_label_set_text (k->status, err->message);
          g_error_free (err);
        }
      g_free (uri);
    }
}

static int
row_sort (GtkListBoxRow *a, GtkListBoxRow *b, gpointer _unused)
{
  (void) _unused;
  GFileType ta = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (a), "kast-type"));
  GFileType tb = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (b), "kast-type"));
  gboolean da = ta == G_FILE_TYPE_DIRECTORY;
  gboolean db = tb == G_FILE_TYPE_DIRECTORY;
  if (da != db)
    return da ? -1 : 1;
  /* name stored as data on row */
  const char *na = g_object_get_data (G_OBJECT (a), "kast-name");
  const char *nb = g_object_get_data (G_OBJECT (b), "kast-name");
  return g_utf8_collate (na, nb);
}

static void
files_navigate (KastFiles *k, GFile *dir)
{
  g_set_object (&k->dir, dir);

  GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (k->list));
  while (child)
    {
      GtkWidget *next = gtk_widget_get_next_sibling (child);
      gtk_list_box_remove (GTK_LIST_BOX (k->list), child);
      child = next;
    }

  char *path = g_file_get_path (dir);
  gtk_editable_set_text (GTK_EDITABLE (k->path_entry), path ? path : "");
  g_free (path);
  gtk_label_set_text (k->status, "");

  GError *err = NULL;
  GFileEnumerator *en = g_file_enumerate_children (dir,
      "standard::name,standard::type,standard::size,time::modified,standard::icon",
      G_FILE_QUERY_INFO_NONE, NULL, &err);
  if (!en)
    {
      gtk_label_set_text (k->status, err ? err->message : "Cannot list folder");
      g_clear_error (&err);
      return;
    }

  GFileInfo *info;
  while ((info = g_file_enumerator_next_file (en, NULL, &err)) != NULL)
    {
      const char *name = g_file_info_get_name (info);
      GFileType type = g_file_info_get_file_type (info);
      GFile *child_f = g_file_get_child (dir, name);

      GtkWidget *row = gtk_list_box_row_new ();
      g_object_set_data_full (G_OBJECT (row), "kast-file", g_object_ref (child_f), g_object_unref);
      g_object_set_data (G_OBJECT (row), "kast-type", GINT_TO_POINTER (type));
      g_object_set_data_full (G_OBJECT (row), "kast-name", g_strdup (name), g_free);

      GtkWidget *hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
      gtk_widget_set_margin_top (hbox, 6);
      gtk_widget_set_margin_bottom (hbox, 6);
      gtk_widget_set_margin_start (hbox, 10);
      gtk_widget_set_margin_end (hbox, 10);
      gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), hbox);

      GIcon *icon = g_file_info_get_icon (info);
      GtkWidget *img = icon ? gtk_image_new_from_gicon (icon)
                            : gtk_image_new_from_icon_name ("text-x-generic-symbolic");
      gtk_box_append (GTK_BOX (hbox), img);

      GtkWidget *vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_widget_set_hexpand (vbox, TRUE);
      GtkWidget *nl = gtk_label_new (name);
      gtk_widget_set_halign (nl, GTK_ALIGN_START);
      gtk_label_set_ellipsize (GTK_LABEL (nl), PANGO_ELLIPSIZE_MIDDLE);
      gtk_box_append (GTK_BOX (vbox), nl);
      char *desc = file_desc (info);
      GtkWidget *dl = gtk_label_new (desc);
      gtk_widget_add_css_class (dl, "dim-label");
      gtk_widget_set_halign (dl, GTK_ALIGN_START);
      gtk_box_append (GTK_BOX (vbox), dl);
      g_free (desc);
      gtk_box_append (GTK_BOX (hbox), vbox);

      gtk_list_box_append (GTK_LIST_BOX (k->list), row);
      g_object_unref (child_f);
      g_object_unref (info);
    }
  if (err)
    {
      gtk_label_set_text (k->status, err->message);
      g_clear_error (&err);
    }
  g_file_enumerator_close (en, NULL, NULL);
  g_object_unref (en);
}

static void
on_back (GtkButton *, gpointer data)
{
  KastFiles *k = data;
  GFile *parent = g_file_get_parent (k->dir);
  if (parent)
    {
      files_navigate (k, parent);
      g_object_unref (parent);
    }
}

static void
on_home (GtkButton *, gpointer data)
{
  KastFiles *k = data;
  GFile *home = g_file_new_for_path (g_get_home_dir ());
  files_navigate (k, home);
  g_object_unref (home);
}

static void
on_go (GtkButton *, gpointer data)
{
  KastFiles *k = data;
  const char *path = gtk_editable_get_text (GTK_EDITABLE (k->path_entry));
  GFile *f = g_file_new_for_path (path);
  GFileType type = g_file_query_file_type (f, G_FILE_QUERY_INFO_NONE, NULL);
  if (type == G_FILE_TYPE_DIRECTORY)
    files_navigate (k, f);
  else
    gtk_label_set_text (k->status, "Not a folder");
  g_object_unref (f);
}

static void
on_new_folder (GtkButton *, gpointer data)
{
  KastFiles *k = data;
  GFile *f = g_file_get_child (k->dir, "New folder");
  GError *err = NULL;
  g_file_make_directory (f, NULL, &err);
  if (err)
    {
      gtk_label_set_text (k->status, err->message);
      g_error_free (err);
    }
  else
    files_navigate (k, k->dir);
  g_object_unref (f);
}

static void
on_trash (GtkButton *, gpointer data)
{
  KastFiles *k = data;
  GtkListBoxRow *row = gtk_list_box_get_selected_row (GTK_LIST_BOX (k->list));
  if (!row)
    {
      gtk_label_set_text (k->status, "Select a file first");
      return;
    }
  GFile *f = g_object_get_data (G_OBJECT (row), "kast-file");
  GError *err = NULL;
  if (!g_file_trash (f, NULL, &err))
    {
      gtk_label_set_text (k->status, err ? err->message : "Cannot trash");
      g_clear_error (&err);
    }
  else
    files_navigate (k, k->dir);
}

GtkWidget *
kast_files_page (KastUi *ui)
{
  KastFiles *k = g_new0 (KastFiles, 1);
  k->ui = ui;

  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_set_margin_top (box, 16);
  gtk_widget_set_margin_bottom (box, 12);
  gtk_widget_set_margin_start (box, 16);
  gtk_widget_set_margin_end (box, 16);

  GtkWidget *toolbar = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);

  GtkWidget *back = gtk_button_new_from_icon_name ("go-previous-symbolic");
  gtk_widget_set_tooltip_text (back, "Back");
  g_signal_connect (back, "clicked", G_CALLBACK (on_back), k);
  gtk_box_append (GTK_BOX (toolbar), back);

  GtkWidget *home = gtk_button_new_from_icon_name ("go-home-symbolic");
  gtk_widget_set_tooltip_text (home, "Home");
  g_signal_connect (home, "clicked", G_CALLBACK (on_home), k);
  gtk_box_append (GTK_BOX (toolbar), home);

  k->path_entry = GTK_ENTRY (gtk_entry_new ());
  gtk_widget_set_hexpand (GTK_WIDGET (k->path_entry), TRUE);
  g_signal_connect (k->path_entry, "activate", G_CALLBACK (on_go), k);
  gtk_box_append (GTK_BOX (toolbar), GTK_WIDGET (k->path_entry));

  GtkWidget *go = gtk_button_new_with_label ("Go");
  g_signal_connect (go, "clicked", G_CALLBACK (on_go), k);
  gtk_box_append (GTK_BOX (toolbar), go);

  GtkWidget *newf = gtk_button_new_from_icon_name ("folder-new-symbolic");
  gtk_widget_set_tooltip_text (newf, "New folder");
  g_signal_connect (newf, "clicked", G_CALLBACK (on_new_folder), k);
  gtk_box_append (GTK_BOX (toolbar), newf);

  GtkWidget *trash = gtk_button_new_from_icon_name ("user-trash-symbolic");
  gtk_widget_set_tooltip_text (trash, "Move selected to trash");
  g_signal_connect (trash, "clicked", G_CALLBACK (on_trash), k);
  gtk_box_append (GTK_BOX (toolbar), trash);
  gtk_box_append (GTK_BOX (box), toolbar);

  GtkWidget *scroll = gtk_scrolled_window_new ();
  gtk_widget_set_hexpand (scroll, TRUE);
  gtk_widget_set_vexpand (scroll, TRUE);
  gtk_widget_add_css_class (scroll, "card");
  gtk_box_append (GTK_BOX (box), scroll);

  k->list = GTK_LIST_BOX (gtk_list_box_new ());
  gtk_list_box_set_sort_func (GTK_LIST_BOX (k->list), row_sort, NULL, NULL);
  g_signal_connect (k->list, "row-activated", G_CALLBACK (on_row_activated), k);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scroll), GTK_WIDGET (k->list));

  k->status = GTK_LABEL (gtk_label_new (""));
  gtk_widget_add_css_class (GTK_WIDGET (k->status), "dim-label");
  gtk_widget_set_halign (GTK_WIDGET (k->status), GTK_ALIGN_START);
  gtk_box_append (GTK_BOX (box), GTK_WIDGET (k->status));

  k->dir = g_file_new_for_path (g_get_home_dir ());
  files_navigate (k, k->dir);

  g_object_set_data_full (G_OBJECT (box), "kast-files", k, g_free);
  return box;
}
