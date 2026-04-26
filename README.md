<!--
SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Onym

Onym is a thesaurus and dictionary for GNOME, built on WordNet. You type a word and Onym shows its
meanings grouped by part of speech, with example sentences, then its synonyms, its antonyms, and the
lexical relations that connect it to other words. Every word on screen is a link, so you look one up
just by clicking it. Search is live, with completion as you type and a suggestion when a word is
misspelt.

The word data comes from WordNet, the lexical database from Princeton University. The query engine is
derived from Artha, an earlier WordNet thesaurus by Sundaram Ramaswamy. Onym is a fresh GTK4 and
libadwaita application around that engine, built to feel at home on GNOME 50 and later.

## Status

The library and a headless command line tool work today. The graphical application is in progress.

- `libonym` is an installable library that turns a word into a structured result. It is the reusable
  core, with a stable model, a pkg-config file, and GObject Introspection so it binds from C, Python,
  JavaScript, and Vala.
- `onym-cli` is a small tool that looks words up from the terminal. It proves the library and drives
  the tests.

## Building

Onym uses Meson. The library needs GLib and the WordNet runtime and development files. On Debian or
Ubuntu:

```
sudo apt install meson wordnet-base wordnet-dev libglib2.0-dev gobject-introspection libgirepository1.0-dev
meson setup _build
meson compile -C _build
meson test -C _build
```

Try it:

```
./_build/tools/onym-cli serendipity
./_build/tools/onym-cli --complete sere
```

## How the code is organised

Onym is three layers, so the one piece of borrowed code stays sealed off and the rest stays easy to
follow.

- The **engine** is the vendored WordNet code. It is the only part that talks to the WordNet C
  library, and it is kept exactly as it came from Artha.
- The **bridge** is the only code that calls the engine. It copies what the engine returns into a
  clean model of plain objects, then frees the engine's data.
- The **library and application** see only that model. They never know WordNet or Artha exist.

`OnymEngine` is the object you ask for a lookup. It hands back an `OnymResult`, which is a list of
sections, each holding definitions, words, or antonyms. The application renders that result and never
parses anything itself. `ARCHITECTURE.md` describes the layers and the files in more detail.

## Licence and credits

Onym is free software under the GPL, version 3 or later. See `COPYING`.

- WordNet is provided by Princeton University under its own permissive licence. Its notice ships with
  the bundled database.
- The query engine is derived from Artha by Sundaram Ramaswamy, under the GPL, version 2 or later.
  See `libonym/engine/PROVENANCE.md`.
- Onym is built with GTK, libadwaita, and GLib from the GNOME project.
