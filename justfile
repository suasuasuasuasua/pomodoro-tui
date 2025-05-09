# Justfile for Pomodoro TUI (C++)

# Build the project using CMake
build:
    cmake -S . -B build
    cmake --build build

# Run the built Pomodoro timer
run: build
    ./build/pomodoro

# Run the Pomodoro timer with debug timers enabled

# (shows short study/break options)
debug: build
    ./build/pomodoro --debug

# Clean build artifacts
clean:
    rm -rf build

# Enter the Nix development shell
dev:
    nix develop

# Build with Nix flake
nix-build:
    nix build

# Run with Nix flake (pass extra args after --)
nix-run *ARGS:
    nix run . -- {{ ARGS }}

# Generate documentation with Doxygen

# Usage: just docs
docs:
    doxygen Doxyfile
