# logos-module-viewer

A Qt UI application for viewing Logos modules.

## Usage

```bash
./logos-module-viewer --module ./path/to/module.dylib
```

This will load the specified Qt plugin module and display its methods in the UI.

## How to Build

### Using Nix (Recommended)

#### Build the Application

```bash
# Build the app (default)
nix build

# Or explicitly
nix build '.#app'
```

The result will include:
- `/bin/logos-module-viewer` - The Module Viewer application

#### Development Shell

```bash
# Enter development shell with all dependencies
nix develop
```

**Note:** In zsh, you need to quote the target (e.g., `'.#app'`) to prevent glob expansion.

If you don't have flakes enabled globally, add experimental flags:

```bash
nix build --extra-experimental-features 'nix-command flakes'
```

The compiled artifacts can be found at `result/`

#### Running the Application

After building with `nix build`, you can run it:

```bash
# Run the application
./result/bin/logos-module-viewer
```

#### Nix Organization

The nix build system is organized into modular files in the `/nix` directory:
- `nix/default.nix` - Common configuration (dependencies, flags, metadata)
- `nix/app.nix` - Application compilation

## Output Structure

When built with Nix:

```
result/
└── bin/
    └── logos-module-viewer    # Qt application
```

## Requirements

### Build Tools
- CMake (3.16 or later)
- Ninja build system
- pkg-config

### Dependencies
- Qt6 (qtbase)
- Qt6 Widgets (included in qtbase)

