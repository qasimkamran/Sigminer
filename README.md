# Sigminer

Sigminer is a prototype C++ tool for extracting simplified function signatures from ELF binaries and shared objects by reading DWARF debug information through LLVM. The
core idea is to take a binary plus a target function name, locate the corresponding DW_TAG_subprogram entry in the DWARF tree, inspect its declared return type and
formal parameters, and reduce those DWARF types into a compact, machine-usable representation. Instead of preserving the full richness of C/C++ type information, the
prototype collapses types into a smaller set of categories such as integer, float, bool, pointer, enum, aggregate, or unknown, while also recording size, signedness,
and whether the value is pointer-shaped. That makes the output suitable for downstream tooling that cares less about source-level type fidelity and more about ABI-level
decoding, tracing, or instrumentation.

The repository now keeps the prototype flow, but split into a small library plus a thin demo executable. The library owns DWARF session setup, subprogram lookup,
type unwrapping, type classification, and signature assembly. The `proto` executable is just a front-end that calls the library and prints the mined signature.

## File map

### Public headers

- `include/sigminer/sigminer.h`: Main public API entrypoint. Declares `Result`, `ReturnCode`, and `GetSignatureFromSharedObjectBySymbol(...)`.
- `include/sigminer/signature.h`: Public data model for extracted signatures and simplified types.

### Internal headers

- `src/internal/dwarf_session.h`: Internal session object for opened binaries and their `DWARFContext`.
- `src/internal/signature_builder.h`: Internal interface for building a `sigminer::Signature` from a subprogram DIE.
- `src/internal/subprogram_finder.h`: Internal interface for locating a target `DW_TAG_subprogram` in the DWARF compile units.
- `src/internal/type_classifier.h`: Internal helpers for turning a resolved DWARF type DIE into a `sigminer::TypeEntry`.
- `src/internal/type_resolver.h`: Internal helpers for stripping typedef/const/volatile-style wrappers and resolving underlying types.

### Library sources

- `src/dwarf_session.cpp`: Opens the input file as an LLVM object file and creates the DWARF session state used by the library.
- `src/sigminer.cpp`: Main orchestration layer for the public API. Wires together file opening, symbol lookup, and signature building.
- `src/signature_builder.cpp`: Builds return and parameter type entries from a function DIE and assembles the final `Signature`.
- `src/subprogram_finder.cpp`: Searches compile units for the requested function/subprogram DIE.
- `src/type_classifier.cpp`: Classifies resolved DWARF types into Sigminer’s reduced type model, including kind, size, signedness, and display name.
- `src/type_resolver.cpp`: Resolves through DWARF wrapper/reference layers to find the underlying type used for classification.

### Prototype executable

- `src/prototype.cpp`: Small CLI demo that calls the library and prints the extracted signature in a prototype-friendly text format.
