# Pomodoro TUI (C++)

A simple terminal-based Pomodoro timer written in C++ using ncurses, managed with Nix flakes.

---

**Built with the help of the GPT-4.1 agent for fun and modern C++ best practices!**

---

## Features

-   Start, pause, and reset the timer
-   TUI display with countdown
-   Keyboard shortcuts: `s` (start/pause), `r` (reset), `q` (quit)
-   Menu to select study and break durations (including debug options)
-   Continuous Pomodoro/break cycles with prompts and quit options
-   Modern, readable C++23 codebase

## Dependencies

-   **Nix**: 2.25.3 or newer
-   **G++**: 13.3.0 or newer (C++23 support)
-   **CMake**: 3.30.5 or newer
-   **ncurses**: (provided by Nix)

All dependencies are managed automatically with Nix flakes.

## Getting Started

### 1. Enter the Development Shell

This will provide you with all required dependencies:

```sh
nix develop
```

### 2. Build and Run with Nix

To build the project:

```sh
nix build
```

The binary will be in `result/bin/pomodoro`.

To run directly with Nix:

```sh
nix run [-- --debug]
```

- Use the `--debug` flag to enable short (10s/5s) study/break options for testing.

### 3. Manual Build with CMake

If you prefer to build manually (after `nix develop`):

```sh
cmake -S . -B build
cmake --build build
./build/pomodoro [--debug]
```

## Source Structure

-   All source code is in the `src/` directory.
-   Main entry point: `src/main.cpp`
-   Core logic: `src/pomodoro.cpp`, `src/pomodoro.h`

## License

MIT
