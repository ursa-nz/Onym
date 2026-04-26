/* wn-index.h
 *
 * SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* The lemma index and the pure string helpers it is built on. Internal to libonym. The helpers are
 * separated out and exported so the unit tests can exercise them without any WordNet data. */

#pragma once

#include <glib.h>

G_BEGIN_DECLS

/* Convert a WordNet lemma to its display form by turning underscores into spaces. Returns a new
 * string, or NULL if raw is NULL. */
char *onym_term_to_display (const char *raw);

/* Convert typed input to WordNet query form: trim surrounding space, then turn spaces into the
 * underscores WordNet uses for collocations. Returns a new string, or NULL if input is NULL. */
char *onym_term_to_query (const char *input);

/* The Levenshtein edit distance between two ASCII strings. Used to rank spelling suggestions. */
guint onym_edit_distance (const char *a, const char *b);

/* The lemma index: every headword in the WordNet database, loaded once, lowercased, in display
 * form, sorted and deduplicated. It backs prefix completion and spelling suggestions. */
typedef struct _WnIndex WnIndex;

/* Load the index from the WordNet data directory. Returns NULL if no index file could be read. */
WnIndex *wn_index_new (const char *search_dir);
void     wn_index_free (WnIndex *self);

/* Return up to max_results lemmas that begin with prefix, as a null terminated array. Never NULL. */
char **wn_index_complete (WnIndex *self, const char *prefix, guint max_results);

/* Return up to max_results lemmas closest to word by edit distance, nearest first, as a null
 * terminated array. Never NULL. Used to answer "did you mean" on a missed lookup. */
char **wn_index_suggest (WnIndex *self, const char *word, guint max_results);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (WnIndex, wn_index_free)

G_END_DECLS
