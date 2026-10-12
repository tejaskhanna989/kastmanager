/* Kast Manager 2.x — PC rating (Tejas Khanna, GPL-3.0-or-later).
 * Quick CPU / memory / disk benchmarks with a 0-10 overall score. */
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include <glibtop/mem.h>

#include "kast.h"

typedef struct {
  KastUi *ui;
  GtkLabel *cpu_v, *mem_v, *disk_v, *gpu_v, *overall_v, *note;
  GtkLevelBar *cpu_b, *mem_b, *disk_b, *overall_b;
  GtkButton *run_btn;
  GtkSpinner *spin;
} KastRating;

typedef struct {
  double cpu, mem, disk, overall;
  char *gpu;
} KastScores;

static double
bench_cpu_ms (void)
{
  /* single-thread prime sieve, fixed workload */
  const int N = 200000;
  gint64 t0 = g_get_monotonic_time ();
  gboolean *sieve = g_new0 (gboolean, N + 1);
  for (int i = 2; i * i <= N; i++)
    {
      if (!sieve[i])
        {
          for (int j = i * i; j <= N; j += i)
            sieve[j] = TRUE;
        }
    }
  volatile int count = 0;
  for (int i = 2; i <= N; i++)
    if (!sieve[i])
      count++;
  g_free (sieve);
  (void) count;
  return (g_get_monotonic_time () - t0) / 1000.0;
}

static double
bench_disk_mbps (void)
{
  /* 32 MB sequential write + read in the user cache dir */
  const gsize MB = 32;
  const gsize total = MB * 1024 * 1024;
  char *path = g_build_filename (g_get_user_cache_dir (), "kast-disk-bench.tmp", NULL);
  char *buf = g_malloc (1024 * 1024);
  memset (buf, 0x5A, 1024 * 1024);

  gint64 t0 = g_get_monotonic_time ();
  int fd = open (path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (fd < 0)
    {
      g_free (buf);
      g_free (path);
      return -1.0;
    }
  for (gsize i = 0; i < MB; i++)
    {
      if (write (fd, buf, 1024 * 1024) != 1024 * 1024)
        break;
    }
  fsync (fd);
  close (fd);
  double wsecs = (g_get_monotonic_time () - t0) / 1000000.0;

  t0 = g_get_monotonic_time ();
  fd = open (path, O_RDONLY);
  gsize got = 0;
  if (fd >= 0)
    {
      ssize_t r;
      while ((r = read (fd, buf, 1024 * 1024)) > 0)
        got += r;
      close (fd);
    }
  double rsecs = (g_get_monotonic_time () - t0) / 1000000.0;
  unlink (path);
  g_free (buf);
  g_free (path);

  if (wsecs <= 0 || rsecs <= 0 || got == 0)
    return -1.0;
  double wmbps = MB / wsecs;
  double rmbps = ((double) got / 1048576.0) / rsecs;
  return (wmbps + rmbps) / 2.0;
}

static char *
gpu_name (void)
{
  char *out = NULL, *err = NULL;
  int status = 0;
  char *argv[] = { "lspci", "-mm", NULL };
  if (g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                    &out, &err, &status, NULL) && status == 0 && out)
    {
      char **lines = g_strsplit (out, "\n", -1);
      for (int i = 0; lines[i]; i++)
        {
          if (strstr (lines[i], "VGA") || strstr (lines[i], "3D controller")
              || strstr (lines[i], "Display controller"))
            {
              /* quoted fields: "slot" "class" "vendor" "device" ... */
              char **q = g_strsplit (lines[i], "\"", -1);
              int nq = g_strv_length (q);
              char *name = nq >= 8 ? g_strdup_printf ("%s %s", q[5], q[7])
                                   : g_strdup (lines[i]);
              g_strfreev (q);
              g_strfreev (lines);
              g_free (out);
              g_free (err);
              return name;
            }
        }
      g_strfreev (lines);
    }
  g_free (out);
  g_free (err);
  return g_strdup ("Unknown GPU");
}

typedef struct {
  KastRating *r;
  GThread *thread;
  GMutex lock;
  gboolean done;
  KastScores *scores;
} KastBenchWait;

static gpointer
bench_thread (gpointer data)
{
  KastBenchWait *w = data;
  KastScores *s = g_new0 (KastScores, 1);

  double ms = bench_cpu_ms ();
  s->cpu = ms > 0 ? CLAMP (3000.0 / ms, 1.0, 99.0) : 0.0;

  glibtop_mem mem;
  glibtop_get_mem (&mem);
  double gb = (double) mem.total / 1073741824.0;
  s->mem = gb >= 32 ? 95 : gb >= 16 ? 85 : gb >= 8 ? 70 : gb >= 4 ? 50 : 30;

  double mbps = bench_disk_mbps ();
  s->disk = mbps > 0 ? CLAMP (100.0 * log10 (1.0 + mbps) / 3.3, 5.0, 99.0) : 0.0;

  s->gpu = gpu_name ();
  s->overall = (s->cpu + s->mem + s->disk) / 30.0; /* 0-10 scale */

  g_mutex_lock (&w->lock);
  w->scores = s;
  w->done = TRUE;
  g_mutex_unlock (&w->lock);
  return NULL;
}

static gboolean
poll_bench (gpointer data)
{
  KastBenchWait *w = data;
  if (!g_mutex_trylock (&w->lock))
    return G_SOURCE_CONTINUE;
  gboolean done = w->done;
  g_mutex_unlock (&w->lock);
  if (!done)
    return G_SOURCE_CONTINUE;

  g_thread_join (w->thread); /* already finished: returns immediately */
  KastScores *s = w->scores;
  KastRating *r = w->r;

  char *t = g_strdup_printf ("%.0f / 100", s->cpu);
  gtk_label_set_text (r->cpu_v, t); g_free (t);
  gtk_level_bar_set_value (r->cpu_b, s->cpu);

  t = g_strdup_printf ("%.0f / 100", s->mem);
  gtk_label_set_text (r->mem_v, t); g_free (t);
  gtk_level_bar_set_value (r->mem_b, s->mem);

  t = g_strdup_printf ("%.0f / 100", s->disk);
  gtk_label_set_text (r->disk_v, t); g_free (t);
  gtk_level_bar_set_value (r->disk_b, s->disk);

  gtk_label_set_text (r->gpu_v, s->gpu);

  t = g_strdup_printf ("%.1f / 10", s->overall);
  gtk_label_set_text (r->overall_v, t); g_free (t);
  gtk_level_bar_set_value (r->overall_b, s->overall * 10.0);

  gtk_label_set_text (r->note, "Scores compare this machine against typical desktops.");
  gtk_widget_set_sensitive (GTK_WIDGET (r->run_btn), TRUE);
  gtk_spinner_stop (r->spin);

  g_free (s->gpu);
  g_free (s);
  g_mutex_clear (&w->lock);
  g_free (w);
  return G_SOURCE_REMOVE;
}

static void
on_run (GtkButton *, gpointer data)
{
  KastRating *r = data;
  gtk_widget_set_sensitive (GTK_WIDGET (r->run_btn), FALSE);
  gtk_spinner_start (r->spin);
  gtk_label_set_text (r->note, "Benchmarking CPU, memory and disk…");
  KastBenchWait *w = g_new0 (KastBenchWait, 1);
  w->r = r;
  g_mutex_init (&w->lock);
  w->thread = g_thread_new ("kast-bench", bench_thread, w);
  g_timeout_add (500, poll_bench, w);
}

static GtkWidget *
score_row (const char *title, GtkLabel **v_out, GtkLevelBar **b_out)
{
  GtkWidget *card = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_add_css_class (card, "card");

  GtkWidget *top = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  GtkWidget *t = gtk_label_new (title);
  gtk_widget_set_halign (t, GTK_ALIGN_START);
  gtk_widget_set_hexpand (t, TRUE);
  PangoAttrList *attrs = pango_attr_list_new ();
  pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
  gtk_label_set_attributes (GTK_LABEL (t), attrs);
  pango_attr_list_unref (attrs);
  gtk_box_append (GTK_BOX (top), t);

  GtkWidget *v = gtk_label_new ("…");
  gtk_widget_add_css_class (v, "dash-value");
  gtk_box_append (GTK_BOX (top), v);
  gtk_box_append (GTK_BOX (card), top);

  GtkWidget *b = gtk_level_bar_new_for_interval (0.0, 100.0);
  gtk_box_append (GTK_BOX (card), b);

  *v_out = GTK_LABEL (v);
  *b_out = GTK_LEVEL_BAR (b);
  return card;
}

GtkWidget *
kast_rating_page (KastUi *ui)
{
  (void) ui;
  KastRating *r = g_new0 (KastRating, 1);

  GtkWidget *scroll = gtk_scrolled_window_new ();
  gtk_widget_set_hexpand (scroll, TRUE);
  gtk_widget_set_vexpand (scroll, TRUE);

  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_widget_set_margin_top (box, 20);
  gtk_widget_set_margin_bottom (box, 24);
  gtk_widget_set_margin_start (box, 24);
  gtk_widget_set_margin_end (box, 24);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scroll), box);

  GtkWidget *head = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  GtkWidget *title = gtk_label_new ("PC Rating");
  gtk_widget_add_css_class (title, "kast-neon-title");
  gtk_widget_set_halign (title, GTK_ALIGN_START);
  gtk_widget_set_hexpand (title, TRUE);
  gtk_box_append (GTK_BOX (head), title);

  r->run_btn = GTK_BUTTON (gtk_button_new_with_label ("Run benchmark"));
  gtk_widget_add_css_class (GTK_WIDGET (r->run_btn), "suggested-action");
  g_signal_connect (r->run_btn, "clicked", G_CALLBACK (on_run), r);
  gtk_box_append (GTK_BOX (head), GTK_WIDGET (r->run_btn));

  r->spin = GTK_SPINNER (gtk_spinner_new ());
  gtk_box_append (GTK_BOX (head), GTK_WIDGET (r->spin));
  gtk_box_append (GTK_BOX (box), head);

  GtkWidget *ocard = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_add_css_class (ocard, "card");
  GtkWidget *ol = gtk_label_new ("Overall score");
  gtk_widget_add_css_class (ol, "dim-label");
  gtk_widget_set_halign (ol, GTK_ALIGN_START);
  gtk_box_append (GTK_BOX (ocard), ol);
  r->overall_v = GTK_LABEL (gtk_label_new ("…"));
  gtk_widget_add_css_class (GTK_WIDGET (r->overall_v), "dash-value");
  PangoAttrList *attrs = pango_attr_list_new ();
  pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
  pango_attr_list_insert (attrs, pango_attr_scale_new (2.4));
  gtk_label_set_attributes (r->overall_v, attrs);
  pango_attr_list_unref (attrs);
  gtk_widget_set_halign (GTK_WIDGET (r->overall_v), GTK_ALIGN_START);
  gtk_box_append (GTK_BOX (ocard), GTK_WIDGET (r->overall_v));
  r->overall_b = GTK_LEVEL_BAR (gtk_level_bar_new_for_interval (0.0, 100.0));
  gtk_box_append (GTK_BOX (ocard), GTK_WIDGET (r->overall_b));
  gtk_box_append (GTK_BOX (box), ocard);

  gtk_box_append (GTK_BOX (box), score_row ("Processor", &r->cpu_v, &r->cpu_b));
  gtk_box_append (GTK_BOX (box), score_row ("Memory", &r->mem_v, &r->mem_b));
  gtk_box_append (GTK_BOX (box), score_row ("Disk", &r->disk_v, &r->disk_b));

  GtkWidget *gcard = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_add_css_class (gcard, "card");
  GtkWidget *gt = gtk_label_new ("Graphics");
  gtk_widget_add_css_class (gt, "dim-label");
  gtk_widget_set_halign (gt, GTK_ALIGN_START);
  gtk_widget_set_hexpand (gt, TRUE);
  PangoAttrList *ga = pango_attr_list_new ();
  pango_attr_list_insert (ga, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
  gtk_label_set_attributes (GTK_LABEL (gt), ga);
  pango_attr_list_unref (ga);
  gtk_box_append (GTK_BOX (gcard), gt);
  r->gpu_v = GTK_LABEL (gtk_label_new ("Press Run benchmark"));
  gtk_widget_set_halign (GTK_WIDGET (r->gpu_v), GTK_ALIGN_START);
  gtk_label_set_wrap (r->gpu_v, TRUE);
  gtk_box_append (GTK_BOX (gcard), GTK_WIDGET (r->gpu_v));
  gtk_box_append (GTK_BOX (box), gcard);

  r->note = GTK_LABEL (gtk_label_new ("Press Run benchmark to score this PC."));
  gtk_widget_add_css_class (GTK_WIDGET (r->note), "dim-label");
  gtk_widget_set_halign (GTK_WIDGET (r->note), GTK_ALIGN_START);
  gtk_box_append (GTK_BOX (box), GTK_WIDGET (r->note));

  g_object_set_data_full (G_OBJECT (scroll), "kast-rating", r, g_free);
  return scroll;
}
