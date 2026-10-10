/* Kast Manager 2.x — dashboard home page (Tejas Khanna, GPL-3.0-or-later). */
#include <glibtop/mem.h>
#include <glibtop/swap.h>
#include <glibtop/loadavg.h>
#include <glibtop/proclist.h>
#include <glibtop/procstate.h>

#include "kast.h"

static const char *captions[6] = { "CPU", "Memory", "Swap", "Load average", "Uptime", "Processes" };
static const char *accents[6] = {
  "dash-accent-cpu", "dash-accent-mem", "dash-accent-swap",
  "dash-accent-load", "dash-accent-uptime", "dash-accent-procs",
};
static const char *targets[6] = {
  KAST_PAGE_GRAPHS, KAST_PAGE_GRAPHS, KAST_PAGE_GRAPHS,
  KAST_PAGE_GRAPHS, KAST_PAGE_DASHBOARD, KAST_PAGE_PROCESSES,
};

static void
on_card_clicked (GtkButton *btn, gpointer data)
{
  KastUi *ui = data;
  const char *page = g_object_get_data (G_OBJECT (btn), "kast-page");
  kast_show_page (ui, page);
}

static GtkWidget *
make_card (KastUi *ui, int idx, gboolean with_bar)
{
  GtkWidget *btn = gtk_button_new ();
  gtk_widget_add_css_class (btn, "card");
  /* per-card accent border via widget name */
  const char *names[6] = {
    "dash_cpu_card", "dash_mem_card", "dash_swap_card",
    "dash_load_card", "dash_uptime_card", "dash_procs_card",
  };
  gtk_widget_set_name (btn, names[idx]);
  gtk_button_set_has_frame (GTK_BUTTON (btn), FALSE);
  g_object_set_data (G_OBJECT (btn), "kast-page", (gpointer) targets[idx]);
  g_signal_connect (btn, "clicked", G_CALLBACK (on_card_clicked), ui);

  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
  gtk_widget_set_margin_top (box, 14);
  gtk_widget_set_margin_bottom (box, 14);
  gtk_widget_set_margin_start (box, 14);
  gtk_widget_set_margin_end (box, 14);
  gtk_button_set_child (GTK_BUTTON (btn), box);

  GtkWidget *cap = gtk_label_new (captions[idx]);
  gtk_widget_add_css_class (cap, "dim-label");
  gtk_widget_set_halign (cap, GTK_ALIGN_START);
  gtk_box_append (GTK_BOX (box), cap);

  GtkWidget *val = gtk_label_new ("—");
  gtk_widget_add_css_class (val, "dash-value");
  gtk_widget_add_css_class (val, accents[idx]);
  gtk_widget_set_halign (val, GTK_ALIGN_START);
  PangoAttrList *attrs = pango_attr_list_new ();
  pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
  pango_attr_list_insert (attrs, pango_attr_scale_new (2.0));
  gtk_label_set_attributes (GTK_LABEL (val), attrs);
  pango_attr_list_unref (attrs);
  gtk_box_append (GTK_BOX (box), val);

  GtkLevelBar *bar = NULL;
  if (with_bar)
    {
      GtkWidget *b = gtk_level_bar_new_for_interval (0.0, 100.0);
      gtk_widget_set_margin_top (b, 10);
      gtk_box_append (GTK_BOX (box), b);
      bar = GTK_LEVEL_BAR (b);
    }

  GtkWidget *det = gtk_label_new ("—");
  gtk_widget_add_css_class (det, "dim-label");
  gtk_widget_set_halign (det, GTK_ALIGN_START);
  gtk_box_append (GTK_BOX (box), det);

  ui->d_val[idx] = GTK_LABEL (val);
  ui->d_det[idx] = GTK_LABEL (det);
  if (idx < 4)
    ui->d_bar[idx] = bar;
  return btn;
}

static gboolean
dash_tick (gpointer data)
{
  KastUi *ui = data;
  char *text, *detail;

  double pct = kast_cpu_percent ();
  text = g_strdup_printf ("%.1f%%", pct);
  detail = g_strdup_printf ("%u logical CPUs", g_get_num_processors ());
  gtk_label_set_text (ui->d_val[0], text);
  gtk_label_set_text (ui->d_det[0], detail);
  gtk_level_bar_set_value (ui->d_bar[0], pct);
  g_free (text);
  g_free (detail);

  glibtop_mem mem;
  glibtop_get_mem (&mem);
  double mempct = mem.total ? 100.0 * (double) mem.user / (double) mem.total : 0.0;
  char *mu = kast_bytes (mem.user);
  char *mt = kast_bytes (mem.total);
  text = g_strdup_printf ("%.1f%%", mempct);
  detail = g_strdup_printf ("%s of %s", mu, mt);
  gtk_label_set_text (ui->d_val[1], text);
  gtk_label_set_text (ui->d_det[1], detail);
  gtk_level_bar_set_value (ui->d_bar[1], mempct);
  g_free (text); g_free (detail); g_free (mu); g_free (mt);

  glibtop_swap swap;
  glibtop_get_swap (&swap);
  double swappct = swap.total ? 100.0 * (double) swap.used / (double) swap.total : 0.0;
  char *su = kast_bytes (swap.used);
  char *st = kast_bytes (swap.total);
  text = g_strdup_printf ("%.1f%%", swappct);
  detail = g_strdup_printf ("%s of %s", su, st);
  gtk_label_set_text (ui->d_val[2], text);
  gtk_label_set_text (ui->d_det[2], detail);
  gtk_level_bar_set_value (ui->d_bar[2], swappct);
  g_free (text); g_free (detail); g_free (su); g_free (st);

  glibtop_loadavg load;
  glibtop_get_loadavg (&load);
  guint ncpu = g_get_num_processors ();
  text = g_strdup_printf ("%.2f", load.loadavg[0]);
  detail = g_strdup_printf ("%.2f %.2f %.2f", load.loadavg[0], load.loadavg[1], load.loadavg[2]);
  gtk_label_set_text (ui->d_val[3], text);
  gtk_label_set_text (ui->d_det[3], detail);
  gtk_level_bar_set_value (ui->d_bar[3], ncpu ? MIN (100.0, 100.0 * load.loadavg[0] / (double) ncpu) : 0.0);
  g_free (text); g_free (detail);

  text = kast_uptime_str ();
  detail = g_strdup ("since boot");
  gtk_label_set_text (ui->d_val[4], text);
  gtk_label_set_text (ui->d_det[4], detail);
  g_free (text); g_free (detail);

  glibtop_proclist proclist;
  pid_t *pids = glibtop_get_proclist (&proclist, 0, 0);
  guint nrunning = 0;
  for (gsize i = 0; i < proclist.number; i++)
    {
      glibtop_proc_state st;
      glibtop_get_proc_state (&st, pids[i]);
      if (st.state == GLIBTOP_PROCESS_RUNNING)
        nrunning++;
    }
  g_free (pids);
  text = g_strdup_printf ("%u", (guint) proclist.number);
  detail = g_strdup_printf ("%u running", nrunning);
  gtk_label_set_text (ui->d_val[5], text);
  gtk_label_set_text (ui->d_det[5], detail);
  g_free (text); g_free (detail);

  return G_SOURCE_CONTINUE;
}

GtkWidget *
kast_dash_page (KastUi *ui)
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

  GtkWidget *title = gtk_label_new ("Kast Manager");
  gtk_widget_add_css_class (title, "kast-neon-title");
  gtk_widget_set_halign (title, GTK_ALIGN_START);
  gtk_box_append (GTK_BOX (box), title);

  GtkWidget *sub = gtk_label_new ("System overview — select a card to dive in");
  gtk_widget_add_css_class (sub, "dim-label");
  gtk_widget_set_halign (sub, GTK_ALIGN_START);
  gtk_box_append (GTK_BOX (box), sub);

  GtkWidget *flow = gtk_flow_box_new ();
  gtk_flow_box_set_homogeneous (GTK_FLOW_BOX (flow), TRUE);
  gtk_flow_box_set_min_children_per_line (GTK_FLOW_BOX (flow), 2);
  gtk_flow_box_set_max_children_per_line (GTK_FLOW_BOX (flow), 3);
  gtk_flow_box_set_row_spacing (GTK_FLOW_BOX (flow), 12);
  gtk_flow_box_set_column_spacing (GTK_FLOW_BOX (flow), 12);
  gtk_flow_box_set_selection_mode (GTK_FLOW_BOX (flow), GTK_SELECTION_NONE);
  gtk_widget_set_hexpand (flow, TRUE);
  gtk_widget_set_name (flow, "dash_grid");
  gtk_box_append (GTK_BOX (box), flow);

  for (int i = 0; i < 6; i++)
    gtk_flow_box_append (GTK_FLOW_BOX (flow), make_card (ui, i, i < 4));

  dash_tick (ui);
  g_timeout_add_seconds (2, dash_tick, ui);
  return scroll;
}
