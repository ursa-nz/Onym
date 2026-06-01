/* onym-engine.c
 *
 * SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* The engine object. It resolves the WordNet data directory once, points the WordNet library at it,
 * delegates lookups to the bridge, and lazily loads the lemma index for completion and suggestions.
 * It holds no WordNet types itself; those stay inside the bridge. */

#include "onym-engine.h"
#include "onym-lookup.h"
#include "wn-index.h"

#ifndef ONYM_WN_DEFAULT_DIR
#define ONYM_WN_DEFAULT_DIR "/usr/share/wordnet"
#endif

struct _OnymEngine
{
  GObject parent_instance;

  char *data_dir;
  gboolean data_checked;
  gboolean data_ok;

  WnIndex *index;
  gboolean index_loaded;
};

G_DEFINE_FINAL_TYPE (OnymEngine, onym_engine, G_TYPE_OBJECT)

G_DEFINE_QUARK (onym-engine-error, onym_engine_error)

static void
onym_engine_finalize (GObject *object)
{
  OnymEngine *self = ONYM_ENGINE (object);

  g_free (self->data_dir);
  g_clear_pointer (&self->index, wn_index_free);

  G_OBJECT_CLASS (onym_engine_parent_class)->finalize (object);
}

static void
onym_engine_class_init (OnymEngineClass *klass)
{
  G_OBJECT_CLASS (klass)->finalize = onym_engine_finalize;
}

static void
onym_engine_init (OnymEngine *self)
{
}

/**
 * onym_engine_new:
 *
 * Create an engine. The WordNet data directory is resolved on the first lookup, and the lemma index
 * loads lazily on the first completion or suggestion.
 *
 * Returns: (transfer full): a new #OnymEngine
 */
OnymEngine *
onym_engine_new (void)
{
  return g_object_new (ONYM_TYPE_ENGINE, NULL);
}

/* Decide where the WordNet database lives. Honour the standard WordNet environment variables first,
 * then fall back to the directory chosen at build time. */
static char *
resolve_data_dir (void)
{
  const char *search = g_getenv ("WNSEARCHDIR");
  if (search != NULL && *search != '\0')
    return g_strdup (search);

  const char *home = g_getenv ("WNHOME");
  if (home != NULL && *home != '\0')
    return g_build_filename (home, "dict", NULL);

  return g_strdup (ONYM_WN_DEFAULT_DIR);
}

/* Resolve and validate the data directory once. Point the WordNet library at it through WNSEARCHDIR
 * when that is unset, so the engine's first lookup initialises against the right data. */
static gboolean
ensure_data (OnymEngine *self, GError **error)
{
  if (!self->data_checked)
    {
      self->data_checked = TRUE;
      self->data_dir = resolve_data_dir ();
      g_setenv ("WNSEARCHDIR", self->data_dir, FALSE);

      char *probe = g_build_filename (self->data_dir, "index.noun", NULL);
      self->data_ok = g_file_test (probe, G_FILE_TEST_EXISTS);
      g_free (probe);
    }

  if (!self->data_ok)
    {
      g_set_error (error, ONYM_ENGINE_ERROR, ONYM_ENGINE_ERROR_NO_DATA,
                   "WordNet database not found in %s", self->data_dir);
      return FALSE;
    }
  return TRUE;
}

static void
ensure_index (OnymEngine *self)
{
  if (self->index_loaded)
    return;

  self->index_loaded = TRUE;
  ensure_data (self, NULL);
  self->index = wn_index_new (self->data_dir);
}

static char **
empty_strv (void)
{
  return g_new0 (char *, 1);
}

/**
 * onym_engine_lookup:
 * @self: an OnymEngine
 * @word: the word to look up, possibly inflected
 * @error: (nullable): return location for an error
 *
 * Look @word up in WordNet. A word that is simply absent returns %NULL with @error unset; a missing
 * database returns %NULL with @error set. WordNet morphology resolves an inflected query to its
 * headword.
 *
 * Returns: (transfer full) (nullable): the entry, or %NULL
 */
OnymResult *
onym_engine_lookup (OnymEngine *self, const char *word, GError **error)
{
  g_return_val_if_fail (ONYM_IS_ENGINE (self), NULL);
  g_return_val_if_fail (word != NULL, NULL);

  if (!ensure_data (self, error))
    return NULL;

  char *query = onym_term_to_query (word);
  if (query == NULL || *query == '\0')
    {
      g_free (query);
      return NULL;
    }

  OnymResult *result = onym_bridge_lookup (query);
  g_free (query);
  return result;
}

/**
 * onym_engine_complete:
 * @self: an OnymEngine
 * @prefix: the text typed so far
 * @max_results: the most matches to return, or 0 for no limit
 *
 * Return headwords that begin with @prefix, for an autocomplete list.
 *
 * Returns: (transfer full) (array zero-terminated=1): the matches
 */
char **
onym_engine_complete (OnymEngine *self, const char *prefix, guint max_results)
{
  g_return_val_if_fail (ONYM_IS_ENGINE (self), empty_strv ());

  ensure_index (self);
  if (self->index == NULL)
    return empty_strv ();
  return wn_index_complete (self->index, prefix, max_results);
}

/**
 * onym_engine_suggest:
 * @self: an OnymEngine
 * @word: the word that was not found
 * @max_results: the most suggestions to return, or 0 for no limit
 *
 * Return headwords close to @word by edit distance, for a did you mean prompt after a missed lookup.
 *
 * Returns: (transfer full) (array zero-terminated=1): the suggestions
 */
char **
onym_engine_suggest (OnymEngine *self, const char *word, guint max_results)
{
  g_return_val_if_fail (ONYM_IS_ENGINE (self), empty_strv ());

  ensure_index (self);
  if (self->index == NULL)
    return empty_strv ();
  return wn_index_suggest (self->index, word, max_results);
}
