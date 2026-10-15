---
name: document-code
description: Rules for documenting Tehreer C code - Doxygen comments for the public API in `Headers/Tehreer/` and plain comments for internal code in `Source/`. Use when writing, changing, or reviewing any comment or Doxygen block, when adding or changing a public function, type, enum, macro, or struct, or when asked to document the library.
---

# Document Tehreer Code

Public headers get Doxygen. Internal code gets plain `/* */` comments. In both, a comment says what the code does, and it is checked against the implementation. Never write a statement that the code does not show.

## Principles

1. **Describe the implementation, not a guess.** Before writing a comment, read the function in `Source/` (and what it calls). Document the actual return values, defaults, clamping, ownership, and failure conditions. When an existing comment disagrees with the code, correct the comment.
2. **Do not change code to match the docs.** If the header and the implementation disagree in a way that is a defect (a function declared but never defined, a symbol used in the public docs but not public), document the real behavior, add a `@warning` when a caller would be misled, and tell the user. Do not move declarations or edit implementation as part of a documentation task.
3. **Say nothing unverified.** No thread-safety claims, performance claims, or "usually" statements unless the code shows them. Ask the user when the code does not settle it.
4. **One rule per case.** Where the headers once varied, the forms below are the rule.

## Public API (`Headers/Tehreer/`)

### Format

- `/** ... */` blocks, wrapped at 100 columns (soft limit), 4-space indented members, Apache 2.0 license block at the top of the file.
- **One comment per declaration.** Never document several functions with one shared comment ("Returns the start and the end..."); Doxygen attaches it to the first one only. Split it into one block per function.
- Wrap identifiers in backticks: types (`TRTypefaceRef`), constants (`TRTrue`, `NULL`), functions with parentheses (`TRFontFileGetTypeface()`), tags (`wght`), values.
- First sentence is a summary in the third person: `Returns ...`, `Creates ...`, `Sets ...`, `Gets ...` (a getter with an out parameter), `Passes ... to the function` (an enumeration). A boolean getter starts with `Returns whether ...`. Details follow in the next paragraph, separated by ` *`.
- Tags come after a blank ` *` line, in this order: `@param` (in declaration order), `@return`, `@warning`. The description is on the next line, indented 6 spaces:

```c
/**
 * Returns the typeface of the run. It is not retained for the caller.
 *
 * @param run
 *      The glyph run.
 * @return
 *      The typeface of the run.
 */
TR_PUBLIC TRTypefaceRef TRGlyphRunGetTypeface(TRGlyphRunRef run);
```

### Functions

- **Every function has a `@param` for every parameter and a `@return` for every non-`void` return.** Short getters too: the object parameter is `The <noun>.` (`The typeface.`, `The glyph run.`), and `@return` restates the result.
- Text for a parameter says what it is and its unit or range when that matters (`Size of the em square, in the unit of the user space.`). Say when `NULL` is allowed and what it means (`The glyph cache, or `NULL` to select the default cache.`). Say when a value MUST be non-zero or MUST NOT be empty, using `MUST`/`MUST NOT`.
- `@return` lists every outcome the code has, in the order the code decides them: the normal value, then each failure value. Failure forms are fixed: `NULL` for pointers and references, `TRFalse` for booleans, `TRInvalidIndex` for indexes; for creators end with `or on failure`. Say what an out parameter holds on failure when the code leaves it untouched (`in which case `distance` is left as it was`).
- A pair of pointer and count getters (`...GetFooPtr` and `...GetFooCount`) refer to each other (`Their number is given by `...GetFooCount()``). A pointer getter states how long the pointer stays valid (`as long as the typeface is alive`) and that the data is owned by the object.
- State defaults of every setter and of every `Create` function that sets properties (`The default is 16.`). Read them from the `Create` implementation.
- State coordinates and units whenever a function has them: the direction of the y axis (`pointing up` for font space, `pointing downward` for layout and drawing), user space versus pixels versus font units, and the origin.
- Enumerations that call back (`Enumerate...`) say in what order the function is called, what `stop` does, and what the return means when the enumeration was stopped.

### Ownership (Create Rule)

- `Create`/`Copy` return an owned reference: `New <thing> that the caller owns`, and for one that a cache keeps as well or a `Copy`, `which the caller owns and has to release`.
- `Get` does not retain: `The <thing> is not retained for the caller, and stays valid as long as the <owner> is alive`.
- A setter that keeps a reference says so: `The renderer retains it.`
- Every `Retain` and `Release` of an object uses these forms:

```c
/**
 * Increments the reference count of a <noun>.
 *
 * @param <name>
 *      The <noun> whose reference count will be incremented.
 * @return
 *      The same <noun> passed in as the parameter.
 */

/**
 * Decrements the reference count of a <noun>. The <noun> will be deallocated when its reference
 * count reaches zero.
 *
 * @param <name>
 *      The <noun> whose reference count will be decremented.
 */
```

  When releasing has a visible effect (a finalize callback, native data destroyed), say it in the second sentence.
- The documentation of the opaque type says what the object is, whether it is immutable, and what its getters hand out. Add thread-safety only when the code shows it (an object that never changes after creation, a lock in the implementation).

### Types

- **Opaque reference typedefs** (`TRFooRef`): a doc block describing the object (see above).
- **Scalar typedefs** (`TRInt8`, `TRGlyphID`, `TRColor`): `A type to represent ...`, with the layout when it is packed.
- **Enumerations** are an anonymous `enum` plus a separate `typedef TRUInt32 TRFoo;`. Document both:
  - the `enum` gets a block saying what the constants select, and every constant gets a trailing `/**< ... */`, aligned in a column;
  - the `typedef` gets its own block: `A type to represent <what>, which is one of the `TRFoo*` constants above.` (add `or a value between them` only when the code accepts that).
  - Flag enums with no typedef (`TRVariationAxisFlag*`) get the block and trailing comments, and the field that holds them says `Combination of the `TRFoo` constants.`
- **Structs and unions**: a block on the type, and a trailing `/**< ... */` for every member, aligned. State that pointer members are owned by the object that returned the struct, whether they may be `NULL`, and the unit. A union says which member goes with which type.
- **Callback typedefs and callback members** have a block with `@param` for every parameter, including `userData` (`The pointer that was passed to the enumeration.`) and `stop` (`Set it to `TRTrue` to stop the enumeration after the function returns; it is `TRFalse` when the function is called.`), and a `@return` for non-`void` callbacks. State when the callback may be `NULL`.
- **Macros** get a block before the `#define`, with `@param` for macro arguments. For a macro defined under `#if`, document each branch's definition once where it is the public one, with the conditions in the text (`On Windows, it expands to ... when `TR_CONFIG_DLL_EXPORT` is defined`).

### Do not

- Use `//` or `/*!` forms, `\param` forms, or `@brief` (the first sentence is the brief).
- Write `Retains the x.` / `Releases the x.` for `Retain`/`Release`.
- Use `TRRange` in docs (the API takes an index and a length).
- Document an implementation detail (`SheenBidi`, FreeType calls, internal structs) beyond what a caller must know.
- Add a `@file` or `@defgroup` block.

## Internal code (`Source/`)

Internal code is not Doxygen. Comments are plain `/* */` and exist where the code is not obvious.

- **Block form**: multi-line comments open with `/*` and put ` * ` on each line; one-liners are `/* ... */`.
- **Internal declarations** (`TR_INTERNAL` functions in `Source/*/TR*.h`) get a comment above them when the contract is not clear from the name: what the function takes over (`The line takes the references of the runs, and the array that holds them is the caller's.`), what it returns on failure (`Returns NULL on failure, in which case the runs are released as well.`), and what the caller MUST provide.
- **Structs**: a comment above a private struct saying what it is and the invariant that links its fields (`A glyph run looks at a range of a text run, which it keeps alive.`). Mark who sets a field (`/* Set by the frame that the line is in. */`).
- **Requirements**: the words `MUST`, `MUST NOT`, and `MAY` mean what RFC 2119 gives them, and appear in the declaration comment and in the assert comment. A requirement on input is checked with `TRAssert` and the comment above it, with no further `if` (see the `code-style` skill):

```c
/* The frame MUST have at least one line. */
TRAssert(ArrayGetCount(lines) > 0);
```

- **Section markers**: in a long `.c` file, `/* ---------- Name ---------- */` before each group of private functions (see `code-style`).
- **Explain why, not what.** Comment the reason for an ordering, a clamp, a rounding, or a workaround (`/* FreeType's y axis points upward while the path's one points downward. */`). Do not comment a line that says what it does.
- **No decoration**: no commented-out code, no `TODO` without the owner's request, no change history, no author notes, no `//`.
- **Foundation headers in `Source/Core/`** (`Object.h`, `Memory.h`, ...) already use Doxygen blocks with `@param`. Keep that form when editing those files; do not convert them, and do not start the form elsewhere.
- Every file starts with the Apache 2.0 license block.

## Workflow

1. Read the header and the implementation of every function being documented (use `graphify query` first when `graphify-out/graph.json` exists).
2. Write or fix comments following the rules above, one declaration at a time.
3. Compare each statement against the code. When the code and the header disagree, apply principle 2.
4. Verify (below). Fix every warning.
5. Report to the user what was corrected, and the problems left in the code.

## Verification

Public headers must compile as strict C89 and Doxygen must report no warnings.

```bash
for h in Headers/Tehreer/TR*.h; do
  echo "#include <Tehreer/$(basename $h)>" | cc -std=c89 -pedantic -Wall -I Headers -x c -fsyntax-only -
done
echo '#include <Tehreer/Tehreer.h>' | cc -std=c89 -pedantic -Wall -I Headers -x c -fsyntax-only -
```

`TRSheenBidi.h` needs the SheenBidi headers on the include path; compile it with them or leave it out of the loop.

Doxygen needs a configuration, which is not committed. Write one in the scratchpad, never in the repository:

```
INPUT                = Headers/Tehreer
FILE_PATTERNS        = TR*.h
GENERATE_HTML        = NO
GENERATE_LATEX       = NO
QUIET                = YES
WARN_IF_UNDOCUMENTED = YES
WARN_NO_PARAMDOC     = NO
EXTRACT_ALL          = NO
PREDEFINED           = TR_PUBLIC= TR_EXTERN_C_BEGIN= TR_EXTERN_C_END=
```

Run `doxygen <config>` from the repository root. A member or typedef that is reported as not documented gets a comment; do not silence the warning in the config. Warnings from `TRSheenBidi.h` come from the missing SheenBidi headers and are expected.

Check that no function was missed: every `TR_PUBLIC` declaration has a `@param` for each parameter and a `@return` when it is not `void`.

## Reporting

When finished, tell the user in a few sentences: which files were documented, which existing comments were wrong and how they were corrected, which problems remain in the code (for example a declared function with no definition), and whether the checks ran. Do not commit unless asked.
