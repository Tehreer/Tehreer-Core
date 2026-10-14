---
name: write-tests
description: Write C++14 tests for the Tehreer C library in the style of the existing suites in `Tests/` (AtomicTests, MutexTests, OnceTests). Use when the user asks to add, write, or extend tests for a Core/API component, or to create a new `*Tests` suite.
---

# Write Tehreer Tests

Tests are C++14, use a custom harness (no Google Test/Catch2), and `<cassert>` assertions. Before writing, read the nearest existing suite (`Tests/OnceTests.*` is the best template; `Tests/AtomicTests.*` for single-threaded value checks) and the source under test, and match them.

## Workflow

1. Read the component's internal header (`Source/Core/*.h`, `Source/API/*.h`) or public header (`Headers/Tehreer/*.h`) to learn the real function names, semantics, and ownership rules.
2. Decide: extend an existing suite (same component) or create a new one (new component).
3. Write/extend `Tests/FooTests.h` and `Tests/FooTests.cpp`.
4. Register new suites in `CMakeLists.txt` (see below).
5. Build with unity **off** and run (see below). Fix failures; never weaken an assertion to make it pass without telling the user.

## File Layout

One suite per component: `Tests/FooTests.h` + `Tests/FooTests.cpp`, class `FooTests` in namespace `Tehreer`.

Both files start with the Apache 2.0 license block (copy it verbatim from an existing test, including the year/author line).

### Header template

```cpp
#ifndef _TEHREER__FOO_TESTS_H
#define _TEHREER__FOO_TESTS_H

namespace Tehreer {

class FooTests {
public:
    FooTests() = default;

    void run();

private:
    void testBasicBehavior();
    void testThreadSafety();
};

}

#endif
```

- Guard is `_TEHREER__<NAME>_TESTS_H` (double underscore after `TEHREER`).
- `run()` is the only public method; every `testXxx()` is private, `void`, no arguments.
- Methods are declared in the same order they are called in `run()`.
- Injected dependencies (e.g. another suite) use an `m_` member prefix.

### Source template

```cpp
#include <cassert>
#include <cstddef>
#include <thread>
#include <vector>

#include <Tehreer/TRBase.h>     /* public headers, only if needed */

extern "C" {
#include <Core/Foo.h>           /* internal C headers */
}

#include "FooTests.h"

using namespace std;
using namespace Tehreer;

constexpr size_t NumThreads = 8;
constexpr size_t Iterations = 10000;

void FooTests::run() {
    testBasicBehavior();
    testThreadSafety();
}

void FooTests::testBasicBehavior() {
    /* arrange, act, assert */
}

#ifdef STANDALONE_TESTING

int main() {
    FooTests tests;
    tests.run();

    return 0;
}

#endif
```

Include order: C++ standard headers (alphabetical), public `<Tehreer/...>` headers, `extern "C" { }` block with internal C headers (`<Core/...>`, `<API/...>`, ...), then the quoted `"FooTests.h"`.

## Style Rules

- 4-space indent, K&R braces, no tabs. Closing `}` of the namespace in the header is at column 0 with no trailing comment.
- Assertions are plain `assert(...)` from `<cassert>`; compare booleans explicitly, as the existing tests do (`assert(x == true)` / `assert(x == false)`).
- Progress output, when wanted, goes through `std::cout`; most existing tests print nothing. Do not add output unless the suite already does.
- Test method names: `testXxx` in camelCase describing the behavior (`testTryLock`, `testRecursiveOnce`, `testAtomicUIntCompareAndSet`). Use the component name as prefix when a suite covers several types (`testAtomicFlag...`, `testAtomicPtr...`).
- File-scope tunables are `constexpr size_t` PascalCase (`NumThreads`, `Iterations`); function-local sizes are `constexpr` too.
- Locals are camelCase. Create objects with the component's `XxxMake()`/`XxxCreate()` factory, and release/destroy anything created (`Release`, `Destroy`) so ASan/leak checks stay clean.
- State shared with lambdas/threads that must outlive the call uses `static`; otherwise capture by `[&]`.
- Use C++14 only: `auto`, lambdas, `constexpr`, `nullptr`, `u"..."`/`U"..."` literals, `std::thread`, `std::atomic`, `std::chrono`. No C++17 (structured bindings, `if constexpr`, `std::optional`, inline variables).
- Do not use `//` comments inside C library code; in tests either style is fine but prefer sparse comments, as the existing tests have almost none.
- Cover: the happy path, edge cases (NULL/empty/zero-length, boundary values, repeated calls), ownership/reference counting, and, for thread-safe components, a multi-thread test plus optionally a stress test. Keep each test method focused on one behavior.

### Thread test pattern

```cpp
vector<thread> threads;

for (size_t i = 0; i < NumThreads; i++) {
    threads.emplace_back([&]() {
        for (size_t j = 0; j < Iterations; j++) {
            /* exercise component */
        }
    });
}

for (auto &t : threads) {
    t.join();
}

assert(counter == NumThreads * Iterations);
```

Use `atomic<size_t>` for counters touched by multiple threads outside the component under test. Never rely on sleeps for correctness; sleeps/yields only widen race windows.

## Registering a New Suite in CMake

In `CMakeLists.txt` under `if(BUILD_TESTS)`:

1. Add `FooTests` to `TEST_TARGETS`.
2. Add a source list alongside the others:

```cmake
  set(FooTests
    Tests/FooTests.h
    Tests/FooTests.cpp
  )
```

The existing `foreach` creates the executable, defines `STANDALONE_TESTING`, links `Tehreer`, and registers it with CTest. Shared helpers, if ever needed, go in `Tests/Utilities/`, which is globbed automatically.

## Fonts and Test Utilities

Tests that need real font data use the small fonts committed in `Tests/Fonts/` (see its README; never depend on fetched dependencies for fixtures). `TEST_FONTS_DIR` points there for every test target. Copy any new font into that folder and list it in the README. Include the helpers from `Tests/Utilities/`:

- `TestFonts.h`: `testFontPath("Roboto-Regular.abc.ttf")`.
- `TestFace.h`: `TestFace face("...")` owns a FreeType library and face, for SFNT-level tests.
- `TestTypeface.h`: `createTestTypeface("...", coordinates)` builds a `TRTypefaceRef` through the internal pipeline; the typeface retains what it needs, so release it once.

Useful fixtures: `Roboto-Regular.abc.ttf` (static, glyphs a/b/c = 1/2/3), `Roboto-Variable.abc.ttf` (wght/wdth axes, 18 named styles), `RocherColorGX.abc.ttf` (variable, 11 palettes of 4 entries), `COLRv0.extents.ttf` (no names, 2 palettes, color glyph 13), `nameID.dup.expected.ttf` (Macintosh-only names).

Assert the contract the component owns, not the internals of dependencies (for example, SheenBidi's intermediate retain counts).

## Build and Run

Tests are built only with unity mode **off**:

```bash
cmake -S. -Bbuild -DCMAKE_BUILD_TYPE=Debug \
  -DTR_CONFIG_UNITY=OFF \
  -DCMAKE_C_STANDARD=90 -DCMAKE_C_EXTENSIONS=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

Single suite: `./build/FooTests`.

If FreeType fails to compile because of `-DCMAKE_C_STANDARD=90`, configure without it for running tests, and check library C89 compliance per file instead:

```bash
clang -std=c89 -pedantic -Wall -Wextra -fsyntax-only -ISource -IHeaders <deps include dirs> Source/Core/Foo.c
```

Also run the suite in a separate sanitizer build (`-DENABLE_ASAN=ON -DENABLE_UBSAN=ON`, directory `build-asan`) and, for threaded code, a `-fsanitize=thread` build. macOS has no LeakSanitizer; use `MallocStackLogging=1 leaks --atExit -- ./build/FooTests` instead.

Always use a **Debug** build: `assert` is compiled out under `NDEBUG` (Release), which would make the tests pass vacuously. Optionally verify with `-DENABLE_ASAN=ON` / `-DENABLE_UBSAN=ON` (and TSan-style scrutiny for thread tests).

## Constraints

- Do not modify library code to make a test pass unless the user asks; report suspected library bugs instead.
- Keep changes minimal: only the test files and the CMake list entries.
- Commit tags, if asked to commit: `[test] ...` (and `[cmake] ...` for build-only changes), branched from `develop`.
