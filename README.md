# Sigminer

A set of C++ libraries for mining signature and type information from ELF object files without having access to any of the source code. Primarily, this repository was built to offer a simple way of taking an object file and its function name to then output simply its return type and parameter types along with some descriptors about these two like size, signedness, etc. (this very functionality can be previewed by running the `proto` executable once built). In an effort to be purpose-fit, I have kept the scope *and implementation* as simple as I could make it, the C ABI is just one example of how purpose-fit is the motive and supplementary files like `printf_specifier_mapper` and `bpftrace_mapper` showcase motive for extending according to usecase.

### `objdump`? `readelf`?

These tools dump info and can be used to check for compatibility with `QuickApprove.sh` but Sigminer let's you get a bit more out of the ELF parsing and use in your tools.

## Usecases

#### 1. Tracing
Generate accurate function prototypes for tracing tools like `bpftrace`
#### 2. Generating FFI
Deriving bindings for other languages through type metadata.
#### 3. Custom Tooling
Lightweight ELF parser that can be extended for tool usecase, see `*_mapper` files for example.
#### 4. Debugging Low-Observability Encironments
Infer function interface when attaching debuggers is costly, particularly helpful in production environments. Provides insights where logging and telemetry is limited as well.

## Check out the wiki pages

I am making a continued effort to write about these modules, their usecases and just generally treat the wiki pages section on Github as a Blog for the project. Saves me from flooding README with walls of text too.

## Contributions

Contributions are welcome as bug reports that include some form of repro steps at the moment, please raise Github issues for these. Not taking PRs or direct patches at the moment until a set of guidelines is in place.
