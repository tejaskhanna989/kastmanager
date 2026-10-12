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
  GtkSearchEntry *fsearch;
  gboolean show_hidden;
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
  const char *fq = k->fsearch
      ? gtk_editable_get_text (GTK_EDITABLE (k->fsearch)) : "";
  char *fqn = (fq && *fq) ? g_utf8_casefold (fq, -1) : NULL;
  while ((info = g_file_enumerator_next_file (en, NULL, &err)) != NULL)
    {
      const char *name = g_file_info_get_name (info);
      if (name[0] == '.' && !k->show_hidden)
        {
          g_object_unref (info);
          continue;
        }
      if (fqn)
        {
          char *nn = g_utf8_casefold (name, -1);
          gboolean hit = strstr (nn, fqn) != NULL;
          g_free (nn);
          if (!hit)
            {
              g_object_unref (info);
              continue;
            }
        }
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
  g_free (fqn);
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
on_fsearch (GtkSearchEntry *, gpointer data)
{
  KastFiles *k = data;
  files_navigate (k, k->dir);
}

static void
on_hidden_toggle (GtkToggleButton *btn, gpointer data)
{
  KastFiles *k = data;
  k->show_hidden = gtk_toggle_button_get_active (btn);
  files_navigate (k, k->dir);
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

static GtkListBoxRow *
files_selected (KastFiles *k)
{
  GtkListBoxRow *row = gtk_list_box_get_selected_row (GTK_LIST_BOX (k->list));
  if (!row)
    gtk_label_set_text (k->status, "Select a file first");
  return row;
}

static void
on_rename_response (GtkDialog *dlg, int resp, gpointer data)
{
  KastFiles *k = data;
  if (resp == GTK_RESPONSE_OK)
    {
      GtkEntry *entry = g_object_get_data (G_OBJECT (dlg), "kast-entry");
      GtkListBoxRow *row = files_selected (k);
      const char *name = entry ? gtk_editable_get_text (GTK_EDITABLE (entry)) : "";
      if (row && name && *name)
        {
          GFile *f = g_object_get_data (G_OBJECT (row), "kast-file");
          GError *err = NULL;
          if (!g_file_set_display_name (f, name, NULL, &err))
            {
              gtk_label_set_text (k->status, err ? err->message : "Cannot rename");
              g_clear_error (&err);
            }
          else
            files_navigate (k, k->dir);
        }
    }
  gtk_window_destroy (GTK_WINDOW (dlg));
}

static void
on_rename (GtkButton *btn, gpointer data)
{
  KastFiles *k = data;
  (void) btn;
  GtkListBoxRow *row = files_selected (k);
  if (!row)
    return;
  const char *old = g_object_get_data (G_OBJECT (row), "kast-name");
  GtkWidget *dlg = gtk_message_dialog_new (GTK_WINDOW (k->ui->window),
      GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
      "Rename “%s” to:", old ? old : "");
  gtk_window_set_title (GTK_WINDOW (dlg), "Rename");
  GtkWidget *entry = gtk_entry_new ();
  if (old)
    gtk_editable_set_text (GTK_EDITABLE (entry), old);
  gtk_widget_set_margin_top (entry, 8);
  gtk_box_append (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dlg))), entry);
  g_object_set_data (G_OBJECT (dlg), "kast-entry", entry);
  gtk_dialog_add_button (GTK_DIALOG (dlg), "Cancel", GTK_RESPONSE_CANCEL);
  gtk_dialog_add_button (GTK_DIALOG (dlg), "Rename", GTK_RESPONSE_OK);
  gtk_dialog_set_default_response (GTK_DIALOG (dlg), GTK_RESPONSE_OK);
  g_signal_connect (dlg, "response", G_CALLBACK (on_rename_response), k);
  gtk_window_present (GTK_WINDOW (dlg));
}

static void
on_props (GtkButton *btn, gpointer data)
{
  KastFiles *k = data;
  (void) btn;
  GtkListBoxRow *row = files_selected (k);
  if (!row)
    return;
  GFile *f = g_object_get_data (G_OBJECT (row), "kast-file");
  const char *name = g_object_get_data (G_OBJECT (row), "kast-name");
  GError *err = NULL;
  GFileInfo *info = g_file_query_info (f,
      "standard::size,time::modified,standard::type,unix::mode,owner::user",
      G_FILE_QUERY_INFO_NONE, NULL, &err);
  if (!info)
    {
      gtk_label_set_text (k->status, err ? err->message : "Cannot read info");
      g_clear_error (&err);
      return;
    }
  char *sz = kast_bytes (g_file_info_get_size (info));
  GDateTime *mt = g_file_info_get_modification_date_time (info);
  char *mts = mt ? g_date_time_format (mt, "%Y-%m-%d %H:%M:%S") : g_strdup ("—");
  char *uri = g_file_get_uri (f);
  const char *owner = g_file_info_get_attribute_string (info, "owner::user");
  guint32 mode = g_file_info_get_attribute_uint32 (info, "unix::mode");
  char *body = g_strdup_printf ("Location: %s\nSize: %s\nModified: %s\nOwner: %s\nPermissions: %04o",
      uri, sz, mts, owner ? owner : "?", mode & 07777);

  GtkWidget *dlg = gtk_message_dialog_new (GTK_WINDOW (k->ui->window),
      GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
      "Properties of %s", name ? name : "");
  GtkWidget *bl = gtk_label_new (body);
  gtk_label_set_selectable (GTK_LABEL (bl), TRUE);
  gtk_widget_set_halign (bl, GTK_ALIGN_START);
  gtk_widget_set_margin_top (bl, 8);
  gtk_box_append (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dlg))), bl);
  g_signal_connect_swapped (dlg, "response", G_CALLBACK (gtk_window_destroy), dlg);
  gtk_window_present (GTK_WINDOW (dlg));

  g_free (sz); g_free (mts); g_free (uri); g_free (body);
  g_object_unref (info);
}

static void
on_copy_path (GtkButton *btn, gpointer data)
{
  KastFiles *k = data;
  (void) btn;
  GtkListBoxRow *row = files_selected (k);
  if (!row)
    return;
  GFile *f = g_object_get_data (G_OBJECT (row), "kast-file");
  char *path = g_file_get_path (f);
  if (!path)
    path = g_file_get_uri (f);
  gdk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (k->list)), path);
  gtk_label_set_text (k->status, "Path copied to clipboard");
  g_free (path);
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

  k->fsearch = GTK_SEARCH_ENTRY (gtk_search_entry_new ());
  g_object_set (k->fsearch, "placeholder-text", "Filter here", NULL);
  gtk_widget_set_size_request (GTK_WIDGET (k->fsearch), 150, -1);
  g_signal_connect (k->fsearch, "search-changed", G_CALLBACK (on_fsearch), k);
  gtk_box_append (GTK_BOX (toolbar), GTK_WIDGET (k->fsearch));

  GtkWidget *hidden = gtk_toggle_button_new_with_label ("Hidden");
  gtk_widget_set_tooltip_text (hidden, "Show hidden files");
  g_signal_connect (hidden, "toggled", G_CALLBACK (on_hidden_toggle), k);
  gtk_box_append (GTK_BOX (toolbar), hidden);

  GtkWidget *newf = gtk_button_new_from_icon_name ("folder-new-symbolic");
  gtk_widget_set_tooltip_text (newf, "New folder");
  g_signal_connect (newf, "clicked", G_CALLBACK (on_new_folder), k);
  gtk_box_append (GTK_BOX (toolbar), newf);

  GtkWidget *trash = gtk_button_new_from_icon_name ("user-trash-symbolic");
  gtk_widget_set_tooltip_text (trash, "Move selected to trash");
  g_signal_connect (trash, "clicked", G_CALLBACK (on_trash), k);
  gtk_box_append (GTK_BOX (toolbar), trash);

  GtkWidget *rename = gtk_button_new_from_icon_name ("document-edit-symbolic");
  gtk_widget_set_tooltip_text (rename, "Rename selected");
  g_signal_connect (rename, "clicked", G_CALLBACK (on_rename), k);
  gtk_box_append (GTK_BOX (toolbar), rename);

  GtkWidget *props = gtk_button_new_from_icon_name ("document-properties-symbolic");
  gtk_widget_set_tooltip_text (props, "Properties of selected");
  g_signal_connect (props, "clicked", G_CALLBACK (on_props), k);
  gtk_box_append (GTK_BOX (toolbar), props);

  GtkWidget *copy = gtk_button_new_from_icon_name ("edit-copy-symbolic");
  gtk_widget_set_tooltip_text (copy, "Copy selected path");
  g_signal_connect (copy, "clicked", G_CALLBACK (on_copy_path), k);
  gtk_box_append (GTK_BOX (toolbar), copy);
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
