# SDL3 + ImGui + OpenGL Sandbox

## Prerequisites

- **CMake** 3.30 or higher
- **C++ Compiler** with C++20 support
- **Git** for fetching dependencies
- **Meson** and **Ninja** — required to build `libepoxy` (handled automatically during the CMake build)

### Platform-specific dependencies

#### Debian / Ubuntu
```bash
sudo apt-get install \
  meson ninja-build pkg-config \
  libegl1-mesa-dev
```

#### macOS — Homebrew
```bash
brew install meson ninja pkg-config
```

#### macOS — MacPorts
```bash
sudo port install meson pkgconfig
```

#### Windows
No manual steps needed. The CI pipeline cross-compiles Windows binaries from
Linux using [llvm-mingw](https://github.com/mstorsjo/llvm-mingw). If you want
to build locally on Windows, install [MSYS2](https://www.msys2.org/) and use
its `mingw64` environment with `meson`, `ninja`, and `mingw-w64-x86_64-clang`.

---

## Building

`libepoxy` is fetched and compiled automatically as part of the CMake build —
no separate build step is required.

1. Clone the repository:
   ```bash
   git clone https://github.com/abaire/sdl_imgui_sandbox.git
   cd sdl_imgui_sandbox
   ```

2. Configure:
   ```bash
   cmake -B build -S .
   ```

3. Build:
   ```bash
   cmake --build build
   ```

4. Run:
   ```bash
   ./build/sandbox        # Linux
   open build/sandbox.app # macOS
   ```
