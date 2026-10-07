/*
 * SPDX-License-Identifier: GPL-3.0-or-later-or-later
 */

#pragma once

#include <sys/types.h>

#include <glib.h>
#include <gmodule.h>

G_BEGIN_DECLS

G_MODULE_EXPORT
gboolean  gsm_selinux_is_enabled       (void);
G_MODULE_EXPORT
char     *gsm_selinux_get_context      (pid_t     pid);

G_END_DECLS
