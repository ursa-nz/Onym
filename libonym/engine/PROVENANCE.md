<!--
SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Vendored WordNet engine

The two files in this directory, `wni.c` and `wni.h`, are vendored verbatim from Artha, the
GTK2 WordNet thesaurus by Sundaram Ramaswamy. They are the WordNet query engine, and they are
the only borrowed source in Onym. Everything else in the tree is original.

## Source

- Project: Artha, the Open Thesaurus.
- Upstream mirror: https://github.com/sria91/artha
- Commit: 3884f4f0fc3a0b0864d429f7875c3309eff6156f
- Files taken: `src/wni.c`, `src/wni.h`.
- Copyright: 2009 to 2014, Sundaram Ramaswamy <legends2k@yahoo.com>.
- Licence: GPL-2.0-or-later, as stated in the file headers.

## Modifications

None. The files are byte for byte identical to upstream. The original headers and copyright
notices are preserved. Their licensing is recorded for REUSE in the repository `REUSE.toml`,
rather than by editing the files, so the borrow stays verifiable against upstream.

## Why it is isolated here

`wni.c` is the only consumer of the WordNet C library (`wn.h`). The library wraps it behind the
internal bridge in `onym-lookup.c`, which is the only file that includes `wni.h`. The public
library API and the application never see WordNet or Artha types. This keeps the borrowed GPL
engine quarantined behind a stable interface, and it keeps the path open to replacing the engine
later without disturbing consumers.

## Integration notes

- The engine self-initialises WordNet through `wninit()` on first use.
- It locates the database through the `WNSEARCHDIR` environment variable, which the library sets
  if it is unset. See `onym-engine.c`.
- The files compile against GLib and the WordNet library only. They do not depend on GTK.
