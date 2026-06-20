/* onym-result.c
 *
 * SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* Implementation of the public lexical model. Each type is a small final GObject. Collections are
 * GListStore instances, exposed to consumers as GListModel. The model is the only thing that
 * crosses the library boundary, so it carries no WordNet or engine types. */

#include "onym-result.h"
#include "onym-result-private.h"

G_DEFINE_ENUM_TYPE (OnymSectionKind, onym_section_kind,
                    G_DEFINE_ENUM_VALUE (ONYM_SECTION_DEFINITIONS, "definitions"),
                    G_DEFINE_ENUM_VALUE (ONYM_SECTION_WORDS, "words"),
                    G_DEFINE_ENUM_VALUE (ONYM_SECTION_ANTONYMS, "antonyms"),
                    G_DEFINE_ENUM_VALUE (ONYM_SECTION_TREE, "tree"),
                    G_DEFINE_ENUM_VALUE (ONYM_SECTION_ETYMOLOGY, "etymology"))

/* OnymWord */

struct _OnymWord
{
  GObject parent_instance;
  char *term;
};

G_DEFINE_FINAL_TYPE (OnymWord, onym_word, G_TYPE_OBJECT)

static void
onym_word_finalize (GObject *object)
{
  OnymWord *self = ONYM_WORD (object);

  g_free (self->term);

  G_OBJECT_CLASS (onym_word_parent_class)->finalize (object);
}

static void
onym_word_class_init (OnymWordClass *klass)
{
  G_OBJECT_CLASS (klass)->finalize = onym_word_finalize;
}

static void
onym_word_init (OnymWord *self)
{
}

OnymWord *
onym_word_new (const char *term)
{
  OnymWord *self = g_object_new (ONYM_TYPE_WORD, NULL);
  self->term = g_strdup (term);
  return self;
}

/**
 * onym_word_get_term:
 * @self: an OnymWord
 *
 * The term, in display form.
 *
 * Returns: (transfer none): the term
 */
const char *
onym_word_get_term (OnymWord *self)
{
  g_return_val_if_fail (ONYM_IS_WORD (self), NULL);
  return self->term;
}

/* OnymDefinition */

struct _OnymDefinition
{
  GObject parent_instance;
  char *pos;
  char *gloss;
  GStrv examples;
};

G_DEFINE_FINAL_TYPE (OnymDefinition, onym_definition, G_TYPE_OBJECT)

static void
onym_definition_finalize (GObject *object)
{
  OnymDefinition *self = ONYM_DEFINITION (object);

  g_free (self->pos);
  g_free (self->gloss);
  g_strfreev (self->examples);

  G_OBJECT_CLASS (onym_definition_parent_class)->finalize (object);
}

static void
onym_definition_class_init (OnymDefinitionClass *klass)
{
  G_OBJECT_CLASS (klass)->finalize = onym_definition_finalize;
}

static void
onym_definition_init (OnymDefinition *self)
{
}

OnymDefinition *
onym_definition_new (const char *pos, const char *gloss, GStrv examples)
{
  OnymDefinition *self = g_object_new (ONYM_TYPE_DEFINITION, NULL);
  self->pos = g_strdup (pos);
  self->gloss = g_strdup (gloss);
  self->examples = examples; /* (transfer full) */
  return self;
}

/**
 * onym_definition_get_pos:
 * @self: an OnymDefinition
 *
 * The part of speech, such as "noun", or %NULL when the sense has none.
 *
 * Returns: (transfer none) (nullable): the part of speech, or %NULL
 */
const char *
onym_definition_get_pos (OnymDefinition *self)
{
  g_return_val_if_fail (ONYM_IS_DEFINITION (self), NULL);
  return self->pos;
}

/**
 * onym_definition_get_gloss:
 * @self: an OnymDefinition
 *
 * The defining text of this sense.
 *
 * Returns: (transfer none): the gloss
 */
const char *
onym_definition_get_gloss (OnymDefinition *self)
{
  g_return_val_if_fail (ONYM_IS_DEFINITION (self), NULL);
  return self->gloss;
}

/**
 * onym_definition_get_examples:
 * @self: an OnymDefinition
 *
 * The example sentences for this sense, or %NULL when there are none.
 *
 * Returns: (transfer none) (nullable) (array zero-terminated=1): the examples, or %NULL
 */
const char * const *
onym_definition_get_examples (OnymDefinition *self)
{
  g_return_val_if_fail (ONYM_IS_DEFINITION (self), NULL);
  return (const char * const *) self->examples;
}

/* OnymAntonym */

struct _OnymAntonym
{
  GObject parent_instance;
  char *term;
  gboolean direct;
  GListStore *implications; /* of OnymWord */
};

G_DEFINE_FINAL_TYPE (OnymAntonym, onym_antonym, G_TYPE_OBJECT)

static void
onym_antonym_finalize (GObject *object)
{
  OnymAntonym *self = ONYM_ANTONYM (object);

  g_free (self->term);
  g_clear_object (&self->implications);

  G_OBJECT_CLASS (onym_antonym_parent_class)->finalize (object);
}

static void
onym_antonym_class_init (OnymAntonymClass *klass)
{
  G_OBJECT_CLASS (klass)->finalize = onym_antonym_finalize;
}

static void
onym_antonym_init (OnymAntonym *self)
{
  self->implications = g_list_store_new (ONYM_TYPE_WORD);
}

OnymAntonym *
onym_antonym_new (const char *term, gboolean direct)
{
  OnymAntonym *self = g_object_new (ONYM_TYPE_ANTONYM, NULL);
  self->term = g_strdup (term);
  self->direct = direct;
  return self;
}

void
onym_antonym_add_implication (OnymAntonym *self, const char *term)
{
  g_return_if_fail (ONYM_IS_ANTONYM (self));

  OnymWord *word = onym_word_new (term);
  g_list_store_append (self->implications, word);
  g_object_unref (word);
}

/**
 * onym_antonym_get_term:
 * @self: an OnymAntonym
 *
 * The opposite term, in display form.
 *
 * Returns: (transfer none): the term
 */
const char *
onym_antonym_get_term (OnymAntonym *self)
{
  g_return_val_if_fail (ONYM_IS_ANTONYM (self), NULL);
  return self->term;
}

/**
 * onym_antonym_get_direct:
 * @self: an OnymAntonym
 *
 * Whether this is a direct antonym, as opposed to an indirect one reached through a similar sense.
 *
 * Returns: %TRUE if direct, %FALSE if indirect
 */
gboolean
onym_antonym_get_direct (OnymAntonym *self)
{
  g_return_val_if_fail (ONYM_IS_ANTONYM (self), FALSE);
  return self->direct;
}

/**
 * onym_antonym_get_implications:
 * @self: an OnymAntonym
 *
 * The related implication terms recorded for this antonym.
 *
 * Returns: (transfer none): the implications, a #GListModel of #OnymWord
 */
GListModel *
onym_antonym_get_implications (OnymAntonym *self)
{
  g_return_val_if_fail (ONYM_IS_ANTONYM (self), NULL);
  return G_LIST_MODEL (self->implications);
}

/* OnymTreeNode */

struct _OnymTreeNode
{
  GObject parent_instance;
  GStrv terms;          /* the synset's terms, each look-up-able */
  char *label;          /* the terms joined, for display and the CLI */
  GListStore *children; /* of OnymTreeNode */
};

G_DEFINE_FINAL_TYPE (OnymTreeNode, onym_tree_node, G_TYPE_OBJECT)

static void
onym_tree_node_finalize (GObject *object)
{
  OnymTreeNode *self = ONYM_TREE_NODE (object);

  g_strfreev (self->terms);
  g_free (self->label);
  g_clear_object (&self->children);

  G_OBJECT_CLASS (onym_tree_node_parent_class)->finalize (object);
}

static void
onym_tree_node_class_init (OnymTreeNodeClass *klass)
{
  G_OBJECT_CLASS (klass)->finalize = onym_tree_node_finalize;
}

static void
onym_tree_node_init (OnymTreeNode *self)
{
  self->children = g_list_store_new (ONYM_TYPE_TREE_NODE);
}

OnymTreeNode *
onym_tree_node_new (GStrv terms)
{
  OnymTreeNode *self = g_object_new (ONYM_TYPE_TREE_NODE, NULL);
  self->terms = terms; /* (transfer full) */
  self->label = (terms != NULL) ? g_strjoinv (", ", terms) : g_strdup ("");
  return self;
}

void
onym_tree_node_add_child (OnymTreeNode *self, OnymTreeNode *child)
{
  g_return_if_fail (ONYM_IS_TREE_NODE (self));
  g_return_if_fail (ONYM_IS_TREE_NODE (child));

  g_list_store_append (self->children, child);
  g_object_unref (child);
}

/**
 * onym_tree_node_get_label:
 * @self: an OnymTreeNode
 *
 * The node's terms joined for display.
 *
 * Returns: (transfer none): the label
 */
const char *
onym_tree_node_get_label (OnymTreeNode *self)
{
  g_return_val_if_fail (ONYM_IS_TREE_NODE (self), NULL);
  return self->label;
}

/**
 * onym_tree_node_get_terms:
 * @self: an OnymTreeNode
 *
 * The synset's terms, each one a word that can be looked up.
 *
 * Returns: (transfer none) (array zero-terminated=1): the terms
 */
const char * const *
onym_tree_node_get_terms (OnymTreeNode *self)
{
  g_return_val_if_fail (ONYM_IS_TREE_NODE (self), NULL);
  return (const char * const *) self->terms;
}

/**
 * onym_tree_node_get_children:
 * @self: an OnymTreeNode
 *
 * The nodes one level deeper in the hierarchy.
 *
 * Returns: (transfer none): the children, a #GListModel of #OnymTreeNode
 */
GListModel *
onym_tree_node_get_children (OnymTreeNode *self)
{
  g_return_val_if_fail (ONYM_IS_TREE_NODE (self), NULL);
  return G_LIST_MODEL (self->children);
}

/* OnymSection */

struct _OnymSection
{
  GObject parent_instance;
  OnymSectionKind kind;
  char *title;
  GListStore *items;
};

G_DEFINE_FINAL_TYPE (OnymSection, onym_section, G_TYPE_OBJECT)

static GType
onym_section_item_type (OnymSectionKind kind)
{
  switch (kind)
    {
    case ONYM_SECTION_DEFINITIONS:
      return ONYM_TYPE_DEFINITION;
    case ONYM_SECTION_WORDS:
    case ONYM_SECTION_ETYMOLOGY:
      return ONYM_TYPE_WORD;
    case ONYM_SECTION_ANTONYMS:
      return ONYM_TYPE_ANTONYM;
    case ONYM_SECTION_TREE:
      return ONYM_TYPE_TREE_NODE;
    default:
      g_assert_not_reached ();
    }
}

static void
onym_section_finalize (GObject *object)
{
  OnymSection *self = ONYM_SECTION (object);

  g_free (self->title);
  g_clear_object (&self->items);

  G_OBJECT_CLASS (onym_section_parent_class)->finalize (object);
}

static void
onym_section_class_init (OnymSectionClass *klass)
{
  G_OBJECT_CLASS (klass)->finalize = onym_section_finalize;
}

static void
onym_section_init (OnymSection *self)
{
}

OnymSection *
onym_section_new (OnymSectionKind kind, const char *title)
{
  OnymSection *self = g_object_new (ONYM_TYPE_SECTION, NULL);
  self->kind = kind;
  self->title = g_strdup (title);
  self->items = g_list_store_new (onym_section_item_type (kind));
  return self;
}

void
onym_section_add (OnymSection *self, gpointer item)
{
  g_return_if_fail (ONYM_IS_SECTION (self));
  g_return_if_fail (G_IS_OBJECT (item));

  g_list_store_append (self->items, item);
  g_object_unref (item);
}

/**
 * onym_section_get_kind:
 * @self: an OnymSection
 *
 * The kind of the section, which tells a consumer the GObject type of its items.
 *
 * Returns: the section kind
 */
OnymSectionKind
onym_section_get_kind (OnymSection *self)
{
  g_return_val_if_fail (ONYM_IS_SECTION (self), ONYM_SECTION_WORDS);
  return self->kind;
}

/**
 * onym_section_get_title:
 * @self: an OnymSection
 *
 * The section's display title.
 *
 * Returns: (transfer none): the title
 */
const char *
onym_section_get_title (OnymSection *self)
{
  g_return_val_if_fail (ONYM_IS_SECTION (self), NULL);
  return self->title;
}

/**
 * onym_section_get_items:
 * @self: an OnymSection
 *
 * The items in the section. Their type matches the section kind: an OnymDefinition, OnymWord, or
 * OnymAntonym.
 *
 * Returns: (transfer none): the items, a #GListModel whose item type matches the section kind
 */
GListModel *
onym_section_get_items (OnymSection *self)
{
  g_return_val_if_fail (ONYM_IS_SECTION (self), NULL);
  return G_LIST_MODEL (self->items);
}

/* OnymResult */

struct _OnymResult
{
  GObject parent_instance;
  char *term;
  GListStore *sections;
};

G_DEFINE_FINAL_TYPE (OnymResult, onym_result, G_TYPE_OBJECT)

static void
onym_result_finalize (GObject *object)
{
  OnymResult *self = ONYM_RESULT (object);

  g_free (self->term);
  g_clear_object (&self->sections);

  G_OBJECT_CLASS (onym_result_parent_class)->finalize (object);
}

static void
onym_result_class_init (OnymResultClass *klass)
{
  G_OBJECT_CLASS (klass)->finalize = onym_result_finalize;
}

static void
onym_result_init (OnymResult *self)
{
  self->sections = g_list_store_new (ONYM_TYPE_SECTION);
}

OnymResult *
onym_result_new (const char *term)
{
  OnymResult *self = g_object_new (ONYM_TYPE_RESULT, NULL);
  self->term = g_strdup (term);
  return self;
}

void
onym_result_add_section (OnymResult *self, OnymSection *section)
{
  g_return_if_fail (ONYM_IS_RESULT (self));
  g_return_if_fail (ONYM_IS_SECTION (section));

  g_list_store_append (self->sections, section);
  g_object_unref (section);
}

/**
 * onym_result_get_term:
 * @self: an OnymResult
 *
 * The resolved headword, which may differ from the query when WordNet applies morphology.
 *
 * Returns: (transfer none): the headword
 */
const char *
onym_result_get_term (OnymResult *self)
{
  g_return_val_if_fail (ONYM_IS_RESULT (self), NULL);
  return self->term;
}

/**
 * onym_result_get_sections:
 * @self: an OnymResult
 *
 * The ordered sections that make up the entry.
 *
 * Returns: (transfer none): the sections, a #GListModel of #OnymSection
 */
GListModel *
onym_result_get_sections (OnymResult *self)
{
  g_return_val_if_fail (ONYM_IS_RESULT (self), NULL);
  return G_LIST_MODEL (self->sections);
}
