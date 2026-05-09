<!--
SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Architecture

Onym is built as two products in one repository: a reusable library, `libonym`, and a thin GTK4 and
libadwaita application that consumes it. The split keeps the one piece of borrowed code sealed behind
a stable interface, and it lets other projects reuse the library to build their own front ends.

## The three layers

Onym has three layers, and the boundary between each one is deliberate.

- The application is the GTK4 and libadwaita interface. It links libonym and reads the public model. It never refers to WordNet or Artha types.
- libonym is the installable library, with a pkg-config file, a GIR, and installed headers. Its public face is the OnymEngine object, which looks words up and offers completion and suggestions, and the model that a lookup returns. Inside it, the bridge in onym-lookup is the only caller of the engine, and wn-index holds the lemma index that backs completion and suggestions.
- The engine is the WordNet code vendored in engine/. It is the only caller of the WordNet C library.

Data flows up and narrows at each boundary. The engine returns WordNet structures. The bridge copies
them into model objects and frees them. From there nothing knows WordNet exists.

## The borrow boundary

`libonym/engine/wni.c` and `wni.h` are vendored verbatim from Artha. They are the only code that
includes the WordNet header `wn.h`. They are built as their own static library with warnings
silenced, and they are never edited, so the borrow stays verifiable against upstream. Their licensing
is recorded in `REUSE.toml` and `libonym/engine/PROVENANCE.md`.

`onym-lookup.c` is the bridge. It is the only file that includes `wni.h`. It runs one engine request,
walks the returned `GSList`, builds an `OnymResult`, and frees the engine response before returning.
A comment at the top of that file maps each WordNet relation to the section it becomes.

## The public model

Every model type is a small final `GObject`, and every collection is a `GListModel`. That means a
result drops straight into a GTK list view and binds cleanly from introspected languages.

- `OnymResult` has a resolved headword and a list of `OnymSection`.
- `OnymSection` has a kind, a title, and a list of items. The kind says what the items are.
- `OnymDefinition` has a part of speech, a gloss, and example sentences.
- `OnymWord` is a single term, such as a synonym, that a consumer can look up to navigate.
- `OnymAntonym` is an opposite, marked direct or indirect, with its implication terms.

Consumers read the model; they never build it. The constructors live in a private header that is not
installed.

## The engine object

`OnymEngine` is the entry point. It resolves the WordNet data directory once, from `WNSEARCHDIR`,
then `WNHOME`, then a directory chosen at build time, and points the WordNet library at it. It
delegates a lookup to the bridge, and it lazily loads the lemma index the first time completion or a
suggestion is asked for.

The WordNet C library keeps global state and is not reentrant, so every call on an engine must come
from one thread. Lookups read a local database and return in well under a millisecond, so in practice
the work stays on the main thread and this is not a constraint.

## Completion and suggestions

`wn-index.c` reads the WordNet index files directly, so it depends on GLib alone. It loads every
headword once, lowercased and in display form, sorted and deduplicated. Prefix completion is then a
binary search, and a "did you mean" suggestion is a bounded edit distance scan over the same list.
The pure helpers it is built on, edit distance and the display and query forms of a term, are
exported so the unit tests can exercise them without any WordNet data.

## Tests

`meson test` runs two suites. `test-index` covers the pure helpers and needs no data, so it always
runs. `test-lookup` exercises real lookups and asserts structural facts rather than exact glosses, so
it stays robust across WordNet releases; it skips itself when the database is absent.
