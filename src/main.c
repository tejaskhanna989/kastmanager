/* Kast Manager 2.x — application shell (Tejas Khanna, GPL-3.0-or-later).
 * Fresh minimal codebase. No GNOME System Monitor code remains. */
#include <config.h>

#include <locale.h>
#include <glib/gi18n.h>

#include "kast-resources.h"
#include "kast.h"

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
  GtkWidget *dlg = gtk_about_dialog_new ();
  g_object_set (dlg,
      "transient-for", ui->window,
      "program-name", "Kast Manager",
      "logo-icon-name", APP_ID,
      "version", VERSION,
      "authors", developers,
      "copyright", "© 2026 Tejas Khanna",
      "license-type", GTK_LICENSE_GPL_3_0,
      "website", "https://github.com/tejaskhanna989/kastmanager",
      "comments", "If Kast Manager helps you, please donate: " KAST_DONATE_URL,
      NULL);
  gtk_window_present (GTK_WINDOW (dlg));
}

static void
on_quit (GSimpleAction *, GVariant *, gpointer data)
{
  KastUi *ui = data;
  gtk_window_close (GTK_WINDOW (ui->window));
}

static void
on_widget (GSimpleAction *, GVariant *, gpointer data)
{
  kast_widget_toggle (data);
}

static void
on_donate_pick (GtkDialog *dlg, int resp, gpointer data)
{
  KastUi *ui = data;
  if (resp == GTK_RESPONSE_YES)
    gtk_show_uri (GTK_WINDOW (ui->window), KAST_DONATE_IN_URL, GDK_CURRENT_TIME);
  else if (resp == GTK_RESPONSE_NO)
    gtk_show_uri (GTK_WINDOW (ui->window), KAST_DONATE_OUT_URL, GDK_CURRENT_TIME);
  gtk_window_destroy (GTK_WINDOW (dlg));
}

static void
kast_donate_ask (KastUi *ui)
{
  GtkWidget *dlg = gtk_message_dialog_new (GTK_WINDOW (ui->window),
      GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
      "Are You From India?");
  gtk_window_set_title (GTK_WINDOW (dlg), "Donate");
  gtk_dialog_add_button (GTK_DIALOG (dlg), "Yes", GTK_RESPONSE_YES);
  gtk_dialog_add_button (GTK_DIALOG (dlg), "No", GTK_RESPONSE_NO);
  gtk_dialog_set_default_response (GTK_DIALOG (dlg), GTK_RESPONSE_YES);
  g_signal_connect (dlg, "response", G_CALLBACK (on_donate_pick), ui);
  gtk_window_present (GTK_WINDOW (dlg));
}

static void
on_donate (GSimpleAction *, GVariant *, gpointer data)
{
  kast_donate_ask (data);
}

static void
on_donate_btn (GtkButton *, gpointer data)
{
  kast_donate_ask (data);
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
  ui->nav_labels[idx] = lab;
  ui->nav_imgs[idx] = img;
  /* owned ref: buttons are reparented between sidebar/bars/panel,
     a bare remove would finalize them */
  g_object_ref (btn);
  return btn;
}

/* sidebar modes: 0 Full, 1 Compact, 2 Hidden, 3 Top, 4 Bottom,
   5 Wide, 6 Minimal, 7 Floating, 8 Right, 9 Auto-hide,
   10 Transparent, 11 Narrow, 12 Text-only, 13 Overlay, 14 Top-floating */
const char *kast_side_mode_names[15] = {
  "Full", "Compact", "Hidden", "Top bar", "Bottom bar",
  "Wide", "Minimal", "Floating", "Right side", "Auto-hide",
  "Transparent", "Narrow", "Text-only", "Overlay", "Top-floating",
};

static void
side_move_nav_home (KastUi *ui)
{
  GtkWidget *navbox = g_object_get_data (G_OBJECT (ui->side_box), "kast-navbox");
  for (int i = 0; i < KAST_NPAGES; i++)
    {
          GtkWidget *parent = gtk_widget_get_parent (ui->nav[i]);
      if (parent)
        gtk_box_remove (GTK_BOX (parent), ui->nav[i]);
      gtk_box_append (GTK_BOX (navbox), ui->nav[i]);
    }
}

void
kast_side_apply (KastUi *ui, int mode)
{
  if (mode < 0 || mode > 14)
    mode = 0;
  ui->side_mode = mode;
  kast_cfg_set ("sidebar-mode", mode);

  side_move_nav_home (ui);
  gtk_widget_set_visible (ui->float_panel, FALSE);

  GtkStyleContext *sctx = gtk_widget_get_style_context (ui->side_box);
  gtk_style_context_remove_class (sctx, "floating");
  gtk_style_context_remove_class (sctx, "rightside");
  gtk_style_context_remove_class (sctx, "ghost");
  GtkStyleContext *tctx = gtk_widget_get_style_context (ui->top_bar);
  gtk_style_context_remove_class (tctx, "floating-bar");

  /* reset to full layout */
  gtk_widget_set_visible (ui->side_box, TRUE);
  gtk_widget_set_visible (ui->top_bar, FALSE);
  gtk_widget_set_visible (ui->bottom_bar, FALSE);
  gtk_widget_set_visible (ui->page_menu_btn, FALSE);
  gtk_widget_set_visible (ui->side_toggle_btn, FALSE);
  gtk_widget_set_size_request (ui->side_box, 188, -1);
  for (int i = 0; i < KAST_NPAGES; i++)
    {
      gtk_widget_set_visible (ui->nav_labels[i], TRUE);
      gtk_widget_set_visible (ui->nav_imgs[i], TRUE);
    }
  gtk_widget_set_visible (ui->side_logo, TRUE);
  gtk_widget_set_visible (ui->side_brand, TRUE);
  gtk_widget_set_visible (ui->side_ver, TRUE);
  gtk_image_set_pixel_size (GTK_IMAGE (ui->side_logo), 56);

  /* layout order back to sidebar-left */
  g_object_ref (ui->side_box);
  g_object_ref (ui->overlay);
  GtkWidget *layout = ui->layout;
  gtk_box_remove (GTK_BOX (layout), ui->side_box);
  gtk_box_remove (GTK_BOX (layout), ui->overlay);
  gtk_box_append (GTK_BOX (layout), ui->side_box);
  gtk_box_append (GTK_BOX (layout), ui->overlay);
  g_object_unref (ui->side_box);
  g_object_unref (ui->overlay);

  switch (mode)
    {
    case 1: /* Compact */
    case 6: /* Minimal */
      gtk_widget_set_size_request (ui->side_box, 64, -1);
      for (int i = 0; i < KAST_NPAGES; i++)
        gtk_widget_set_visible (ui->nav_labels[i], FALSE);
      gtk_widget_set_visible (ui->side_brand, FALSE);
      gtk_widget_set_visible (ui->side_ver, FALSE);
      gtk_image_set_pixel_size (GTK_IMAGE (ui->side_logo), 32);
      if (mode == 6)
        gtk_widget_set_visible (ui->side_logo, FALSE);
      break;
    case 2: /* Hidden */
      gtk_widget_set_visible (ui->side_box, FALSE);
      gtk_widget_set_visible (ui->page_menu_btn, TRUE);
      break;
    case 3: /* Top bar */
    case 4: /* Bottom bar */
      {
        gtk_widget_set_visible (ui->side_box, FALSE);
        GtkWidget *bar = mode == 3 ? ui->top_bar : ui->bottom_bar;
        for (int i = 0; i < KAST_NPAGES; i++)
          {
            gtk_box_remove (GTK_BOX (gtk_widget_get_parent (ui->nav[i])), ui->nav[i]);
            gtk_box_append (GTK_BOX (bar), ui->nav[i]);
          }
        gtk_widget_set_visible (bar, TRUE);
      }
      break;
    case 5: /* Wide */
      gtk_widget_set_size_request (ui->side_box, 250, -1);
      break;
    case 7: /* Floating */
      gtk_style_context_add_class (sctx, "floating");
      break;
    case 8: /* Right side */
      g_object_ref (ui->side_box);
      g_object_ref (ui->overlay);
      gtk_box_remove (GTK_BOX (layout), ui->side_box);
      gtk_box_remove (GTK_BOX (layout), ui->overlay);
      gtk_box_append (GTK_BOX (layout), ui->overlay);
      gtk_box_append (GTK_BOX (layout), ui->side_box);
      g_object_unref (ui->side_box);
      g_object_unref (ui->overlay);
      gtk_style_context_add_class (sctx, "rightside");
      break;
    case 9: /* Auto-hide */
      gtk_widget_set_visible (ui->side_box, FALSE);
      gtk_widget_set_visible (ui->side_toggle_btn, TRUE);
      break;
    case 10: /* Transparent */
      gtk_style_context_add_class (sctx, "ghost");
      break;
    case 11: /* Narrow */
      gtk_widget_set_size_request (ui->side_box, 48, -1);
      for (int i = 0; i < KAST_NPAGES; i++)
        gtk_widget_set_visible (ui->nav_labels[i], FALSE);
      gtk_widget_set_visible (ui->side_brand, FALSE);
      gtk_widget_set_visible (ui->side_ver, FALSE);
      gtk_widget_set_visible (ui->side_logo, FALSE);
      break;
    case 12: /* Text-only */
      for (int i = 0; i < KAST_NPAGES; i++)
        gtk_widget_set_visible (ui->nav_imgs[i], FALSE);
      break;
    case 13: /* Overlay */
      {
        gtk_widget_set_visible (ui->side_box, FALSE);
        for (int i = 0; i < KAST_NPAGES; i++)
          {
            gtk_box_remove (GTK_BOX (gtk_widget_get_parent (ui->nav[i])), ui->nav[i]);
            gtk_box_append (GTK_BOX (ui->float_panel), ui->nav[i]);
          }
        gtk_widget_set_visible (ui->float_panel, TRUE);
        gtk_widget_set_visible (ui->side_toggle_btn, TRUE);
      }
      break;
    case 14: /* Top-floating */
      {
        gtk_widget_set_visible (ui->side_box, FALSE);
        for (int i = 0; i < KAST_NPAGES; i++)
          {
            gtk_box_remove (GTK_BOX (gtk_widget_get_parent (ui->nav[i])), ui->nav[i]);
            gtk_box_append (GTK_BOX (ui->top_bar), ui->nav[i]);
          }
        gtk_style_context_add_class (tctx, "floating-bar");
        gtk_widget_set_visible (ui->top_bar, TRUE);
      }
      break;
    default: /* Full */
      break;
    }
  kast_nav_sync (ui);
}

const char *kast_theme_names[3] = { "Dark", "Light", "System" };

void
kast_apply_theme (KastUi *ui, int mode)
{
  if (mode < 0 || mode > 2)
    mode = 0;
  ui->theme_mode = mode;
  kast_cfg_set ("theme", mode);

  AdwStyleManager *sm = adw_style_manager_get_default ();
  if (mode == 1)
    adw_style_manager_set_color_scheme (sm, ADW_COLOR_SCHEME_FORCE_LIGHT);
  else if (mode == 2)
    adw_style_manager_set_color_scheme (sm, ADW_COLOR_SCHEME_DEFAULT);
  else
    adw_style_manager_set_color_scheme (sm, ADW_COLOR_SCHEME_PREFER_DARK);

  GdkDisplay *dpy = gdk_display_get_default ();
  if (ui->theme_css)
    {
      gtk_style_context_remove_provider_for_display (dpy,
          GTK_STYLE_PROVIDER (ui->theme_css));
      g_object_unref (ui->theme_css);
      ui->theme_css = NULL;
    }
  if (mode == 2)
    return; /* System: pure stock Adwaita, no Kast CSS */
  ui->theme_css = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (ui->theme_css,
      mode == 1 ? "/io/github/tejaskhanna989/KastManager/style-light.css"
                : "/io/github/tejaskhanna989/KastManager/style.css");
  gtk_style_context_add_provider_for_display (dpy,
      GTK_STYLE_PROVIDER (ui->theme_css),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static void
on_side_toggle (GtkButton *, gpointer data)
{
  KastUi *ui = data;
  if (ui->side_mode == 13)
    gtk_widget_set_visible (ui->float_panel,
        !gtk_widget_get_visible (ui->float_panel));
  else
    gtk_widget_set_visible (ui->side_box,
        !gtk_widget_get_visible (ui->side_box));
}

static void
on_show_page (GSimpleAction *, GVariant *param, gpointer data)
{
  kast_show_page (data, g_variant_get_string (param, NULL));
}

static void
on_startup (GApplication *gapp, gpointer data)
{
  KastUi *ui = data;
  ui->app = GTK_APPLICATION (gapp);

  g_resources_register (kast_get_resource ());

  /* Stock Adwaita: immune to quirky host themes. */
  g_object_set (gtk_settings_get_default (), "gtk-theme-name", "Adwaita", NULL);

  ui->theme_css = NULL;
  kast_apply_theme (ui, kast_cfg_get ("theme", 0));

  ui->nav_names[0] = KAST_PAGE_DASHBOARD;
  ui->nav_names[1] = KAST_PAGE_PROCESSES;
  ui->nav_names[2] = KAST_PAGE_GRAPHS;
  ui->nav_names[3] = KAST_PAGE_DISKS;
  ui->nav_names[4] = KAST_PAGE_FILES;
  ui->nav_names[5] = KAST_PAGE_RATING;
  ui->nav_names[6] = KAST_PAGE_CONTROLS;
  ui->nav_names[7] = KAST_PAGE_INFO;
  ui->nav_names[8] = KAST_PAGE_JOURNAL;
  ui->nav_names[9] = KAST_PAGE_CONNS;
  ui->nav_names[10] = KAST_PAGE_SENSORS;
  ui->nav_names[11] = KAST_PAGE_STARTUP;
  ui->nav_names[12] = KAST_PAGE_USAGE;
  ui->nav_names[13] = KAST_PAGE_USERS;
  ui->nav_names[14] = KAST_PAGE_DEVICES;
  ui->nav_names[15] = KAST_PAGE_MODULES;
  ui->nav_names[16] = KAST_PAGE_CRON;
  ui->nav_names[17] = KAST_PAGE_BATTERY;
  ui->nav_names[18] = KAST_PAGE_NETIF;
  ui->nav_names[19] = KAST_PAGE_BLUE;
  ui->nav_names[20] = KAST_PAGE_PRINTERS;
  ui->nav_names[21] = KAST_PAGE_AUDIO;
  ui->nav_names[22] = KAST_PAGE_DISPLAYS;
  ui->nav_names[23] = KAST_PAGE_INPUT;
  ui->nav_names[24] = KAST_PAGE_POWER;
  ui->nav_names[25] = KAST_PAGE_FONTS;
  ui->nav_names[26] = KAST_PAGE_ENV;
  ui->nav_names[27] = KAST_PAGE_TRASH;
  ui->nav_names[28] = KAST_PAGE_CLIP;
  ui->nav_names[29] = KAST_PAGE_KEYS;

  GtkWidget *win = gtk_application_window_new (GTK_APPLICATION (gapp));
  gtk_window_set_title (GTK_WINDOW (win), "Kast Manager");
  gtk_window_set_default_size (GTK_WINDOW (win), 980, 680);
  gtk_window_set_icon_name (GTK_WINDOW (win), APP_ID);
  ui->window = win;

  GtkWidget *header = gtk_header_bar_new ();
  gtk_window_set_titlebar (GTK_WINDOW (win), header);

  GtkWidget *htitle = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  GtkWidget *hlogo = gtk_image_new_from_resource ("/io/github/tejaskhanna989/KastManager/kast-logo.svg");
  gtk_image_set_pixel_size (GTK_IMAGE (hlogo), 22);
  gtk_box_append (GTK_BOX (htitle), hlogo);
  GtkWidget *htitle_l = gtk_label_new ("Kast Manager");
  gtk_widget_add_css_class (htitle_l, "kast-header-title");
  gtk_box_append (GTK_BOX (htitle), htitle_l);
  gtk_header_bar_set_title_widget (GTK_HEADER_BAR (header), htitle);

  GtkWidget *menu_btn = gtk_menu_button_new ();
  gtk_menu_button_set_icon_name (GTK_MENU_BUTTON (menu_btn), "open-menu-symbolic");
  gtk_widget_set_tooltip_text (menu_btn, "Main Menu");
  gtk_header_bar_pack_end (GTK_HEADER_BAR (header), menu_btn);

  ui->side_toggle_btn = gtk_toggle_button_new ();
  gtk_button_set_icon_name (GTK_BUTTON (ui->side_toggle_btn), "pan-start-symbolic");
  gtk_widget_set_tooltip_text (ui->side_toggle_btn, "Toggle sidebar");
  gtk_widget_set_visible (ui->side_toggle_btn, FALSE);
  g_signal_connect (ui->side_toggle_btn, "clicked", G_CALLBACK (on_side_toggle), ui);
  gtk_header_bar_pack_start (GTK_HEADER_BAR (header), ui->side_toggle_btn);

  ui->page_menu_btn = gtk_menu_button_new ();
  gtk_menu_button_set_icon_name (GTK_MENU_BUTTON (ui->page_menu_btn), "view-grid-symbolic");
  gtk_widget_set_tooltip_text (ui->page_menu_btn, "Go to page");
  gtk_widget_set_visible (ui->page_menu_btn, FALSE);
  gtk_header_bar_pack_start (GTK_HEADER_BAR (header), ui->page_menu_btn);

  GMenu *pagemenu = g_menu_new ();
  g_menu_append (pagemenu, "Dashboard", "app.show-page::dashboard");
  g_menu_append (pagemenu, "Processes", "app.show-page::processes");
  g_menu_append (pagemenu, "Graphs", "app.show-page::graphs");
  g_menu_append (pagemenu, "File Systems", "app.show-page::disks");
  g_menu_append (pagemenu, "Files", "app.show-page::files");
  g_menu_append (pagemenu, "Rating", "app.show-page::rating");
  g_menu_append (pagemenu, "Controls", "app.show-page::controls");
  g_menu_append (pagemenu, "Info", "app.show-page::info");
  g_menu_append (pagemenu, "Journal", "app.show-page::journal");
  g_menu_append (pagemenu, "Connections", "app.show-page::conns");
  g_menu_append (pagemenu, "Sensors", "app.show-page::sensors");
  g_menu_append (pagemenu, "Startup", "app.show-page::startup");
  g_menu_append (pagemenu, "Disk usage", "app.show-page::usage");
  g_menu_append (pagemenu, "Users", "app.show-page::users");
  g_menu_append (pagemenu, "Devices", "app.show-page::devices");
  g_menu_append (pagemenu, "Modules", "app.show-page::modules");
  g_menu_append (pagemenu, "Cron", "app.show-page::cron");
  g_menu_append (pagemenu, "Battery", "app.show-page::battery");
  g_menu_append (pagemenu, "Network", "app.show-page::netif");
  g_menu_append (pagemenu, "Bluetooth", "app.show-page::blue");
  g_menu_append (pagemenu, "Printers", "app.show-page::printers");
  g_menu_append (pagemenu, "Audio", "app.show-page::audio");
  g_menu_append (pagemenu, "Displays", "app.show-page::displays");
  g_menu_append (pagemenu, "Input", "app.show-page::input");
  g_menu_append (pagemenu, "Power", "app.show-page::power");
  g_menu_append (pagemenu, "Fonts", "app.show-page::fonts");
  g_menu_append (pagemenu, "Environment", "app.show-page::env");
  g_menu_append (pagemenu, "Trash", "app.show-page::trash");
  g_menu_append (pagemenu, "Clipboard", "app.show-page::clip");
  g_menu_append (pagemenu, "Shortcuts", "app.show-page::keys");
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (ui->page_menu_btn),
                                  G_MENU_MODEL (pagemenu));

  GMenu *menu = g_menu_new ();
  g_menu_append (menu, "Mini widget", "app.widget");
  g_menu_append (menu, "Donate ♥", "app.donate");
  g_menu_append (menu, "About Kast Manager", "app.about");
  g_menu_append (menu, "Quit", "app.quit");
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (menu_btn), G_MENU_MODEL (menu));

  GtkWidget *layout = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_window_set_child (GTK_WINDOW (win), layout);
  ui->layout = layout;

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
  ui->side_logo = logo;

  GtkWidget *brand = gtk_label_new ("KAST");
  gtk_widget_add_css_class (brand, "kast-brand");
  gtk_widget_set_halign (brand, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (side), brand);
  ui->side_brand = brand;

  GtkWidget *sep = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_widget_set_margin_top (sep, 8);
  gtk_widget_set_margin_bottom (sep, 8);
  gtk_box_append (GTK_BOX (side), sep);

  GtkWidget *navscroll = gtk_scrolled_window_new ();
  gtk_widget_set_vexpand (navscroll, TRUE);
  gtk_widget_set_hexpand (navscroll, TRUE);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (navscroll),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_box_append (GTK_BOX (side), navscroll);

  GtkWidget *navbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (navscroll), navbox);
  ui->side_box = side;
  /* nav buttons live in navbox; top/bottom modes reparent them */
  g_object_set_data (G_OBJECT (side), "kast-navbox", navbox);

  gtk_box_append (GTK_BOX (navbox), nav_button (ui, 0, "view-grid-symbolic", "Dashboard"));
  gtk_box_append (GTK_BOX (navbox), nav_button (ui, 1, "kast-processes-symbolic", "Processes"));
  gtk_box_append (GTK_BOX (navbox), nav_button (ui, 2, "kast-resources-symbolic", "Graphs"));
  gtk_box_append (GTK_BOX (navbox), nav_button (ui, 3, "drive-harddisk-symbolic", "File Systems"));
  gtk_box_append (GTK_BOX (navbox), nav_button (ui, 4, "folder-symbolic", "Files"));
  gtk_box_append (GTK_BOX (navbox), nav_button (ui, 5, "starred-symbolic", "Rating"));
  gtk_box_append (GTK_BOX (navbox), nav_button (ui, 6, "preferences-system-symbolic", "Controls"));
  gtk_box_append (GTK_BOX (navbox), nav_button (ui, 7, "dialog-information-symbolic", "Info"));
  gtk_box_append (GTK_BOX (navbox), nav_button (ui, 8, "document-properties-symbolic", "Journal"));
  gtk_box_append (GTK_BOX (navbox), nav_button (ui, 9, "network-wired-symbolic", "Connections"));
  gtk_box_append (GTK_BOX (navbox), nav_button (ui, 10, "computer-symbolic", "Sensors"));
  gtk_box_append (GTK_BOX (navbox), nav_button (ui, 11, "system-run-symbolic", "Startup"));
  gtk_box_append (GTK_BOX (navbox), nav_button (ui, 12, "drive-harddisk-symbolic", "Disk usage"));
  gtk_box_append (GTK_BOX (navbox), nav_button (ui, 13, "user-info-symbolic", "Users"));
  gtk_box_append (GTK_BOX (navbox), nav_button (ui, 14, "video-display-symbolic", "Devices"));
  gtk_box_append (GTK_BOX (navbox), nav_button (ui, 15, "utilities-terminal-symbolic", "Modules"));
  gtk_box_append (GTK_BOX (navbox), nav_button (ui, 16, "dialog-information-symbolic", "Cron"));
  gtk_box_append (GTK_BOX (navbox), nav_button (ui, 17, "battery-symbolic", "Battery"));
  gtk_box_append (GTK_BOX (navbox), nav_button (ui, 18, "network-wired-symbolic", "Network"));
  gtk_box_append (GTK_BOX (navbox), nav_button (ui, 19, "bluetooth-symbolic", "Bluetooth"));
  gtk_box_append (GTK_BOX (navbox), nav_button (ui, 20, "printer-symbolic", "Printers"));
  gtk_box_append (GTK_BOX (navbox), nav_button (ui, 21, "multimedia-player-symbolic", "Audio"));
  gtk_box_append (GTK_BOX (navbox), nav_button (ui, 22, "video-display-symbolic", "Displays"));
  gtk_box_append (GTK_BOX (navbox), nav_button (ui, 23, "input-keyboard-symbolic", "Input"));
  gtk_box_append (GTK_BOX (navbox), nav_button (ui, 24, "system-run-symbolic", "Power"));
  gtk_box_append (GTK_BOX (navbox), nav_button (ui, 25, "user-info-symbolic", "Fonts"));
  gtk_box_append (GTK_BOX (navbox), nav_button (ui, 26, "utilities-terminal-symbolic", "Environment"));
  gtk_box_append (GTK_BOX (navbox), nav_button (ui, 27, "user-trash-symbolic", "Trash"));
  gtk_box_append (GTK_BOX (navbox), nav_button (ui, 28, "edit-copy-symbolic", "Clipboard"));
  gtk_box_append (GTK_BOX (navbox), nav_button (ui, 29, "dialog-information-symbolic", "Shortcuts"));

  GtkWidget *donate_btn = gtk_button_new_with_label ("♥ Donate");
  gtk_widget_add_css_class (donate_btn, "suggested-action");
  gtk_widget_set_halign (donate_btn, GTK_ALIGN_FILL);
  gtk_widget_set_margin_top (donate_btn, 6);
  g_signal_connect (donate_btn, "clicked", G_CALLBACK (on_donate_btn), ui);
  gtk_box_append (GTK_BOX (side), donate_btn);

  GtkWidget *ver = gtk_label_new ("v" VERSION);
  gtk_widget_add_css_class (ver, "dim-label");
  gtk_widget_set_halign (ver, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (side), ver);
  ui->side_ver = ver;

  /* ---- main column (wrapped in overlay for Overlay mode) ---- */
  GtkWidget *right = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_hexpand (right, TRUE);
  ui->right_col = right;

  GtkWidget *overlay = gtk_overlay_new ();
  gtk_widget_set_hexpand (overlay, TRUE);
  gtk_box_append (GTK_BOX (layout), overlay);
  ui->overlay = overlay;
  gtk_overlay_set_child (GTK_OVERLAY (overlay), right);

  ui->float_panel = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  gtk_widget_add_css_class (ui->float_panel, "kast-float-panel");
  gtk_widget_set_halign (ui->float_panel, GTK_ALIGN_START);
  gtk_widget_set_valign (ui->float_panel, GTK_ALIGN_FILL);
  gtk_widget_set_margin_top (ui->float_panel, 12);
  gtk_widget_set_margin_bottom (ui->float_panel, 12);
  gtk_widget_set_margin_start (ui->float_panel, 12);
  gtk_widget_set_size_request (ui->float_panel, 200, -1);
  gtk_widget_set_visible (ui->float_panel, FALSE);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), ui->float_panel);

  ui->top_bar = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_add_css_class (ui->top_bar, "kast-topbar");
  gtk_widget_set_visible (ui->top_bar, FALSE);
  gtk_box_append (GTK_BOX (right), ui->top_bar);

  GtkWidget *stack = adw_view_stack_new ();
  gtk_widget_set_hexpand (stack, TRUE);
  gtk_widget_set_vexpand (stack, TRUE);
  gtk_box_append (GTK_BOX (right), stack);
  ui->stack = ADW_VIEW_STACK (stack);

  ui->bottom_bar = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_add_css_class (ui->bottom_bar, "kast-topbar");
  gtk_widget_set_visible (ui->bottom_bar, FALSE);
  gtk_box_append (GTK_BOX (right), ui->bottom_bar);

  adw_view_stack_add_titled (ui->stack, kast_dash_page (ui), KAST_PAGE_DASHBOARD, "Dashboard");
  adw_view_stack_add_titled (ui->stack, kast_procs_page (ui), KAST_PAGE_PROCESSES, "Processes");
  adw_view_stack_add_titled (ui->stack, kast_graphs_page (ui), KAST_PAGE_GRAPHS, "Graphs");
  adw_view_stack_add_titled (ui->stack, kast_disks_page (ui), KAST_PAGE_DISKS, "File Systems");
  adw_view_stack_add_titled (ui->stack, kast_files_page (ui), KAST_PAGE_FILES, "Files");
  adw_view_stack_add_titled (ui->stack, kast_rating_page (ui), KAST_PAGE_RATING, "Rating");
  adw_view_stack_add_titled (ui->stack, kast_controls_page (ui), KAST_PAGE_CONTROLS, "Controls");
  adw_view_stack_add_titled (ui->stack, kast_sysinfo_page (ui), KAST_PAGE_INFO, "Info");
  adw_view_stack_add_titled (ui->stack, kast_journal_page (ui), KAST_PAGE_JOURNAL, "Journal");
  adw_view_stack_add_titled (ui->stack, kast_conns_page (ui), KAST_PAGE_CONNS, "Connections");
  adw_view_stack_add_titled (ui->stack, kast_sensors_page (ui), KAST_PAGE_SENSORS, "Sensors");
  adw_view_stack_add_titled (ui->stack, kast_startup_page (ui), KAST_PAGE_STARTUP, "Startup");
  adw_view_stack_add_titled (ui->stack, kast_usage_page (ui), KAST_PAGE_USAGE, "Disk usage");
  adw_view_stack_add_titled (ui->stack, kast_users_page (ui), KAST_PAGE_USERS, "Users");
  adw_view_stack_add_titled (ui->stack, kast_devices_page (ui), KAST_PAGE_DEVICES, "Devices");
  adw_view_stack_add_titled (ui->stack, kast_modules_page (ui), KAST_PAGE_MODULES, "Modules");
  adw_view_stack_add_titled (ui->stack, kast_cron_page (ui), KAST_PAGE_CRON, "Cron");
  adw_view_stack_add_titled (ui->stack, kast_battery_page (ui), KAST_PAGE_BATTERY, "Battery");
  adw_view_stack_add_titled (ui->stack, kast_netif_page (ui), KAST_PAGE_NETIF, "Network");
  adw_view_stack_add_titled (ui->stack, kast_blue_page (ui), KAST_PAGE_BLUE, "Bluetooth");
  adw_view_stack_add_titled (ui->stack, kast_printers_page (ui), KAST_PAGE_PRINTERS, "Printers");
  adw_view_stack_add_titled (ui->stack, kast_audio_page (ui), KAST_PAGE_AUDIO, "Audio");
  adw_view_stack_add_titled (ui->stack, kast_displays_page (ui), KAST_PAGE_DISPLAYS, "Displays");
  adw_view_stack_add_titled (ui->stack, kast_input_page (ui), KAST_PAGE_INPUT, "Input");
  adw_view_stack_add_titled (ui->stack, kast_power_page (ui), KAST_PAGE_POWER, "Power");
  adw_view_stack_add_titled (ui->stack, kast_fonts_page (ui), KAST_PAGE_FONTS, "Fonts");
  adw_view_stack_add_titled (ui->stack, kast_env_page (ui), KAST_PAGE_ENV, "Environment");
  adw_view_stack_add_titled (ui->stack, kast_trash_page (ui), KAST_PAGE_TRASH, "Trash");
  adw_view_stack_add_titled (ui->stack, kast_clipboard_page (ui), KAST_PAGE_CLIP, "Clipboard");
  adw_view_stack_add_titled (ui->stack, kast_shortcuts_page (ui), KAST_PAGE_KEYS, "Shortcuts");
  adw_view_stack_set_visible_child_name (ui->stack, KAST_PAGE_DASHBOARD);
  g_signal_connect (stack, "notify::visible-child", G_CALLBACK (on_page_changed), ui);

  const GActionEntry app_actions[] = {
    { "about", on_about, NULL, NULL, NULL, { 0 } },
    { "widget", on_widget, NULL, NULL, NULL, { 0 } },
    { "donate", on_donate, NULL, NULL, NULL, { 0 } },
    { "show-page", on_show_page, "s", NULL, NULL, { 0 } },
    { "quit", on_quit, NULL, NULL, NULL, { 0 } },
  };
  g_action_map_add_action_entries (G_ACTION_MAP (gapp), app_actions, 5, ui);

  kast_nav_sync (ui);
  kast_side_apply (ui, kast_cfg_get ("sidebar-mode", 0));
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
      G_APPLICATION_FLAGS_NONE);
  g_application_set_resource_base_path (G_APPLICATION (app),
      "/io/github/tejaskhanna989/KastManager");
  g_signal_connect (app, "startup", G_CALLBACK (on_startup), ui);
  int status = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);
  g_free (ui);
  return status;
}
