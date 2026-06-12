/* onym-window.c
 *
 * SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* The main window. It owns an OnymEngine and drives lookups from the search field, from word and
 * recent activations, and from history. Search is live: each keystroke updates a completion popover
 * drawn from the WordNet index and, after a short pause, looks the word up silently. Pressing Enter,
 * choosing a completion, or clicking a word or suggestion is a committed lookup that records history
 * and recent words. */

#include "onym-window.h"
#include "onym-result-view.h"

#include <gdk/gdkkeysyms.h>
#include <onym.h>

#define ONYM_RECENT_LIMIT 10
#define ONYM_COMPLETION_MAX 8
#define ONYM_DEBOUNCE_MS 250

/* The most extended words the sidebar lists. Prolific roots like genus extend into the thousands,
 * and a list that long stops being browsable while costing a widget per row, so the queries are
 * capped well past the point anyone scrolls to. */
#define ONYM_EXTENSION_MAX 200

struct _OnymWindow
{
  AdwApplicationWindow parent_instance;

  GtkSearchEntry *search_entry;
  GtkStack *stack;
  OnymResultView *result_view;
  AdwStatusPage *notfound_page;
  GtkMenuButton *menu_button;
  AdwOverlaySplitView *split_view;
  GtkListBox *extensions_list;

  OnymEngine *engine;
  GSettings *settings;
  OnymResult *current_result; /* the result on screen, for re-rendering when a setting changes */

  GPtrArray *history; /* resolved headwords, in visit order */
  int history_pos;    /* index of the current word, or -1 */

  GMenu *recents_menu;
  GSimpleAction *back_action;
  GSimpleAction *forward_action;

  GtkPopover *completion_popover;
  GtkListBox *completion_list;
  guint completion_count;
  guint debounce_id;
  gboolean suppress_changed; /* true while we set the entry text ourselves */

  char *sidebar_term; /* the term the sidebar's word lists were built for */
  guint sidebar_rows; /* loadable word rows in the sidebar, headings not counted */
  gboolean window_is_wide; /* the width breakpoint state, half of the sidebar's visibility rule */
};

G_DEFINE_FINAL_TYPE (OnymWindow, onym_window, ADW_TYPE_APPLICATION_WINDOW)

static void hide_completions (OnymWindow *self);
static void update_sidebar_lists (OnymWindow *self, const char *term);
static void update_sidebar_visibility (OnymWindow *self);

OnymWindow *
onym_window_new (AdwApplication *application)
{
  return g_object_new (ONYM_TYPE_WINDOW, "application", application, NULL);
}

/* Set the search text without retriggering the live search handler. */
static void
set_entry_text (OnymWindow *self, const char *text)
{
  self->suppress_changed = TRUE;
  gtk_editable_set_text (GTK_EDITABLE (self->search_entry), text);
  self->suppress_changed = FALSE;
}

static void
update_history_actions (OnymWindow *self)
{
  g_simple_action_set_enabled (self->back_action, self->history_pos > 0);
  g_simple_action_set_enabled (self->forward_action,
                               self->history_pos >= 0
                                 && self->history_pos < (int) self->history->len - 1);
}

static void
rebuild_recents_menu (OnymWindow *self)
{
  g_menu_remove_all (self->recents_menu);

  char **recents = g_settings_get_strv (self->settings, "recent-words");
  for (guint i = 0; recents[i] != NULL; i++)
    {
      GMenuItem *item = g_menu_item_new (recents[i], NULL);
      g_menu_item_set_action_and_target_value (item, "win.lookup",
                                               g_variant_new_string (recents[i]));
      g_menu_append_item (self->recents_menu, item);
      g_object_unref (item);
    }
  g_strfreev (recents);
}

/* Move @term to the front of the recent words, deduplicated and capped. */
static void
remember_recent (OnymWindow *self, const char *term)
{
  char **current = g_settings_get_strv (self->settings, "recent-words");

  GStrvBuilder *builder = g_strv_builder_new ();
  g_strv_builder_add (builder, term);
  guint count = 1;
  for (guint i = 0; current[i] != NULL && count < ONYM_RECENT_LIMIT; i++)
    {
      if (g_strcmp0 (current[i], term) != 0)
        {
          g_strv_builder_add (builder, current[i]);
          count++;
        }
    }

  GStrv updated = g_strv_builder_end (builder);
  g_settings_set_strv (self->settings, "recent-words", (const char * const *) updated);
  g_strv_builder_unref (builder);
  g_strfreev (updated);
  g_strfreev (current);

  rebuild_recents_menu (self);
}

static void
push_history (OnymWindow *self, const char *term)
{
  if (self->history_pos >= 0
      && g_strcmp0 (g_ptr_array_index (self->history, self->history_pos), term) == 0)
    return;

  while ((int) self->history->len > self->history_pos + 1)
    g_ptr_array_remove_index (self->history, self->history->len - 1);

  g_ptr_array_add (self->history, g_strdup (term));
  self->history_pos = self->history->len - 1;
  update_history_actions (self);
}

static void
display_result (OnymWindow *self, OnymResult *result)
{
  g_set_object (&self->current_result, result);
  onym_result_view_set_tree_expansion (self->result_view,
                                       g_settings_get_enum (self->settings, "tree-expansion"));
  onym_result_view_set_result (self->result_view, result);
  update_sidebar_lists (self, onym_result_get_term (result));
  gtk_stack_set_visible_child_name (self->stack, "result");
}

/* Re-render the result on screen when the tree expansion preference changes. */
static void
on_tree_expansion_changed (GSettings *settings, const char *key, OnymWindow *self)
{
  if (self->current_result == NULL
      || g_strcmp0 (gtk_stack_get_visible_child_name (self->stack), "result") != 0)
    return;

  onym_result_view_set_tree_expansion (self->result_view, g_settings_get_enum (settings, key));
  onym_result_view_set_result (self->result_view, self->current_result);
}

/* Keep the window title in step with what is shown, so the frame announces the current word to a
 * screen reader and labels the window in the shell. */
static void
on_visible_child_changed (GtkStack *stack, GParamSpec *pspec, OnymWindow *self)
{
  if (g_strcmp0 (gtk_stack_get_visible_child_name (stack), "result") == 0
      && self->current_result != NULL)
    gtk_window_set_title (GTK_WINDOW (self), onym_result_get_term (self->current_result));
  else
    gtk_window_set_title (GTK_WINDOW (self), "Onym");
}

/* Build a flow of clickable suggestion chips, each wired to the win.lookup action. */
static GtkWidget *
build_suggestions (char **suggestions)
{
  GtkWidget *flow = g_object_new (GTK_TYPE_FLOW_BOX,
                                  "accessible-role", GTK_ACCESSIBLE_ROLE_LIST, NULL);
  gtk_flow_box_set_selection_mode (GTK_FLOW_BOX (flow), GTK_SELECTION_NONE);
  gtk_widget_set_halign (flow, GTK_ALIGN_CENTER);
  gtk_flow_box_set_column_spacing (GTK_FLOW_BOX (flow), 6);
  gtk_flow_box_set_row_spacing (GTK_FLOW_BOX (flow), 6);

  for (guint i = 0; suggestions[i] != NULL; i++)
    {
      GtkWidget *button = gtk_button_new_with_label (suggestions[i]);
      gtk_widget_add_css_class (button, "flat");
      gtk_widget_add_css_class (button, "word-chip");
      gtk_actionable_set_action_name (GTK_ACTIONABLE (button), "win.lookup");
      gtk_actionable_set_action_target_value (GTK_ACTIONABLE (button),
                                              g_variant_new_string (suggestions[i]));
      gtk_accessible_update_property (GTK_ACCESSIBLE (button),
                                      GTK_ACCESSIBLE_PROPERTY_DESCRIPTION, "Suggestion", -1);

      GtkWidget *cell = g_object_new (GTK_TYPE_FLOW_BOX_CHILD,
                                      "accessible-role", GTK_ACCESSIBLE_ROLE_LIST_ITEM, NULL);
      gtk_widget_set_focusable (cell, FALSE);
      gtk_flow_box_child_set_child (GTK_FLOW_BOX_CHILD (cell), button);
      gtk_flow_box_append (GTK_FLOW_BOX (flow), cell);
    }
  return flow;
}

static void
show_not_found (OnymWindow *self, const char *word)
{
  set_entry_text (self, word);

  char *title = g_strdup_printf ("No entry for \"%s\"", word);
  adw_status_page_set_title (self->notfound_page, title);
  g_free (title);

  char **suggestions = onym_engine_suggest (self->engine, word, 5);
  if (suggestions[0] != NULL)
    {
      adw_status_page_set_description (self->notfound_page, "Did you mean");
      adw_status_page_set_child (self->notfound_page, build_suggestions (suggestions));
    }
  else
    {
      adw_status_page_set_description (self->notfound_page, NULL);
      adw_status_page_set_child (self->notfound_page, NULL);
    }
  g_strfreev (suggestions);

  gtk_stack_set_visible_child_name (self->stack, "notfound");
}

/* Look @word up and show it. @record pushes the resolved headword onto the history and recent words.
 * @show_miss decides whether a miss shows the not-found page or is silently ignored, which is how
 * the live, as-you-type path avoids flashing not-found on partial words. */
static void
show_word (OnymWindow *self, const char *word, gboolean record, gboolean show_miss)
{
  hide_completions (self);

  char *trimmed = g_strstrip (g_strdup (word));
  if (*trimmed == '\0')
    {
      if (show_miss)
        gtk_stack_set_visible_child_name (self->stack, "welcome");
      g_free (trimmed);
      return;
    }

  GError *error = NULL;
  OnymResult *result = onym_engine_lookup (self->engine, trimmed, &error);

  if (error != NULL)
    {
      adw_status_page_set_title (self->notfound_page, "Database not found");
      adw_status_page_set_description (self->notfound_page, error->message);
      adw_status_page_set_child (self->notfound_page, NULL);
      gtk_stack_set_visible_child_name (self->stack, "notfound");
      g_error_free (error);
    }
  else if (result == NULL)
    {
      if (show_miss)
        show_not_found (self, trimmed);
    }
  else
    {
      const char *term = onym_result_get_term (result);
      set_entry_text (self, term);
      display_result (self, result);
      if (record)
        {
          push_history (self, term);
          remember_recent (self, term);
        }
      g_object_unref (result);
    }

  g_free (trimmed);
}

void
onym_window_search (OnymWindow *self, const char *word)
{
  g_return_if_fail (ONYM_IS_WINDOW (self));

  show_word (self, word, TRUE, TRUE);
}

/* More-words sidebar. */

static int
compare_sidebar_terms (gconstpointer a, gconstpointer b)
{
  return g_strcmp0 (*(const char * const *) a, *(const char * const *) b);
}

/* Append a subheading row naming the group that follows it. The row takes no activation and no
 * selection, so keyboard focus skips straight between the words. */
static void
append_sidebar_heading (OnymWindow *self, const char *title)
{
  GtkWidget *label = gtk_label_new (title);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_widget_add_css_class (label, "heading");
  gtk_widget_set_margin_top (label, 10);
  gtk_widget_set_margin_bottom (label, 2);

  GtkWidget *row = gtk_list_box_row_new ();
  gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), label);
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), FALSE);
  gtk_list_box_row_set_selectable (GTK_LIST_BOX_ROW (row), FALSE);
  gtk_list_box_append (self->extensions_list, row);
}

/* Append one loadable word row to the sidebar. */
static void
append_sidebar_word (OnymWindow *self, const char *word)
{
  GtkWidget *label = gtk_label_new (word);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);

  GtkWidget *row = gtk_list_box_row_new ();
  gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), label);
  g_object_set_data_full (G_OBJECT (row), "term", g_strdup (word), g_free);
  gtk_list_box_append (self->extensions_list, row);
  self->sidebar_rows++;
}

/* Append a sorted group of words under @title, skipping adjacent duplicates (the union of two
 * completion queries can carry the same entry twice) and capping the group. */
static void
append_sidebar_group (OnymWindow *self, const char *title, GPtrArray *words)
{
  if (words->len == 0)
    return;
  g_ptr_array_sort (words, compare_sidebar_terms);
  append_sidebar_heading (self, title);

  const char *previous = NULL;
  guint added = 0;
  for (guint i = 0; i < words->len && added < ONYM_EXTENSION_MAX; i++)
    {
      const char *word = g_ptr_array_index (words, i);
      if (g_strcmp0 (word, previous) == 0)
        continue;
      previous = word;
      append_sidebar_word (self, word);
      added++;
    }
}

/* Mark @term's row as the current position when the sidebar lists it, so browsing a family keeps
 * showing where in the list the reading column sits. */
static void
select_sidebar_row (OnymWindow *self, const char *term)
{
  char *folded = g_ascii_strdown (term, -1);
  GtkListBoxRow *row = NULL;

  for (int i = 0; (row = gtk_list_box_get_row_at_index (self->extensions_list, i)) != NULL; i++)
    {
      const char *word = g_object_get_data (G_OBJECT (row), "term");
      if (word != NULL && g_strcmp0 (word, folded) == 0)
        break;
    }
  if (row != NULL)
    gtk_list_box_select_row (self->extensions_list, row);
  else
    gtk_list_box_unselect_all (self->extensions_list);
  g_free (folded);
}

/* Empty the sidebar and forget the term its lists were built for. */
static void
clear_sidebar (OnymWindow *self)
{
  GtkListBoxRow *row;
  while ((row = gtk_list_box_get_row_at_index (self->extensions_list, 0)) != NULL)
    gtk_list_box_remove (self->extensions_list, GTK_WIDGET (row));
  self->sidebar_rows = 0;
  g_clear_pointer (&self->sidebar_term, g_free);
}

/* Whether @folded still belongs to the family the sidebar lists: it begins with the letters of
 * the list's root term, the way every listed word does. */
static gboolean
term_in_sidebar_family (OnymWindow *self, const char *folded)
{
  if (self->sidebar_term == NULL)
    return FALSE;
  char *root = g_ascii_strdown (self->sidebar_term, -1);
  gboolean within = g_str_has_prefix (folded, root);
  g_free (root);
  return within;
}

/* Rebuild the sidebar for @term: first the headwords that extend it by further space-separated
 * words, family into family Acanthaceae, then the other completions of its letters, doc into
 * doctor. Completion is asked for the term and for the term plus a trailing space, since the cap
 * can make either query surface entries the other misses. A term with nothing of its own keeps
 * the previous list while it still belongs to that list's family: the sidebar is the index of the
 * family being browsed, and following a listed word should not tear that index down. A jump
 * somewhere unrelated, the dice or a chip, clears the stale index instead. */
static void
update_sidebar_lists (OnymWindow *self, const char *term)
{
  if (g_strcmp0 (self->sidebar_term, term) == 0)
    {
      select_sidebar_row (self, term);
      return;
    }

  /* Completion returns the lowercased display form, so fold the needle the same way. */
  char *folded = g_ascii_strdown (term, -1);
  char *spaced = g_strconcat (folded, " ", NULL);
  char **on_term = onym_engine_complete (self->engine, folded, ONYM_EXTENSION_MAX);
  char **on_spaced = onym_engine_complete (self->engine, spaced, ONYM_EXTENSION_MAX);

  /* The arrays borrow the strings; both queries stay alive until the rows have copied them. */
  GPtrArray *extensions = g_ptr_array_new ();
  GPtrArray *completions = g_ptr_array_new ();
  for (guint i = 0; on_term[i] != NULL; i++)
    {
      if (g_str_has_prefix (on_term[i], spaced))
        g_ptr_array_add (extensions, on_term[i]);
      else if (g_strcmp0 (on_term[i], folded) != 0)
        g_ptr_array_add (completions, on_term[i]);
    }
  for (guint i = 0; on_spaced[i] != NULL; i++)
    if (g_str_has_prefix (on_spaced[i], spaced))
      g_ptr_array_add (extensions, on_spaced[i]);

  if (extensions->len > 0 || completions->len > 0)
    {
      clear_sidebar (self);
      self->sidebar_term = g_strdup (term);

      append_sidebar_group (self, "Extended words", extensions);
      append_sidebar_group (self, "Completions", completions);
    }
  else if (!term_in_sidebar_family (self, folded))
    {
      clear_sidebar (self);
    }
  select_sidebar_row (self, term);

  g_ptr_array_unref (extensions);
  g_ptr_array_unref (completions);
  g_strfreev (on_term);
  g_strfreev (on_spaced);
  g_free (spaced);
  g_free (folded);
}

/* The sidebar is part of the window's anatomy whenever the window is wide, the way Nautilus
 * treats its own: a word with nothing to list shows the placeholder rather than taking the pane
 * down, so the pane never flickers and the dice button stays reachable. Narrow windows collapse
 * it, and when collapsing would strand keyboard focus inside, the search entry takes it back. */
static void
update_sidebar_visibility (OnymWindow *self)
{
  gboolean show = self->window_is_wide;

  if (!show && adw_overlay_split_view_get_show_sidebar (self->split_view))
    {
      GtkWidget *focus = gtk_window_get_focus (GTK_WINDOW (self));
      GtkWidget *sidebar = adw_overlay_split_view_get_sidebar (self->split_view);
      if (focus != NULL && sidebar != NULL && gtk_widget_is_ancestor (focus, sidebar))
        gtk_widget_grab_focus (GTK_WIDGET (self->search_entry));
    }

  adw_overlay_split_view_set_show_sidebar (self->split_view, show);
}

static void
on_extension_row_activated (GtkListBox *list, GtkListBoxRow *row, OnymWindow *self)
{
  show_word (self, g_object_get_data (G_OBJECT (row), "term"), TRUE, TRUE);
}

/* Completion popover. */

static void
hide_completions (OnymWindow *self)
{
  gtk_popover_popdown (self->completion_popover);
  gtk_accessible_reset_relation (GTK_ACCESSIBLE (self->search_entry),
                                 GTK_ACCESSIBLE_RELATION_ACTIVE_DESCENDANT);
}

static void
accept_completion (OnymWindow *self, const char *word)
{
  hide_completions (self);
  set_entry_text (self, word);
  show_word (self, word, TRUE, TRUE);
}

static void
update_completions (OnymWindow *self)
{
  GtkListBoxRow *row;
  while ((row = gtk_list_box_get_row_at_index (self->completion_list, 0)) != NULL)
    gtk_list_box_remove (self->completion_list, GTK_WIDGET (row));
  self->completion_count = 0;

  char *typed = g_strstrip (g_strdup (gtk_editable_get_text (GTK_EDITABLE (self->search_entry))));
  char **matches = (*typed != '\0') ? onym_engine_complete (self->engine, typed, ONYM_COMPLETION_MAX)
                                     : g_new0 (char *, 1);

  for (guint i = 0; matches[i] != NULL; i++)
    {
      GtkWidget *label = gtk_label_new (matches[i]);
      gtk_label_set_xalign (GTK_LABEL (label), 0.0);
      gtk_widget_set_margin_start (label, 6);
      gtk_widget_set_margin_end (label, 6);

      GtkWidget *list_row = gtk_list_box_row_new ();
      gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (list_row), label);
      g_object_set_data_full (G_OBJECT (list_row), "term", g_strdup (matches[i]), g_free);
      gtk_list_box_append (self->completion_list, list_row);
      self->completion_count++;
    }

  /* Hide the popover when there is nothing to add, or only the exact word the user already typed. */
  gboolean only_exact = self->completion_count == 1 && g_ascii_strcasecmp (matches[0], typed) == 0;
  if (self->completion_count > 0 && !only_exact)
    gtk_popover_popup (self->completion_popover);
  else
    hide_completions (self);

  g_strfreev (matches);
  g_free (typed);
}

static gboolean
on_debounce (gpointer user_data)
{
  OnymWindow *self = user_data;
  self->debounce_id = 0;

  char *typed = g_strstrip (g_strdup (gtk_editable_get_text (GTK_EDITABLE (self->search_entry))));
  if (*typed != '\0')
    {
      OnymResult *result = onym_engine_lookup (self->engine, typed, NULL);
      if (result != NULL)
        {
          display_result (self, result);
          g_object_unref (result);
        }
    }
  g_free (typed);
  return G_SOURCE_REMOVE;
}

static void
on_search_changed (GtkSearchEntry *entry, OnymWindow *self)
{
  if (self->suppress_changed)
    return;

  update_completions (self);

  g_clear_handle_id (&self->debounce_id, g_source_remove);
  self->debounce_id = g_timeout_add (ONYM_DEBOUNCE_MS, on_debounce, self);
}

static void
select_relative (OnymWindow *self, int delta)
{
  if (self->completion_count == 0)
    return;

  GtkListBoxRow *current = gtk_list_box_get_selected_row (self->completion_list);
  int index = current != NULL ? gtk_list_box_row_get_index (current) : (delta > 0 ? -1 : 0);
  int next = CLAMP (index + delta, 0, (int) self->completion_count - 1);

  GtkListBoxRow *row = gtk_list_box_get_row_at_index (self->completion_list, next);
  gtk_list_box_select_row (self->completion_list, row);
  if (row != NULL)
    gtk_accessible_update_relation (GTK_ACCESSIBLE (self->search_entry),
                                    GTK_ACCESSIBLE_RELATION_ACTIVE_DESCENDANT,
                                    GTK_ACCESSIBLE (row), -1);
}

static gboolean
on_entry_key_pressed (GtkEventControllerKey *controller, guint keyval, guint keycode,
                      GdkModifierType state, OnymWindow *self)
{
  if (!gtk_widget_get_mapped (GTK_WIDGET (self->completion_popover)))
    return FALSE;

  switch (keyval)
    {
    case GDK_KEY_Down:
      select_relative (self, 1);
      return TRUE;
    case GDK_KEY_Up:
      select_relative (self, -1);
      return TRUE;
    case GDK_KEY_Escape:
      hide_completions (self);
      return TRUE;
    case GDK_KEY_Return:
    case GDK_KEY_KP_Enter:
      {
        GtkListBoxRow *row = gtk_list_box_get_selected_row (self->completion_list);
        if (row != NULL)
          {
            accept_completion (self, g_object_get_data (G_OBJECT (row), "term"));
            return TRUE;
          }
        return FALSE;
      }
    default:
      return FALSE;
    }
}

static void
on_completion_row_activated (GtkListBox *list, GtkListBoxRow *row, OnymWindow *self)
{
  accept_completion (self, g_object_get_data (G_OBJECT (row), "term"));
}

static void
on_search_activate (GtkSearchEntry *entry, OnymWindow *self)
{
  hide_completions (self);
  show_word (self, gtk_editable_get_text (GTK_EDITABLE (entry)), TRUE, TRUE);
}

static void
on_word_activated (OnymResultView *view, const char *term, OnymWindow *self)
{
  show_word (self, term, TRUE, TRUE);
}

static void
on_lookup_action (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  OnymWindow *self = user_data;
  show_word (self, g_variant_get_string (parameter, NULL), TRUE, TRUE);
}

/* Surprise me: look up a headword picked at random from the whole index. A NULL pick means the
 * database is missing, and the search paths already explain that better than a random miss would. */
static void
on_random_action (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  OnymWindow *self = user_data;
  char *word = onym_engine_random_word (self->engine);
  if (word != NULL)
    show_word (self, word, TRUE, TRUE);
  g_free (word);
}

static void
on_history_back (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  OnymWindow *self = user_data;
  if (self->history_pos > 0)
    {
      self->history_pos--;
      show_word (self, g_ptr_array_index (self->history, self->history_pos), FALSE, TRUE);
      update_history_actions (self);
    }
}

static void
on_history_forward (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  OnymWindow *self = user_data;
  if (self->history_pos < (int) self->history->len - 1)
    {
      self->history_pos++;
      show_word (self, g_ptr_array_index (self->history, self->history_pos), FALSE, TRUE);
      update_history_actions (self);
    }
}

static gboolean
on_close_request (GtkWindow *window, gpointer user_data)
{
  OnymWindow *self = ONYM_WINDOW (window);
  int width, height;

  gtk_window_get_default_size (window, &width, &height);
  g_settings_set_int (self->settings, "window-width", width);
  g_settings_set_int (self->settings, "window-height", height);
  g_settings_set_boolean (self->settings, "window-maximized", gtk_window_is_maximized (window));

  return FALSE;
}

static GSimpleAction *
add_action (OnymWindow *self, const char *name, const GVariantType *parameter_type, GCallback callback)
{
  GSimpleAction *action = g_simple_action_new (name, parameter_type);
  g_signal_connect (action, "activate", callback, self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (action));
  g_object_unref (action);
  return action;
}

static void
build_primary_menu (OnymWindow *self)
{
  self->recents_menu = g_menu_new ();

  GMenu *actions_section = g_menu_new ();
  g_menu_append (actions_section, "Preferences", "app.preferences");
  g_menu_append (actions_section, "About Onym", "app.about");

  GMenu *menu = g_menu_new ();
  g_menu_append_section (menu, "Recent", G_MENU_MODEL (self->recents_menu));
  g_menu_append_section (menu, NULL, G_MENU_MODEL (actions_section));

  gtk_menu_button_set_menu_model (self->menu_button, G_MENU_MODEL (menu));

  g_object_unref (actions_section);
  g_object_unref (menu);
}

static void
build_completion_popover (OnymWindow *self)
{
  self->completion_popover = GTK_POPOVER (gtk_popover_new ());
  gtk_widget_set_parent (GTK_WIDGET (self->completion_popover), GTK_WIDGET (self->search_entry));
  gtk_popover_set_autohide (self->completion_popover, FALSE);
  gtk_popover_set_has_arrow (self->completion_popover, FALSE);
  gtk_popover_set_position (self->completion_popover, GTK_POS_BOTTOM);
  gtk_widget_set_halign (GTK_WIDGET (self->completion_popover), GTK_ALIGN_START);
  gtk_widget_add_css_class (GTK_WIDGET (self->completion_popover), "menu");

  GtkWidget *scroller = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller), GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (scroller), 280);
  gtk_scrolled_window_set_propagate_natural_height (GTK_SCROLLED_WINDOW (scroller), TRUE);
  gtk_widget_set_size_request (scroller, 240, -1);

  self->completion_list = GTK_LIST_BOX (gtk_list_box_new ());
  gtk_list_box_set_selection_mode (self->completion_list, GTK_SELECTION_BROWSE);
  g_signal_connect (self->completion_list, "row-activated",
                    G_CALLBACK (on_completion_row_activated), self);

  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scroller), GTK_WIDGET (self->completion_list));
  gtk_popover_set_child (self->completion_popover, scroller);

  /* Tell assistive technology the field offers completions. */
  gtk_accessible_update_property (GTK_ACCESSIBLE (self->search_entry),
                                  GTK_ACCESSIBLE_PROPERTY_HAS_POPUP, TRUE, -1);
}

static void
restore_window_state (OnymWindow *self)
{
  gtk_window_set_default_size (GTK_WINDOW (self),
                               g_settings_get_int (self->settings, "window-width"),
                               g_settings_get_int (self->settings, "window-height"));

  /* On a first run there is no size of the user's own to honour, so open wide enough for the
   * sidebar when the screen has the room; the schema default stays a modest reading column for
   * small screens. A user value, once saved, always wins. */
  g_autoptr (GVariant) sized = g_settings_get_user_value (self->settings, "window-width");
  if (sized == NULL)
    {
      GListModel *monitors = gdk_display_get_monitors (gtk_widget_get_display (GTK_WIDGET (self)));
      g_autoptr (GdkMonitor) monitor = g_list_model_get_item (monitors, 0);
      if (monitor != NULL)
        {
          GdkRectangle geometry;
          gdk_monitor_get_geometry (monitor, &geometry);
          if (geometry.width >= 1000)
            gtk_window_set_default_size (GTK_WINDOW (self), 900, 640);
        }
    }

  if (g_settings_get_boolean (self->settings, "window-maximized"))
    gtk_window_maximize (GTK_WINDOW (self));
}

static void
on_roomy_apply (AdwBreakpoint *breakpoint, OnymWindow *self)
{
  gtk_widget_add_css_class (GTK_WIDGET (self->result_view), "onym-roomy");
}

static void
on_roomy_unapply (AdwBreakpoint *breakpoint, OnymWindow *self)
{
  gtk_widget_remove_css_class (GTK_WIDGET (self->result_view), "onym-roomy");
}

/* Add a little space between sections once the window is tall enough to spare it. */
static void
setup_spacing_breakpoint (OnymWindow *self)
{
  AdwBreakpoint *breakpoint
    = adw_breakpoint_new (adw_breakpoint_condition_parse ("min-height: 700px"));
  g_signal_connect (breakpoint, "apply", G_CALLBACK (on_roomy_apply), self);
  g_signal_connect (breakpoint, "unapply", G_CALLBACK (on_roomy_unapply), self);
  adw_application_window_add_breakpoint (ADW_APPLICATION_WINDOW (self), breakpoint);
}

static void
on_wide_apply (AdwBreakpoint *breakpoint, OnymWindow *self)
{
  self->window_is_wide = TRUE;
  adw_overlay_split_view_set_collapsed (self->split_view, FALSE);
  update_sidebar_visibility (self);
}

static void
on_wide_unapply (AdwBreakpoint *breakpoint, OnymWindow *self)
{
  self->window_is_wide = FALSE;
  update_sidebar_visibility (self);
  adw_overlay_split_view_set_collapsed (self->split_view, TRUE);
}

/* Collapse the extended words sidebar away when the window cannot spare the width. The split view
 * starts collapsed in the template, so a window opening narrow needs no apply to get there. */
static void
setup_sidebar_breakpoint (OnymWindow *self)
{
  AdwBreakpoint *breakpoint
    = adw_breakpoint_new (adw_breakpoint_condition_parse ("min-width: 700px"));
  g_signal_connect (breakpoint, "apply", G_CALLBACK (on_wide_apply), self);
  g_signal_connect (breakpoint, "unapply", G_CALLBACK (on_wide_unapply), self);
  adw_application_window_add_breakpoint (ADW_APPLICATION_WINDOW (self), breakpoint);
}

static void
onym_window_dispose (GObject *object)
{
  OnymWindow *self = ONYM_WINDOW (object);

  g_clear_handle_id (&self->debounce_id, g_source_remove);
  if (self->completion_popover != NULL)
    gtk_widget_unparent (GTK_WIDGET (g_steal_pointer (&self->completion_popover)));
  g_clear_object (&self->engine);
  g_clear_object (&self->current_result);
  g_clear_object (&self->settings);
  g_clear_object (&self->recents_menu);
  g_clear_pointer (&self->history, g_ptr_array_unref);
  g_clear_pointer (&self->sidebar_term, g_free);

  gtk_widget_dispose_template (GTK_WIDGET (object), ONYM_TYPE_WINDOW);

  G_OBJECT_CLASS (onym_window_parent_class)->dispose (object);
}

static void
onym_window_class_init (OnymWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = onym_window_dispose;

  /* Register the custom widget so the template can instantiate it. */
  g_type_ensure (ONYM_TYPE_RESULT_VIEW);

  gtk_widget_class_set_template_from_resource (widget_class, "/nz/ursa/Onym/onym-window.ui");
  gtk_widget_class_bind_template_child (widget_class, OnymWindow, search_entry);
  gtk_widget_class_bind_template_child (widget_class, OnymWindow, stack);
  gtk_widget_class_bind_template_child (widget_class, OnymWindow, result_view);
  gtk_widget_class_bind_template_child (widget_class, OnymWindow, notfound_page);
  gtk_widget_class_bind_template_child (widget_class, OnymWindow, menu_button);
  gtk_widget_class_bind_template_child (widget_class, OnymWindow, split_view);
  gtk_widget_class_bind_template_child (widget_class, OnymWindow, extensions_list);
}

static void
onym_window_init (OnymWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->engine = onym_engine_new ();
  self->settings = g_settings_new ("nz.ursa.Onym");
  self->history = g_ptr_array_new_with_free_func (g_free);
  self->history_pos = -1;

  restore_window_state (self);
  setup_spacing_breakpoint (self);
  setup_sidebar_breakpoint (self);

  add_action (self, "lookup", G_VARIANT_TYPE_STRING, G_CALLBACK (on_lookup_action));
  add_action (self, "random", NULL, G_CALLBACK (on_random_action));
  self->back_action = add_action (self, "history-back", NULL, G_CALLBACK (on_history_back));
  self->forward_action = add_action (self, "history-forward", NULL, G_CALLBACK (on_history_forward));
  update_history_actions (self);

  build_primary_menu (self);
  rebuild_recents_menu (self);
  build_completion_popover (self);

  GtkEventController *keys = gtk_event_controller_key_new ();
  gtk_event_controller_set_propagation_phase (keys, GTK_PHASE_CAPTURE);
  g_signal_connect (keys, "key-pressed", G_CALLBACK (on_entry_key_pressed), self);
  gtk_widget_add_controller (GTK_WIDGET (self->search_entry), keys);

  g_signal_connect (self->search_entry, "changed", G_CALLBACK (on_search_changed), self);
  g_signal_connect (self->search_entry, "activate", G_CALLBACK (on_search_activate), self);
  g_signal_connect (self->result_view, "word-activated", G_CALLBACK (on_word_activated), self);
  g_signal_connect (self->extensions_list, "row-activated",
                    G_CALLBACK (on_extension_row_activated), self);
  g_signal_connect (self->stack, "notify::visible-child-name",
                    G_CALLBACK (on_visible_child_changed), self);
  g_signal_connect (self->settings, "changed::tree-expansion",
                    G_CALLBACK (on_tree_expansion_changed), self);
  g_signal_connect (self, "close-request", G_CALLBACK (on_close_request), NULL);

  gtk_widget_grab_focus (GTK_WIDGET (self->search_entry));
}
