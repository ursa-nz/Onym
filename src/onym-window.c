/* onym-window.c
 *
 * SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* The main window. It owns an OnymEngine, runs a lookup when the user presses Enter or activates a
 * word, and shows one of three pages: a welcome state, a not found state, or the result. */

#include "onym-window.h"
#include "onym-result-view.h"

#include <onym.h>

struct _OnymWindow
{
  AdwApplicationWindow parent_instance;

  GtkSearchEntry *search_entry;
  GtkStack *stack;
  OnymResultView *result_view;
  AdwStatusPage *notfound_page;

  OnymEngine *engine;
};

G_DEFINE_FINAL_TYPE (OnymWindow, onym_window, ADW_TYPE_APPLICATION_WINDOW)

OnymWindow *
onym_window_new (AdwApplication *application)
{
  return g_object_new (ONYM_TYPE_WINDOW, "application", application, NULL);
}

static void
show_not_found (OnymWindow *self, const char *word)
{
  char *title = g_strdup_printf ("No entry for \"%s\"", word);
  adw_status_page_set_title (self->notfound_page, title);
  g_free (title);

  char **suggestions = onym_engine_suggest (self->engine, word, 5);
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

static void
onym_window_lookup (OnymWindow *self, const char *word)
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
      adw_status_page_set_title (self->notfound_page, "Database not found");
      adw_status_page_set_description (self->notfound_page, error->message);
      gtk_stack_set_visible_child_name (self->stack, "notfound");
      g_error_free (error);
    }
  else if (result == NULL)
    {
      show_not_found (self, trimmed);
    }
  else
    {
      onym_result_view_set_result (self->result_view, result);
      gtk_stack_set_visible_child_name (self->stack, "result");
      g_object_unref (result);
    }

  g_free (trimmed);
}

void
onym_window_search (OnymWindow *self, const char *word)
{
  g_return_if_fail (ONYM_IS_WINDOW (self));

  gtk_editable_set_text (GTK_EDITABLE (self->search_entry), word);
  onym_window_lookup (self, word);
}

static void
on_search_activate (GtkSearchEntry *entry, OnymWindow *self)
{
  onym_window_lookup (self, gtk_editable_get_text (GTK_EDITABLE (entry)));
}

static void
on_word_activated (OnymResultView *view, const char *term, OnymWindow *self)
{
  gtk_editable_set_text (GTK_EDITABLE (self->search_entry), term);
  onym_window_lookup (self, term);
}

static void
onym_window_dispose (GObject *object)
{
  OnymWindow *self = ONYM_WINDOW (object);

  g_clear_object (&self->engine);
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
}

static void
onym_window_init (OnymWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->engine = onym_engine_new ();

  g_signal_connect (self->search_entry, "activate", G_CALLBACK (on_search_activate), self);
  g_signal_connect (self->result_view, "word-activated", G_CALLBACK (on_word_activated), self);

  gtk_widget_grab_focus (GTK_WIDGET (self->search_entry));
}
