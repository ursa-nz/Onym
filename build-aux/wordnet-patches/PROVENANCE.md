<!--
SPDX-FileCopyrightText: 2026 ursa.nz <code@ursa.nz>
SPDX-License-Identifier: GPL-3.0-or-later
-->

# WordNet build patches

The Flatpak bundles WordNet so the application needs nothing installed. WordNet 3.0 predates modern
toolchains and was never updated, so it does not build clean on a current SDK and carries unfixed
buffer-overflow CVEs. Rather than maintain our own fixes, we stand on the Debian project's work and
apply its WordNet patch series in full. A tested whole is safer than a hand-picked subset.

## Sources

- Upstream is WordNet 3.0 from Princeton University. Princeton's own tarball is at
  `https://wordnetcode.princeton.edu/3.0/WordNet-3.0.tar.gz`
  (sha256 `640db279c949a88f61f851dd54ebbb22d003f8b90b85267042ef85a3781d3a52`).
- The patches here are Debian's series for `wordnet` 3.0-41, from
  `https://deb.debian.org/debian/pool/main/w/wordnet/wordnet_3.0-41.debian.tar.xz`.
- Debian repacked the orig tarball, so the patches are made against
  `https://deb.debian.org/debian/pool/main/w/wordnet/wordnet_3.0.orig.tar.gz`
  (sha256 `73572005ef8eb15be48ea1010d18082b80bfbf8684b78ce64bc3abf11db1f95f`), which the Flatpak
  manifest pins as the source. `series` is the apply order.

## Licensing

- WordNet itself, the library and the database, is under the permissive WordNet 3.0 licence from
  Princeton. That licence ships with the bundled database.
- The patch files are Debian packaging, licensed GPL-2.0-or-later by the Debian WordNet maintainers,
  recorded in `REUSE.toml`.

## What matters for Onym

We build the library only, so the Tcl, Tk, browser, manual page, and Python patches are inert. The
ones that earn their place are the toolchain fixes, `gcc-14.patch` and `gcc-15.patch`, and the
security fixes, `50_CVE-2008-2149_buffer_overflows.patch` and `51_overflows*.patch`.
