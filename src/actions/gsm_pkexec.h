/*
 * SPDX-License-Identifier: GPL-3.0-or-later-or-later
 */

#pragma once

#include <glib.h>
#include <gmodule.h>

G_BEGIN_DECLS

gboolean gsm_pkexec_create_root_password_dialog (const char *command);
gboolean procman_has_pkexec                     (void) G_GNUC_CONST;

G_END_DECLS
