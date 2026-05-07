/* onym-result-private.h
 *
 * SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* Constructors and mutators for the model. These are internal to libonym; only the bridge builds
 * results. The header is not installed, so consumers see a read only model. */

#pragma once

#include "onym-result.h"

G_BEGIN_DECLS

OnymWord *onym_word_new (const char *term);

OnymDefinition *onym_definition_new (const char *pos, const char *gloss, GStrv examples);

OnymAntonym *onym_antonym_new             (const char *term, gboolean direct);
void         onym_antonym_add_implication (OnymAntonym *self, const char *term);

OnymTreeNode *onym_tree_node_new       (GStrv terms); /* (transfer full) terms */
void          onym_tree_node_add_child (OnymTreeNode *self, OnymTreeNode *child); /* (transfer full) */

OnymSection *onym_section_new (OnymSectionKind kind, const char *title);
void         onym_section_add (OnymSection *self, gpointer item); /* (transfer full) */

OnymResult *onym_result_new         (const char *term);
void        onym_result_add_section (OnymResult *self, OnymSection *section); /* (transfer full) */

G_END_DECLS
