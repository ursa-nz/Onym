/* onym-cli.c
 *
 * SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* A headless front end to libonym. It proves the library without any graphical interface, and its
 * deterministic output drives the golden tests. Usage:
 *
 *   onym-cli WORD               look a word up and print its entry
 *   onym-cli --dump WORD        the same, named for use as a stable snapshot
 *   onym-cli --complete PREFIX  print headwords that begin with PREFIX
 *   onym-cli --suggest WORD     print spelling suggestions for WORD */

#include <onym.h>

#include <glib.h>

static void
print_strv (char **strv)
{
  for (guint i = 0; strv[i] != NULL; i++)
    g_print ("%s\n", strv[i]);
}

static void
print_definitions (GListModel *items)
{
  guint n = g_list_model_get_n_items (items);
  for (guint i = 0; i < n; i++)
    {
      OnymDefinition *def = g_list_model_get_item (items, i);
      const char *pos = onym_definition_get_pos (def);
      if (pos != NULL)
        g_print ("  - (%s) %s\n", pos, onym_definition_get_gloss (def));
      else
        g_print ("  - %s\n", onym_definition_get_gloss (def));

      const char * const *examples = onym_definition_get_examples (def);
      for (guint e = 0; examples != NULL && examples[e] != NULL; e++)
        g_print ("      \"%s\"\n", examples[e]);

      g_object_unref (def);
    }
}

static void
print_words (GListModel *items)
{
  guint n = g_list_model_get_n_items (items);
  for (guint i = 0; i < n; i++)
    {
      OnymWord *word = g_list_model_get_item (items, i);
      g_print ("  - %s\n", onym_word_get_term (word));
      g_object_unref (word);
    }
}

static void
print_antonyms (GListModel *items)
{
  guint n = g_list_model_get_n_items (items);
  for (guint i = 0; i < n; i++)
    {
      OnymAntonym *antonym = g_list_model_get_item (items, i);
      g_print ("  - %s (%s)\n", onym_antonym_get_term (antonym),
               onym_antonym_get_direct (antonym) ? "direct" : "indirect");

      GListModel *implications = onym_antonym_get_implications (antonym);
      guint m = g_list_model_get_n_items (implications);
      for (guint j = 0; j < m; j++)
        {
          OnymWord *word = g_list_model_get_item (implications, j);
          g_print ("      -> %s\n", onym_word_get_term (word));
          g_object_unref (word);
        }
      g_object_unref (antonym);
    }
}

static void
print_tree_node (OnymTreeNode *node, int depth)
{
  for (int i = 0; i <= depth; i++)
    g_print ("  ");
  g_print ("- %s\n", onym_tree_node_get_label (node));

  GListModel *children = onym_tree_node_get_children (node);
  guint n = g_list_model_get_n_items (children);
  for (guint i = 0; i < n; i++)
    {
      OnymTreeNode *child = g_list_model_get_item (children, i);
      print_tree_node (child, depth + 1);
      g_object_unref (child);
    }
}

static void
print_tree (GListModel *items)
{
  guint n = g_list_model_get_n_items (items);
  for (guint i = 0; i < n; i++)
    {
      OnymTreeNode *root = g_list_model_get_item (items, i);
      print_tree_node (root, 0);
      g_object_unref (root);
    }
}

static void
render_result (OnymResult *result)
{
  g_print ("term: %s\n", onym_result_get_term (result));

  GListModel *sections = onym_result_get_sections (result);
  guint n = g_list_model_get_n_items (sections);
  for (guint i = 0; i < n; i++)
    {
      OnymSection *section = g_list_model_get_item (sections, i);
      GListModel *items = onym_section_get_items (section);

      g_print ("[%s]\n", onym_section_get_title (section));
      switch (onym_section_get_kind (section))
        {
        case ONYM_SECTION_DEFINITIONS:
          print_definitions (items);
          break;
        case ONYM_SECTION_WORDS:
          print_words (items);
          break;
        case ONYM_SECTION_ANTONYMS:
          print_antonyms (items);
          break;
        case ONYM_SECTION_TREE:
          print_tree (items);
          break;
        default:
          break;
        }
      g_object_unref (section);
    }
}

static int
do_lookup (OnymEngine *engine, const char *word)
{
  GError *error = NULL;
  OnymResult *result = onym_engine_lookup (engine, word, &error);

  if (error != NULL)
    {
      g_printerr ("error: %s\n", error->message);
      g_error_free (error);
      return 1;
    }

  if (result == NULL)
    {
      g_print ("No entry for \"%s\".\n", word);
      char **suggestions = onym_engine_suggest (engine, word, 5);
      if (suggestions[0] != NULL)
        {
          char *joined = g_strjoinv (", ", suggestions);
          g_print ("Did you mean: %s\n", joined);
          g_free (joined);
        }
      g_strfreev (suggestions);
      return 0;
    }

  render_result (result);
  g_object_unref (result);
  return 0;
}

int
main (int argc, char **argv)
{
  const char *mode = NULL;
  const char *arg = NULL;

  if (argc == 2)
    {
      arg = argv[1];
    }
  else if (argc == 3)
    {
      mode = argv[1];
      arg = argv[2];
    }
  else
    {
      g_printerr ("usage: onym-cli [--dump|--complete|--suggest] WORD\n");
      return 2;
    }

  OnymEngine *engine = onym_engine_new ();
  int status;

  if (mode == NULL || g_strcmp0 (mode, "--dump") == 0)
    {
      status = do_lookup (engine, arg);
    }
  else if (g_strcmp0 (mode, "--complete") == 0)
    {
      char **matches = onym_engine_complete (engine, arg, 20);
      print_strv (matches);
      g_strfreev (matches);
      status = 0;
    }
  else if (g_strcmp0 (mode, "--suggest") == 0)
    {
      char **suggestions = onym_engine_suggest (engine, arg, 10);
      print_strv (suggestions);
      g_strfreev (suggestions);
      status = 0;
    }
  else
    {
      g_printerr ("usage: onym-cli [--dump|--complete|--suggest] WORD\n");
      status = 2;
    }

  g_object_unref (engine);
  return status;
}
