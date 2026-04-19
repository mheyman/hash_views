# hash_view

`hash_view` is a header-only C++23 library that exposes cryptographic hashing
and hash verification as range adaptors.

You can pipe an input range into a hashing view to produce:

- a standalone hash range
- the original input with the hash appended
- a single-element verification view that yields `true` or `false`

## Status

This project may still evolve with new hash algorithms but I expect the 
existing API to remain stable.

I built this as an exercise for a really odd range adapter where output size
and input size don't really relate, the separate vs append behavior, the odd
(and not typically seen) input padding used when hashing to a view of
multi-byte elements, and, finally, the single-bool-holding verification result.
Because of all this, the underlying details have a lot of moving parts, like a
rolling buffer that only appears when needed and output behavior compiles 
differently depending on a template parameter.

## Features

- Header-only CMake package exported as `sph-hash::sph-hash`
- C++23 range adaptor API
- Hash generation and verification via `|` pipelines
- Separate or appended hash output
- Raw byte output or padded multi-byte output
- BLAKE2b parameter support for key, salt, and personalization
- Test coverage for:
  - BLAKE2b
  - BLAKE3
  - SHA-256
  - SHA-512
  - SHA3-256
  - SHA3-512

## Requirements

- C++23 compiler
- CMake 3.28 or newer
- vcpkg toolchain setup

Vcpkg port dependencies:

- `libsodium`
- `blake3`

Test-only dependencies:

- `doctest`
- `fmt`
- `daw-json-link`
- `magic-enum`

## Installation

This repository is set up to use `vcpkg` through the included
`CMakePresets.json`. With CMake-findable `libsodium` and `blake3`, vcpkg
isn't required.

### Build the library

Pick a preset that matches your platform and compiler, then configure and
build:

```bash
cmake --preset gcc-release
cmake --build out/build/gcc-release
cmake --install out/build/gcc-release
```

For developer builds with tests enabled:

```bash
cmake --preset gcc-debug-develop
cmake --build out/build/gcc-debug-develop
ctest --preset gcc-test-debug
```

Available preset families include:

- `gcc-*`
- `clang-*`
- `msvc-*`
- `clang-win-*`

### Consume from another CMake project

After installation:

```cmake
find_package(sph-hash CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE sph-hash::sph-hash)
```

## Usage

The public API lives primarily in:

- `sph/ranges/views/hash.h`
- `sph/ranges/views/hash_verify.h`

### Create a separate hash

```cpp
#include <ranges>
#include <string_view>
#include <vector>
#include <sph/ranges/views/hash.h>

auto input = std::string_view{"hello world"};

auto hash = input
    | sph::views::hash<sph::hash_algorithm::sha256>(24)
    | std::ranges::to<std::vector>();
```

Passing `0` uses the algorithm's full hash size. Passing a non-zero value
requests that many hash bytes.

### Append the hash to the original input

```cpp
#include <cstdint>
#include <ranges>
#include <vector>
#include <sph/hash_format.h>
#include <sph/hash_site.h>
#include <sph/ranges/views/hash.h>

std::vector<uint8_t> input{ 'h', 'e', 'l', 'l', 'o' };

auto appended = input
    | sph::views::hash<
        sph::hash_algorithm::sha256,
        uint8_t,
        sph::hash_format::raw,
        sph::hash_site::append>(24)
    | std::ranges::to<std::vector>();
```

In append mode, the output begins with the original input bytes and ends with
the computed hash bytes.

### Verify against a separate hash

```cpp
#include <cstdint>
#include <ranges>
#include <vector>
#include <sph/ranges/views/hash.h>
#include <sph/ranges/views/hash_verify.h>

std::vector<uint8_t> input{ 'h', 'e', 'l', 'l', 'o' };

auto hash = input
    | sph::views::hash<sph::hash_algorithm::sha256>(24)
    | std::ranges::to<std::vector>();

bool ok = *(input
    | sph::views::hash_verify<sph::hash_algorithm::sha256>(hash)
    | std::ranges::begin());
```

`hash_verify` returns a single-element range whose value is the verification
result.

### Verify appended data

```cpp
#include <cstdint>
#include <ranges>
#include <vector>
#include <sph/hash_format.h>
#include <sph/hash_site.h>
#include <sph/ranges/views/hash.h>
#include <sph/ranges/views/hash_verify.h>

std::vector<uint8_t> input{ 'h', 'e', 'l', 'l', 'o' };

auto appended = input
    | sph::views::hash<
        sph::hash_algorithm::sha256,
        uint8_t,
        sph::hash_format::raw,
        sph::hash_site::append>(24)
    | std::ranges::to<std::vector>();

bool ok = *(appended
    | sph::views::hash_verify<sph::hash_algorithm::sha256>(24)
    | std::ranges::begin());
```

### Use padded multi-byte output

For output element types larger than one byte, padded output lets the hash fit
cleanly into the destination element type.

```cpp
#include <cstdint>
#include <ranges>
#include <vector>
#include <sph/hash_format.h>
#include <sph/ranges/views/hash.h>

std::vector<uint8_t> input{ 'h', 'e', 'l', 'l', 'o' };

auto hash_words = input
    | sph::views::hash<
        sph::hash_algorithm::sha512,
        std::uint64_t,
        sph::hash_format::padded>(24)
    | std::ranges::to<std::vector>();
```

Padding uses a `0x80` terminator followed by `0x00` bytes as needed to fill the
destination element boundary.

### Supply BLAKE2b parameters

```cpp
#include <array>
#include <cstdint>
#include <ranges>
#include <vector>
#include <sph/blake2b_parameters.h>
#include <sph/ranges/views/hash.h>

std::vector<uint8_t> input{ 0x01, 0x02, 0x03 };
std::array<uint8_t, 16> salt{};
std::array<uint8_t, 16> personal{};

auto blake2b = sph::views::hash<sph::hash_algorithm::blake2b>(24)
    .with_blake2b_parameters(sph::blake2b_parameters{
        .salt = salt,
        .personal = personal,
    });

auto hash = input | blake2b | std::ranges::to<std::vector>();
```

For BLAKE2b, invalid key, salt, or personalization sizes are rejected with
`std::invalid_argument`.

## Supported Algorithms

- `sph::hash_algorithm::blake2b`
- `sph::hash_algorithm::blake3`
- `sph::hash_algorithm::sha256`
- `sph::hash_algorithm::sha512`
- `sph::hash_algorithm::sha3_256`
- `sph::hash_algorithm::sha3_512`

Use `sph::hash_param<A>::hash_byte_count()` to query the maximum output size for
an algorithm.

## Testing

The test suite lives under [`test`](./test) and uses JSON test vectors plus
round-trip coverage.

Run tests through a developer preset:

```bash
cmake --preset clang-debug-develop
cmake --build out/build/clang-debug-develop
ctest --preset clang-test-debug
```

## Project Layout

```text
hash/
  include/sph/...       Public headers
test/
  *.json                Test vectors
  unit_tests.cpp        Doctest-based coverage
CMakePresets.json       Preset-based configure/test workflow
vcpkg.json              Dependency manifest
```

## Limitations

- Hash sizes are currently byte-based, not bit-based.
- Padded output is primarily intended for multi-byte destination element types.
- The library currently documents usage through source and tests; richer docs
  can still be added.

## Contributing

Issues and pull requests are welcome.

## License

This repository is released under [`CC0 1.0`](./LICENSE).
