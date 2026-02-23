# Sigminer
Mining type info from shared object files.

## 0) What you need to know up front

You need **one of**:

* a **function address** (best; e.g. from `dlsym()` or from your uprobe attach address), or
* a **symbol name** (works if it exists in `.dynsym` / `.symtab`)

DWARF lookup by **address range** is the most robust.

---

## 1) Open the ELF `.so` with LLVM Object

Use:

* `llvm::object::createBinary(path)` → gives you a `Binary`
* cast to `llvm::object::ObjectFile` (ELF)

You’ll use the `ObjectFile` for:

* symbol lookup (name → address)
* building a `DWARFContext`

---

## 2) Resolve the function address (if you only have a name)

If you have **symbol name**:

* iterate `Obj.symbols()` (and/or `Obj.dynamic_symbols()`)
* match `SymbolRef::getName()`
* get its **address** via `SymbolRef::getAddress()` (returns object-relative “virtual” address)

**Gotcha:** in shared libs, symbols can be:

* undefined imports,
* versioned names (`foo@@GLIBC_2.xx` in some contexts),
* or “IFUNC”/PLT related. Prefer `.symtab` when available; `.dynsym` for exported only.

If you already have a runtime pointer (e.g. `dlsym`) you can map it to object addresses later, but easiest is: **work in ELF virtual addresses** from the object file.

---

## 3) Create a DWARF context

Create:

* `auto DICtx = llvm::DWARFContext::create(Obj);`

This reads DWARF sections from the object (and sometimes can follow linked debug info, but in practice you may need to locate separate debug files yourself; more on that below).

---

## 4) Find the DWARF subprogram for that address

Goal: find the **`DW_TAG_subprogram` DIE** whose PC range covers your function address.

High-level steps:

1. Iterate compile units:

   * `for (auto &CU : DICtx->compile_units())`
2. For each CU, get its DIE tree root:

   * `DWARFDie CUDie = CU->getUnitDIE();`
3. Walk DIEs looking for `DW_TAG_subprogram` and test if it contains the address:

   * check `DW_AT_low_pc` / `DW_AT_high_pc` or `DW_AT_ranges`
   * LLVM has helpers like `DWARFDie::getLowAndHighPC(...)` and range iterators (varies slightly by LLVM version).

**Important:** some functions are described as:

* `DW_TAG_subprogram` with ranges
* or `DW_TAG_inlined_subroutine` for inlined instances (you usually want the “real” subprogram for signature)

For your use-case (decode ABI args), you want the **subprogram’s type**, not variable locations.

---

## 5) Extract return + parameter type DIEs

Once you have the `DWARFDie FnDie` for the function:

### Return type

* Return type is `DW_AT_type` on the subprogram DIE.
* If missing → `void`.

### Parameter types

Iterate children:

* `DW_TAG_formal_parameter`:

  * read `DW_AT_type` attribute → that DIE is the param type
* Ignore `DW_TAG_unspecified_parameters` (varargs “...”) if present

---

## 6) Reduce each type DIE to a primitive “decoder type”

You said you want “primitive types” for decoding args/retval. That typically means mapping down to:

* integer signed/unsigned + width
* float + width
* bool + width
* pointer + width
* (optional) enum (treat as int of width)
* otherwise: unknown / aggregate

### Implement “peel wrappers” loop

You’ll see chains like:
`typedef -> const -> pointer -> base_type`

So implement a helper:

**`PrimitiveType classify(DWARFDie T)`**:

1. While `T.tag()` is one of:

   * `DW_TAG_typedef`
   * `DW_TAG_const_type`
   * `DW_TAG_volatile_type`
   * `DW_TAG_restrict_type`
   * `DW_TAG_atomic_type`
   * `DW_TAG_shared_type` (rare)
     then set `T = T.getAttributeValueAsReferencedDie(DW_AT_type)` and continue

2. If `T.tag() == DW_TAG_pointer_type`:

   * pointer size:

     * prefer `DW_AT_byte_size` if present
     * else use address size from the CU (`CU->getAddressByteSize()`)
   * return `{kind:pointer, size:N}`

3. If `T.tag() == DW_TAG_base_type`:

   * read `DW_AT_encoding` and `DW_AT_byte_size`
   * map encodings:

     * `DW_ATE_signed`, `DW_ATE_signed_char` → signed int
     * `DW_ATE_unsigned`, `DW_ATE_unsigned_char` → unsigned int
     * `DW_ATE_boolean` → bool
     * `DW_ATE_float` → float
     * (optionally) `DW_ATE_UTF`, etc. → treat as unsigned int or unknown
   * return with size

4. If `T.tag() == DW_TAG_enumeration_type`:

   * try `DW_AT_type` underlying base type; if present, recurse
   * else fall back to byte_size + assume signed/unsigned unknown

5. If `T.tag() == DW_TAG_subroutine_type`:

   * this is a **function pointer type** if it arrived via a pointer; for ABI decoding, treat as pointer-sized

6. If `T.tag()` is struct/union/array:

   * for ABI arg decoding you usually **treat as pointer** if the arg is actually a pointer
   * but if the arg is passed **by value** (rare in C APIs but possible), you’ll need ABI rules. For a first pass, mark as “aggregate/unknown”.

**For tracing function args in C**, most non-primitive things are pointers anyway, so this works well.

---

## 7) (Real world) Make it work with separate debug info

Many `.so` are stripped and the DWARF lives in a separate file (e.g. `/usr/lib/debug/.build-id/xx/yyyyyy.debug`).

LLVM’s `DWARFContext::create` reads what’s in the object you opened. If it’s stripped, you need to:

* locate the separate debug file (by build-id / `.gnu_debuglink`), open *that* ObjectFile, and build DWARFContext from it **instead** (or use it as the DWARF source while still using the original ELF for symbols).

In other words, you may need a small “debug file resolver”:

* read `.note.gnu.build-id` → build-id
* map to debug path
* or read `.gnu_debuglink` section → filename + CRC
  Then open the debug ELF and create `DWARFContext` from it.

(If you skip this step, your code will work great on your own `-g` builds but fail on system libs.)

---

## 8) Output: a minimal signature object for your BPF decoder

Return something like:

```text
Signature {
  ret: {kind, size}
  params: [{kind, size}, ...]
  has_varargs: bool
}
```

Then your eBPF-side / user-space decoder can:

* read raw regs/stack slots
* interpret width/signedness
* treat pointers as u64 and optionally dereference with `bpf_probe_read_user` etc.

---

## Minimal LLVM API checklist (so you don’t get lost)

You’ll almost certainly touch these headers/classes:

* `llvm/Object/Binary.h`
* `llvm/Object/ObjectFile.h`
* `llvm/Object/SymbolSize.h` (optional)
* `llvm/DebugInfo/DWARF/DWARFContext.h`
* `llvm/DebugInfo/DWARF/DWARFDie.h`
* `llvm/DebugInfo/DWARF/DWARFUnit.h`
* `llvm/DebugInfo/DWARF/DWARFFormValue.h` (for attribute extraction)
* `llvm/Support/Error.h`, `llvm/Support/MemoryBuffer.h`

---

## One implementation tip that saves pain

When matching a function by address, don’t require exact equality of `low_pc`.
Instead:

* find a DIE whose ranges **contain** the address
* or choose the DIE whose `low_pc` is the nearest <= address and contains it

This handles:

* symbols pointing into thunks,
* identical code folding,
* minor address differences.

---

## Workflow

Direct LLVM calls for ELF/symbol/DWARF lookup (in orchestration code) → return/parameter extraction → recursive type reduction → primitive classification → signature emission.

## Project Structure

Sigminer now keeps the project-specific logic and calls LLVM APIs directly where needed, instead of wrapping each LLVM step in its own module.

1. End-to-end orchestration and public API surface
   - `src/sigminer.cpp`
   - `include/sigminer/sigminer.h`

2. Return/parameter extraction + recursive type reduction
   - `src/signature_builder.cpp`
   - `src/internal/signature_builder.h`

3. Primitive classification
   - `src/type_classifier.cpp`
   - `src/internal/type_classifier.h`

4. Signature emission / public result model
   - `include/sigminer/signature.h`

5. Build/version metadata
   - `CMakeLists.txt`
   - `include/sigminer/version.h`

## Development Overview

Sigminer opens the target shared object (.so) with LLVM Object APIs, resolves the function address (if needed), builds a DWARF context, and finds the corresponding subprogram DIE by address range. These steps are done directly with LLVM APIs from orchestration code rather than through project-local wrappers.

After locating the correct subprogram DIE, Sigminer extracts type information. The function’s return type is obtained from the DW_AT_type attribute on the subprogram DIE, defaulting to void if absent. Parameter types are gathered by iterating over child DIEs tagged as DW_TAG_formal_parameter, reading each parameter’s DW_AT_type. If a DW_TAG_unspecified_parameters entry appears, the function is marked as variadic. At this stage, the raw type DIEs often represent layered type constructs such as typedefs, qualifiers, or pointers rather than primitive forms.

To normalize these types for ABI decoding, Sigminer implements a recursive “type peeling” classifier. It repeatedly resolves wrapper DIEs such as DW_TAG_typedef, DW_TAG_const_type, DW_TAG_volatile_type, and similar qualifiers by following their referenced DW_AT_type. Once reduced to a concrete type, it classifies the DIE according to its tag. DW_TAG_base_type entries are mapped using their DW_AT_encoding and DW_AT_byte_size into primitive categories such as signed/unsigned integers, booleans, or floating-point types. Pointer types are identified via DW_TAG_pointer_type, using either their explicit byte size or the compile unit’s address size. Enumerations are reduced to their underlying integer types when available. Subroutine types reached through pointers are treated as pointer-sized values for ABI purposes. Aggregate types such as structs, unions, and arrays are marked as unknown or aggregate unless ABI-specific handling is later introduced.

In real-world scenarios, stripped binaries may keep debug information in separate files. That path can still be added, but it is intentionally not split into a dedicated wrapper module in this layout.

Finally, Sigminer emits a minimal, normalized signature representation suitable for BPF-based tracing and argument decoding. The resulting structure contains the return type classification, a list of parameter primitive descriptors (kind and size), and a flag indicating variadic functions. This compact signature enables user-space or eBPF components to read raw registers or stack slots and interpret them correctly according to width and signedness, treating pointers as fixed-width integers and optionally dereferencing them safely.
