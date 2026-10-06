# Test Fonts

Small fonts used by the tests. They are copied from the HarfBuzz test suite
(`test/api/fonts`, https://github.com/harfbuzz/harfbuzz) so that the tests do not depend on any
fetched dependency.

| File | Used for |
|------|----------|
| `Roboto-Regular.abc.ttf` | Static font subset (glyphs `a`, `b`, `c`) |
| `Roboto-Variable.abc.ttf` | Variable font with `wght` and `wdth` axes and 18 named styles |
| `RocherColorGX.abc.ttf` | Variable color font with 11 palettes of 4 entries |
| `COLRv0.extents.ttf` | Color font without names, 2 palettes, color glyph 13 |
| `nameID.dup.expected.ttf` | Font with Macintosh-only English names |
| `varc-6868.ttf` | Font without an OS/2 table |

The Roboto subsets derive from Roboto (Apache License 2.0), and `RocherColorGX` is licensed under
the SIL Open Font License; the remaining fonts are HarfBuzz test data. Check the upstream license
terms before redistributing these files outside the repository's tests.
