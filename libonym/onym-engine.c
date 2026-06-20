/* onym-engine.c
 *
 * SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* The engine object. It resolves the WordNet data directory once, opens the shared onym-engine
 * core over it read-only, and delegates lookups, completion, and suggestions to it. Where the
 * data lives is application policy, so the environment variables are honoured here; the core
 * itself only ever sees an explicit directory. */

#include "onym-engine.h"
#include "onym-lookup.h"

#include <onym-core.h>

/* The build sets this to the WordNet data directory: the prepared onym-data submodule for a local
 * build, or the bundled path for a package. There is no system fallback; the engine reads the data
 * the project ships, never whatever WordNet a host happens to have. */
#ifndef ONYM_WN_DEFAULT_DIR
#error "ONYM_WN_DEFAULT_DIR must be set by the build (meson resolves it from the onym-data submodule)"
#endif

struct _OnymEngine
{
  GObject parent_instance;

  char *data_dir;
  gboolean core_checked;
  OnymCoreEngine *core;
};

G_DEFINE_FINAL_TYPE (OnymEngine, onym_engine, G_TYPE_OBJECT)

G_DEFINE_QUARK (onym-engine-error, onym_engine_error)

static void
onym_engine_finalize (GObject *object)
{
  OnymEngine *self = ONYM_ENGINE (object);

  g_free (self->data_dir);
  g_clear_pointer (&self->core, onym_core_free);

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
 * Create an engine. The WordNet data directory is resolved, and the database opened, on the
 * first call that needs it.
 *
 * Returns: (transfer full): a new #OnymEngine
 */
OnymEngine *
onym_engine_new (void)
{
  return g_object_new (ONYM_TYPE_ENGINE, NULL);
}

/* Decide where the WordNet database lives. Honour the standard WordNet environment variables
 * first, then fall back to the directory chosen at build time. */
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

/* Resolve the data directory and open the core over it, once. The core reads the database in
 * place, read-only, and is immutable after open. */
static gboolean
ensure_core (OnymEngine *self, GError **error)
{
  if (!self->core_checked)
    {
      self->core_checked = TRUE;
      self->data_dir = resolve_data_dir ();

      char *message = NULL;
      self->core = onym_core_open (self->data_dir, &message);
      if (self->core == NULL)
        g_debug ("onym-engine: %s", message != NULL ? message : "open failed");
      onym_core_string_free (message);
    }

  if (self->core == NULL)
    {
      g_set_error (error, ONYM_ENGINE_ERROR, ONYM_ENGINE_ERROR_NO_DATA,
                   "WordNet database not found in %s", self->data_dir);
      return FALSE;
    }
  return TRUE;
}

static char **
empty_strv (void)
{
  return g_new0 (char *, 1);
}

/* Copy an engine string array into a GStrv the caller frees with g_strfreev. */
static char **
copy_and_free_strv (char **core_strv)
{
  char **out = g_strdupv (core_strv);
  onym_core_strv_free (core_strv);
  return out != NULL ? out : empty_strv ();
}

/**
 * onym_engine_lookup:
 * @self: an OnymEngine
 * @word: the word to look up, possibly inflected
 * @error: (nullable): return location for an error
 *
 * Look @word up in WordNet. A word that is simply absent returns %NULL with @error unset; a
 * missing database returns %NULL with @error set. WordNet morphology resolves an inflected query
 * to its headword.
 *
 * Returns: (transfer full) (nullable): the entry, or %NULL
 */
OnymResult *
onym_engine_lookup (OnymEngine *self, const char *word, GError **error)
{
  g_return_val_if_fail (ONYM_IS_ENGINE (self), NULL);
  g_return_val_if_fail (word != NULL, NULL);

  if (!ensure_core (self, error))
    return NULL;

  return onym_bridge_lookup (self->core, word);
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

  if (!ensure_core (self, NULL))
    return empty_strv ();
  return copy_and_free_strv (onym_core_complete (self->core, prefix, max_results));
}

/**
 * onym_engine_suggest:
 * @self: an OnymEngine
 * @word: the word that was not found
 * @max_results: the most suggestions to return, or 0 for no limit
 *
 * Return headwords close to @word by edit distance, for a did you mean prompt after a missed
 * lookup.
 *
 * Returns: (transfer full) (array zero-terminated=1): the suggestions
 */
char **
onym_engine_suggest (OnymEngine *self, const char *word, guint max_results)
{
  g_return_val_if_fail (ONYM_IS_ENGINE (self), empty_strv ());

  if (!ensure_core (self, NULL))
    return empty_strv ();
  return copy_and_free_strv (onym_core_suggest (self->core, word, max_results));
}

/**
 * onym_engine_random_word:
 * @self: an OnymEngine
 *
 * Pick a headword at random from the lemma index, for a "surprise me" action. The core is
 * immutable once opened and takes no randomness itself, so the choice is made here: read the
 * count, pick an index with g_random_int_range(), and fetch the headword at it.
 *
 * Returns: (transfer full) (nullable): a headword in display form, freed with g_free(), or %NULL
 *   when the database is missing or holds no headwords
 */
char *
onym_engine_random_word (OnymEngine *self)
{
  g_return_val_if_fail (ONYM_IS_ENGINE (self), NULL);

  if (!ensure_core (self, NULL))
    return NULL;

  /* WordNet 3.0 indexes about 147,000 headwords, so the count sits comfortably inside the
   * gint32 range g_random_int_range works in. */
  size_t count = onym_core_lemma_count (self->core);
  if (count == 0)
    return NULL;

  gint32 limit = (gint32) MIN (count, (size_t) G_MAXINT32);
  char *lemma = onym_core_lemma_at (self->core, (size_t) g_random_int_range (0, limit));
  char *word = g_strdup (lemma);
  onym_core_string_free (lemma);
  return word;
}
