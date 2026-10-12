/* Kast Manager 2.x — mini widget window (Tejas Khanna, GPL-3.0-or-later).
 * Small always-on-top CPU/memory widget. Toggle from the app menu. */
#include <glibtop/mem.h>

#include <config.h>

#include "kast.h"

typedef struct {
  KastUi *ui;
  GtkLevelBar *cpu_bar;
  GtkLevelBar *mem_bar;
  GtkLabel *cpu_l;
  GtkLabel *mem_l;
} KastMini;

static gboolean
mini_tick (gpointer data)
{
  KastMini *m = data;
  double pct = kast_cpu_percent ();
  char *t = g_strdup_printf ("CPU %.0f%%", pct);
  gtk_label_set_text (m->cpu_l, t);
  g_free (t);
  gtk_level_bar_set_value (m->cpu_bar, pct);

  glibtop_mem mem;
  glibtop_get_mem (&mem);
  double mempct = mem.total ? 100.0 * (double) mem.user / (double) mem.total : 0.0;
  t = g_strdup_printf ("MEM %.0f%%", mempct);
  gtk_label_set_text (m->mem_l, t);
  g_free (t);
  gtk_level_bar_set_value (m->mem_bar, mempct);
  return G_SOURCE_CONTINUE;
}

static void
mini_row (GtkBox *box, const char *name, GtkLabel **l_out, GtkLevelBar **b_out)
{
  GtkWidget *row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  GtkWidget *l = gtk_label_new (name);
  gtk_widget_set_size_request (l, 74, -1);
  gtk_widget_add_css_class (l, "dim-label");
  gtk_box_append (GTK_BOX (row), l);
  GtkWidget *b = gtk_level_bar_new_for_interval (0.0, 100.0);
  gtk_widget_set_hexpand (b, TRUE);
  gtk_widget_set_valign (b, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (row), b);
  gtk_box_append (box, row);
  *l_out = GTK_LABEL (l);
  *b_out = GTK_LEVEL_BAR (b);
}

void
kast_widget_toggle (KastUi *ui)
{
  if (ui->mini && gtk_widget_get_visible (ui->mini))
    {
      gtk_widget_set_visible (ui->mini, FALSE);
      return;
    }
  if (!ui->mini)
    {
      KastMini *m = g_new0 (KastMini, 1);
      m->ui = ui;
      GtkWidget *w = gtk_window_new ();
      gtk_window_set_title (GTK_WINDOW (w), "Kast Widget");
      gtk_window_set_default_size (GTK_WINDOW (w), 260, 110);
      gtk_window_set_resizable (GTK_WINDOW (w), FALSE);
      gtk_window_set_transient_for (GTK_WINDOW (w), GTK_WINDOW (ui->window));
      gtk_window_set_hide_on_close (GTK_WINDOW (w), TRUE);
      gtk_window_set_icon_name (GTK_WINDOW (w), APP_ID);

      GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
      gtk_widget_set_margin_top (box, 12);
      gtk_widget_set_margin_bottom (box, 12);
      gtk_widget_set_margin_start (box, 12);
      gtk_widget_set_margin_end (box, 12);
      gtk_window_set_child (GTK_WINDOW (w), box);

      mini_row (GTK_BOX (box), "", &m->cpu_l, &m->cpu_bar);
      mini_row (GTK_BOX (box), "", &m->mem_l, &m->mem_bar);

      g_object_set_data_full (G_OBJECT (w), "kast-mini", m, g_free);
      g_timeout_add_seconds (1, mini_tick, m);
      mini_tick (m);
      ui->mini = w;
    }
  gtk_widget_set_visible (ui->mini, TRUE);
  gtk_window_present (GTK_WINDOW (ui->mini));
}
