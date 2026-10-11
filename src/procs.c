/* Kast Manager 2.x — processes page (Tejas Khanna, GPL-3.0-or-later).
 * Plain ListBox table: sortable, searchable, End/Kill. */
#include <glibtop/proclist.h>
#include <glibtop/procstate.h>
#include <glibtop/proctime.h>
#include <glibtop/procmem.h>
#include <pwd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#include "kast.h"

/* ---- row object ---- */

#define KAST_TYPE_PROC (kast_proc_get_type ())
G_DECLARE_FINAL_TYPE (KastProc, kast_proc, KAST, PROC, GObject)

struct _KastProc {
  GObject parent;
  int pid;
  char *name;
  char *user;
  uid_t uid;
  double cpu;
  guint64 mem;
};

G_DEFINE_TYPE (KastProc, kast_proc, G_TYPE_OBJECT)

static void
kast_proc_finalize (GObject *obj)
{
  KastProc *p = KAST_PROC (obj);
  g_free (p->name);
  g_free (p->user);
  G_OBJECT_CLASS (kast_proc_parent_class)->finalize (obj);
}

static void
kast_proc_class_init (KastProcClass *klass)
{
  G_OBJECT_CLASS (klass)->finalize = kast_proc_finalize;
}

static void
kast_proc_init (KastProc *p)
{
  p->name = g_strdup ("");
  p->user = g_strdup ("");
}

/* ---- page state ---- */

enum { SORT_NAME, SORT_USER, SORT_CPU, SORT_MEM, SORT_PID };

typedef struct {
  KastUi *ui;
  GtkListBox *list;
  GtkSearchEntry *search;
  GtkButton *end_btn;
  GtkButton *kill_btn;
  GtkButton *sort_btns[5];
  GPtrArray *rows; /* KastProc*, ref'd */
  GHashTable *last; /* pid -> ProcTimes* */
  guint64 total_last;
  gboolean have_total;
  int sort_key;
  gboolean sort_desc;
} KastProcs;

typedef struct {
  guint64 utime;
  guint64 stime;
} ProcTimes;

static KastProcs *current; /* single page instance */

static const char *
uid_name (uid_t uid)
{
  struct passwd *pw = getpwuid (uid);
  return pw ? pw->pw_name : "?";
}

static int
cmp_rows (gconstpointer a, gconstpointer b, gpointer data)
{
  KastProcs *k = data;
  const KastProc *pa = *(KastProc * const *) a;
  const KastProc *pb = *(KastProc * const *) b;
  int r = 0;
  switch (k->sort_key)
    {
    case SORT_NAME: r = g_utf8_collate (pa->name, pb->name); break;
    case SORT_USER: r = g_utf8_collate (pa->user, pb->user); break;
    case SORT_CPU: r = (pa->cpu > pb->cpu) - (pa->cpu < pb->cpu); break;
    case SORT_MEM: r = (pa->mem > pb->mem) - (pa->mem < pb->mem); break;
    case SORT_PID:
    default: r = (pa->pid > pb->pid) - (pa->pid < pb->pid); break;
    }
  return k->sort_desc ? -r : r;
}

static gboolean
row_visible (KastProcs *k, KastProc *p)
{
  const char *q = gtk_editable_get_text (GTK_EDITABLE (k->search));
  if (!q || !*q)
    return TRUE;
  char *qn = g_utf8_casefold (q, -1);
  char *nn = g_utf8_casefold (p->name, -1);
  char *un = g_utf8_casefold (p->user, -1);
  char pidbuf[32];
  g_snprintf (pidbuf, sizeof pidbuf, "%d", p->pid);
  gboolean hit = strstr (nn, qn) || strstr (un, qn) || strstr (pidbuf, q);
  g_free (qn); g_free (nn); g_free (un);
  return hit;
}

static void
refill_rows (KastProcs *k)
{
  /* remember selection */
  int sel_pid = -1;
  GtkListBoxRow *sel = gtk_list_box_get_selected_row (k->list);
  if (sel)
    {
      KastProc *p = g_object_get_data (G_OBJECT (sel), "kast-proc");
      if (p)
        sel_pid = p->pid;
    }

  GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (k->list));
  while (child)
    {
      GtkWidget *next = gtk_widget_get_next_sibling (child);
      gtk_list_box_remove (k->list, child);
      child = next;
    }

  g_ptr_array_sort_with_data (k->rows, cmp_rows, k);

  for (guint i = 0; i < k->rows->len; i++)
    {
      KastProc *p = g_ptr_array_index (k->rows, i);
      if (!row_visible (k, p))
        continue;

      GtkWidget *row = gtk_list_box_row_new ();
      g_object_set_data_full (G_OBJECT (row), "kast-proc", g_object_ref (p), g_object_unref);

      GtkWidget *grid = gtk_grid_new ();
      gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
      gtk_widget_set_margin_top (grid, 5);
      gtk_widget_set_margin_bottom (grid, 5);
      gtk_widget_set_margin_start (grid, 10);
      gtk_widget_set_margin_end (grid, 10);
      gtk_widget_set_hexpand (grid, TRUE);
      gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), grid);

      GtkWidget *name = gtk_label_new (p->name);
      gtk_widget_set_halign (name, GTK_ALIGN_START);
      gtk_widget_set_hexpand (name, TRUE);
      gtk_label_set_ellipsize (GTK_LABEL (name), PANGO_ELLIPSIZE_END);
      gtk_grid_attach (GTK_GRID (grid), name, 0, 0, 1, 1);

      GtkWidget *user = gtk_label_new (p->user);
      gtk_widget_set_halign (user, GTK_ALIGN_START);
      gtk_widget_set_size_request (user, 90, -1);
      gtk_label_set_ellipsize (GTK_LABEL (user), PANGO_ELLIPSIZE_END);
      gtk_grid_attach (GTK_GRID (grid), user, 1, 0, 1, 1);

      char *cpu_txt = g_strdup_printf ("%.1f%%", p->cpu);
      GtkWidget *cpu = gtk_label_new (cpu_txt);
      g_free (cpu_txt);
      gtk_widget_add_css_class (cpu, "dash-value");
      gtk_widget_set_halign (cpu, GTK_ALIGN_END);
      gtk_widget_set_size_request (cpu, 70, -1);
      gtk_grid_attach (GTK_GRID (grid), cpu, 2, 0, 1, 1);

      char *mem_txt = kast_bytes (p->mem);
      GtkWidget *mem = gtk_label_new (mem_txt);
      g_free (mem_txt);
      gtk_widget_set_halign (mem, GTK_ALIGN_END);
      gtk_widget_set_size_request (mem, 90, -1);
      gtk_grid_attach (GTK_GRID (grid), mem, 3, 0, 1, 1);

      char *pid_txt = g_strdup_printf ("%d", p->pid);
      GtkWidget *pid = gtk_label_new (pid_txt);
      g_free (pid_txt);
      gtk_widget_add_css_class (pid, "dim-label");
      gtk_widget_set_halign (pid, GTK_ALIGN_END);
      gtk_widget_set_size_request (pid, 70, -1);
      gtk_grid_attach (GTK_GRID (grid), pid, 4, 0, 1, 1);

      gtk_list_box_append (k->list, row);
      if (p->pid == sel_pid)
        gtk_list_box_select_row (k->list, GTK_LIST_BOX_ROW (row));
    }
}

static void
enumerate_procs (KastProcs *k)
{
  glibtop_cpu cpu;
  glibtop_get_cpu (&cpu);
  guint64 total = cpu.total ? cpu.total
      : cpu.user + cpu.nice + cpu.sys + cpu.idle + cpu.iowait + cpu.irq + cpu.softirq;

  glibtop_proclist proclist;
  pid_t *pids = glibtop_get_proclist (&proclist, 0, 0);

  g_ptr_array_set_size (k->rows, 0);

  for (gsize i = 0; i < proclist.number; i++)
    {
      glibtop_proc_state state;
      glibtop_get_proc_state (&state, pids[i]);
      if (!state.cmd[0])
        continue;

      glibtop_proc_time ptime;
      glibtop_get_proc_time (&ptime, pids[i]);
      guint64 pt = ptime.utime + ptime.stime;

      double pct = 0.0;
      ProcTimes *prev = g_hash_table_lookup (k->last, GINT_TO_POINTER (pids[i]));
      if (prev && k->have_total && total > k->total_last)
        {
          guint64 dtotal = total - k->total_last;
          guint64 dproc = pt > (prev->utime + prev->stime) ? pt - (prev->utime + prev->stime) : 0;
          pct = 100.0 * (double) dproc / (double) dtotal;
          pct = CLAMP (pct, 0.0, 100.0 * g_get_num_processors ());
        }
      ProcTimes *now = g_new (ProcTimes, 1);
      now->utime = ptime.utime;
      now->stime = ptime.stime;
      g_hash_table_replace (k->last, GINT_TO_POINTER (pids[i]), now);

      glibtop_proc_mem pmem;
      glibtop_get_proc_mem (&pmem, pids[i]);

      KastProc *row = g_object_new (KAST_TYPE_PROC, NULL);
      row->pid = pids[i];
      g_free (row->name);
      row->name = g_strdup (state.cmd);
      g_free (row->user);
      row->user = g_strdup (uid_name (state.uid));
      row->uid = state.uid;
      row->cpu = pct;
      row->mem = pmem.rss;
      g_ptr_array_add (k->rows, row);
    }
  g_free (pids);

  k->total_last = total;
  k->have_total = TRUE;
}

static void
refresh_procs (KastProcs *k)
{
  enumerate_procs (k);
  refill_rows (k);
}

static gboolean
procs_tick (gpointer data)
{
  KastProcs *k = data;
  const char *page = adw_view_stack_get_visible_child_name (k->ui->stack);
  if (g_strcmp0 (page, KAST_PAGE_PROCESSES) == 0)
    refresh_procs (k);
  return G_SOURCE_CONTINUE;
}

static KastProc *
selected_proc (KastProcs *k)
{
  GtkListBoxRow *row = gtk_list_box_get_selected_row (k->list);
  if (!row)
    return NULL;
  return g_object_get_data (G_OBJECT (row), "kast-proc");
}

typedef struct {
  int pid;
  int sig;
  uid_t uid;
} KastKillReq;

static void
on_kill_response (GtkDialog *dlg, int resp, gpointer data)
{
  KastKillReq *req = data;
  if (resp == GTK_RESPONSE_OK)
    {
      if (req->uid == (uid_t) geteuid ())
        {
          if (kill (req->pid, req->sig) != 0)
            g_warning ("kill(%d) failed: %s", req->pid, g_strerror (errno));
        }
      else
        {
          const char *sigarg = req->sig == SIGKILL ? "-KILL" : "-TERM";
          char *pidarg = g_strdup_printf ("%d", req->pid);
          char *argv[] = { "pkexec", "--disable-internal-agent", "kill",
                           (char *) sigarg, pidarg, NULL };
          GError *err = NULL;
          g_spawn_async (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &err);
          if (err)
            {
              g_warning ("pkexec failed: %s", err->message);
              g_error_free (err);
            }
          g_free (pidarg);
        }
    }
  g_free (req);
}

static void
do_signal (KastProcs *k, int sig)
{
  KastProc *p = selected_proc (k);
  if (!p)
    return;

  const char *signame = sig == SIGKILL ? "kill" : "end";
  GtkWidget *dlg = gtk_message_dialog_new (GTK_WINDOW (k->ui->window),
      GTK_DIALOG_MODAL,
      GTK_MESSAGE_QUESTION,
      GTK_BUTTONS_NONE,
      "This will %s “%s” (PID %d).",
      signame, p->name, p->pid);
  gtk_window_set_title (GTK_WINDOW (dlg),
      sig == SIGKILL ? "Kill process?" : "End process?");
  gtk_dialog_add_button (GTK_DIALOG (dlg), "Cancel", GTK_RESPONSE_CANCEL);
  GtkWidget *ok_btn = gtk_dialog_add_button (GTK_DIALOG (dlg),
      sig == SIGKILL ? "Kill" : "End", GTK_RESPONSE_OK);
  gtk_widget_add_css_class (ok_btn, "destructive-action");
  gtk_dialog_set_default_response (GTK_DIALOG (dlg), GTK_RESPONSE_CANCEL);
  KastKillReq *req = g_new (KastKillReq, 1);
  req->pid = p->pid;
  req->sig = sig;
  req->uid = p->uid;
  g_signal_connect (dlg, "response", G_CALLBACK (on_kill_response), req);
  g_signal_connect_swapped (dlg, "response", G_CALLBACK (gtk_window_destroy), dlg);
  gtk_window_present (GTK_WINDOW (dlg));
}

static void
update_buttons (KastProcs *k)
{
  gboolean has = selected_proc (k) != NULL;
  gtk_widget_set_sensitive (GTK_WIDGET (k->end_btn), has);
  gtk_widget_set_sensitive (GTK_WIDGET (k->kill_btn), has);
}

static void
on_row_selected (GtkListBox *, GtkListBoxRow *, gpointer data)
{
  update_buttons (data);
}

static void
on_end (GtkButton *, gpointer data) { do_signal (data, SIGTERM); }
static void
on_kill (GtkButton *, gpointer data) { do_signal (data, SIGKILL); }
static void
on_refresh_btn (GtkButton *, gpointer data) { refresh_procs (data); }
static void
on_search (GtkSearchEntry *, gpointer data) { refill_rows (data); }

static void
on_sort_btn (GtkButton *btn, gpointer data)
{
  KastProcs *k = current;
  (void) data;
  int key = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (btn), "kast-sort"));
  if (k->sort_key == key)
    k->sort_desc = !k->sort_desc;
  else
    {
      k->sort_key = key;
      k->sort_desc = (key == SORT_CPU || key == SORT_MEM);
    }
  static const char *names[5] = { "Process", "User", "CPU %", "Memory", "PID" };
  for (int i = 0; i < 5; i++)
    {
      char *t = (i == k->sort_key)
          ? g_strdup_printf ("%s %s", names[i], k->sort_desc ? "▼" : "▲")
          : g_strdup (names[i]);
      gtk_button_set_label (k->sort_btns[i], t);
      g_free (t);
    }
  refill_rows (k);
}

GtkWidget *
kast_procs_page (KastUi *ui)
{
  KastProcs *k = g_new0 (KastProcs, 1);
  current = k;
  k->ui = ui;
  k->last = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
  k->rows = g_ptr_array_new_with_free_func (g_object_unref);
  k->sort_key = SORT_CPU;
  k->sort_desc = TRUE;

  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_set_margin_top (box, 16);
  gtk_widget_set_margin_bottom (box, 12);
  gtk_widget_set_margin_start (box, 16);
  gtk_widget_set_margin_end (box, 16);

  GtkWidget *bar = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  k->search = GTK_SEARCH_ENTRY (gtk_search_entry_new ());
  gtk_search_entry_set_placeholder_text (k->search, "Search processes");
  gtk_widget_set_hexpand (GTK_WIDGET (k->search), TRUE);
  g_signal_connect (k->search, "search-changed", G_CALLBACK (on_search), k);
  gtk_box_append (GTK_BOX (bar), GTK_WIDGET (k->search));

  GtkWidget *refresh = gtk_button_new_with_label ("Refresh");
  g_signal_connect (refresh, "clicked", G_CALLBACK (on_refresh_btn), k);
  gtk_box_append (GTK_BOX (bar), refresh);

  k->end_btn = GTK_BUTTON (gtk_button_new_with_label ("End"));
  gtk_widget_add_css_class (GTK_WIDGET (k->end_btn), "destructive-action");
  g_signal_connect (k->end_btn, "clicked", G_CALLBACK (on_end), k);
  gtk_box_append (GTK_BOX (bar), GTK_WIDGET (k->end_btn));

  k->kill_btn = GTK_BUTTON (gtk_button_new_with_label ("Kill"));
  gtk_widget_add_css_class (GTK_WIDGET (k->kill_btn), "destructive-action");
  g_signal_connect (k->kill_btn, "clicked", G_CALLBACK (on_kill), k);
  gtk_box_append (GTK_BOX (bar), GTK_WIDGET (k->kill_btn));
  gtk_box_append (GTK_BOX (box), bar);

  /* sortable header */
  GtkWidget *header = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (header), 12);
  gtk_widget_set_margin_start (header, 10);
  gtk_widget_set_margin_end (header, 10);
  const char *names[5] = { "Process", "User", "CPU %", "Memory", "PID" };
  for (int i = 0; i < 5; i++)
    {
      GtkWidget *b = gtk_button_new_with_label (i == SORT_CPU ? "CPU % ▼" : names[i]);
      gtk_button_set_has_frame (GTK_BUTTON (b), FALSE);
      gtk_widget_add_css_class (b, "dim-label");
      g_object_set_data (G_OBJECT (b), "kast-sort", GINT_TO_POINTER (i));
      g_signal_connect (b, "clicked", G_CALLBACK (on_sort_btn), k);
      k->sort_btns[i] = GTK_BUTTON (b);
      if (i == 0)
        {
          gtk_widget_set_halign (b, GTK_ALIGN_START);
          gtk_widget_set_hexpand (b, TRUE);
        }
      else
        gtk_widget_set_halign (b, GTK_ALIGN_END);
      gtk_grid_attach (GTK_GRID (header), b, i, 0, 1, 1);
    }
  gtk_box_append (GTK_BOX (box), header);

  GtkWidget *scroll = gtk_scrolled_window_new ();
  gtk_widget_set_hexpand (scroll, TRUE);
  gtk_widget_set_vexpand (scroll, TRUE);
  gtk_widget_add_css_class (scroll, "card");
  gtk_box_append (GTK_BOX (box), scroll);

  k->list = GTK_LIST_BOX (gtk_list_box_new ());
  gtk_list_box_set_selection_mode (k->list, GTK_SELECTION_SINGLE);
  g_signal_connect (k->list, "row-selected", G_CALLBACK (on_row_selected), k);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scroll), GTK_WIDGET (k->list));

  update_buttons (k);
  refresh_procs (k);
  g_timeout_add_seconds (3, procs_tick, k);
  return box;
}
