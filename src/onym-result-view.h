/* onym-result-view.h
 *
 * SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* The widget that renders an OnymResult as a vertical reading column of sections. It emits
 * "word-activated" with a term when a synonym or antonym is clicked, so the window can look it up. */

#pragma once

#include <gtk/gtk.h>

#include <onym.h>

G_BEGIN_DECLS

#define ONYM_TYPE_RESULT_VIEW (onym_result_view_get_type ())
G_DECLARE_FINAL_TYPE (OnymResultView, onym_result_view, ONYM, RESULT_VIEW, GtkWidget)

GtkWidget *onym_result_view_new (void);
void       onym_result_view_set_result (OnymResultView *self, OnymResult *result);

/* How relation tree nodes open by default: 0 collapsed, 1 linear chains, 2 everything. Matches the
 * TreeExpansion enum in the settings schema. Takes effect on the next set_result. */
void onym_result_view_set_tree_expansion (OnymResultView *self, int mode);

G_END_DECLS
