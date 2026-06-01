/* wn-index.c
 *
 * SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* The lemma index and its string helpers. The index reads the WordNet index files directly, so it
 * depends on GLib only and not on the WordNet library. It holds every headword once, lowercased and
 * in display form, sorted, which makes prefix completion a binary search and spelling suggestions a
 * bounded edit distance scan. */

#include "wn-index.h"

#include <string.h>

struct _WnIndex
{
  GPtrArray *lemmas; /* sorted, deduplicated, owned strings */
};

/* String helpers. */

char *
onym_term_to_display (const char *raw)
{
  if (raw == NULL)
    return NULL;
  return g_strdelimit (g_strdup (raw), "_", ' ');
}

char *
onym_term_to_query (const char *input)
{
  if (input == NULL)
    return NULL;
  return g_strdelimit (g_strstrip (g_strdup (input)), " ", '_');
}

guint
onym_edit_distance (const char *a, const char *b)
{
  if (a == NULL)
    a = "";
  if (b == NULL)
    b = "";

  gsize la = strlen (a);
  gsize lb = strlen (b);
  if (la == 0)
    return lb;
  if (lb == 0)
    return la;

  guint *prev = g_new (guint, lb + 1);
  guint *cur = g_new (guint, lb + 1);
  for (gsize j = 0; j <= lb; j++)
    prev[j] = j;

  for (gsize i = 1; i <= la; i++)
    {
      cur[0] = i;
      for (gsize j = 1; j <= lb; j++)
        {
          guint cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
          guint del = prev[j] + 1;
          guint ins = cur[j - 1] + 1;
          guint sub = prev[j - 1] + cost;
          guint best = del < ins ? del : ins;
          cur[j] = sub < best ? sub : best;
        }
      guint *swap = prev;
      prev = cur;
      cur = swap;
    }

  guint result = prev[lb];
  g_free (prev);
  g_free (cur);
  return result;
}

/* Index loading. */

static gint
compare_lemmas (gconstpointer a, gconstpointer b)
{
  const char *sa = *(const char * const *) a;
  const char *sb = *(const char * const *) b;
  return strcmp (sa, sb);
}

/* Read one WordNet index file, appending each lemma in lowercased display form. Lines that begin
 * with a space are the licence header and are skipped. The lemma is the first space delimited
 * field on a line. */
static void
load_index_file (GPtrArray *lemmas, const char *path)
{
  char *contents = NULL;
  if (!g_file_get_contents (path, &contents, NULL, NULL))
    return;

  for (char *p = contents; *p != '\0';)
    {
      char *eol = strchr (p, '\n');
      gsize line_len = eol != NULL ? (gsize) (eol - p) : strlen (p);

      if (line_len > 0 && p[0] != ' ')
        {
          char *space = memchr (p, ' ', line_len);
          gsize token_len = space != NULL ? (gsize) (space - p) : line_len;
          char *raw = g_strndup (p, token_len);
          char *display = g_strdelimit (raw, "_", ' '); /* in place, display == raw */
          g_ptr_array_add (lemmas, g_ascii_strdown (display, -1));
          g_free (raw);
        }

      if (eol == NULL)
        break;
      p = eol + 1;
    }

  g_free (contents);
}

WnIndex *
wn_index_new (const char *search_dir)
{
  if (search_dir == NULL)
    return NULL;

  GPtrArray *raw = g_ptr_array_new_with_free_func (g_free);
  const char *files[] = { "index.noun", "index.verb", "index.adj", "index.adv" };
  for (gsize i = 0; i < G_N_ELEMENTS (files); i++)
    {
      char *path = g_build_filename (search_dir, files[i], NULL);
      load_index_file (raw, path);
      g_free (path);
    }

  if (raw->len == 0)
    {
      g_ptr_array_unref (raw);
      return NULL;
    }

  g_ptr_array_sort (raw, compare_lemmas);

  /* Move pointers into a deduplicated array, freeing the duplicates we drop. */
  g_ptr_array_set_free_func (raw, NULL);
  GPtrArray *lemmas = g_ptr_array_new_with_free_func (g_free);
  const char *previous = NULL;
  for (guint i = 0; i < raw->len; i++)
    {
      char *lemma = g_ptr_array_index (raw, i);
      if (previous != NULL && strcmp (lemma, previous) == 0)
        {
          g_free (lemma);
        }
      else
        {
          g_ptr_array_add (lemmas, lemma);
          previous = lemma;
        }
    }
  g_ptr_array_unref (raw);

  WnIndex *self = g_new0 (WnIndex, 1);
  self->lemmas = lemmas;
  return self;
}

void
wn_index_free (WnIndex *self)
{
  if (self == NULL)
    return;
  g_ptr_array_unref (self->lemmas);
  g_free (self);
}

/* The first position whose lemma is greater than or equal to key. */
static guint
lower_bound (GPtrArray *lemmas, const char *key)
{
  guint lo = 0;
  guint hi = lemmas->len;
  while (lo < hi)
    {
      guint mid = (lo + hi) / 2;
      if (strcmp (g_ptr_array_index (lemmas, mid), key) < 0)
        lo = mid + 1;
      else
        hi = mid;
    }
  return lo;
}

char **
wn_index_complete (WnIndex *self, const char *prefix, guint max_results)
{
  GPtrArray *out = g_ptr_array_new ();

  if (self != NULL && prefix != NULL && *prefix != '\0')
    {
      char *needle = g_strdelimit (g_ascii_strdown (prefix, -1), "_", ' ');
      for (guint i = lower_bound (self->lemmas, needle);
           i < self->lemmas->len && (max_results == 0 || out->len < max_results);
           i++)
        {
          const char *lemma = g_ptr_array_index (self->lemmas, i);
          if (!g_str_has_prefix (lemma, needle))
            break;
          g_ptr_array_add (out, g_strdup (lemma));
        }
      g_free (needle);
    }

  g_ptr_array_add (out, NULL);
  return (char **) g_ptr_array_free (out, FALSE);
}

typedef struct
{
  const char *term;
  guint distance;
} Candidate;

static gint
compare_candidates (gconstpointer a, gconstpointer b)
{
  const Candidate *ca = a;
  const Candidate *cb = b;
  if (ca->distance != cb->distance)
    return ca->distance < cb->distance ? -1 : 1;
  return strcmp (ca->term, cb->term);
}

char **
wn_index_suggest (WnIndex *self, const char *word, guint max_results)
{
  GPtrArray *out = g_ptr_array_new ();

  if (self != NULL && word != NULL && *word != '\0')
    {
      char *needle = g_strdelimit (g_ascii_strdown (word, -1), "_", ' ');
      gsize needle_len = strlen (needle);
      GArray *candidates = g_array_new (FALSE, FALSE, sizeof (Candidate));

      for (guint i = 0; i < self->lemmas->len; i++)
        {
          const char *lemma = g_ptr_array_index (self->lemmas, i);
          gsize lemma_len = strlen (lemma);
          gsize length_gap = lemma_len > needle_len ? lemma_len - needle_len : needle_len - lemma_len;
          if (length_gap > 2)
            continue;

          guint distance = onym_edit_distance (needle, lemma);
          if (distance > 0 && distance <= 2)
            {
              Candidate candidate = { lemma, distance };
              g_array_append_val (candidates, candidate);
            }
        }

      g_array_sort (candidates, compare_candidates);
      for (guint i = 0; i < candidates->len && (max_results == 0 || out->len < max_results); i++)
        g_ptr_array_add (out, g_strdup (g_array_index (candidates, Candidate, i).term));

      g_array_free (candidates, TRUE);
      g_free (needle);
    }

  g_ptr_array_add (out, NULL);
  return (char **) g_ptr_array_free (out, FALSE);
}
