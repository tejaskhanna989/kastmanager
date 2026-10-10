/* Kast Manager 2.x — resource graphs, hand-drawn (Tejas Khanna, GPL-3.0-or-later). */
#include <glibtop/mem.h>

#include "kast.h"

#define HIST 120

typedef struct {
  double cpu[HIST];
  double mem[HIST];
  GtkDrawingArea *cpu_area;
  GtkDrawingArea *mem_area;
  GtkLabel *cpu_now;
  GtkLabel *mem_now;
} KastGraphs;

static void
draw_history (GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer data)
{
  double *hist = data;
  const double pad = 8.0;
  double w = width - 2 * pad, h = height - 2 * pad;

  /* grid */
  cairo_set_source_rgba (cr, 0.55, 0.58, 0.62, 0.18);
  cairo_set_line_width (cr, 1.0);
  for (int i = 1; i < 4; i++)
    {
      double y = pad + h * i / 4.0;
      cairo_move_to (cr, pad, y);
      cairo_line_to (cr, pad + w, y);
      cairo_stroke (cr);
    }

  /* area fill */
  cairo_pattern_t *grad = cairo_pattern_create_linear (0, pad, 0, pad + h);
  gboolean is_cpu = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (area), "kast-is-cpu"));
  if (is_cpu)
    {
      cairo_pattern_add_color_stop_rgba (grad, 0.0, 0.0, 0.94, 1.0, 0.45);
      cairo_pattern_add_color_stop_rgba (grad, 1.0, 0.0, 0.94, 1.0, 0.03);
    }
  else
    {
      cairo_pattern_add_color_stop_rgba (grad, 0.0, 0.79, 0.36, 1.0, 0.45);
      cairo_pattern_add_color_stop_rgba (grad, 1.0, 0.79, 0.36, 1.0, 0.03);
    }

  cairo_move_to (cr, pad, pad + h);
  for (int i = 0; i < HIST; i++)
    {
      double x = pad + w * i / (HIST - 1);
      double y = pad + h * (1.0 - CLAMP (hist[i], 0.0, 100.0) / 100.0);
      cairo_line_to (cr, x, y);
    }
  cairo_line_to (cr, pad + w, pad + h);
  cairo_close_path (cr);
  cairo_set_source (cr, grad);
  cairo_fill (cr);
  cairo_pattern_destroy (grad);

  /* line */
  if (is_cpu)
    cairo_set_source_rgb (cr, 0.0, 0.94, 1.0);
  else
    cairo_set_source_rgb (cr, 0.79, 0.36, 1.0);
  cairo_set_line_width (cr, 2.0);
  cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
  for (int i = 0; i < HIST; i++)
    {
      double x = pad + w * i / (HIST - 1);
      double y = pad + h * (1.0 - CLAMP (hist[i], 0.0, 100.0) / 100.0);
      if (i == 0)
        cairo_move_to (cr, x, y);
      else
        cairo_line_to (cr, x, y);
    }
  cairo_stroke (cr);
}

static gboolean
graphs_tick (gpointer data)
{
  KastGraphs *g = data;

  memmove (g->cpu, g->cpu + 1, sizeof (double) * (HIST - 1));
  g->cpu[HIST - 1] = kast_cpu_percent ();

  glibtop_mem mem;
  glibtop_get_mem (&mem);
  double mempct = mem.total ? 100.0 * (double) mem.user / (double) mem.total : 0.0;
  memmove (g->mem, g->mem + 1, sizeof (double) * (HIST - 1));
  g->mem[HIST - 1] = mempct;

  char *text = g_strdup_printf ("%.1f%%", g->cpu[HIST - 1]);
  gtk_label_set_text (g->cpu_now, text);
  g_free (text);
  char *mu = kast_bytes (mem.user);
  char *mt = kast_bytes (mem.total);
  text = g_strdup_printf ("%s of %s (%.1f%%)", mu, mt, mempct);
  gtk_label_set_text (g->mem_now, text);
  g_free (text); g_free (mu); g_free (mt);

  gtk_widget_queue_draw (GTK_WIDGET (g->cpu_area));
  gtk_widget_queue_draw (GTK_WIDGET (g->mem_area));
  return G_SOURCE_CONTINUE;
}

static GtkWidget *
graph_card (const char *title, GtkDrawingArea **area_out, GtkLabel **now_out, gboolean is_cpu)
{
  GtkWidget *card = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_add_css_class (card, "card");

  GtkWidget *head = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  GtkWidget *t = gtk_label_new (title);
  gtk_widget_set_halign (t, GTK_ALIGN_START);
  gtk_widget_set_hexpand (t, TRUE);
  PangoAttrList *attrs = pango_attr_list_new ();
  pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
  gtk_label_set_attributes (GTK_LABEL (t), attrs);
  pango_attr_list_unref (attrs);
  gtk_box_append (GTK_BOX (head), t);

  GtkWidget *now = gtk_label_new ("—");
  gtk_widget_add_css_class (now, "dash-value");
  gtk_widget_add_css_class (now, is_cpu ? "dash-accent-cpu" : "dash-accent-mem");
  gtk_box_append (GTK_BOX (head), now);
  gtk_box_append (GTK_BOX (card), head);

  GtkWidget *a = gtk_drawing_area_new ();
  gtk_widget_set_size_request (a, -1, 150);
  gtk_widget_set_hexpand (a, TRUE);
  g_object_set_data (G_OBJECT (a), "kast-is-cpu", GINT_TO_POINTER (is_cpu));
  gtk_box_append (GTK_BOX (card), a);

  *area_out = GTK_DRAWING_AREA (a);
  *now_out = GTK_LABEL (now);
  return card;
}

GtkWidget *
kast_graphs_page (KastUi *ui)
{
  (void) ui;
  KastGraphs *g = g_new0 (KastGraphs, 1);

  GtkWidget *scroll = gtk_scrolled_window_new ();
  gtk_widget_set_hexpand (scroll, TRUE);
  gtk_widget_set_vexpand (scroll, TRUE);

  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_widget_set_margin_top (box, 20);
  gtk_widget_set_margin_bottom (box, 24);
  gtk_widget_set_margin_start (box, 24);
  gtk_widget_set_margin_end (box, 24);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scroll), box);

  GtkWidget *title = gtk_label_new ("Resource graphs");
  gtk_widget_add_css_class (title, "kast-neon-title");
  gtk_widget_set_halign (title, GTK_ALIGN_START);
  gtk_box_append (GTK_BOX (box), title);

  GtkWidget *cpu_card = graph_card ("CPU history", &g->cpu_area, &g->cpu_now, TRUE);
  GtkWidget *mem_card = graph_card ("Memory history", &g->mem_area, &g->mem_now, FALSE);
  gtk_box_append (GTK_BOX (box), cpu_card);
  gtk_box_append (GTK_BOX (box), mem_card);

  gtk_drawing_area_set_draw_func (g->cpu_area, draw_history, g->cpu, NULL);
  gtk_drawing_area_set_draw_func (g->mem_area, draw_history, g->mem, NULL);

  graphs_tick (g);
  g_timeout_add_seconds (1, graphs_tick, g);
  g_object_set_data_full (G_OBJECT (scroll), "kast-graphs", g, g_free);
  return scroll;
}
