/* onym-result-view.c
 *
 * SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* Renders an OnymResult into a reading column: the headword, then each section. Definitions are
 * numbered lines with a dimmed part of speech and indented examples. Synonyms and antonyms are
 * wrapped chips. Clicking a chip emits "word-activated" so the window can look that word up. */

#include "onym-result-view.h"

struct _OnymResultView
{
  GtkWidget parent_instance;
  GtkBox *box;
};

G_DEFINE_FINAL_TYPE (OnymResultView, onym_result_view, GTK_TYPE_WIDGET)

enum
{
  WORD_ACTIVATED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

GtkWidget *
onym_result_view_new (void)
{
  return g_object_new (ONYM_TYPE_RESULT_VIEW, NULL);
}

static void
clear_box (OnymResultView *self)
{
  GtkWidget *child;
  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self->box))) != NULL)
    gtk_box_remove (self->box, child);
}

static void
on_chip_clicked (GtkButton *button, OnymResultView *self)
{
  const char *term = g_object_get_data (G_OBJECT (button), "onym-term");
  g_signal_emit (self, signals[WORD_ACTIVATED], 0, term);
}

static GtkWidget *
make_chip (OnymResultView *self, const char *term)
{
  GtkWidget *button = gtk_button_new_with_label (term);
  gtk_widget_add_css_class (button, "flat");
  gtk_widget_add_css_class (button, "word-chip");
  g_object_set_data_full (G_OBJECT (button), "onym-term", g_strdup (term), g_free);
  g_signal_connect (button, "clicked", G_CALLBACK (on_chip_clicked), self);
  return button;
}

static GtkWidget *
make_heading (const char *title)
{
  GtkWidget *label = gtk_label_new (title);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_widget_add_css_class (label, "heading");
  gtk_widget_add_css_class (label, "onym-section-heading");
  return label;
}

static GtkWidget *
make_chip_flow (OnymResultView *self, OnymSection *section)
{
  GtkFlowBox *flow = GTK_FLOW_BOX (gtk_flow_box_new ());
  gtk_flow_box_set_selection_mode (flow, GTK_SELECTION_NONE);
  gtk_flow_box_set_homogeneous (flow, FALSE);
  gtk_flow_box_set_column_spacing (flow, 6);
  gtk_flow_box_set_row_spacing (flow, 6);
  gtk_flow_box_set_max_children_per_line (flow, 1000);

  GListModel *items = onym_section_get_items (section);
  gboolean antonyms = onym_section_get_kind (section) == ONYM_SECTION_ANTONYMS;
  guint n = g_list_model_get_n_items (items);
  for (guint i = 0; i < n; i++)
    {
      gpointer item = g_list_model_get_item (items, i);
      const char *term = antonyms ? onym_antonym_get_term (item) : onym_word_get_term (item);
      gtk_flow_box_append (flow, make_chip (self, term));
      g_object_unref (item);
    }
  return GTK_WIDGET (flow);
}

static GtkWidget *
make_definitions (GListModel *items)
{
  GtkBox *box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 12));

  guint n = g_list_model_get_n_items (items);
  for (guint i = 0; i < n; i++)
    {
      OnymDefinition *def = g_list_model_get_item (items, i);
      GtkBox *row = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 3));

      GtkWidget *gloss = gtk_label_new (NULL);
      gtk_label_set_wrap (GTK_LABEL (gloss), TRUE);
      gtk_label_set_xalign (GTK_LABEL (gloss), 0.0);

      const char *pos = onym_definition_get_pos (def);
      char *gloss_esc = g_markup_escape_text (onym_definition_get_gloss (def), -1);
      char *markup;
      if (pos != NULL)
        {
          char *pos_esc = g_markup_escape_text (pos, -1);
          markup = g_strdup_printf ("%u.  <span alpha='60%%'>%s</span>  %s", i + 1, pos_esc, gloss_esc);
          g_free (pos_esc);
        }
      else
        {
          markup = g_strdup_printf ("%u.  %s", i + 1, gloss_esc);
        }
      gtk_label_set_markup (GTK_LABEL (gloss), markup);
      g_free (markup);
      g_free (gloss_esc);
      gtk_box_append (row, gloss);

      const char * const *examples = onym_definition_get_examples (def);
      for (guint e = 0; examples != NULL && examples[e] != NULL; e++)
        {
          GtkWidget *example = gtk_label_new (NULL);
          gtk_label_set_wrap (GTK_LABEL (example), TRUE);
          gtk_label_set_xalign (GTK_LABEL (example), 0.0);
          gtk_widget_set_margin_start (example, 18);

          char *example_esc = g_markup_escape_text (examples[e], -1);
          char *example_markup = g_strdup_printf ("<span alpha='55%%'><i>\"%s\"</i></span>", example_esc);
          gtk_label_set_markup (GTK_LABEL (example), example_markup);
          g_free (example_markup);
          g_free (example_esc);
          gtk_box_append (row, example);
        }

      gtk_box_append (box, GTK_WIDGET (row));
      g_object_unref (def);
    }
  return GTK_WIDGET (box);
}

/* A node's synset terms, each as a clickable chip, so any related word can be looked up. */
static GtkWidget *
make_tree_terms (OnymResultView *self, OnymTreeNode *node)
{
  GtkFlowBox *flow = GTK_FLOW_BOX (gtk_flow_box_new ());
  gtk_flow_box_set_selection_mode (flow, GTK_SELECTION_NONE);
  gtk_flow_box_set_column_spacing (flow, 4);
  gtk_flow_box_set_row_spacing (flow, 4);
  gtk_flow_box_set_max_children_per_line (flow, 1000);
  gtk_widget_set_halign (GTK_WIDGET (flow), GTK_ALIGN_START);

  const char * const *terms = onym_tree_node_get_terms (node);
  for (guint i = 0; terms != NULL && terms[i] != NULL; i++)
    gtk_flow_box_append (flow, make_chip (self, terms[i]));

  return GTK_WIDGET (flow);
}

/* One node of a relation tree. A node with children becomes a collapsible expander whose chips form
 * the header and whose children are indented with a guide line; a leaf is just its chips. A node is
 * expanded by default when it has a single child, so a linear is-a chain is visible at once while a
 * branchy node such as a list of kinds stays tidy. Clicking a chip looks that word up; clicking the
 * triangle expands. */
static GtkWidget *
make_tree_node (OnymResultView *self, OnymTreeNode *node)
{
  GListModel *children = onym_tree_node_get_children (node);
  guint n = g_list_model_get_n_items (children);

  GtkWidget *terms = make_tree_terms (self, node);
  if (n == 0)
    return terms;

  GtkWidget *expander = gtk_expander_new (NULL);
  gtk_expander_set_label_widget (GTK_EXPANDER (expander), terms);
  gtk_expander_set_expanded (GTK_EXPANDER (expander), n == 1);

  GtkWidget *child_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  gtk_widget_add_css_class (child_box, "onym-tree-children");
  for (guint i = 0; i < n; i++)
    {
      OnymTreeNode *child = g_list_model_get_item (children, i);
      gtk_box_append (GTK_BOX (child_box), make_tree_node (self, child));
      g_object_unref (child);
    }
  gtk_expander_set_child (GTK_EXPANDER (expander), child_box);

  return expander;
}

static GtkWidget *
make_tree (OnymResultView *self, OnymSection *section)
{
  GtkBox *box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 6));

  GListModel *items = onym_section_get_items (section);
  guint n = g_list_model_get_n_items (items);
  for (guint i = 0; i < n; i++)
    {
      OnymTreeNode *root = g_list_model_get_item (items, i);
      gtk_box_append (box, make_tree_node (self, root));
      g_object_unref (root);
    }
  return GTK_WIDGET (box);
}

void
onym_result_view_set_result (OnymResultView *self, OnymResult *result)
{
  g_return_if_fail (ONYM_IS_RESULT_VIEW (self));

  clear_box (self);
  if (result == NULL)
    return;

  GtkWidget *headword = gtk_label_new (onym_result_get_term (result));
  gtk_label_set_xalign (GTK_LABEL (headword), 0.0);
  gtk_widget_add_css_class (headword, "title-1");
  gtk_widget_set_margin_bottom (headword, 6);
  gtk_box_append (self->box, headword);

  GListModel *sections = onym_result_get_sections (result);
  guint n = g_list_model_get_n_items (sections);
  for (guint i = 0; i < n; i++)
    {
      OnymSection *section = g_list_model_get_item (sections, i);
      gtk_box_append (self->box, make_heading (onym_section_get_title (section)));

      switch (onym_section_get_kind (section))
        {
        case ONYM_SECTION_DEFINITIONS:
          gtk_box_append (self->box, make_definitions (onym_section_get_items (section)));
          break;
        case ONYM_SECTION_WORDS:
        case ONYM_SECTION_ANTONYMS:
          gtk_box_append (self->box, make_chip_flow (self, section));
          break;
        case ONYM_SECTION_TREE:
          gtk_box_append (self->box, make_tree (self, section));
          break;
        default:
          break;
        }
      g_object_unref (section);
    }
}

static void
onym_result_view_dispose (GObject *object)
{
  OnymResultView *self = ONYM_RESULT_VIEW (object);

  if (self->box != NULL)
    {
      gtk_widget_unparent (GTK_WIDGET (self->box));
      self->box = NULL;
    }

  G_OBJECT_CLASS (onym_result_view_parent_class)->dispose (object);
}

static void
onym_result_view_class_init (OnymResultViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = onym_result_view_dispose;

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);

  /**
   * OnymResultView::word-activated:
   * @self: the view
   * @term: the activated term
   *
   * Emitted when a synonym or antonym chip is clicked, so the word can be looked up.
   */
  signals[WORD_ACTIVATED] = g_signal_new ("word-activated", G_TYPE_FROM_CLASS (klass),
                                          G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL,
                                          G_TYPE_NONE, 1, G_TYPE_STRING);
}

static void
onym_result_view_init (OnymResultView *self)
{
  self->box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 6));
  gtk_widget_set_parent (GTK_WIDGET (self->box), GTK_WIDGET (self));
}
