/* onym-lookup.h
 *
 * SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* The bridge from the shared engine to the public model. Internal to libonym. This is the only
 * part of the library that builds model objects from engine output. */

#pragma once

#include "onym-result.h"

#include <onym-core.h>

G_BEGIN_DECLS

/* Build the model for a query by walking the engine's entry. The engine normalises the query
 * itself. Returns NULL when the word is not in the database, which is not an error. The caller
 * owns the returned result. */
OnymResult *onym_bridge_lookup (const OnymCoreEngine *core, const char *query);

G_END_DECLS
