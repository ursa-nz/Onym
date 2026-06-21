/* onym-lookup.c
 *
 * SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* The bridge from the shared engine to the public model. It walks the plain C entry that
 * onym-engine's C ABI returns and copies it into model GObjects, then frees the entry in one
 * call. After this file, nothing in the library touches engine types.
 *
 * The engine decides everything lexical: section order and titles, headword resolution, the
 * direct and indirect antonym distinction, and the dropping of empty sections. The bridge only
 * changes representation, so the model carries the engine's answer byte for byte. */

#include "onym-lookup.h"
#include "onym-result-private.h"

/* Copy a NULL-terminated engine string array into a GStrv, or NULL when it is empty, which is
 * how the model marks absent examples. */
static GStrv
strv_copy (char **strv)
{
  if (strv == NULL || strv[0] == NULL)
    return NULL;
  return g_strdupv (strv);
}

static void
add_definitions (OnymResult *out, const OnymCoreSection *section)
{
  OnymSection *built = onym_section_new (ONYM_SECTION_DEFINITIONS, section->title);
  for (size_t i = 0; i < section->n_items; i++)
    {
      const OnymCoreDefinition *def = &section->definitions[i];
      onym_section_add (built, onym_definition_new (def->pos, def->gloss,
                                                    strv_copy (def->examples)));
    }
  onym_result_add_section (out, built);
}

/* Both the flat Words sections and the Etymology section arrive as a string array in the engine's
 * words slot; the kind is all that differs, and it tells the view to render chips or prose. */
static void
add_word_section (OnymResult *out, const OnymCoreSection *section, OnymSectionKind kind)
{
  OnymSection *built = onym_section_new (kind, section->title);
  for (size_t i = 0; i < section->n_items; i++)
    onym_section_add (built, onym_word_new (section->words[i]));
  onym_result_add_section (out, built);
}

static void
add_antonyms (OnymResult *out, const OnymCoreSection *section)
{
  OnymSection *built = onym_section_new (ONYM_SECTION_ANTONYMS, section->title);
  for (size_t i = 0; i < section->n_items; i++)
    {
      const OnymCoreAntonym *item = &section->antonyms[i];
      OnymAntonym *antonym = onym_antonym_new (item->term, item->direct != 0);
      for (char **implication = item->implications;
           implication != NULL && *implication != NULL; implication++)
        onym_antonym_add_implication (antonym, *implication);
      onym_section_add (built, antonym);
    }
  onym_result_add_section (out, built);
}

/* Convert one engine tree node into an OnymTreeNode, recursing into its children. */
static OnymTreeNode *
convert_node (const OnymCoreTreeNode *node)
{
  OnymTreeNode *out = onym_tree_node_new (g_strdupv (node->terms));
  for (OnymCoreTreeNode **child = node->children; *child != NULL; child++)
    onym_tree_node_add_child (out, convert_node (*child));
  return out;
}

static void
add_tree (OnymResult *out, const OnymCoreSection *section)
{
  OnymSection *built = onym_section_new (ONYM_SECTION_TREE, section->title);
  for (OnymCoreTreeNode **node = section->tree; *node != NULL; node++)
    onym_section_add (built, convert_node (*node));
  onym_result_add_section (out, built);
}

static void
add_translations (OnymResult *out, const OnymCoreSection *section)
{
  OnymSection *built = onym_section_new (ONYM_SECTION_TRANSLATIONS, section->title);
  for (size_t i = 0; i < section->n_items; i++)
    {
      const OnymCoreSenseTranslations *block = &section->translations[i];
      OnymSenseTranslations *sense = onym_sense_translations_new (block->pos, block->gloss);
      for (size_t l = 0; l < block->n_languages; l++)
        {
          const OnymCoreLanguageWords *lang = &block->languages[l];
          onym_sense_translations_add_language (sense, lang->language, strv_copy (lang->words));
        }
      onym_section_add (built, sense);
    }
  onym_result_add_section (out, built);
}

OnymResult *
onym_bridge_lookup (const OnymCoreEngine *core, const char *query)
{
  g_return_val_if_fail (core != NULL, NULL);
  g_return_val_if_fail (query != NULL, NULL);

  OnymCoreEntry *entry = onym_core_lookup (core, query);
  if (entry == NULL)
    return NULL;

  OnymResult *out = onym_result_new (entry->term);
  for (size_t i = 0; i < entry->n_sections; i++)
    {
      const OnymCoreSection *section = &entry->sections[i];
      switch (section->kind)
        {
        case ONYM_CORE_SECTION_DEFINITIONS:
          add_definitions (out, section);
          break;
        case ONYM_CORE_SECTION_WORDS:
          add_word_section (out, section, ONYM_SECTION_WORDS);
          break;
        case ONYM_CORE_SECTION_ANTONYMS:
          add_antonyms (out, section);
          break;
        case ONYM_CORE_SECTION_TREE:
          add_tree (out, section);
          break;
        case ONYM_CORE_SECTION_ETYMOLOGY:
          add_word_section (out, section, ONYM_SECTION_ETYMOLOGY);
          break;
        case ONYM_CORE_SECTION_TRANSLATIONS:
          add_translations (out, section);
          break;
        default:
          break;
        }
    }

  onym_core_entry_free (entry);
  return out;
}
