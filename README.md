# Lunify

A fast, dependency-free Lua and Luau minifier written in C++.

Lunify compresses Lua 5.1+, Lua 5.2+ and Roblox Luau source by stripping
comments, removing redundant whitespace, renaming locals, folding constant
expressions, eliminating dead code, and optionally XOR-encoding string
literals. It ships as a static library plus a CLI.

## Features

- Token-level minification (always safe, used as a fallback on parse errors)
- Full AST pipeline with a real parser and semantic analysis
- Three compression levels, tunable via CLI flags or a JSON config file
- Removes comments (line and long) and unneeded whitespace
- Strips Luau type annotations and `type` statements
- Shortens number literals (`3.00` -> `3`, `0.5` -> `.5`)
- Renames locals by usage frequency to the shortest valid names
- Constant folding of arithmetic, string concat, comparisons, boolean logic
- Dead code elimination (unreachable code, unused locals, unused `local function`)
- Inlines single-use literal locals
- Removes unused trailing function parameters
- Optional XOR string encoding with an inline `_D` decoder (level 3)
- Optional debug-call stripping (`print`/`warn`)
- Rename prefix mode for greppable output
- `keep` list to preserve specific identifiers

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Only a C++17 compiler and CMake 3.16+ are required.

## CLI usage

```sh
./build/lunify_cli input.lua                 # minify to stdout
./build/lunify_cli input.lua -o out.lua      # write to file
./build/lunify_cli -l 3 input.lua            # max compression
./build/lunify_cli -c lunify.json input.lua  # config file
```

Full option list: `./build/lunify_cli --help`.

## Config file

```json
{
  "level": 2,
  "stripDebug": true,
  "encodeStrings": false,
  "renamePrefix": "",
  "keep": ["_G"]
}
```

See `docs/configuration.md` for the complete reference and
`examples/lunify.example.json` for a full example.

## Library usage

```cpp
#include <lunify/minifier.hpp>
#include <lunify/options.hpp>

lunify::Options opts;          // defaults to level 2
opts.level = 3;
lunify::applyLevel(opts);

lunify::MinifyResult r = lunify::minify(source, opts);
if (r.ok) {
    std::cout << r.output;
}
```

## Layout

```
include/lunify/   public headers
src/              implementation
tests/            unit tests (CTest)
examples/         sample input and config
benchmarks/       micro benchmark
```

## License

MIT
