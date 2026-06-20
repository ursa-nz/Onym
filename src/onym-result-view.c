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
  int expansion; /* 0 collapsed, 1 linear chains, 2 everything */
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

void
onym_result_view_set_tree_expansion (OnymResultView *self, int mode)
{
  g_return_if_fail (ONYM_IS_RESULT_VIEW (self));
  self->expansion = mode;
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

/* Put the term stashed on @widget onto the clipboard as plain text. One handler serves every chip
 * and the headword because the term travels as widget data rather than per-action state. */
static void
on_copy_activated (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  GtkWidget *widget = GTK_WIDGET (user_data);
  const char *term = g_object_get_data (G_OBJECT (widget), "onym-copy-term");
  gdk_clipboard_set_text (gtk_widget_get_clipboard (widget), term);
}

/* Present the copy menu for @widget, building it on first use so the many chips a lookup creates
 * do not each carry an idle popover. Negative coordinates mean the request came from the keyboard,
 * so the menu points at the widget itself rather than at a click position. */
static void
show_copy_menu (GtkWidget *widget, double x, double y)
{
  GtkPopover *popover = g_object_get_data (G_OBJECT (widget), "onym-copy-menu");
  if (popover == NULL)
    {
      g_autoptr (GMenu) menu = g_menu_new ();
      g_menu_append (menu, "Copy", "onym-term.copy");
      popover = GTK_POPOVER (gtk_popover_menu_new_from_model (G_MENU_MODEL (menu)));
      gtk_widget_set_parent (GTK_WIDGET (popover), widget);
      g_object_set_data (G_OBJECT (widget), "onym-copy-menu", popover);
    }

  if (x >= 0 && y >= 0)
    {
      GdkRectangle anchor = { (int)x, (int)y, 1, 1 };
      gtk_popover_set_pointing_to (popover, &anchor);
    }
  else
    {
      gtk_popover_set_pointing_to (popover, NULL);
    }
  gtk_popover_popup (popover);
}

/* A secondary click opens the copy menu at the pointer. The sequence is claimed so the press does
 * not also reach the chip's activation path. */
static void
on_copy_menu_pressed (GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data)
{
  GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
  show_copy_menu (widget, x, y);
}

/* Shift+F10 and the Menu key open the same copy menu, so keyboard users get the same path that
 * pointer users get. */
static gboolean
on_copy_menu_shortcut (GtkWidget *widget, GVariant *args, gpointer user_data)
{
  show_copy_menu (widget, -1, -1);
  return TRUE;
}

/* A popover parented with gtk_widget_set_parent is not unparented for us, and GTK warns if a
 * widget is finalised with children left, so drop the menu when its owner is destroyed. Chips are
 * recreated on every lookup, which makes this the path that keeps rebuilds leak free. */
static void
on_copy_menu_owner_destroy (GtkWidget *widget, gpointer user_data)
{
  GtkWidget *popover = g_object_get_data (G_OBJECT (widget), "onym-copy-menu");
  if (popover != NULL)
    {
      gtk_widget_unparent (popover);
      g_object_set_data (G_OBJECT (widget), "onym-copy-menu", NULL);
    }
}

/* Give @widget a context menu with one Copy item that puts @term on the clipboard as plain text.
 * Wires a secondary-click gesture plus the standard context-menu keys, and scopes a per-widget
 * action group so each menu copies its own term. Shared by the chips and the headword so the
 * wiring lives in one place. */
static void
attach_copy_menu (GtkWidget *widget, const char *term)
{
  g_object_set_data_full (G_OBJECT (widget), "onym-copy-term", g_strdup (term), g_free);

  g_autoptr (GSimpleActionGroup) group = g_simple_action_group_new ();
  g_autoptr (GSimpleAction) copy = g_simple_action_new ("copy", NULL);
  g_signal_connect (copy, "activate", G_CALLBACK (on_copy_activated), widget);
  g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (copy));
  gtk_widget_insert_action_group (widget, "onym-term", G_ACTION_GROUP (group));

  GtkGesture *gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), GDK_BUTTON_SECONDARY);
  g_signal_connect (gesture, "pressed", G_CALLBACK (on_copy_menu_pressed), NULL);
  gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (gesture));

  GtkShortcutTrigger *trigger
      = gtk_alternative_trigger_new (gtk_keyval_trigger_new (GDK_KEY_F10, GDK_SHIFT_MASK),
                                     gtk_keyval_trigger_new (GDK_KEY_Menu, 0));
  GtkEventController *shortcuts = gtk_shortcut_controller_new ();
  gtk_shortcut_controller_add_shortcut (
      GTK_SHORTCUT_CONTROLLER (shortcuts),
      gtk_shortcut_new (trigger, gtk_callback_action_new (on_copy_menu_shortcut, NULL, NULL)));
  gtk_widget_add_controller (widget, shortcuts);

  g_signal_connect (widget, "destroy", G_CALLBACK (on_copy_menu_owner_destroy), NULL);
}

/* A clickable term. @relation is what the term is to the headword, set as the chip's accessible
 * description so a screen reader reads, for example, "felicitous, synonym". */
static GtkWidget *
make_chip (OnymResultView *self, const char *term, const char *relation)
{
  GtkWidget *button = gtk_button_new_with_label (term);
  gtk_widget_add_css_class (button, "flat");
  gtk_widget_add_css_class (button, "word-chip");
  g_object_set_data_full (G_OBJECT (button), "onym-term", g_strdup (term), g_free);
  g_signal_connect (button, "clicked", G_CALLBACK (on_chip_clicked), self);
  attach_copy_menu (button, term);
  if (relation != NULL)
    gtk_accessible_update_property (GTK_ACCESSIBLE (button),
                                    GTK_ACCESSIBLE_PROPERTY_DESCRIPTION, relation, -1);
  return button;
}

/* Append @chip to @flow inside a cell of the given role. A flow box exposes as a grid by default,
 * which a screen reader reads as a table; rolling the flow and its cells ourselves keeps the
 * announcement accurate and quiet. */
static void
flow_append (GtkFlowBox *flow, GtkWidget *chip, GtkAccessibleRole cell_role)
{
  GtkWidget *cell = g_object_new (GTK_TYPE_FLOW_BOX_CHILD, "accessible-role", cell_role, NULL);
  gtk_widget_set_focusable (cell, FALSE);
  gtk_flow_box_child_set_child (GTK_FLOW_BOX_CHILD (cell), chip);
  gtk_flow_box_append (flow, GTK_WIDGET (cell));
}

static GtkWidget *
make_heading (const char *title)
{
  GtkWidget *label = g_object_new (GTK_TYPE_LABEL,
                                   "label", title,
                                   "xalign", 0.0,
                                   "accessible-role", GTK_ACCESSIBLE_ROLE_HEADING,
                                   NULL);
  gtk_widget_add_css_class (label, "heading");
  gtk_widget_add_css_class (label, "onym-section-heading");
  gtk_accessible_update_property (GTK_ACCESSIBLE (label),
                                  GTK_ACCESSIBLE_PROPERTY_LEVEL, 2, -1);
  return label;
}

static GtkWidget *
make_chip_flow (OnymResultView *self, OnymSection *section)
{
  GtkFlowBox *flow = GTK_FLOW_BOX (g_object_new (GTK_TYPE_FLOW_BOX,
                                                 "accessible-role", GTK_ACCESSIBLE_ROLE_LIST, NULL));
  gtk_flow_box_set_selection_mode (flow, GTK_SELECTION_NONE);
  gtk_flow_box_set_homogeneous (flow, FALSE);
  gtk_flow_box_set_column_spacing (flow, 6);
  gtk_flow_box_set_row_spacing (flow, 6);
  gtk_flow_box_set_max_children_per_line (flow, 1000);

  GListModel *items = onym_section_get_items (section);
  gboolean antonyms = onym_section_get_kind (section) == ONYM_SECTION_ANTONYMS;
  const char *title = onym_section_get_title (section);
  guint n = g_list_model_get_n_items (items);
  for (guint i = 0; i < n; i++)
    {
      gpointer item = g_list_model_get_item (items, i);
      const char *term;
      const char *relation;
      if (antonyms)
        {
          term = onym_antonym_get_term (item);
          relation = onym_antonym_get_direct (item) ? "Direct antonym" : "Indirect antonym";
        }
      else
        {
          term = onym_word_get_term (item);
          relation = title;
        }
      flow_append (flow, make_chip (self, term, relation), GTK_ACCESSIBLE_ROLE_LIST_ITEM);
      g_object_unref (item);
    }
  return GTK_WIDGET (flow);
}

static GtkWidget *
make_definitions (GListModel *items)
{
  GtkBox *box = GTK_BOX (g_object_new (GTK_TYPE_BOX,
                                       "orientation", GTK_ORIENTATION_VERTICAL,
                                       "spacing", 12,
                                       "accessible-role", GTK_ACCESSIBLE_ROLE_LIST,
                                       NULL));

  guint n = g_list_model_get_n_items (items);
  for (guint i = 0; i < n; i++)
    {
      OnymDefinition *def = g_list_model_get_item (items, i);
      GtkBox *row = GTK_BOX (g_object_new (GTK_TYPE_BOX,
                                           "orientation", GTK_ORIENTATION_VERTICAL,
                                           "spacing", 3,
                                           "accessible-role", GTK_ACCESSIBLE_ROLE_LIST_ITEM,
                                           NULL));

      GtkWidget *gloss = gtk_label_new (NULL);
      gtk_label_set_wrap (GTK_LABEL (gloss), TRUE);
      gtk_label_set_xalign (GTK_LABEL (gloss), 0.0);
      gtk_label_set_selectable (GTK_LABEL (gloss), TRUE);

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
          gtk_label_set_selectable (GTK_LABEL (example), TRUE);
          gtk_widget_set_margin_start (example, 18);
          gtk_widget_add_css_class (example, "dim-label");

          char *example_esc = g_markup_escape_text (examples[e], -1);
          char *example_markup = g_strdup_printf ("<i>\"%s\"</i>", example_esc);
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

/* Etymology paragraphs: plain, wrapped, selectable prose rather than chips, because the text is a
 * sentence to read and copy, not a term to navigate to. Each OnymWord item holds one paragraph. A
 * lone etymology reads as one paragraph; when a word has several distinct origins, each gets a
 * bullet so they group as a list instead of floating as disconnected sentences. */
static GtkWidget *
make_prose (GListModel *items)
{
  GtkBox *box = GTK_BOX (g_object_new (GTK_TYPE_BOX,
                                       "orientation", GTK_ORIENTATION_VERTICAL,
                                       "spacing", 8,
                                       "accessible-role", GTK_ACCESSIBLE_ROLE_LIST,
                                       NULL));
  guint n = g_list_model_get_n_items (items);
  for (guint i = 0; i < n; i++)
    {
      OnymWord *paragraph = g_list_model_get_item (items, i);
      GtkWidget *label = g_object_new (GTK_TYPE_LABEL,
                                       "label", onym_word_get_term (paragraph),
                                       "wrap", TRUE,
                                       "xalign", 0.0,
                                       "hexpand", TRUE,
                                       "selectable", TRUE,
                                       "accessible-role", GTK_ACCESSIBLE_ROLE_LIST_ITEM,
                                       NULL);
      if (n == 1)
        {
          gtk_box_append (box, label);
        }
      else
        {
          GtkWidget *row = g_object_new (GTK_TYPE_BOX,
                                         "orientation", GTK_ORIENTATION_HORIZONTAL,
                                         "spacing", 8,
                                         "accessible-role", GTK_ACCESSIBLE_ROLE_LIST_ITEM,
                                         NULL);
          GtkWidget *bullet = gtk_label_new ("•");
          gtk_widget_set_valign (bullet, GTK_ALIGN_START);
          gtk_widget_add_css_class (bullet, "dim-label");
          gtk_box_append (GTK_BOX (row), bullet);
          gtk_box_append (GTK_BOX (row), label);
          gtk_box_append (box, row);
        }
      g_object_unref (paragraph);
    }
  return GTK_WIDGET (box);
}

/* A node's synset terms, each as a clickable chip. The relation is carried by the enclosing tree
 * item, so the chips themselves stay unadorned and a screen reader does not repeat it per term. */
static GtkWidget *
make_tree_terms (OnymResultView *self, OnymTreeNode *node)
{
  GtkFlowBox *flow = GTK_FLOW_BOX (g_object_new (GTK_TYPE_FLOW_BOX,
                                                 "accessible-role", GTK_ACCESSIBLE_ROLE_GENERIC,
                                                 NULL));
  gtk_flow_box_set_selection_mode (flow, GTK_SELECTION_NONE);
  gtk_flow_box_set_column_spacing (flow, 4);
  gtk_flow_box_set_row_spacing (flow, 4);
  gtk_flow_box_set_max_children_per_line (flow, 1000);
  gtk_widget_set_hexpand (GTK_WIDGET (flow), TRUE);

  const char * const *terms = onym_tree_node_get_terms (node);
  for (guint i = 0; terms != NULL && terms[i] != NULL; i++)
    flow_append (flow, make_chip (self, terms[i], NULL), GTK_ACCESSIBLE_ROLE_GENERIC);

  return GTK_WIDGET (flow);
}

/* Swap the disclosure arrow and update the node's expanded state for assistive technology. */
static void
on_disclosure_toggled (GtkToggleButton *toggle, GtkWidget *tree_item)
{
  gboolean active = gtk_toggle_button_get_active (toggle);
  gtk_button_set_icon_name (GTK_BUTTON (toggle),
                            active ? "pan-down-symbolic" : "pan-end-symbolic");
  gtk_accessible_update_state (GTK_ACCESSIBLE (tree_item),
                               GTK_ACCESSIBLE_STATE_EXPANDED, active, -1);
}

/* One node of a relation tree, exposed as a tree item that reports its depth so a screen reader can
 * place it in the hierarchy. A node with children gets a disclosure beside its chips and a revealer
 * holding its indented children, which form a group; the disclosure carries the node's expanded
 * state and the group it controls. A leaf is just its chips. A GtkRevealer is used rather than a
 * GtkExpander because it allocates and propagates its child's height reliably as the chips reflow,
 * which a nested GtkExpander does not. The default open state follows the expansion mode: nothing,
 * linear chains only, or everything. */
static GtkWidget *
make_tree_node (OnymResultView *self, OnymTreeNode *node, guint level)
{
  GListModel *children = onym_tree_node_get_children (node);
  guint n = g_list_model_get_n_items (children);

  GtkWidget *node_box = g_object_new (GTK_TYPE_BOX,
                                      "orientation", GTK_ORIENTATION_VERTICAL,
                                      "spacing", 4,
                                      "accessible-role", GTK_ACCESSIBLE_ROLE_TREE_ITEM,
                                      NULL);
  gtk_accessible_update_property (GTK_ACCESSIBLE (node_box),
                                  GTK_ACCESSIBLE_PROPERTY_LEVEL, (int) level,
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, onym_tree_node_get_label (node), -1);

  GtkWidget *terms = make_tree_terms (self, node);
  if (n == 0)
    {
      gtk_box_append (GTK_BOX (node_box), terms);
      return node_box;
    }

  GtkWidget *header = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  GtkWidget *toggle = gtk_toggle_button_new ();
  gtk_button_set_icon_name (GTK_BUTTON (toggle), "pan-end-symbolic");
  gtk_widget_add_css_class (toggle, "flat");
  gtk_widget_add_css_class (toggle, "onym-disclosure");
  gtk_widget_set_valign (toggle, GTK_ALIGN_START);
  gtk_accessible_update_property (GTK_ACCESSIBLE (toggle),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, "Toggle nested terms", -1);
  gtk_box_append (GTK_BOX (header), toggle);
  gtk_box_append (GTK_BOX (header), terms);
  gtk_box_append (GTK_BOX (node_box), header);

  GtkWidget *child_box = g_object_new (GTK_TYPE_BOX,
                                       "orientation", GTK_ORIENTATION_VERTICAL,
                                       "spacing", 4,
                                       "accessible-role", GTK_ACCESSIBLE_ROLE_GROUP,
                                       NULL);
  gtk_widget_add_css_class (child_box, "onym-tree-children");
  for (guint i = 0; i < n; i++)
    {
      OnymTreeNode *child = g_list_model_get_item (children, i);
      gtk_box_append (GTK_BOX (child_box), make_tree_node (self, child, level + 1));
      g_object_unref (child);
    }

  GtkWidget *revealer = gtk_revealer_new ();
  gtk_revealer_set_transition_type (GTK_REVEALER (revealer),
                                    GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
  gtk_revealer_set_child (GTK_REVEALER (revealer), child_box);
  gtk_box_append (GTK_BOX (node_box), revealer);

  g_object_bind_property (toggle, "active", revealer, "reveal-child",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_accessible_update_relation (GTK_ACCESSIBLE (toggle), GTK_ACCESSIBLE_RELATION_CONTROLS,
                                  GTK_ACCESSIBLE (child_box), NULL, -1);
  g_signal_connect (toggle, "toggled", G_CALLBACK (on_disclosure_toggled), node_box);

  gboolean open = self->expansion == 2 || (self->expansion == 1 && n == 1);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), open);
  gtk_button_set_icon_name (GTK_BUTTON (toggle), open ? "pan-down-symbolic" : "pan-end-symbolic");
  gtk_accessible_update_state (GTK_ACCESSIBLE (node_box),
                               GTK_ACCESSIBLE_STATE_EXPANDED, open, -1);

  return node_box;
}

static GtkWidget *
make_tree (OnymResultView *self, OnymSection *section)
{
  GtkBox *box = GTK_BOX (g_object_new (GTK_TYPE_BOX,
                                       "orientation", GTK_ORIENTATION_VERTICAL,
                                       "spacing", 6,
                                       "accessible-role", GTK_ACCESSIBLE_ROLE_TREE,
                                       NULL));

  GListModel *items = onym_section_get_items (section);
  guint n = g_list_model_get_n_items (items);
  for (guint i = 0; i < n; i++)
    {
      OnymTreeNode *root = g_list_model_get_item (items, i);
      gtk_box_append (box, make_tree_node (self, root, 1));
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

  GtkWidget *headword = g_object_new (GTK_TYPE_LABEL,
                                      "label", onym_result_get_term (result),
                                      "xalign", 0.0,
                                      "accessible-role", GTK_ACCESSIBLE_ROLE_HEADING,
                                      NULL);
  gtk_widget_add_css_class (headword, "title-1");
  gtk_widget_set_margin_bottom (headword, 6);
  gtk_accessible_update_property (GTK_ACCESSIBLE (headword),
                                  GTK_ACCESSIBLE_PROPERTY_LEVEL, 1, -1);

  /* The headword offers the same copy menu as the chips. A label is not focusable by default, so
   * make it so; without focus the context-menu keys could never reach the title. It stays a level
   * one heading, so the reading order a screen reader presents is unchanged. */
  gtk_widget_set_focusable (headword, TRUE);
  attach_copy_menu (headword, onym_result_get_term (result));
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
        case ONYM_SECTION_ETYMOLOGY:
          gtk_box_append (self->box, make_prose (onym_section_get_items (section)));
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
  self->expansion = 1;
}
