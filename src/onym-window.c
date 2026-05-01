/* onym-window.c
 *
 * SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* The main window. It owns an OnymEngine and runs a lookup when the user presses Enter, activates a
 * word, picks a recent word, or steps through history. It keeps a navigation stack for back and
 * forward, a deduplicated recent words list in GSettings, and it remembers its size. */

#include "onym-window.h"
#include "onym-result-view.h"

#include <onym.h>

#define ONYM_RECENT_LIMIT 10

struct _OnymWindow
{
  AdwApplicationWindow parent_instance;

  GtkSearchEntry *search_entry;
  GtkStack *stack;
  OnymResultView *result_view;
  AdwStatusPage *notfound_page;
  GtkMenuButton *menu_button;

  OnymEngine *engine;
  GSettings *settings;

  GPtrArray *history; /* resolved headwords, in visit order */
  int history_pos;    /* index of the current word, or -1 */

  GMenu *recents_menu;          /* the dynamic section of the primary menu */
  GSimpleAction *back_action;   /* borrowed; owned by the action map */
  GSimpleAction *forward_action;
};

G_DEFINE_FINAL_TYPE (OnymWindow, onym_window, ADW_TYPE_APPLICATION_WINDOW)

OnymWindow *
onym_window_new (AdwApplication *application)
{
  return g_object_new (ONYM_TYPE_WINDOW, "application", application, NULL);
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

/* Look @word up and show the matching page. When @record is true the resolved headword is pushed
 * onto the history and added to the recent words. History navigation passes false. */
static void
show_word (OnymWindow *self, const char *word, gboolean record)
{
  char *trimmed = g_strstrip (g_strdup (word));
  if (*trimmed == '\0')
    {
      gtk_stack_set_visible_child_name (self->stack, "welcome");
      g_free (trimmed);
      return;
    }

  GError *error = NULL;
  OnymResult *result = onym_engine_lookup (self->engine, trimmed, &error);

  if (error != NULL)
    {
      gtk_editable_set_text (GTK_EDITABLE (self->search_entry), trimmed);
      adw_status_page_set_title (self->notfound_page, "Database not found");
      adw_status_page_set_description (self->notfound_page, error->message);
      gtk_stack_set_visible_child_name (self->stack, "notfound");
      g_error_free (error);
    }
  else if (result == NULL)
    {
      gtk_editable_set_text (GTK_EDITABLE (self->search_entry), trimmed);

      char *title = g_strdup_printf ("No entry for \"%s\"", trimmed);
      adw_status_page_set_title (self->notfound_page, title);
      g_free (title);

      char **suggestions = onym_engine_suggest (self->engine, trimmed, 5);
      if (suggestions[0] != NULL)
        {
          char *joined = g_strjoinv (", ", suggestions);
          char *description = g_strdup_printf ("Did you mean: %s", joined);
          adw_status_page_set_description (self->notfound_page, description);
          g_free (description);
          g_free (joined);
        }
      else
        {
          adw_status_page_set_description (self->notfound_page, NULL);
        }
      g_strfreev (suggestions);

      gtk_stack_set_visible_child_name (self->stack, "notfound");
    }
  else
    {
      const char *term = onym_result_get_term (result);
      gtk_editable_set_text (GTK_EDITABLE (self->search_entry), term);
      onym_result_view_set_result (self->result_view, result);
      gtk_stack_set_visible_child_name (self->stack, "result");

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

  show_word (self, word, TRUE);
}

static void
on_search_activate (GtkSearchEntry *entry, OnymWindow *self)
{
  show_word (self, gtk_editable_get_text (GTK_EDITABLE (entry)), TRUE);
}

static void
on_word_activated (OnymResultView *view, const char *term, OnymWindow *self)
{
  show_word (self, term, TRUE);
}

static void
on_lookup_action (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  OnymWindow *self = user_data;
  show_word (self, g_variant_get_string (parameter, NULL), TRUE);
}

static void
on_history_back (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  OnymWindow *self = user_data;
  if (self->history_pos > 0)
    {
      self->history_pos--;
      show_word (self, g_ptr_array_index (self->history, self->history_pos), FALSE);
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
      show_word (self, g_ptr_array_index (self->history, self->history_pos), FALSE);
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

  GMenu *about_section = g_menu_new ();
  g_menu_append (about_section, "About Onym", "app.about");

  GMenu *menu = g_menu_new ();
  g_menu_append_section (menu, "Recent", G_MENU_MODEL (self->recents_menu));
  g_menu_append_section (menu, NULL, G_MENU_MODEL (about_section));

  gtk_menu_button_set_menu_model (self->menu_button, G_MENU_MODEL (menu));

  g_object_unref (about_section);
  g_object_unref (menu);
}

static void
restore_window_state (OnymWindow *self)
{
  gtk_window_set_default_size (GTK_WINDOW (self),
                               g_settings_get_int (self->settings, "window-width"),
                               g_settings_get_int (self->settings, "window-height"));
  if (g_settings_get_boolean (self->settings, "window-maximized"))
    gtk_window_maximize (GTK_WINDOW (self));
}

static void
onym_window_dispose (GObject *object)
{
  OnymWindow *self = ONYM_WINDOW (object);

  g_clear_object (&self->engine);
  g_clear_object (&self->settings);
  g_clear_object (&self->recents_menu);
  g_clear_pointer (&self->history, g_ptr_array_unref);

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

  add_action (self, "lookup", G_VARIANT_TYPE_STRING, G_CALLBACK (on_lookup_action));
  self->back_action = add_action (self, "history-back", NULL, G_CALLBACK (on_history_back));
  self->forward_action = add_action (self, "history-forward", NULL, G_CALLBACK (on_history_forward));
  update_history_actions (self);

  build_primary_menu (self);
  rebuild_recents_menu (self);

  g_signal_connect (self->search_entry, "activate", G_CALLBACK (on_search_activate), self);
  g_signal_connect (self->result_view, "word-activated", G_CALLBACK (on_word_activated), self);
  g_signal_connect (self, "close-request", G_CALLBACK (on_close_request), NULL);

  gtk_widget_grab_focus (GTK_WIDGET (self->search_entry));
}
