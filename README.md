# Tehreer Core

**A portable text engine in plain C89.** Fonts, Unicode text and rich attributes behind a small, reference-counted API, built to run the same way on every major operating system.

## Highlights

- **Plain C89, tiny surface.** The public API needs only `stddef.h`, `stdint.h`, `stdlib.h` and `string.h`, so it is easy to bind from any language.
- **Real-world fonts.** TrueType and OpenType files, variable fonts (axes and named styles) and color palettes.
- **Attributed text.** UTF-8, UTF-16 and UTF-32 text with mutable editing, batch updates and per-range attributes such as typeface and point size.
- **Standing on proven libraries.** [FreeType](https://freetype.org) for fonts, [HarfBuzz](https://harfbuzz.github.io) for shaping, [SheenBidi](https://github.com/Tehreer/SheenBidi) for the Unicode Bidirectional Algorithm and [libunibreak](https://github.com/adah1972/libunibreak) for line breaking, all fetched and built for you.
- **Built to be trusted.** Thread-safe internals, with the test suite run under AddressSanitizer, UBSanitizer and ThreadSanitizer.

> **Status:** early development. Fonts, typefaces, text and attributes are available today; shaping and rendering are implemented internally and are being exposed next.

## A quick look

```c
#include <string.h>
#include <Tehreer/Tehreer.h>

int main(void)
{
    const char *string = "Hello, world";
    TRMutableTextRef text = TRTextCreateMutable(TRStringEncodingUTF8);
    TRTextAppendCodeUnits(text, string, strlen(string));

    TRAttribute size;
    size.type = TRAttributePointSize;
    size.value.pointSize = 18.0f;
    TRTextSetAttribute(text, 0, 5, &size);

    TRTextRelease(text);

    return 0;
}
```

## Build and test

You need CMake 3.16+, a C compiler, a C++14 compiler for the tests, and Git for fetching dependencies.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DTR_CONFIG_UNITY=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

The default `TR_CONFIG_UNITY=ON` builds the library as a single translation unit; tests are only built with it off. Use `-DENABLE_ASAN=ON` and `-DENABLE_UBSAN=ON` for sanitizer builds.

## Use it in your project

```sh
cmake --install build --prefix /path/to/prefix
```

```cmake
find_package(Tehreer REQUIRED)
target_link_libraries(app PRIVATE Tehreer::Tehreer)
```

A `tehreer.pc` file is installed for pkg-config users.

## License

Licensed under the [Apache License 2.0](LICENSE). Third-party components are listed in [NOTICE](NOTICE).
