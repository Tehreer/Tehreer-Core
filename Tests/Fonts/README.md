# Test Fonts

Small fonts used by the tests. They are copied from the HarfBuzz test suite
(`test/api/fonts`, https://github.com/harfbuzz/harfbuzz) so that the tests do not depend on any
fetched dependency.

| File | Used for |
|------|----------|
| `Roboto-Regular.abc.ttf` | Static font subset (glyphs `a`, `b`, `c`) |
| `Roboto-Regular.names.ttf` | `Roboto-Regular.abc.ttf` with a `post` table of format 2.0 that names the glyphs `.notdef`, `a`, `b` and `c` (made for these tests) |
| `Roboto-Variable.abc.ttf` | Variable font with `wght` and `wdth` axes and 18 named styles |
| `Roboto-Variable.hidden.ttf` | `Roboto-Variable.abc.ttf` with the hidden flag set on the `wght` axis (made for these tests) |
| `RocherColorGX.abc.ttf` | Variable color font with 11 palettes of 4 entries |
| `COLRv0.extents.ttf` | Color font without names, 2 palettes, color glyph 13 |
| `nameID.dup.expected.ttf` | Font with Macintosh-only English names (Mac Roman) |
| `varc-6868.ttf` | Font without an OS/2 table |
| `NotoColorEmoji-CBDT.flags.ttf` | Bitmap-only color font (`CBDT`/`CBLC`, one strike of 109 pixels, no outlines) with 18 glyphs, among them the flags of the United States and the United Kingdom |

The Roboto subsets derive from Roboto (Apache License 2.0), and `RocherColorGX` and the Noto Color
Emoji subset (https://github.com/googlefonts/noto-emoji, subset with the HarfBuzz subsetter) are
licensed under the SIL Open Font License; the remaining fonts are HarfBuzz test data. Check the upstream license
terms before redistributing these files outside the repository's tests.
