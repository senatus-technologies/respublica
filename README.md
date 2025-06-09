# Celeritas

Project "Celeritas" is a next generation decentralized network that can handle 500,000+ TPS and sub-second finality.

### Project Structure

This project's structure follows the [Pitchfork](https://api.csswg.org/bikeshed/?force=1&url=https://raw.githubusercontent.com/vector-of-bool/pitchfork/develop/data/spec.bs) specification.

```
├── build/    # An ephemeral directory for building the project. Not checked in, but excluded via .gitignore.
├── data/     # Contains Protobuf definitions.
├── external/ # Contains external projects not available through Conan.
├── include/  # Contains all public headers.
├── src/      # Contains all source code, private headers, and unit tests.
├── tests/    # Contains integration tests, benchmarks, and profilers.
└── tools/    # Contains additional tooling, primarily WASM test code.
```

### Requirements

- A modern compiler capable of C++ 23 (clang 19+ is recommended).
- [Conan Package Manager](https://conan.io/downloads).

### Building

Celeritas's build process is managed using CMake. Additionally, all dependencies are managed and built through Conan or included directly as vendored projects under `external`. This means that all dependencies are downloaded and built during configuration rather than relying on system installed libraries.

```
cmake --preset default
cmake --build --preset release
```

CMake profiles are used for configuring and building the project.

Configuration Profiles:
 - `default`

Build Profiles:
 - `debug`
 - `release`

### Testing

Tests are built automatically.

Unit tests and integration tests can be invoked individually (e.g. `./build/tests/integration/Release/integration` or `./build/src/koinos/crypto/Release/crypto_tests`) or through ctest.

```
ctest --profile all
```

The profiler and benchmark must be run explicitly.

```
./build/tests/benchmark/Release/benchmark
```

```
./build/tests/profile/Release/profile
```

The benchmark should be ran with nice for better performance.

```
sudo nice --adjustment=-20 ./build/tests/benchmark/Release/benchmark
```

The profiler will write artifacts to the project root directory of the form `cpu.transactions.cpu.out`. Tests using at least 1 GiB of heap memory will also write heap profile(s).

Use `pprof` to view the results (on Ubuntu the package and binary are `google-pprof`).

```
pprof --text ./build/tests/profile/Release/profile coin.transactions.cpu.out
```

Increasing sample frequency of the profiler can improve accuracy at the cost of speed. The default frequency is 1000.

```
CPUPROFILE_FREQUENCY=10000 ./build/tests/profile/Release/profile
```

### TCMalloc

Celeritas links to TCMalloc for better concurrent memory performance. TCMalloc [recommends] system level configurations for optimal performance of Transparent Huge Pages (THP).

```
/sys/kernel/mm/transparent_hugepage/enabled:
    [always] madvise never

/sys/kernel/mm/transparent_hugepage/defrag:
    always defer [defer+madvise] madvise never`

/sys/kernel/mm/transparent_hugepage/khugepaged/max_ptes_none:
    0

/proc/sys/vm/overcommit_memory:
    1
```

### Conan

Celeritas uses Conan for package management. Most configuration options for dependencies are able to be specified in the provided `conanfile.txt`. However, some dependencies do not expose the necessary configuration options. As a workaround, they can be specified in the Conan profile located at `~/.conan2/profiles/default`.

Add the following to the end of the profile:

```
[conf]
tools.cmake.cmaketoolchain:extra_variables={"WAMR_DISABLE_HW_BOUND_CHECK": "1", "WAMR_DISABLE_WRITE_GS_BASE": "1", "ABSL_ALLOCATOR_NOTHROW": "1"}
```

Conan will not detect these changes and automatically rebuild. If you already built these dependencies prior to making this change, you will need to rebuild them. The easiest way to do this is to have conan remove the dependencies locally and force a rebuild.

```
conan remove "wasm-micro-runtime/*"
conan remove "abseil/*"
```

### Formatting

Formatting of the source code is enforced by ClangFormat. If ClangFormat is installed, build targets will be automatically generated. The library's code style can be reviewed by uploading the included `.clang-format` to https://clang-format-configurator.site/.

The target `format.check` can be built to check formatting and `format.fix` to attempt to automatically fix formatting. It is recommended to check and manually fix formatting as automatic formatting can unintentionally change code.

```
cmake --build --profile release --target format.check
```

### Coverage

A coverage build can be configured using the coverage profile. Using the coverage preset currently requires building with clang.

```
cmake --preset coverage
```

As before, all tests can be run using `ctest` or individually by calling their specific library.

```
ctest --preset all -j
```

This will create coverage reports in the build directory next to each test binary. The following commands can be used to easily find, merge, and generate a unified coverage report.

```
find ./ -name "*.profraw" | xargs llvm-profdata merge --output=celeritas.profdata
```

```
find ./ -name "*_tests" | xargs printf -- "-object %s " | xargs llvm-cov show --instr-profile=celeritas.profdata --format=html --output-dir=./build/coverage_report --ignore-filename-regex="external/.*" --ignore-filename-regex=".*\.test\.cpp" --ignore-
filename-regex="tests/.*"
```

This will generate a local html report in `build/coverage_report`.

### Static Analysis

A lot of static analysis is done during a normal build using `-Wall` and `-pedantic-errors`. More analysis can be done using `clang-tidy`. To configure this build, use the `static-analysis` preset  and then build as normal.

```
cmake --preset static-analysis
cmake --build --preset release
```

### Sanitizers

Sanitizers may also be run using different profiles. The three sanitizers, address, stack, and thread, may be configured using their respective profiles. Once configured, build normally and the sanitizers will be included in all compiled binaries.

```
cmake --preset address-sanitizer
```

```
cmake --preset stack-sanitizer
```

```
cmake --preset thread-sanitizer
```

### Contributing

As an open source project, contributions are welcome and appreciated. Before contributing, please read the [Contribution Guidelines](CONTRIBUTING.md).
