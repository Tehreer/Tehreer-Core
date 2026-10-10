---
name: develop
description: Implements and modifies Tehreer C library code (Source/ and Headers/Tehreer/) following the project's strict C89 and style rules. Use for new features, bug fixes, and refactors in the library.
---

You are a developer on Tehreer, a reference-counted, object-based C text engine. Read `CLAUDE.md` at the repo root first and follow it exactly; this file only adds workflow.

## Workflow

1. **Understand.** Read the relevant public header (`Headers/Tehreer/TR*.h`), its implementation (`Source/API/`), and the internal module it touches (`Source/Core/`, `Source/Text/`, ...). Find the nearest similar code and mirror its naming, brace style, includes, and ownership conventions.
2. **Implement** with minimal, focused changes. Do not refactor unrelated code.
3. **Verify** with a non-unity, strict C89 Debug build, then run the tests:

   ```bash
   cmake -S. -Bbuild -DCMAKE_BUILD_TYPE=Debug \
     -DTR_CONFIG_UNITY=OFF \
     -DCMAKE_C_STANDARD=90 -DCMAKE_C_EXTENSIONS=OFF
   cmake --build build
   ctest --test-dir build --output-on-failure
   ```

   If FreeType fails to build under `-DCMAKE_C_STANDARD=90`, configure without that flag and check each changed source with `clang -std=c89 -pedantic -fsyntax-only` instead. Also run the tests in an ASan+UBSan build.

   Also confirm the unity build still compiles (`-DTR_CONFIG_UNITY=ON`, the default). If you add or remove a `.c` file, update the CMake lists and `Source/Tehreer.c`.
4. **Tests.** When behavior changes or new logic is added, use the `write-tests` skill to add or extend a suite in `Tests/`. If the change is not testable with the current harness, say so.
5. **Report** what changed, what was built and run, and anything you could not verify.

## Hard rules

- Follow `.claude/skills/code-style/SKILL.md`: single exit with a meaningfully named returned variable declared on top, variables declared in the innermost block that uses them, initialized declarations before uninitialized ones in every block, no bare `{ }` blocks, at most 3 nesting levels (extract `static` helpers beyond that), and function order static → internal → public (header order, with section markers in long files and private structs at the top), intermediate values in named locals, `NumberMin`/`NumberMax`, dependencies allocated before their owner object, `TRAssert` for invariants, `TR*Ref` types without internal aliases, `Core/Array.h` for growable lists, and names that state the quantity returned.
- Ask before deciding behavior that the code does not already settle (for example whether an empty frame or line is allowed); do not guess.
- Strict C89: `/* */` comments only, declarations at block start, no VLAs, no C99+ features. C11 atomics only via the existing feature detection.
- Public headers carry Doxygen docs (`@param`, `@return`) and `TR_PUBLIC`; internal struct layouts stay in `Source/` headers, never in `Headers/Tehreer/`.
- Internal symbols use `TR_INTERNAL` / `TR_PRIVATE`; they become `static` in unity builds, so avoid name collisions across files.
- Respect the dependency limit: only `stddef.h`, `stdint.h`, `stdlib.h`, `string.h`.
- Use `TRAssert` with a preceding `/* ... MUST ... */` comment for invariants.
- Reference counting: every create/retain has a matching release; check ownership on every error path.
- Do not commit or push unless asked. If asked: base on `develop`, subject `[scope] imperative summary` (`[lib]`, `[test]`, `[cmake]`, `[ci]`).
- Do not weaken or delete tests to get a green build; report suspected conflicts instead.
