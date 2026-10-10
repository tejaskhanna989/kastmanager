/* Kast Manager 2.x — application shell (Tejas Khanna, GPL-3.0-or-later).
 * Fresh minimal codebase. No GNOME System Monitor code remains. */
#include <config.h>

#include <locale.h>
#include <glib/gi18n.h>

#include "kast-resources.h"
#include "kast.h"

static const char *page_names[KAST_NPAGES] = {
  KAST_PAGE_DASHBOARD, KAST_PAGE_PROCESSES, KAST_PAGE_GRAPHS,
  KAST_PAGE_DISKS, KAST_PAGE_FILES,
};

void
kast_show_page (KastUi *ui, const char *page)
{
  adw_view_stack_set_visible_child_name (ui->stack, page);
  kast_nav_sync (ui);
}

void
kast_nav_sync (KastUi *ui)
{
  const char *page = adw_view_stack_get_visible_child_name (ui->stack);
  for (int i = 0; i < KAST_NPAGES; i++)
    {
      GtkStyleContext *ctx = gtk_widget_get_style_context (ui->nav[i]);
      if (g_strcmp0 (page, ui->nav_names[i]) == 0)
        gtk_style_context_add_class (ctx, "kast-nav-active");
      else
        gtk_style_context_remove_class (ctx, "kast-nav-active");
    }
}

static void
on_nav_clicked (GtkButton *btn, gpointer data)
{
  KastUi *ui = data;
  const char *page = g_object_get_data (G_OBJECT (btn), "kast-page");
  kast_show_page (ui, page);
}

static void
on_page_changed (AdwViewStack *, GParamSpec *, gpointer data)
{
  kast_nav_sync (data);
}

static void
on_about (GSimpleAction *, GVariant *, gpointer data)
{
  KastUi *ui = data;
  const char *developers[] = { "Tejas Khanna", NULL };
  AdwDialog *dlg = ADW_DIALOG (g_object_new (ADW_TYPE_ABOUT_DIALOG,
      "application-name", "Kast Manager",
      "application-icon", APP_ID,
      "developer-name", "Tejas Khanna",
      "version", VERSION,
      "developers", developers,
      "copyright", "© 2026 Tejas Khanna",
      "license-type", GTK_LICENSE_GPL_3_0,
      "website", "https://github.com/tejaskhanna989/kastmanager",
      "issue-url", "https://github.com/tejaskhanna989/kastmanager/issues",
      NULL));
  adw_dialog_present (dlg, ui->window);
}

static void
on_quit (GSimpleAction *, GVariant *, gpointer data)
{
  KastUi *ui = data;
  gtk_window_close (GTK_WINDOW (ui->window));
}

static GtkWidget *
nav_button (KastUi *ui, int idx, const char *icon, const char *label)
{
  GtkWidget *btn = gtk_button_new ();
  gtk_widget_add_css_class (btn, "kast-nav");
  gtk_button_set_has_frame (GTK_BUTTON (btn), FALSE);
  gtk_widget_set_halign (btn, GTK_ALIGN_FILL);
  g_object_set_data (G_OBJECT (btn), "kast-page", (gpointer) ui->nav_names[idx]);
  g_signal_connect (btn, "clicked", G_CALLBACK (on_nav_clicked), ui);

  GtkWidget *row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  GtkWidget *img = gtk_image_new_from_icon_name (icon);
  gtk_box_append (GTK_BOX (row), img);
  GtkWidget *lab = gtk_label_new (label);
  gtk_widget_set_halign (lab, GTK_ALIGN_START);
  gtk_box_append (GTK_BOX (row), lab);
  gtk_button_set_child (GTK_BUTTON (btn), row);

  ui->nav[idx] = btn;
  return btn;
}

static void
on_startup (GApplication *gapp, gpointer data)
{
  KastUi *ui = data;
  ui->app = GTK_APPLICATION (gapp);

  g_resources_register (kast_get_resource ());

  /* Stock Adwaita dark: immune to quirky host themes. */
  g_object_set (gtk_settings_get_default (), "gtk-theme-name", "Adwaita", NULL);
  AdwStyleManager *sm = adw_style_manager_get_default ();
  adw_style_manager_set_color_scheme (sm, ADW_COLOR_SCHEME_PREFER_DARK);

  GtkCssProvider *css = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (css, "/io/github/tejaskhanna989/KastManager/style.css");
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (css),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (css);

  ui->nav_names[0] = KAST_PAGE_DASHBOARD;
  ui->nav_names[1] = KAST_PAGE_PROCESSES;
  ui->nav_names[2] = KAST_PAGE_GRAPHS;
  ui->nav_names[3] = KAST_PAGE_DISKS;
  ui->nav_names[4] = KAST_PAGE_FILES;

  GtkWidget *win = adw_application_window_new (GTK_APPLICATION (gapp));
  gtk_window_set_title (GTK_WINDOW (win), "Kast Manager");
  gtk_window_set_default_size (GTK_WINDOW (win), 980, 680);
  gtk_window_set_icon_name (GTK_WINDOW (win), APP_ID);
  ui->window = win;

  GtkWidget *layout = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  adw_application_window_set_content (ADW_APPLICATION_WINDOW (win), layout);

  /* ---- sidebar ---- */
  GtkWidget *side = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  gtk_widget_add_css_class (side, "kast-sidebar");
  gtk_widget_set_size_request (side, 188, -1);
  gtk_widget_set_margin_top (side, 14);
  gtk_widget_set_margin_bottom (side, 14);
  gtk_widget_set_margin_start (side, 10);
  gtk_widget_set_margin_end (side, 10);
  gtk_box_append (GTK_BOX (layout), side);

  GtkWidget *logo = gtk_image_new_from_resource ("/io/github/tejaskhanna989/KastManager/kast-logo.svg");
  gtk_image_set_pixel_size (GTK_IMAGE (logo), 56);
  gtk_widget_set_halign (logo, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_bottom (logo, 6);
  gtk_box_append (GTK_BOX (side), logo);

  GtkWidget *brand = gtk_label_new ("KAST");
  gtk_widget_add_css_class (brand, "kast-brand");
  gtk_widget_set_halign (brand, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (side), brand);

  GtkWidget *sep = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_widget_set_margin_top (sep, 8);
  gtk_widget_set_margin_bottom (sep, 8);
  gtk_box_append (GTK_BOX (side), sep);

  gtk_box_append (GTK_BOX (side), nav_button (ui, 0, "view-grid-symbolic", "Dashboard"));
  gtk_box_append (GTK_BOX (side), nav_button (ui, 1, "processes-symbolic", "Processes"));
  gtk_box_append (GTK_BOX (side), nav_button (ui, 2, "resources-symbolic", "Graphs"));
  gtk_box_append (GTK_BOX (side), nav_button (ui, 3, "drive-harddisk-symbolic", "File Systems"));
  gtk_box_append (GTK_BOX (side), nav_button (ui, 4, "folder-symbolic", "Files"));

  GtkWidget *spacer = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_vexpand (spacer, TRUE);
  gtk_box_append (GTK_BOX (side), spacer);

  GtkWidget *ver = gtk_label_new ("v" VERSION);
  gtk_widget_add_css_class (ver, "dim-label");
  gtk_widget_set_halign (ver, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (side), ver);

  /* ---- main column ---- */
  GtkWidget *tview = adw_toolbar_view_new ();
  gtk_widget_set_hexpand (tview, TRUE);
  gtk_box_append (GTK_BOX (layout), tview);

  GtkWidget *header = adw_header_bar_new ();
  adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (tview), header);

  GtkWidget *htitle = gtk_label_new ("Kast Manager");
  gtk_widget_add_css_class (htitle, "kast-header-title");
  adw_header_bar_set_title_widget (ADW_HEADER_BAR (header), htitle);

  GtkWidget *menu_btn = gtk_menu_button_new ();
  gtk_menu_button_set_icon_name (GTK_MENU_BUTTON (menu_btn), "open-menu-symbolic");
  gtk_widget_set_tooltip_text (menu_btn, "Main Menu");
  adw_header_bar_pack_end (ADW_HEADER_BAR (header), menu_btn);

  GMenu *menu = g_menu_new ();
  g_menu_append (menu, "About Kast Manager", "app.about");
  g_menu_append (menu, "Quit", "app.quit");
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (menu_btn), G_MENU_MODEL (menu));

  GtkWidget *stack = adw_view_stack_new ();
  gtk_widget_set_hexpand (stack, TRUE);
  gtk_widget_set_vexpand (stack, TRUE);
  adw_toolbar_view_set_content (ADW_TOOLBAR_VIEW (tview), stack);
  ui->stack = ADW_VIEW_STACK (stack);

  adw_view_stack_add_titled (ui->stack, kast_dash_page (ui), KAST_PAGE_DASHBOARD, "Dashboard");
  adw_view_stack_add_titled (ui->stack, kast_procs_page (ui), KAST_PAGE_PROCESSES, "Processes");
  adw_view_stack_add_titled (ui->stack, kast_graphs_page (ui), KAST_PAGE_GRAPHS, "Graphs");
  adw_view_stack_add_titled (ui->stack, kast_disks_page (ui), KAST_PAGE_DISKS, "File Systems");
  adw_view_stack_add_titled (ui->stack, kast_files_page (ui), KAST_PAGE_FILES, "Files");
  adw_view_stack_set_visible_child_name (ui->stack, KAST_PAGE_DASHBOARD);
  g_signal_connect (stack, "notify::visible-child", G_CALLBACK (on_page_changed), ui);

  const GActionEntry app_actions[] = {
    { "about", on_about, NULL, NULL, NULL, { 0 } },
    { "quit", on_quit, NULL, NULL, NULL, { 0 } },
  };
  g_action_map_add_action_entries (G_ACTION_MAP (gapp), app_actions, 2, ui);

  kast_nav_sync (ui);
  gtk_window_present (GTK_WINDOW (win));
}

int
main (int argc, char *argv[])
{
  setlocale (LC_ALL, "");
  g_set_application_name ("Kast Manager");
  g_set_prgname ("kast-manager");

  KastUi *ui = g_new0 (KastUi, 1);
  GtkApplication *app = gtk_application_new (APP_ID,
      G_APPLICATION_DEFAULT_FLAGS);
  g_application_set_resource_base_path (G_APPLICATION (app),
      "/io/github/tejaskhanna989/KastManager");
  g_signal_connect (app, "startup", G_CALLBACK (on_startup), ui);
  int status = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);
  g_free (ui);
  return status;
}
