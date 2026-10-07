---
name: code-style
description: Control-flow and file-layout rules for Tehreer C library code (single exit, result variable, nesting limit, function ordering). Use when writing, changing, reviewing, or refactoring any `.c` file under `Source/`.
---

# Tehreer Control Flow and Function Ordering

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
GlyphRunRef glyphRun;
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

1. **Private `static` functions**: helpers used only in this file. Ordered by dependency: a callee is defined before its caller, so no forward declarations are needed.
2. **Internal functions** (`TR_INTERNAL`): declared in a `Source/` header. Same order as their declaration in that header.
3. **Public API functions** (`TR_PUBLIC` in `Headers/Tehreer/`): at the end of the file, in exactly the order of their declaration in the public header.

Never interleave the groups. Do not add forward declarations to get around ordering; if a private helper must call a later function, reorder or restructure.

## Applying the rules to an existing function

1. List every `return`, `goto`, `break`, `continue` that is not the last statement.
2. Choose the returned variable, a meaningful name for it, and its default.
3. Rewrite guards as positive `if` blocks; check nesting depth afterwards and split into helpers if it exceeds 3.
4. Move each declaration into the innermost block that uses it, order the declarations at the top of each block, and remove any bare `{ }` block.
5. Move the function into the right group and position.
6. Rebuild with unity off and run the tests (see `develop` agent). Behavior must not change: keep the same return value for each failure condition and the same cleanup on each path.
