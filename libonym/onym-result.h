/* onym-result.h
 *
 * SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* The public lexical model that a lookup returns. Every type is a GObject and every collection is
 * a GListModel, so a result drops straight into a GTK list view and binds cleanly from introspected
 * languages. The model is read only for consumers; libonym builds it internally. */

#pragma once

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

/* The kind of content a section holds. The kind tells a consumer which GObject type the section's
 * items are, so it can pick a renderer without inspecting an item first. */
typedef enum
{
  ONYM_SECTION_DEFINITIONS,  /* items are OnymDefinition */
  ONYM_SECTION_WORDS,        /* items are OnymWord */
  ONYM_SECTION_ANTONYMS,     /* items are OnymAntonym */
  ONYM_SECTION_TREE,         /* items are OnymTreeNode */
  ONYM_SECTION_ETYMOLOGY,    /* items are OnymWord, each a prose paragraph, not a navigable term */
  ONYM_SECTION_TRANSLATIONS, /* items are OnymSenseTranslations, one per looked-up sense */
} OnymSectionKind;

GType onym_section_kind_get_type (void);
#define ONYM_TYPE_SECTION_KIND (onym_section_kind_get_type ())

/* OnymWord: a single activatable term, such as a synonym. A consumer looks the term up again to
 * navigate between words. */
#define ONYM_TYPE_WORD (onym_word_get_type ())
G_DECLARE_FINAL_TYPE (OnymWord, onym_word, ONYM, WORD, GObject)

const char *onym_word_get_term (OnymWord *self);

/* OnymDefinition: one sense of a word. The part of speech and the example sentences may be absent.
 * The examples are a null terminated array, owned by the definition. */
#define ONYM_TYPE_DEFINITION (onym_definition_get_type ())
G_DECLARE_FINAL_TYPE (OnymDefinition, onym_definition, ONYM, DEFINITION, GObject)

const char         *onym_definition_get_pos      (OnymDefinition *self);
const char         *onym_definition_get_gloss    (OnymDefinition *self);
const char * const *onym_definition_get_examples (OnymDefinition *self);

/* OnymAntonym: an opposite of the looked up word. It is either a direct antonym or an indirect one,
 * and it may carry related implication terms. */
#define ONYM_TYPE_ANTONYM (onym_antonym_get_type ())
G_DECLARE_FINAL_TYPE (OnymAntonym, onym_antonym, ONYM, ANTONYM, GObject)

const char *onym_antonym_get_term         (OnymAntonym *self);
gboolean    onym_antonym_get_direct       (OnymAntonym *self);
GListModel *onym_antonym_get_implications (OnymAntonym *self); /* of OnymWord */

/* OnymTreeNode: one node of a lexical hierarchy, such as an is-a or part-of relation. A node is one
 * synset, so it carries several terms; each is a word that can be looked up. The label is those terms
 * joined for display. Children are the nodes one level deeper. */
#define ONYM_TYPE_TREE_NODE (onym_tree_node_get_type ())
G_DECLARE_FINAL_TYPE (OnymTreeNode, onym_tree_node, ONYM, TREE_NODE, GObject)

const char         *onym_tree_node_get_label    (OnymTreeNode *self);
const char * const *onym_tree_node_get_terms    (OnymTreeNode *self);
GListModel         *onym_tree_node_get_children (OnymTreeNode *self); /* of OnymTreeNode */

/* OnymLanguageWords: one language's words for a sense. The words are the terms other languages use
 * for the concept, plain display text rather than navigable headwords. */
#define ONYM_TYPE_LANGUAGE_WORDS (onym_language_words_get_type ())
G_DECLARE_FINAL_TYPE (OnymLanguageWords, onym_language_words, ONYM, LANGUAGE_WORDS, GObject)

const char         *onym_language_words_get_language (OnymLanguageWords *self);
const char * const *onym_language_words_get_words    (OnymLanguageWords *self);

/* OnymSenseTranslations: one looked-up sense's translations. It carries the sense's part of speech
 * and gloss, as the definitions do, so a consumer can name the meaning, and the words other languages
 * use for it grouped by language. */
#define ONYM_TYPE_SENSE_TRANSLATIONS (onym_sense_translations_get_type ())
G_DECLARE_FINAL_TYPE (OnymSenseTranslations, onym_sense_translations, ONYM, SENSE_TRANSLATIONS, GObject)

const char *onym_sense_translations_get_pos       (OnymSenseTranslations *self);
const char *onym_sense_translations_get_gloss     (OnymSenseTranslations *self);
GListModel *onym_sense_translations_get_languages (OnymSenseTranslations *self); /* of OnymLanguageWords */

/* OnymSection: a titled group of items of one kind. */
#define ONYM_TYPE_SECTION (onym_section_get_type ())
G_DECLARE_FINAL_TYPE (OnymSection, onym_section, ONYM, SECTION, GObject)

OnymSectionKind  onym_section_get_kind  (OnymSection *self);
const char      *onym_section_get_title (OnymSection *self);
GListModel      *onym_section_get_items (OnymSection *self);

/* OnymResult: the whole entry for a looked up word, as an ordered list of sections. The term is the
 * resolved headword, which may differ from the query when WordNet applies morphology. */
#define ONYM_TYPE_RESULT (onym_result_get_type ())
G_DECLARE_FINAL_TYPE (OnymResult, onym_result, ONYM, RESULT, GObject)

const char *onym_result_get_term     (OnymResult *self);
GListModel *onym_result_get_sections (OnymResult *self); /* of OnymSection */

G_END_DECLS
