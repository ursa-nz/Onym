<!--
SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Architecture

Onym is built as two products in one repository: a reusable library, `libonym`, and a thin GTK4 and
libadwaita application that consumes it. The split keeps the lexical machinery sealed behind a
stable interface, and it lets other projects reuse the library to build their own front ends.

## The three layers

Onym has three layers, and the boundary between each one is deliberate.

- The application is the GTK4 and libadwaita interface. It links libonym and reads the public model.
  It never refers to engine types.
- libonym is the installable library, with a pkg-config file, a GIR, and installed headers.
  Its public face is the OnymEngine object, which looks words up and offers completion and
  suggestions, and the model that a lookup returns. Inside it, the bridge in onym-lookup is the
  only code that builds model objects from engine output.
- The engine is onym-engine, the shared Rust core developed in its own repository and linked in as
  a static archive through its C ABI. It owns the WordNet file parsing, the morphology, the lookup
  rules, and the lemma index, and the conformance kit there pins its behaviour byte for byte. The
  same core drives Onymdroid, so both applications answer every query identically.

Data flows up and narrows at each boundary. The engine returns plain C structs. The bridge copies
them into model objects and frees them. From there nothing knows WordNet exists.

## The engine boundary

The build expects an onym-engine checkout beside this repository (override with
`-Donym_engine_dir`), compiles it with cargo through `build-aux/cargo-build.sh`, and links the
resulting relocatable object into libonym; a shared libonym absorbs it and a static one ships it
inside the archive, so the installed library is self-contained either way. The C contract is the engine's hand-written `onym-core.h`.

`onym-lookup.c` is the bridge. It runs one engine lookup, walks the returned entry, builds an
`OnymResult`, and frees the entry in one call before returning. The engine decides everything
lexical, section order and titles included; the bridge only changes representation.

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
then `WNHOME`, then a directory chosen at build time, and opens the core over it. Where the data
lives is application policy, so the environment variables are honoured here; the core itself only
ever sees an explicit directory, which it reads in place, read-only. Lookups, completion, and
suggestions all delegate to the core, so the lemma index loads once inside it.

The core is immutable after open and safe for concurrent reads, but the engine object opens it
lazily without locking, so calls on one engine stay on one thread. Lookups read a local database
and return in well under a millisecond, so in practice the work stays on the main thread and this
is not a constraint.

## Tests

`meson test` runs three checks. `test-lookup` exercises real lookups, completion, and suggestions
through the public API and asserts structural facts rather than exact glosses; it skips itself
when the database is absent. The byte-exact behaviour is pinned by the onym-engine conformance
kit, which drives `onym-cli` as its dumper. Two more checks validate the packaging:
`appstreamcli validate --strict` over the metainfo and `desktop-file-validate` over the desktop
entry, each run when its tool is present.

## Packaging

The `data/` directory holds the GSettings schema, the icons, the desktop entry, and the AppStream
metainfo. For distribution, the Flatpak manifest in `build-aux/` bundles the WordNet database from
Debian's prebuilt wordnet-base package, so the application needs nothing installed; the engine is
compiled into the binary, so nothing else of WordNet ships.
