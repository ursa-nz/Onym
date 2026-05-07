/* onym-lookup.c
 *
 * SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* The bridge from the WordNet engine to the public model. It walks the GSList that the engine
 * returns and copies what it needs into model GObjects, then frees the engine response. After this
 * file, nothing in the library touches WordNet or Artha types.
 *
 * WordNet to section mapping:
 *   OVERVIEW definitions  -> a Definitions section of OnymDefinition, grouped sense by sense
 *   OVERVIEW synonyms     -> a Synonyms section of OnymWord
 *   ANTONYMS              -> an Antonyms section of OnymAntonym, direct and indirect
 *   DERIVATIONS, SIMILAR, ATTRIBUTES, CAUSES, ENTAILS -> Words sections of OnymWord
 *   PERTAINYMS, HYPERNYMS, HYPONYMS, HOLONYMS, MERONYMS -> Tree sections of OnymTreeNode
 *   CLASS                 -> a Domains words section
 *
 * The tree relations arrive as GNode hierarchies, grown to full depth only in advanced mode, so the
 * lookup requests advanced mode. The model carries the section order, so display order is decided
 * here. */

#include "onym-lookup.h"
#include "onym-result-private.h"
#include "wn-index.h"

#include "engine/wni.h"

/* Every relation the model can present. */
#define ONYM_LOOKUP_FLAGS WORDNET_INTERFACE_ALL

/* Map a WordNet part of speech code to a display name. */
static const char *
pos_name (guint8 pos)
{
  switch (pos)
    {
    case 1:
      return "noun";
    case 2:
      return "verb";
    case 3:
      return "adjective";
    case 4:
      return "adverb";
    case 5:
      return "adjective"; /* satellite adjective */
    default:
      return NULL;
    }
}

/* Find the response node for a relation, or NULL if the engine returned none. */
static WNINym *
find_nym (GSList *results, WNIRequestFlags id)
{
  for (GSList *l = results; l != NULL; l = l->next)
    {
      WNINym *nym = l->data;
      if (nym != NULL && nym->id == id)
        return nym;
    }
  return NULL;
}

/* The resolved headword, in display form, from the overview. May be NULL. */
static char *
overview_headword (WNINym *overview)
{
  if (overview != NULL && overview->data != NULL)
    {
      WNIOverview *ov = overview->data;
      if (ov->definitions_list != NULL)
        {
          WNIDefinitionItem *item = ov->definitions_list->data;
          if (item != NULL && item->lemma != NULL)
            return onym_term_to_display (item->lemma);
        }
    }
  return NULL;
}

/* Copy a list of example sentence strings into a GStrv, or NULL when there are none. */
static GStrv
examples_to_strv (GSList *examples)
{
  if (examples == NULL)
    return NULL;

  GStrvBuilder *builder = g_strv_builder_new ();
  for (GSList *l = examples; l != NULL; l = l->next)
    if (l->data != NULL)
      g_strv_builder_add (builder, l->data);

  GStrv out = g_strv_builder_end (builder);
  g_strv_builder_unref (builder);

  if (out != NULL && out[0] == NULL)
    {
      g_strfreev (out);
      return NULL;
    }
  return out;
}

/* Add the section only if it gathered at least one item, otherwise drop it. */
static void
add_section_if_filled (OnymResult *out, OnymSection *section)
{
  if (g_list_model_get_n_items (onym_section_get_items (section)) > 0)
    onym_result_add_section (out, section);
  else
    g_object_unref (section);
}

static void
add_definitions (OnymResult *out, WNIOverview *ov)
{
  if (ov == NULL || ov->definitions_list == NULL)
    return;

  OnymSection *section = onym_section_new (ONYM_SECTION_DEFINITIONS, "Definitions");
  for (GSList *il = ov->definitions_list; il != NULL; il = il->next)
    {
      WNIDefinitionItem *item = il->data;
      if (item == NULL)
        continue;

      const char *pos = pos_name (item->pos);
      for (GSList *dl = item->definitions; dl != NULL; dl = dl->next)
        {
          WNIDefinition *def = dl->data;
          if (def == NULL || def->definition == NULL)
            continue;
          onym_section_add (section,
                            onym_definition_new (pos, def->definition,
                                                 examples_to_strv (def->examples)));
        }
    }
  add_section_if_filled (out, section);
}

static void
add_words (OnymResult *out, const char *title, GSList *items)
{
  if (items == NULL)
    return;

  OnymSection *section = onym_section_new (ONYM_SECTION_WORDS, title);
  for (GSList *l = items; l != NULL; l = l->next)
    {
      WNIPropertyItem *item = l->data;
      if (item == NULL || item->term == NULL)
        continue;
      char *display = onym_term_to_display (item->term);
      onym_section_add (section, onym_word_new (display));
      g_free (display);
    }
  add_section_if_filled (out, section);
}

static void
add_antonyms (OnymResult *out, GSList *antonyms)
{
  if (antonyms == NULL)
    return;

  OnymSection *section = onym_section_new (ONYM_SECTION_ANTONYMS, "Antonyms");
  for (GSList *l = antonyms; l != NULL; l = l->next)
    {
      WNIAntonymItem *item = l->data;
      if (item == NULL || item->term == NULL)
        continue;

      char *display = onym_term_to_display (item->term);
      OnymAntonym *antonym = onym_antonym_new (display, item->relation == DIRECT_ANT);
      g_free (display);

      for (GSList *il = item->implications; il != NULL; il = il->next)
        {
          WNIImplication *implication = il->data;
          if (implication != NULL && implication->term != NULL)
            {
              char *implied = onym_term_to_display (implication->term);
              onym_antonym_add_implication (antonym, implied);
              g_free (implied);
            }
        }
      onym_section_add (section, antonym);
    }
  add_section_if_filled (out, section);
}

/* Add a flat relation section, looking up its node by request flag. */
static void
add_relation (OnymResult *out, GSList *results, WNIRequestFlags id, const char *title)
{
  WNINym *nym = find_nym (results, id);
  if (nym != NULL && nym->data != NULL)
    add_words (out, title, ((WNIProperties *) nym->data)->properties_list);
}

/* Convert one engine GNode, whose data is a WNITreeList of synset terms, into an OnymTreeNode,
 * recursing into its children. */
static OnymTreeNode *
convert_gnode (GNode *node)
{
  WNITreeList *list = node->data;

  GStrvBuilder *builder = g_strv_builder_new ();
  for (GSList *l = (list != NULL) ? list->word_list : NULL; l != NULL; l = l->next)
    {
      WNIImplication *implication = l->data;
      if (implication == NULL || implication->term == NULL)
        continue;
      char *display = onym_term_to_display (implication->term);
      g_strv_builder_add (builder, display);
      g_free (display);
    }

  OnymTreeNode *out = onym_tree_node_new (g_strv_builder_end (builder));
  g_strv_builder_unref (builder);

  for (GNode *child = g_node_first_child (node); child != NULL; child = g_node_next_sibling (child))
    onym_tree_node_add_child (out, convert_gnode (child));

  return out;
}

/* Add a hierarchical relation section. Each engine property is a GNode root, one per sense, whose
 * children are the top level nodes. Top level nodes are deduplicated by label across senses. */
static void
add_tree (OnymResult *out, GSList *results, WNIRequestFlags id, const char *title)
{
  WNINym *nym = find_nym (results, id);
  if (nym == NULL || nym->data == NULL)
    return;

  OnymSection *section = onym_section_new (ONYM_SECTION_TREE, title);
  GHashTable *seen = g_hash_table_new (g_str_hash, g_str_equal);

  for (GSList *l = ((WNIProperties *) nym->data)->properties_list; l != NULL; l = l->next)
    {
      GNode *root = l->data;
      if (root == NULL)
        continue;

      for (GNode *child = g_node_first_child (root); child != NULL;
           child = g_node_next_sibling (child))
        {
          OnymTreeNode *node = convert_gnode (child);
          const char *label = onym_tree_node_get_label (node);
          if (label != NULL && g_hash_table_contains (seen, label))
            {
              g_object_unref (node);
              continue;
            }
          g_hash_table_add (seen, (gpointer) label);
          onym_section_add (section, node);
        }
    }

  g_hash_table_destroy (seen);
  add_section_if_filled (out, section);
}

/* Add the domain or category terms as a words section. */
static void
add_domains (OnymResult *out, GSList *results)
{
  WNINym *nym = find_nym (results, WORDNET_INTERFACE_CLASS);
  if (nym == NULL || nym->data == NULL)
    return;

  OnymSection *section = onym_section_new (ONYM_SECTION_WORDS, "Domains");
  for (GSList *l = ((WNIProperties *) nym->data)->properties_list; l != NULL; l = l->next)
    {
      WNIClassItem *item = l->data;
      if (item == NULL || item->term == NULL)
        continue;
      char *display = onym_term_to_display (item->term);
      onym_section_add (section, onym_word_new (display));
      g_free (display);
    }
  add_section_if_filled (out, section);
}

OnymResult *
onym_bridge_lookup (const char *query)
{
  g_return_val_if_fail (query != NULL, NULL);

  /* The engine may write to the search string, so hand it a copy. */
  char *search = g_strdup (query);
  GSList *results = NULL;
  gboolean found = wni_request_nyms (search, &results, ONYM_LOOKUP_FLAGS, TRUE);
  g_free (search);

  if (!found || results == NULL)
    {
      if (results != NULL)
        wni_free (&results);
      return NULL;
    }

  WNINym *overview = find_nym (results, WORDNET_INTERFACE_OVERVIEW);
  char *headword = overview_headword (overview);
  OnymResult *out = onym_result_new (headword != NULL ? headword : query);
  g_free (headword);

  if (overview != NULL && overview->data != NULL)
    {
      WNIOverview *ov = overview->data;
      add_definitions (out, ov);
      add_words (out, "Synonyms", ov->synonyms_list);
    }

  WNINym *antonyms = find_nym (results, WORDNET_INTERFACE_ANTONYMS);
  if (antonyms != NULL && antonyms->data != NULL)
    add_antonyms (out, ((WNIProperties *) antonyms->data)->properties_list);

  add_relation (out, results, WORDNET_INTERFACE_DERIVATIONS, "Derived forms");
  add_relation (out, results, WORDNET_INTERFACE_SIMILAR, "Similar to");
  add_relation (out, results, WORDNET_INTERFACE_ATTRIBUTES, "Attributes");
  add_relation (out, results, WORDNET_INTERFACE_CAUSES, "Causes");
  add_relation (out, results, WORDNET_INTERFACE_ENTAILS, "Entails");

  add_tree (out, results, WORDNET_INTERFACE_PERTAINYMS, "Pertains to");
  add_tree (out, results, WORDNET_INTERFACE_HYPERNYMS, "Is a kind of");
  add_tree (out, results, WORDNET_INTERFACE_HYPONYMS, "Kinds");
  add_tree (out, results, WORDNET_INTERFACE_HOLONYMS, "Part of");
  add_tree (out, results, WORDNET_INTERFACE_MERONYMS, "Parts");
  add_domains (out, results);

  wni_free (&results);
  return out;
}
