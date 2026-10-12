/* Kast Manager 2.x — fresh minimal codebase (Tejas Khanna, GPL-3.0-or-later).
 * Shared application state. No GNOME System Monitor code remains. */
#ifndef KAST_H
#define KAST_H

#include <gtk/gtk.h>
#include <adwaita.h>

#define KAST_PAGE_DASHBOARD "dashboard"
#define KAST_PAGE_PROCESSES "processes"
#define KAST_PAGE_GRAPHS    "graphs"
#define KAST_PAGE_DISKS     "disks"
#define KAST_PAGE_FILES     "files"
#define KAST_PAGE_RATING    "rating"
#define KAST_PAGE_CONTROLS  "controls"
#define KAST_PAGE_INFO      "info"
#define KAST_NPAGES 8

typedef struct {
  GtkApplication *app;
  GtkWidget *window;
  AdwViewStack *stack;
  GtkWidget *mini; /* widget window, may be NULL/hidden */

  GtkWidget *nav[KAST_NPAGES];
  const char *nav_names[KAST_NPAGES];
  GtkWidget *nav_labels[KAST_NPAGES];
  GtkWidget *nav_imgs[KAST_NPAGES];
  GtkWidget *side_box;
  GtkWidget *side_logo;
  GtkWidget *side_brand;
  GtkWidget *side_ver;
  GtkWidget *layout;
  GtkWidget *right_col;
  GtkWidget *top_bar;
  GtkWidget *bottom_bar;
  GtkWidget *overlay;
  GtkWidget *float_panel;
  GtkWidget *page_menu_btn;
  GtkWidget *side_toggle_btn;
  int side_mode; /* 0-9 */

  /* dashboard cards: 0 cpu, 1 mem, 2 swap, 3 load, 4 uptime, 5 procs,
     6 net-up, 7 net-down, 8 battery */
  GtkLabel *d_val[9];
  GtkLabel *d_det[9];
  GtkLevelBar *d_bar[9]; /* NULL when the card has no bar */
} KastUi;

/* util.c — stat samplers + formatting */
double kast_cpu_percent (void);          /* total CPU %, keeps its own history */
char  *kast_bytes (guint64 size);        /* g_format_size, must g_free */
char  *kast_uptime_str (void);           /* e.g. "3h 24m", must g_free */
int    kast_cfg_get (const char *key, int fallback);
void   kast_cfg_set (const char *key, int value);

/* main.c */
void kast_show_page (KastUi *ui, const char *page);
void kast_nav_sync (KastUi *ui);
void kast_side_apply (KastUi *ui, int mode);
extern const char *kast_side_mode_names[15];

/* pages — each returns the widget to add to the stack */
GtkWidget *kast_dash_page (KastUi *ui);
GtkWidget *kast_procs_page (KastUi *ui);
GtkWidget *kast_graphs_page (KastUi *ui);
GtkWidget *kast_disks_page (KastUi *ui);
GtkWidget *kast_files_page (KastUi *ui);
GtkWidget *kast_rating_page (KastUi *ui);
GtkWidget *kast_controls_page (KastUi *ui);
GtkWidget *kast_sysinfo_page (KastUi *ui);
void kast_widget_toggle (KastUi *ui);

#endif /* KAST_H */
