# Sigminer

Sigminer is a prototype C++ tool for extracting simplified function signatures from ELF binaries and shared objects by reading DWARF debug information through LLVM. The
core idea is to take a binary plus a target function name, locate the corresponding DW_TAG_subprogram entry in the DWARF tree, inspect its declared return type and
formal parameters, and reduce those DWARF types into a compact, machine-usable representation. Instead of preserving the full richness of C/C++ type information, the
prototype collapses types into a smaller set of categories such as integer, float, bool, pointer, enum, aggregate, or unknown, while also recording size, signedness,
and whether the value is pointer-shaped. That makes the output suitable for downstream tooling that cares less about source-level type fidelity and more about ABI-level
decoding, tracing, or instrumentation.

The current repository is split between an implementation prototype and the intended longer-term library structure. The executable prototype in src/prototype.cpp
contains the full working flow end to end: it initializes LLVM, opens the input ELF as an object file, builds a DWARFContext, searches compile units for a matching
subprogram DIE, resolves wrapper types like typedef and const, classifies the underlying type, and prints the mined signature. Around that, the repository defines a
cleaner public API and a modular layout for a future library version, with separate areas planned for orchestration, signature building, and type classification, but
those library pieces are still mostly scaffolding. In other words, the project already proves the core concept of mining callable type information from DWARF, while
also serving as the foundation for a more complete signature extraction library aimed at tracing, BPF-style argument decoding, and other binary introspection workflows.
