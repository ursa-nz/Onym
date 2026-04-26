/* test-index.c
 *
 * SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* Unit tests for the pure string helpers behind completion and suggestions. These need no WordNet
 * data, so they always run. */

#include "wn-index.h"

#include <glib.h>

static void
test_edit_distance (void)
{
  g_assert_cmpuint (onym_edit_distance ("colour", "color"), ==, 1);
  g_assert_cmpuint (onym_edit_distance ("kitten", "sitting"), ==, 3);
  g_assert_cmpuint (onym_edit_distance ("abc", "abc"), ==, 0);
  g_assert_cmpuint (onym_edit_distance ("", "abc"), ==, 3);
  g_assert_cmpuint (onym_edit_distance ("abc", ""), ==, 3);
  g_assert_cmpuint (onym_edit_distance ("flaw", "lawn"), ==, 2);
}

static void
test_to_display (void)
{
  char *display = onym_term_to_display ("ice_cream");
  g_assert_cmpstr (display, ==, "ice cream");
  g_free (display);

  g_assert_null (onym_term_to_display (NULL));
}

static void
test_to_query (void)
{
  char *query = onym_term_to_query ("  ice cream ");
  g_assert_cmpstr (query, ==, "ice_cream");
  g_free (query);

  g_assert_null (onym_term_to_query (NULL));
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/onym/edit-distance", test_edit_distance);
  g_test_add_func ("/onym/term-display", test_to_display);
  g_test_add_func ("/onym/term-query", test_to_query);
  return g_test_run ();
}
