---
name: code-style
description: Control-flow and file-layout rules for Tehreer C library code (single exit, result variable, nesting limit, function ordering). Use when writing, changing, reviewing, or refactoring any `.c` file under `Source/`.
---

# Tehreer Control Flow, Layout and Naming

These rules apply to every function in `Source/**/*.c`, in addition to the naming and formatting rules in `CLAUDE.md`. When touching a function that violates them, bring it into compliance.

## 1. No early exit from a function

Every function has exactly one exit: the last statement of its body.

- No `return` before the last line. Void functions have no `return` at all.
- No `goto`.
- Guard clauses (`if (!x) { return NULL; }`) are not allowed. Invert them into a positive `if` that wraps the work.
- Loops may use `break` and `continue` whenever that reads better than the alternative. Do not pile extra conditions or flag variables into the loop header just to avoid them: `for (...; index < count; ...)` with an `if (...) { break; }` inside is preferred over `for (...; !isFound && index < count; ...)`. Readability is the goal; choose the form a reader understands fastest.

## 2. Result variable

Declare the variable that the function returns at the top of the function with its default (failure) value, assign it inside the `if` branches, and return it on the last line. Name it after what it holds, not `result`: the object's name (`glyphImage`, `path`, `glyphRun`, `typeface`), the quantity (`advance`, `lineIndex`, `breakIndex`), or a boolean that reads well (`isFound`, `isSet`, `hasNext`). Use `result` only when nothing in the context names it better.

```c
TRBoolean TRFooSetBar(TRFooRef foo, const Bar *bar, TRUInteger count)
{
    TRBoolean isSet = TRFalse;
    Bar *copy;

    if (bar && count > 0) {
        copy = AllocatorAllocateBlock(sizeof(Bar) * count);

        if (copy) {
            memcpy(copy, bar, sizeof(Bar) * count);

            AllocatorDeallocateBlock(foo->bar);
            foo->bar = copy;
            foo->barCount = count;

            isSet = TRTrue;
        }
    }

    return isSet;
}
```

- A function that creates an object returns that object's variable itself: set it to `NULL` on failure (after releasing it) rather than keeping a second variable that mirrors it.
- Early-exit cleanup (`free` then `return`) becomes: do the work inside the success branch and release in the same function after the branch, before the single `return`.
- Do not carry a default that silently masks failure; the default must be the correct value for "nothing was done".

## 3. Nesting limit

At most **3 levels** of nested blocks inside a function body (the body itself is level 0). An `if` containing an `if` containing an `if` is the maximum; a fourth nested `if`/`for`/`while`/`switch` must be moved into a helper function.

- `else if` chains count as one level.
- When a limit is hit, extract the inner block into a `static` helper named for what it does, passing in only what it needs. Prefer helpers that return a value or `TRBoolean` rather than ones that write through several out-parameters.
- Extract earlier if it makes the main function read as a short list of steps.

## 4. Declaration order

In every block (a function body or the body of an `if`, `for`, `while`, `do`, `switch`), the declarations at its top are ordered so that variables declared with an initial value come first, and variables declared without one come after. Keep the original relative order inside each group, so an initializer can still use an earlier variable. The variable that is returned comes first among the initialized ones, as in the example above.

```c
TRFloat advance = 0.0f;                 /* initialized: first */
TRUInteger count = line->runCount;
TRUInteger index;                       /* uninitialized: after */
TRGlyphRunRef glyphRun;
```

A declaration that mixes both, such as `TRFloat a = 0.0f, b;`, is split in two. If an initializer needs a variable that has no initial value (for example `sizeof(buffer)` or `&paint`), declare it without the initializer and assign it in a statement after the declarations.

## 5. Declare variables where they are used

Declare a variable in the innermost block that contains all of its uses, not at the top of the function by default. A variable used in only one `if` branch is declared at the top of that branch; a variable used in several separate blocks is declared at the top of the block that encloses them all. Declarations still sit at the start of their block (C89), ordered as in section 4.

- The variable that the function returns, and anything else used across the function, stays at the function level.
- Do not move a variable into a loop body when its value has to survive from one iteration to the next (accumulators, counters, `hasPrevious`-style state). Do move one that is always assigned first thing in each iteration.
- Do not move an initialized variable if its initializer would then be evaluated at a different time with a different result (for example `oldIndex = glyphIndex` before `glyphIndex` is reassigned), or if it has side effects.
- Never move a declaration into a `switch` body, and do not open a block only to hold it (see section 6).

## 6. No bare blocks

Every `{ }` block follows a keyword: `if`, `else`, `for`, `while`, `do`, `switch`, or a `case` label. Never open a block on its own just to declare locals or limit scope; declare the variables at the top of the function or of the enclosing keyword block, or move the code into a helper function. The opening brace stays on the same line as its keyword (K&R), never on a line by itself.

## 7. Function ordering inside a `.c` file

From top to bottom, after includes, macros, and type definitions:

1. **Private `static` functions**: helpers used only in this file. Ordered by dependency: a callee is defined before its caller, so no forward declarations are needed. In a long file, group them into sections by what they handle and start each with a marker comment, `/* ---------- Paragraph Handling ---------- */` (the form `Source/API/TRBase.h` uses). Name and order the sections after the same code in the sibling platform libraries when there is one (for example `FrameResolver.swift` in Tehreer-Cocoa: paragraph, line, layout), and add a section when dependencies do not allow the exact order.
2. **Internal functions** (`TR_INTERNAL`): declared in a `Source/` header. Same order as their declaration in that header.
3. **Public API functions** (`TR_PUBLIC` in `Headers/Tehreer/`): at the end of the file, in exactly the order of their declaration in the public header.

Private structs and typedefs (callback contexts, helper records) are declared at the top of the file, right after the includes, before the first function, never next to the function that uses them.

Never interleave the groups. Do not add forward declarations to get around ordering; if a private helper must call a later function, reorder or restructure.

## 8. Name intermediate values, and read each value once

Compute derived values into named locals instead of nesting calls or expressions inside an argument list, so that they can be seen while debugging.

- Build a struct such as a `TRRect` in its own variable, declared right after the variables it is made from and assigned in a statement of its own, then pass the variable. Do this for every rect in the function, not only the first.
- Read a getter once. When a value is compared and then assigned, or used twice, store it in a local first (`TRFloat ascent = TRGlyphRunGetAscent(glyphRun);`), then use the local.
- Declare a variable above the ones that are computed from it (`rangeEnd` before `visualEnd`), so that each initializer only uses what is already declared.

## 9. Minimum and maximum

Use `NumberMin(a, b)` and `NumberMax(a, b)` from `API/TRBase.h` instead of writing `(a < b ? a : b)` or `(a > b ? a : b)`. The arguments are evaluated twice, so pass plain variables or fields.

## 10. Allocation and creation order

Allocate the dependencies of an object (its arrays and buffers) before the object itself, and create the object only when they succeeded. On failure, free what was allocated and release whatever ownership the caller handed over, in one place before the single `return`.

- Do not create an object first and then allocate its members into it.
- Do not keep a half-built state to check later (`line && (count == 0 || line->runs)`).
- Growable lists use the shared `Array` from `Core/Array.h`; do not hand-roll `items`, `count`, `capacity` fields and doubling logic.

## 11. Invariants are assertions, not defensive branches

When a function requires something of its input (a frame has at least one line, a line has at least one run, a range is non-empty), document it with `TRAssert` and a `/* ... MUST ... */` comment, and write the code for the valid case only. Do not add an `if` that tests the same thing again, and do not special-case the empty input. Ask the owner before deciding that an empty case is allowed.

## 12. Types and references

- The public `TRFooRef` (a `const` pointer) is the type for every reference to an object, internal code included. Do not add an internal alias such as `FooRef` for the same struct.
- Where the code must modify the object (creation, filling in fields), use `TRFoo *`. Convert to `TRFooRef` by plain assignment, with no cast.
- Internal helper types that have no public counterpart keep the `Foo` and `FooRef` pair.

## 13. Function naming

- Every internal (`TR_INTERNAL`) function of a public type carries the `TR` prefix, like its public functions: `TRComposedLineCreate`, `TRGlyphRunCreateCopy`, `TRTypesetterFindParagraph`, never `ComposedLineCreate`. Only helpers of types that have no public counterpart (`TextRun`, `FaceMetadata`, ...) go without it. An internal function is named after its class (`TRComposedLineGetTop`). Do not write a second, internal copy of a function that the public API already has: call the public one.
- The internal functions, structs and keys of a public type live in its own `Source/API/TRFoo.h` and `TRFoo.c`, next to the public API functions. Do not make a second `Foo.c`/`Foo.h` in another folder (`Graphics/`, `Layout/`, ...) for the same type; those folders are for classes that have no public type. Order inside the file stays static, then internal, then public.
- When an internal function would get the name of a public one, the public function is the implementation: define it once, in the file that owns the data, and drop the internal copy and any forwarding wrapper (`TRGlyphCacheClear`, `TRGlyphCacheSetCapacity`).
- Say which quantity a function returns. A line has several kinds of distance and index, so they are `TRComposedLineGetCodeUnitDistance` and `TRComposedLineGetCodeUnitIndex`, not `GetDistance` and `GetIndexOfCodeUnit`. Prefer `Get<Subject><Quantity>` over `GetIndexOf<Subject>`.
- A public function has one purpose that its name states. Do not add a getter with several optional out-parameters; keep such lookups internal until there is a clear use for them.
- In public structs, keep identifying members (name, tag, flags) together at the top, before the numeric members.

## 14. Memory, containers and state

- Never call `malloc`, `calloc`, `realloc` or `free` directly, in any folder. Use `AllocatorAllocateBlock`, `AllocatorAllocateZeroedBlock`, `AllocatorReallocateBlock` and `AllocatorDeallocateBlock` from `Core/Allocator.h`. Only `Core/Allocator.c` touches the C allocator.
- Use `Array` from `Core/Array.h` for every growable or table-like block of items (lists of lines, hash buckets), including the ones that a public object keeps, instead of a raw pointer with a count.
- Give an enum an explicit `Unknown` value (zero) instead of a separate `hasX` flag next to the field.
- Public types are mutable struct pointers when the library has to modify them after creation (for example a `TRGlyphImageRef` that gets native data); do not hide that with a `const` typedef and a cast.
- Do not lock for state that is not shared. Check what a library call really touches (the outline functions of FreeType only read the allocator of the library) before wrapping it in the library mutex.
- Public API shape: when a function would return several items that need releasing, prefer an enumeration with a callback (as `TRPathEnumerate` does). It needs no allocation and no release call. A public object type is for things that have an identity.
- Do not expose accessors that only give back what the caller passed in (`userData`), and do not put knobs in a public contract that one caller needs (`leading`); use a kind enum instead of a boolean flag (`TRReplacementKind`).
- Replace bit twiddling on raw bytes with named macros (`ReadUInt16BE`, offsets and sizes), and use the decoding functions that SheenBidi gives (`SBCodepointDecodePrevious...`) instead of rewriting them.
- Name a local after the type of what it holds (`shapableFace`), and do not leave blank lines inside a group of `#undef`s.

## Applying the rules to an existing function

1. List every `return`, `goto`, `break`, `continue` that is not the last statement.
2. Choose the returned variable, a meaningful name for it, and its default.
3. Rewrite guards as positive `if` blocks; check nesting depth afterwards and split into helpers if it exceeds 3.
4. Move each declaration into the innermost block that uses it, order the declarations at the top of each block, and remove any bare `{ }` block.
5. Name intermediate values, replace ternary min/max with the macros, and use the `TR*Ref` types (sections 8, 9, 12).
6. Move the function into the right group and position.
7. Rebuild with unity off and run the tests (see `develop` agent). Behavior must not change: keep the same return value for each failure condition and the same cleanup on each path.
