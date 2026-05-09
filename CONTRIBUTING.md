<!--
SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Contributing to Onym

Thank you for your interest in Onym. This guide covers how to build the project, the conventions the
code follows, and how to submit a change. Onym aims to feel like a native GNOME application, so the
bar is GNOME Circle and Flathub quality: it follows the Human Interface Guidelines, it is accessible,
and it is translatable.

If you are new to the codebase, read `README.md` for what the app does and `ARCHITECTURE.md` for how
it is put together. The short version is that Onym is a reusable library, `libonym`, plus a thin GTK4
and libadwaita application that consumes it.

## Building

Onym uses Meson. On Debian or Ubuntu, install the dependencies:

```
sudo apt install meson ninja-build gcc pkg-config \
  libglib2.0-dev libgtk-4-dev libadwaita-1-dev \
  gobject-introspection libgirepository1.0-dev \
  wordnet-base wordnet-dev
```

Then configure and build:

```
meson setup _build
meson compile -C _build
meson test -C _build
```

## Running

The application reads its settings schema from the build tree when it is run uninstalled, so launch
it like this:

```
GSETTINGS_SCHEMA_DIR=$PWD/_build/data ./_build/src/onym
```

The WordNet database is expected at `/usr/share/wordnet`, which the `wordnet-base` package provides.
There is also a headless tool, `./_build/tools/onym-cli WORD`, which prints the full result for a
word, including the relation trees. It is the quickest way to check the engine without the interface.

## Tests

Run the suite with `meson test -C _build`. There are two suites. The first covers the pure helper
functions and needs no data, so it always runs. The second exercises real lookups and skips itself
when the WordNet database is absent. Add a test with any change that fixes a bug or adds behaviour to
the library, and keep the suite green.

## How the code is organised

Onym is three layers, and the boundary between them is by design.

- The engine, `libonym/engine/wni.c` and `wni.h`, is vendored from Artha and is the only code that
  talks to the WordNet C library. Do not edit it. Its licensing and origin are recorded in
  `REUSE.toml` and `libonym/engine/PROVENANCE.md`.
- The bridge, `libonym/onym-lookup.c`, is the only file that includes the engine header. It copies
  the engine's output into the public model and frees the engine's data.
- The library and the application see only the model. They never refer to WordNet or Artha types.

Keep this separation. New code in the library or the application should depend on the model, not on
the engine.

## Coding style

- The C style is the GNU and GTK convention: two space indentation, the return type on its own line in
  a definition, and a space before every parenthesis. A `.clang-format` and an `.editorconfig` are
  checked in. Run `clang-format -i` on the files you change before you commit.
- Use GObject for types, with `G_DECLARE_FINAL_TYPE`. Avoid global state.
- Namespace everything you write with `Onym`, `onym_`, and `ONYM_`. The vendored engine keeps its own
  `wni_` prefix.
- Build the interface from `.ui` templates compiled into a GResource, not from widgets hand-built in
  C.
- Manage memory with `g_autoptr`, `g_autofree`, `g_clear_object`, and the like. Give the model types
  clear ownership.
- Guard public functions with `g_return_if_fail` and report failures through a `GError`.
- Document every public function with a gi-docgen and GTK-Doc comment that states the parameters, the
  ownership of the return value, and any constraints. Comment the reason for non-obvious code, and let
  precise names carry the rest.
- Keep functions small and focused. Do not leave stubs, placeholder implementations, or TODO comments,
  and do not copy a pattern that should be shared. Tidy duplication and dead code before you submit.

## Language and writing

- Write in Australian/New Zealand English in identifiers, comments, interface strings, and documentation. 
  Use spellings such as colour, behaviour, organise, and licence as a noun. Where a third party API uses
  a United States spelling, such as `gtk_widget_add_css_class`, keep the API name as the library defines
  it.
- Prefer short, complete sentences. Avoid em dashes, emojis, and hype/market language, in the code and in
  the documentation.

## Licensing

- New code is licensed GPL-3.0-or-later. The vendored engine stays GPL-2.0-or-later under its original
  notice.
- Every file carries an SPDX header, or is covered by an entry in `REUSE.toml`. Run `reuse lint` and
  keep it passing. Continuous integration checks it.

## Commits and changes

- Make one logical change per commit. Each commit should build, pass the tests, and pass `reuse lint`.
- Write commit messages in the conventional style, for example `feat:`, `fix:`, `ci:`, or `docs:`,
  followed by a short summary. Explain in the body what the change does and why, in plain language.
- Keep changes small and reviewable. Discuss a large or structural change before you write it, so the
  approach can be agreed first.
- When a change affects the interface, run the application and confirm the result by eye. A before and
  after note, or a screenshot, helps a reviewer.

## Accessibility

Accessibility is a requirement, not a finishing touch. Anything you add must be fully operable from
the keyboard and must carry an accessible name. Do not convey meaning through colour alone. Respect the
system font scale, high contrast, and reduced motion. Test new interface work with the Orca screen
reader and with the keyboard only.

## Submitting a change

Onym is hosted on Forgejo at https://forge.ursa.nz/ursa-nz/Onym. Push your branch and open a pull
request against `main`. Describe the change and how you tested it. The continuous integration workflow
builds the project, runs the tests, and checks REUSE compliance; please make sure it passes.
