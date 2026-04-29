/* onym-application.h
 *
 * SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* The application object. It owns the actions and the shared style, and it presents the window. */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define ONYM_TYPE_APPLICATION (onym_application_get_type ())
G_DECLARE_FINAL_TYPE (OnymApplication, onym_application, ONYM, APPLICATION, AdwApplication)

OnymApplication *onym_application_new (void);

G_END_DECLS
