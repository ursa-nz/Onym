/* onym-engine.h
 *
 * SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* OnymEngine is the entry point to the library. It wraps the shared onym-engine core behind a
 * small object and returns the public model. It also offers prefix completion and spelling
 * suggestions from the core's lemma index.
 *
 * The core is immutable once opened and safe for concurrent reads, but this object opens it
 * lazily without locking, so keep calls on one engine to one thread. Lookups read a local
 * database and return in well under a millisecond, so this is rarely a constraint in practice. */

#pragma once

#include <glib-object.h>

#include "onym-result.h"

G_BEGIN_DECLS

#define ONYM_TYPE_ENGINE (onym_engine_get_type ())
G_DECLARE_FINAL_TYPE (OnymEngine, onym_engine, ONYM, ENGINE, GObject)

#define ONYM_ENGINE_ERROR (onym_engine_error_quark ())
GQuark onym_engine_error_quark (void);

typedef enum
{
  ONYM_ENGINE_ERROR_NO_DATA, /* the WordNet database could not be located */
} OnymEngineError;

OnymEngine *onym_engine_new (void);

/* Look a word up. Returns the entry, or NULL. When the word is simply not in the database the
 * return is NULL with error unset; when the database is missing the return is NULL with error set.
 * The query may be inflected; WordNet morphology resolves it to a headword. */
OnymResult *onym_engine_lookup (OnymEngine *self, const char *word, GError **error);

/* Return up to max_results headwords that begin with prefix, as a null terminated array. Never
 * NULL; the caller frees it with g_strfreev. */
char **onym_engine_complete (OnymEngine *self, const char *prefix, guint max_results);

/* Return up to max_results headwords closest to word by edit distance, for a "did you mean" prompt
 * after a missed lookup. Never NULL; the caller frees it with g_strfreev. */
char **onym_engine_suggest (OnymEngine *self, const char *word, guint max_results);

/* Pick a headword at random from the lemma index, for a "surprise me" action. Returns NULL when
 * the database is missing or holds no headwords; otherwise the caller frees it with g_free. */
char *onym_engine_random_word (OnymEngine *self);

G_END_DECLS
