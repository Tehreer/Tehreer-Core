# Tehreer — Agent Guide

This document guides AI assistants working on the Tehreer codebase.

## Project Summary

Tehreer is a text engine whose aim is to provide core implementation for all major operating systems. Version is defined in `Headers/Tehreer/TRVersion.h`. Licensed under Apache 2.0.

**Key constraints:**

- Object-based, reference-counted API
- Depends only on `stddef.h`, `stdint.h`, `stdlib.h`, and `string.h`

## Repository Layout

| Path | Role |
|------|------|
| `Headers/Tehreer/` | Public, installable API headers |
| `Source/API/` | Public API implementations + internal API headers |
| `Source/Core/` | Memory, objects, atomics, Mutex |
| `Source/Tehreer.c` | Unity-build aggregator |
| `Tests/` | C++14 test harness |

**Header duality:** Public declarations live in `Headers/Tehreer/TR*.h`. Internal struct layouts and helpers live in `Source/API/TR*.h` and other `Source/*/` headers. Never expose internal struct definitions in public headers.

## Language Standards

### C Library — C89 (ANSI C)

The library targets **strict C89**. CI enforces this:

- CMake: `-DCMAKE_C_STANDARD=90 -DCMAKE_C_EXTENSIONS=OFF`

**Rules for new and changed library code:**

- Use `/* */` comments only — no `//`
- Declare variables at the start of a block when possible
- Use braced `case` blocks to introduce locals inside `switch`
- `stdint.h` fixed-width types are acceptable (`TRInt8`, `TRUInt32`, etc.)
- C11 atomics are feature-detected at compile time (`Source/API/TRBase.h`) — do not assume C11 for portable logic
- Avoid VLAs, compound literals outside macros, and other C99+ features

### Tests — C++14

Tests use C++14 (`CMAKE_CXX_STANDARD 14`.

- Custom harness — no Google Test, Catch2, or similar
- Each suite is a `*Tests` class with a public `run()` and private `testXxx()` methods
- Assertions via `<cassert>`; progress output via `std::cout`
- Namespace: `Tehreer`

## C Code Style

No automated formatter is configured. Match the conventions below, derived from existing headers and sources.

### Formatting

- **4 spaces** indentation, no tabs
- **K&R braces:** opening `{` on the same line as the function or control statement
- **`switch`:** `case` labels indented one level under `switch`; multi-statement cases use braced blocks
- Spaces around binary operators (`==`, `&&`, `||`)
- Align consecutive assignments in struct initialization and typedef blocks
- Soft line-length target: 80–100 columns

### Naming

| Kind | Convention | Example |
|------|------------|---------|
| Public functions, types, macros | `TR` + PascalCase | `TRTextCreate`, `TRStringEncodingUTF8` |
| Opaque references | `TR` + Name + `Ref` | `TRTypefaceRef` |
| Internal structs | `_Tag` + short name + `Ref` | `AttributeRegistry`, `AttributeRegistryRef` |
| Internal functions | PascalCase module prefix | `RenderableFaceCopyTable`, `GlyphBitmapCreateFromSlot` |
| Private struct fields | Leading `_` | `_base`, `_text` |
| Local variables | camelCase | `isInitialized`, `dictIndex` |
| Boolean locals | `is` prefix | `isAllocated`, `isEnsured` |
| Config macros | `TR_CONFIG_*` | `TR_CONFIG_UNITY` |

### Visibility

- `TR_PUBLIC` — exported public API (in `Headers/Tehreer/`)
- `TR_INTERNAL` / `TR_PRIVATE` — internal symbols; becomes `static` in unity builds

```c
#ifdef TR_CONFIG_UNITY
#define TR_INTERNAL static
#define TR_PRIVATE static
#else
#define TR_INTERNAL
#define TR_PRIVATE
#endif
```

### Header Guards

| Scope | Pattern | Example |
|-------|---------|---------|
| Public | `_TEHREER_<NAME>_H` | `_TEHREER_TEXT_H` |
| Internal | `_TEHREER_<FOLDER>_<NAME>_H` | `_TEHREER_CORE_MEMORY_H` |
| Umbrella | `_TEHREER_H` | `Headers/Tehreer/Tehreer.h` |

### Include Order

**Public headers** include siblings as `<Tehreer/TRBase.h>` and wrap declarations in `TR_EXTERN_C_BEGIN` / `TR_EXTERN_C_END`.

**`.c` implementation files:**

1. Standard headers (`<stddef.h>`, `<stdlib.h>`, …)
2. Internal module headers via angle brackets: `<API/...>`, `<Core/...>`, `<Font/...>`, `<Graphics/...>`
3. Quoted local companion: `#include "TRTypeface.h"`

### Comments

- Apache 2.0 license block at the top of every file
- **Public API:** Doxygen `/** ... */` with `@param`, `@return`; backtick-quoted type names

```c
/**
 * Creates a new immutable text object that is an exact copy of the source text.
 *
 * @param text
 *      Source text object to copy.
 * @return
 *      New immutable copy, or `NULL` on failure.
 */
TR_PUBLIC TRTextRef TRTextCreateCopy(...);
```

### Typedef and Enum Patterns

Fixed-width aliases with column-aligned names:

```c
typedef int8_t                      TRInt8;
typedef uint32_t                    TRUInt32;
```

Enum constants with a separate typedef (not combined `typedef enum`):

```c
enum {
    TRSlopePlain = 0,
    TRSlopeItalic = 1,
    TRSlopeOblique = 2
};
typedef TRUInt32 TRSlope;
```

Struct with tag, short name, and pointer typedef:

```c
typedef struct _GlyphBitmap {
    /* ... */
    BitmapFormat format;
    TRUInt8 *buffer;
} GlyphBitmap, *GlyphBitmapRef;
```

Boolean constants:

```c
enum { TRFalse = 0, TRTrue = 1 };
typedef TRUInt8 TRBoolean;
```

Statement macros use `do { ... } while (0)`:

```c
#define SetItemValue(list_, index_, value_)             \
do {                                                    \
    CheckItemIndex(list_, index_);                      \
    (list_)->items[index_] = (value_);                  \
} while (0)
```

## C++ Test Code Style

- One suite per component: `FooTests.h` + `FooTests.cpp`, class `FooTests`
- Header guard: `_TEHREER__FOO_TESTS_H`
- Member prefix `m_` for injected dependencies (e.g. `m_atomicTest`)
- Include public API as `<Tehreer/...>`; internal C headers inside `extern "C" { ... }`
- C++14 features in use: `auto`, `constexpr`, `nullptr`, `u"..."` / `U"..."` literals, `std::thread`, `std::atomic`

```cpp
namespace Tehreer {

class OnceTests {
public:
    void run();

private:
    void testRecursiveOnce();
};

}
```

## Build and Test

### CMake

```bash
cmake -S. -Bbuild -DCMAKE_BUILD_TYPE=Debug \
  -DTR_CONFIG_UNITY=OFF \
  -DCMAKE_C_STANDARD=90 -DCMAKE_C_EXTENSIONS=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

Run a single suite:

```bash
./build/OnceTests
```

### Important Build Flags

| Flag | Effect |
|------|--------|
| `TR_CONFIG_UNITY=ON` (default) | Single translation unit via `Source/Tehreer.c` — CMake **skip building test targets entirely** when this is on |
| `TR_CONFIG_UNITY=OFF` | Per-file compilation — required for running tests |
| `TR_CONFIG_DLL_EXPORT` / `TR_CONFIG_DLL_IMPORT` | Windows shared library export/import |

CMake build **one standalone executable per suite** (e.g. `AtomicTests`, `OnceTests`), each compiled with a `STANDALONE_TESTING` define that enables the `#ifdef STANDALONE_TESTING` / `int main(...)` block at the bottom of each `Tests/*.cpp` file.

## Commits and Branches

- `master` is the stable/release branch; `develop` is the integration branch for ongoing work — base new work on `develop` unless told otherwise.
- Subject lines commonly use a bracketed scope tag followed by an imperative summary: `[lib] ...`, `[test] ...`, `[cmake] ...`, `[ci] ...`. Omit the tag only when a change doesn't fit a single scope (e.g. `Update README`).

## Agent Guidelines

### Do

- Match naming, brace style, and include patterns in the nearest file
- Add Doxygen documentation to new public API in `Headers/Tehreer/`
- Put internal struct definitions in `Source/` headers only
- Run tests with unity mode **off** after substantive library changes
- Use `TRAssert` with a preceding `/* ... MUST ... */` comment for invariants
- Keep changes minimal and focused on the task at hand
- Update CMake file lists when files are added/removed/renamed
- Base branch work on `develop`; use a `[scope]` commit tag matching the area touched

### Don't

- Use `//` comments in C sources
- Break C89 compatibility in library code
- Expose internal struct layouts in public headers
