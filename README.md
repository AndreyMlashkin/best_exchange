# Best Exchange

Desktop application for finding the best offer when exchanging currencies
to Russian rubles.

## Technology

- C++20
- Qt 6 Widgets
- CMake
- Conan 2

## Build

Installing Qt through Conan can take a while. Run these commands only when you
are ready to download and build the dependencies:

```shell
# One-time setup on a new machine
conan profile detect

conan install . --output-folder=build --build=missing -s build_type=Release
cmake --preset conan-release
cmake --build --preset conan-release
```

For a debug build:

```shell
conan install . --output-folder=build --build=missing -s build_type=Debug
cmake --preset conan-debug
cmake --build --preset conan-debug
```

The project uses Qt 6.11.1 because older Qt releases do not compile with Apple
Clang 21 on Apple Silicon: they select `__yield` before the supported
`__builtin_arm_yield` intrinsic.

## BestChange API

The application uses the official BestChange API v2. Page scraping is not
used. An API key is required and can be obtained by joining the BestChange
referral program. Enter the key in the application; it is kept in memory only.

The application loads all currencies and exchangers, finds every Tether USDT
network and every RUB payout method, requests all corresponding pairs in one
batch, and displays the best active offer for each pair. Double-click an
exchanger name to open its BestChange redirect URL.

- API documentation: https://bestchange.app/
- API key registration: https://www.bestchange.com/partner/
