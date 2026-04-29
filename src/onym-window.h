/* onym-window.h
 *
 * SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* The main window: a search field and a single scrolling page of results. */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define ONYM_TYPE_WINDOW (onym_window_get_type ())
G_DECLARE_FINAL_TYPE (OnymWindow, onym_window, ONYM, WINDOW, AdwApplicationWindow)

OnymWindow *onym_window_new (AdwApplication *application);

/* Put @word in the search field and look it up. */
void onym_window_search (OnymWindow *self, const char *word);

G_END_DECLS
