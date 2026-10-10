/* Kast Manager 2.x — stat samplers + formatting (Tejas Khanna, GPL-3.0-or-later). */
#include <glibtop/cpu.h>
#include <glibtop/uptime.h>

#include "kast.h"

static guint64 last_total = 0;
static guint64 last_idle = 0;
static gboolean have_last = FALSE;

double
kast_cpu_percent (void)
{
  glibtop_cpu cpu;
  glibtop_get_cpu (&cpu);

  guint64 total = cpu.total ? cpu.total
      : cpu.user + cpu.nice + cpu.sys + cpu.idle + cpu.iowait + cpu.irq + cpu.softirq;
  guint64 idle = cpu.idle + cpu.iowait;
  double pct = 0.0;

  if (have_last && total > last_total)
    {
      guint64 dtotal = total - last_total;
      guint64 didle = idle > last_idle ? idle - last_idle : 0;
      pct = 100.0 * (1.0 - (double) didle / (double) dtotal);
      pct = CLAMP (pct, 0.0, 100.0);
    }

  last_total = total;
  last_idle = idle;
  have_last = TRUE;
  return pct;
}

char *
kast_bytes (guint64 size)
{
  return g_format_size (size);
}

char *
kast_uptime_str (void)
{
  glibtop_uptime uptime;
  glibtop_get_uptime (&uptime);
  guint64 secs = (guint64) uptime.uptime;
  guint days = secs / 86400;
  guint hours = (secs % 86400) / 3600;
  guint mins = (secs % 3600) / 60;

  if (days > 0)
    return g_strdup_printf ("%ud %uh", days, hours);
  if (hours > 0)
    return g_strdup_printf ("%uh %um", hours, mins);
  return g_strdup_printf ("%um", mins);
}
