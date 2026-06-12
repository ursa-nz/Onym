/* onym-application.c
 *
 * SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* The application object. On startup it loads the shared style sheet and installs the actions. On
 * activation it presents the window. */

#include "onym-application.h"
#include "onym-window.h"

struct _OnymApplication
{
  AdwApplication parent_instance;
};

G_DEFINE_FINAL_TYPE (OnymApplication, onym_application, ADW_TYPE_APPLICATION)

OnymApplication *
onym_application_new (void)
{
  return g_object_new (ONYM_TYPE_APPLICATION,
                       "application-id", "nz.ursa.Onym",
                       "flags", G_APPLICATION_HANDLES_COMMAND_LINE,
                       NULL);
}

static void
on_about_action (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  OnymApplication *self = user_data;
  GtkWindow *window = gtk_application_get_active_window (GTK_APPLICATION (self));

  const char *developers[] = { "ursa.nz", NULL };
  const char *wordnet[] = {
    "Princeton University https://wordnet.princeton.edu",
    "Debian WordNet maintainers https://tracker.debian.org/pkg/wordnet",
    NULL,
  };
  const char *artha[] = { "Sundaram Ramaswamy https://artha.sourceforge.net", NULL };
  const char *engine[] = { "onym-engine https://forge.ursa.nz/ursa-nz/onym-engine", NULL };
  const char *gnome[] = {
    "GTK https://gtk.org",
    "libadwaita https://gnome.pages.gitlab.gnome.org/libadwaita/",
    "GLib https://docs.gtk.org/glib/",
    NULL,
  };

  AdwAboutDialog *about = ADW_ABOUT_DIALOG (adw_about_dialog_new ());
  adw_about_dialog_set_application_name (about, "Onym");
  adw_about_dialog_set_application_icon (about, "nz.ursa.Onym");
  adw_about_dialog_set_developer_name (about, "ursa.nz");
  adw_about_dialog_set_version (about, ONYM_VERSION);
  adw_about_dialog_set_license_type (about, GTK_LICENSE_GPL_3_0);
  adw_about_dialog_set_copyright (about, "© 2026 ursa.nz");
  adw_about_dialog_set_website (about, "https://ursa.nz");
  adw_about_dialog_set_issue_url (about, "https://forge.ursa.nz/ursa-nz/Onym/issues");
  /* The engine sentence matches the Android app's About screen so both apps describe onym-engine
   * the same way. The link below it carries the credit to the engine's own repository. */
  adw_about_dialog_add_link (about, "onym-engine", "https://forge.ursa.nz/ursa-nz/onym-engine");
  adw_about_dialog_set_comments (
    about,
    "A thesaurus and dictionary built on WordNet. The lookup engine is onym-engine, a shared "
    "Rust core whose behaviour derives from Artha, an earlier WordNet thesaurus by Sundaram "
    "Ramaswamy, under the GPL.\n\n"
    "Crafted on Kaurna Pangkarra and in Narrm, on Woiwurrung and Boonwurrung Country, with "
    "respect to the Kaurna, Wurundjeri, and Bunurong peoples, their languages, and their "
    "continuing connection to this Country.");
  adw_about_dialog_set_developers (about, developers);
  adw_about_dialog_add_acknowledgement_section (about, "Built on WordNet", wordnet);
  adw_about_dialog_add_acknowledgement_section (about, "Engine derived from Artha", artha);
  adw_about_dialog_add_acknowledgement_section (about, "The shared Rust core", engine);
  adw_about_dialog_add_acknowledgement_section (about, "Made with GNOME", gnome);

  adw_dialog_present (ADW_DIALOG (about), GTK_WIDGET (window));
}

static void
on_quit_action (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  g_application_quit (G_APPLICATION (user_data));
}

static void
on_expansion_selected (AdwComboRow *row, GParamSpec *pspec, gpointer user_data)
{
  GSettings *settings = user_data;
  g_settings_set_enum (settings, "tree-expansion", (int) adw_combo_row_get_selected (row));
}

/* A small preferences dialog: for now, how far relation trees open by default. The choice maps to the
 * tree-expansion setting, and the window re-renders when it changes. */
static void
on_preferences_action (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  OnymApplication *self = user_data;
  GtkWindow *window = gtk_application_get_active_window (GTK_APPLICATION (self));

  const char *options[] = { "Collapsed", "Linear chains", "Everything", NULL };
  AdwComboRow *trees = ADW_COMBO_ROW (adw_combo_row_new ());
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (trees), "Expand relation trees");
  adw_action_row_set_subtitle (ADW_ACTION_ROW (trees),
                               "How the is-a, kinds, and part-of trees open by default");
  GtkStringList *model = gtk_string_list_new (options);
  adw_combo_row_set_model (trees, G_LIST_MODEL (model));
  g_object_unref (model);

  GSettings *settings = g_settings_new ("nz.ursa.Onym");
  adw_combo_row_set_selected (trees, g_settings_get_enum (settings, "tree-expansion"));
  g_signal_connect (trees, "notify::selected", G_CALLBACK (on_expansion_selected), settings);

  AdwPreferencesGroup *group = ADW_PREFERENCES_GROUP (adw_preferences_group_new ());
  adw_preferences_group_set_title (group, "Relations");
  adw_preferences_group_add (group, GTK_WIDGET (trees));

  AdwPreferencesPage *page = ADW_PREFERENCES_PAGE (adw_preferences_page_new ());
  adw_preferences_page_add (page, group);

  AdwPreferencesDialog *dialog = ADW_PREFERENCES_DIALOG (adw_preferences_dialog_new ());
  adw_preferences_dialog_add (dialog, page);
  g_object_set_data_full (G_OBJECT (dialog), "settings", settings, g_object_unref);

  adw_dialog_present (ADW_DIALOG (dialog), GTK_WIDGET (window));
}

static const GActionEntry app_actions[] = {
  { "preferences", on_preferences_action, NULL, NULL, NULL, { 0 } },
  { "about", on_about_action, NULL, NULL, NULL, { 0 } },
  { "quit", on_quit_action, NULL, NULL, NULL, { 0 } },
};

static void
onym_application_startup (GApplication *application)
{
  OnymApplication *self = ONYM_APPLICATION (application);

  G_APPLICATION_CLASS (onym_application_parent_class)->startup (application);

  GtkCssProvider *provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider, "/nz/ursa/Onym/onym.css");
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (provider);

  g_action_map_add_action_entries (G_ACTION_MAP (self), app_actions,
                                   G_N_ELEMENTS (app_actions), self);

  const char *quit_accels[] = { "<primary>q", NULL };
  gtk_application_set_accels_for_action (GTK_APPLICATION (self), "app.quit", quit_accels);

  const char *preferences_accels[] = { "<primary>comma", NULL };
  gtk_application_set_accels_for_action (GTK_APPLICATION (self), "app.preferences",
                                         preferences_accels);

  /* R for random. Onym has nothing to reload, so the shortcut is free, and it keeps the surprise
   * me action reachable when no button for it is on screen. */
  const char *random_accels[] = { "<primary>r", NULL };
  gtk_application_set_accels_for_action (GTK_APPLICATION (self), "win.random", random_accels);
}

static void
onym_application_activate (GApplication *application)
{
  GtkWindow *window = gtk_application_get_active_window (GTK_APPLICATION (application));

  if (window == NULL)
    window = GTK_WINDOW (onym_window_new (ADW_APPLICATION (application)));

  gtk_window_present (window);
}

/* Allow "onym WORD" to open the window already showing that word. With no argument it simply opens
 * the welcome state. A second invocation forwards its word to the running instance. */
static int
onym_application_command_line (GApplication *application, GApplicationCommandLine *command_line)
{
  int argc = 0;
  char **argv = g_application_command_line_get_arguments (command_line, &argc);

  g_application_activate (application);

  if (argc > 1)
    {
      GtkWindow *window = gtk_application_get_active_window (GTK_APPLICATION (application));
      onym_window_search (ONYM_WINDOW (window), argv[1]);
    }

  g_strfreev (argv);
  return 0;
}

static void
onym_application_class_init (OnymApplicationClass *klass)
{
  GApplicationClass *application_class = G_APPLICATION_CLASS (klass);

  application_class->startup = onym_application_startup;
  application_class->activate = onym_application_activate;
  application_class->command_line = onym_application_command_line;
}

static void
onym_application_init (OnymApplication *self)
{
}
