# Code Quality Analyzer

Static analysis tool for measuring and improving the quality, maintainability, and evolution of C and C++ source projects. It targets issues that don't prevent compilation but progressively degrade design: excessively long functions, high cyclomatic complexity, deep nesting, poor naming, duplication, and more.

This repository is currently in the **foundation phase**: the project builds, runs, and tests itself. It discovers C and C++ source files, but does not yet perform static analysis on them.

## Requisitos

- Compilador compatible con C++17 (GCC, Clang o MSVC).
- CMake >= 3.20 (necesario para `ctest --test-dir`; la configuración y compilación funcionan desde CMake 3.16).

## Compilación

```bash
cmake -S . -B build
cmake --build build
```

## Tests

```bash
ctest --test-dir build --output-on-failure
```

## Ejecución

```bash
./build/codeqa --help
./build/codeqa --version
```

En Windows, el ejecutable se genera normalmente en `build/Debug/codeqa.exe` o `build/Release/codeqa.exe`, según el generador y la configuración utilizados.

## Descubrimiento de archivos

```bash
./build/codeqa discover ./ruta/al/proyecto
./build/codeqa discover ./ruta/al/proyecto --exclude "*/vendor/*"
```

Recorre recursivamente el directorio indicado y localiza los archivos fuente C/C++ soportados (`.c`, `.cc`, `.cpp`, `.cxx`, `.hpp`, `.hh`, `.hxx`; `.h` se cuenta como ambiguo). Excluye por defecto `.git`, `build/`, `cmake-build-*/` y cualquier directorio oculto; `--exclude <glob>` (repetible) añade exclusiones adicionales. No lee el contenido de los archivos — solo enumera rutas y metadatos.

## Códigos de salida

| Código | Significado |
|--------|--------------|
| 0 | Éxito |
| 2 | Argumentos inválidos |
| 4 | Error interno no controlado |

## Estado

Proyecto en fase de fundación. Descubre archivos fuente C/C++, pero todavía no los analiza.
