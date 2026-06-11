/* test-lookup.c
 *
 * SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* Integration tests against the real WordNet database. They assert structural facts rather than
 * exact glosses, so they stay robust across WordNet releases. They skip themselves when the data is
 * not present, which keeps the suite runnable anywhere. */

#include <onym.h>

#include <glib.h>

static gboolean
data_available (void)
{
  const char *dir = g_getenv ("WNSEARCHDIR");
  if (dir == NULL || *dir == '\0')
    dir = "/usr/share/wordnet";

  char *probe = g_build_filename (dir, "index.noun", NULL);
  gboolean ok = g_file_test (probe, G_FILE_TEST_EXISTS);
  g_free (probe);
  return ok;
}

static OnymSection *
find_section (OnymResult *result, OnymSectionKind kind)
{
  GListModel *sections = onym_result_get_sections (result);
  guint n = g_list_model_get_n_items (sections);
  for (guint i = 0; i < n; i++)
    {
      OnymSection *section = g_list_model_get_item (sections, i);
      if (onym_section_get_kind (section) == kind)
        return section;
      g_object_unref (section);
    }
  return NULL;
}

static void
test_lookup_basic (void)
{
  if (!data_available ())
    {
      g_test_skip ("WordNet data not present");
      return;
    }

  OnymEngine *engine = onym_engine_new ();
  GError *error = NULL;
  OnymResult *result = onym_engine_lookup (engine, "dog", &error);

  g_assert_no_error (error);
  g_assert_nonnull (result);
  g_assert_cmpstr (onym_result_get_term (result), ==, "dog");

  OnymSection *definitions = find_section (result, ONYM_SECTION_DEFINITIONS);
  g_assert_nonnull (definitions);
  g_assert_cmpuint (g_list_model_get_n_items (onym_section_get_items (definitions)), >, 0);
  g_object_unref (definitions);

  g_object_unref (result);
  g_object_unref (engine);
}

static void
test_lookup_morphology (void)
{
  if (!data_available ())
    {
      g_test_skip ("WordNet data not present");
      return;
    }

  OnymEngine *engine = onym_engine_new ();
  OnymResult *result = onym_engine_lookup (engine, "mice", NULL);

  g_assert_nonnull (result);
  g_assert_cmpstr (onym_result_get_term (result), ==, "mouse");

  g_object_unref (result);
  g_object_unref (engine);
}

static void
test_lookup_miss (void)
{
  if (!data_available ())
    {
      g_test_skip ("WordNet data not present");
      return;
    }

  OnymEngine *engine = onym_engine_new ();
  GError *error = NULL;
  OnymResult *result = onym_engine_lookup (engine, "zzzxyqq", &error);

  g_assert_no_error (error);
  g_assert_null (result);

  g_object_unref (engine);
}

static void
test_lookup_tree (void)
{
  if (!data_available ())
    {
      g_test_skip ("WordNet data not present");
      return;
    }

  OnymEngine *engine = onym_engine_new ();
  OnymResult *result = onym_engine_lookup (engine, "dog", NULL);

  g_assert_nonnull (result);

  OnymSection *tree = find_section (result, ONYM_SECTION_TREE);
  g_assert_nonnull (tree);

  GListModel *items = onym_section_get_items (tree);
  g_assert_cmpuint (g_list_model_get_n_items (items), >, 0);

  OnymTreeNode *root = g_list_model_get_item (items, 0);
  g_assert_cmpuint (g_list_model_get_n_items (onym_tree_node_get_children (root)), >, 0);
  g_object_unref (root);

  g_object_unref (tree);
  g_object_unref (result);
  g_object_unref (engine);
}

static void
test_complete (void)
{
  if (!data_available ())
    {
      g_test_skip ("WordNet data not present");
      return;
    }

  OnymEngine *engine = onym_engine_new ();
  char **matches = onym_engine_complete (engine, "serend", 10);

  gboolean found = FALSE;
  for (guint i = 0; matches[i] != NULL; i++)
    if (g_strcmp0 (matches[i], "serendipity") == 0)
      found = TRUE;
  g_assert_true (found);

  g_strfreev (matches);
  g_object_unref (engine);
}

static void
test_random_word (void)
{
  if (!data_available ())
    {
      g_test_skip ("WordNet data not present");
      return;
    }

  OnymEngine *engine = onym_engine_new ();
  char *word = onym_engine_random_word (engine);

  /* The pick is random but the contract is not: with data present the word is non-empty, and any
   * headword the index can pick must itself look up. */
  g_assert_nonnull (word);
  g_assert_true (*word != '\0');

  OnymResult *result = onym_engine_lookup (engine, word, NULL);
  g_assert_nonnull (result);

  g_object_unref (result);
  g_free (word);
  g_object_unref (engine);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/onym/lookup/basic", test_lookup_basic);
  g_test_add_func ("/onym/lookup/morphology", test_lookup_morphology);
  g_test_add_func ("/onym/lookup/miss", test_lookup_miss);
  g_test_add_func ("/onym/lookup/tree", test_lookup_tree);
  g_test_add_func ("/onym/lookup/complete", test_complete);
  g_test_add_func ("/onym/lookup/random", test_random_word);
  return g_test_run ();
}
