/* onym-lookup.h
 *
 * SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* The bridge from the WordNet engine to the public model. Internal to libonym. This is the only
 * part of the library that knows the engine exists. */

#pragma once

#include "onym-result.h"

G_BEGIN_DECLS

/* Build the model for a query by walking the WordNet engine response. The query must already be in
 * WordNet form; see onym_term_to_query. Returns NULL when the word is not in the database, which is
 * not an error. The caller owns the returned result. */
OnymResult *onym_bridge_lookup (const char *query);

G_END_DECLS
