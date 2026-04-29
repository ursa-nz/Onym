/* main.c
 *
 * SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* The application entry point. */

#include "onym-application.h"

int
main (int argc, char *argv[])
{
  g_autoptr (OnymApplication) app = onym_application_new ();
  return g_application_run (G_APPLICATION (app), argc, argv);
}
