# Fase 2 — Descubrimiento de archivos fuente: diseño

Fecha: 2026-08-04
Estado: aprobado para pasar a plan de implementación

## 1. Objetivo

Localizar los archivos fuente C/C++ de un proyecto a partir de una ruta raíz, aplicando exclusiones de directorio y de patrón, y exponer el resultado tanto como biblioteca reutilizable (`codeqa::infrastructure` / `codeqa::application`) como a través de un subcomando real de la CLI: `codeqa discover <path>`.

Esta fase **no** lee el contenido de los archivos. Solo enumera rutas y metadatos. La carga de contenido queda para el frontend que lo necesite (Fase 4, tree-sitter), que leerá cada archivo directamente cuando lo procese.

## 2. Alcance

- recorrido recursivo de un directorio raíz;
- clasificación por extensión (C, C++, header ambiguo);
- exclusión de un conjunto fijo de directorios conocidos;
- exclusión adicional mediante patrones glob pasados por CLI (`--exclude`);
- orden determinista de los archivos descubiertos;
- manejo de errores de acceso por archivo sin abortar el descubrimiento completo;
- manejo de una ruta raíz inexistente o inválida como error claro de CLI;
- subcomando `codeqa discover <path> [--exclude <glob>]...` con salida de texto.

### Fuera de alcance (explícitamente diferido)

- Lectura o carga del contenido de los archivos (Fase 4 y posteriores).
- Detección real del lenguaje de los `.h` ambiguos (Fase 3).
- Configuración de exclusiones vía `codeqa.json` (Fase 8) — en esta fase las exclusiones de patrón solo existen como flag `--exclude` de la CLI.
- Límite de tamaño de archivo o tratamiento especial de archivos vacíos: se descubren con normalidad, sin umbral.
- Seguir enlaces simbólicos.
- Formato de salida JSON (Fase 9, Informes).

## 3. Decisiones de diseño

1. **Solo descubrimiento, sin carga de contenido.** Evita mantener en memoria el contenido de proyectos grandes cuando aún no hay nada que lo consuma; tree-sitter (Fase 4) puede leer los archivos directamente.
2. **Exclusiones de directorio: lista fija embebida.** `.git`, `build/`, `cmake-build-*/`, `.worktrees/`, y cualquier directorio oculto (nombre que empieza por `.`). Sin auto-descubrimiento de configuración, coherente con el determinismo ya fijado en la Fase 1.
3. **Patrones de exclusión: flag `--exclude` repetible en la CLI**, no diferido por completo a la Fase 8. Fase 8 reutilizará el mismo motor de coincidencia de patrones para exponerlo también vía `codeqa.json`.
4. **`discover` es un subcomando real y usable en esta fase**, no solo un caso de uso interno — da valor demostrable y establece el patrón de subcomandos que reutilizarán fases futuras (p.ej. `analyze`).
5. **No seguir enlaces simbólicos** durante el recorrido. Evita ciclos y comportamiento no determinista sin necesidad de detección de ciclos.
6. **Archivos vacíos y archivos grandes se descubren con normalidad**, sin exclusión ni umbral de tamaño. El tamaño se registra en los metadatos por si una fase futura decide imponer un límite práctico.
7. **Errores de acceso por archivo se reportan como `codeqa::core::Diagnostic`**, reutilizando el mismo tipo que usarán los errores de parsing más adelante — un único canal de diagnósticos en todo el pipeline.
8. **Recorrido con `std::filesystem::recursive_directory_iterator` + poda** (`disable_recursion_pending()` al entrar en un directorio excluido). El orden de iteración no es determinista entre plataformas, así que la lista final se ordena por ruta normalizada antes de devolverse.
9. **Coincidencia de patrones glob mediante un traductor glob→regex minimalista** (`*`, `?`, comparación por segmentos de ruta) sobre `std::regex`. Sin dependencias nuevas; el subconjunto de sintaxis glob soportado queda acotado a lo que cubre este traductor.
10. **La ruta raíz inexistente o inválida es un error de CLI, no un `Diagnostic`.** `RunApplication` valida la raíz con `std::filesystem::exists`/`is_directory` antes de invocar el caso de uso, de forma consistente con la validación de opciones desconocidas ya existente. Esto mantiene el motor de descubrimiento libre de un segundo canal de error para "no hay nada que descubrir".

## 4. Arquitectura y modelo de datos

Nueva capa `codeqa::infrastructure` (prevista desde la Fase 1, no creada hasta ahora), con el motor de recorrido de archivos. `codeqa::application` añade un caso de uso fino que la envuelve, siguiendo el mismo patrón que `GetApplicationInfo` (clase con `execute()`).

```
include/codeqa/infrastructure/
├── ExtensionCategory.h      (enum class)
├── DiscoveredFile.h         (struct)
├── DiscoveryOptions.h       (struct)
├── DiscoveryResult.h        (struct)
├── GlobMatcher.h            (traductor glob→regex)
└── DiscoverSourceFiles.h    (motor de recorrido)

src/infrastructure/
├── GlobMatcher.cpp
└── DiscoverSourceFiles.cpp

include/codeqa/application/
└── DiscoverProjectSources.h   (caso de uso: delega en infrastructure, sin lógica propia)

src/application/
└── DiscoverProjectSources.cpp
```

### Modelo de datos

```cpp
// codeqa::infrastructure
enum class ExtensionCategory { C, Cpp, AmbiguousHeader };

struct DiscoveredFile
{
    std::filesystem::path path;      // normalizada, relativa a la raíz
    ExtensionCategory category;
    std::uintmax_t sizeBytes;
};

struct DiscoveryOptions
{
    std::filesystem::path rootPath;
    std::vector<std::string> excludePatterns;   // globs de --exclude, además de la lista fija embebida
};

struct DiscoveryResult
{
    std::vector<DiscoveredFile> files;                    // orden determinista por ruta
    std::vector<codeqa::core::Diagnostic> diagnostics;    // uno por archivo inaccesible
    std::size_t excludedCount{0};                         // ver nota de conteo más abajo
};
```

**Nota sobre el conteo de exclusiones:** como el recorrido poda los directorios excluidos sin descender en ellos (decisión 8), no es posible conocer cuántos archivos hay dentro de un directorio excluido sin recorrerlo — y hacerlo anularía el propósito de la poda (evitar recorrer árboles grandes como `build/`). Por tanto `excludedCount` cuenta **entradas excluidas en el punto en que se excluyen**: un directorio completo excluido por la lista fija cuenta como 1, no como N archivos dentro; un archivo excluido individualmente por `--exclude` cuenta como 1. Esto difiere de una lectura literal de "Excluded files: 14" del roadmap como recuento profundo de archivos, pero es la lectura consistente con la estrategia de recorrido ya aprobada.

`ExtensionCategory` es una clasificación **solo por extensión**, no la detección de lenguaje real de la Fase 3 (que probablemente necesite mirar contenido para los `.h` ambiguos). Por eso vive en `infrastructure`, no en `core`: no se comparte todavía con el resto del pipeline.

Extensiones reconocidas (del roadmap):

```text
C:   .c
C++: .cc .cpp .cxx .hpp .hh .hxx
Ambiguo: .h
```

Los archivos con extensión no soportada (`.py`, `.md`, etc.) no se descubren y no cuentan como excluidos: "excluido" queda reservado para lo descartado por directorio fijo o por `--exclude`.

`DiscoverSourceFiles` (infraestructura) asume que `rootPath` existe y es un directorio — esa validación ya la hizo `RunApplication` antes de invocar el caso de uso (decisión 10).

## 5. CLI

```
codeqa discover <path> [--exclude <glob>]...
```

`RunApplication` pasa de reconocer 3 formas (vacío/`-h`/`-v`) a reconocer también el verbo `discover`:

- `discover` sin `<path>` → `Error [CLI002]: Missing required argument <path> for 'discover'.`, `ExitCode::InvalidArguments`.
- `<path>` no existe o no es un directorio → `Error [CLI003]: Path '<path>' does not exist or is not a directory.`, `ExitCode::InvalidArguments`.
- Recorrido completado (incluso con archivos inaccesibles individuales) → `ExitCode::Success`, mostrando el resumen de texto.
- `--help` documenta `discover` y `--exclude`.

Salida de texto (formateada en `RunApplication` a partir de `DiscoveryResult`):

```text
Files discovered: 42
C files: 7
C++ files: 29
Ambiguous headers: 6
Excluded files: 14
Read errors: 0
```

`Read errors` es `diagnostics.size()`. `Excluded files` es `excludedCount` (directorio fijo + patrones `--exclude`).

## 6. Manejo de errores

| Situación | Resultado |
|---|---|
| Ruta raíz no existe / no es directorio | `ExitCode::InvalidArguments`, mensaje CLI003, no se invoca el caso de uso |
| Falta `<path>` tras `discover` | `ExitCode::InvalidArguments`, mensaje CLI002 |
| Archivo individual inaccesible durante el recorrido (permisos, etc.) | Se añade un `codeqa::core::Diagnostic` a `DiscoveryResult.diagnostics`; el recorrido continúa; `ExitCode::Success` al final |
| Excepción inesperada no contemplada | Capturada por la barrera try/catch ya existente en `main.cpp`; `ExitCode::InternalError` |

## 7. Tests

- **Unitarios** (`tests/unit/DiscoverSourceFilesTest.cpp`): árboles de directorios reales creados en un directorio temporal (`std::filesystem::temp_directory_path()`) dentro del propio test, sin mocks, limpiados al finalizar. Casos: categorización por extensión (incluyendo `.h` como ambiguo), exclusión de cada directorio de la lista fija, `--exclude` con patrones glob, orden determinista, symlink no seguido, archivo inaccesible → `Diagnostic` sin abortar el resto, archivo vacío incluido con normalidad.
- **Unitarios** (`tests/unit/GlobMatcherTest.cpp`): el traductor glob→regex de forma aislada (`*`, `?`, comparación por segmentos).
- **Integración** (CTest, mismo patrón que `CheckCliOutput.cmake` de la Fase 1): fixture pequeño y real en `tests/integration/fixtures/discover-sample/` (unos pocos `.c`/`.cpp`/`.h` y un subdirectorio excluido por la lista fija), invocando el binario `codeqa discover` real y comprobando la salida de texto y el exit code.

## 8. Criterios de aceptación

- todos los archivos con extensión soportada bajo la raíz se descubren;
- las exclusiones por directorio fijo y por `--exclude` se respetan;
- el resultado mantiene un orden estable entre ejecuciones;
- una ruta raíz inexistente produce un error de CLI claro (`ExitCode::InvalidArguments`);
- un archivo individual inaccesible no detiene el resto del descubrimiento;
- `codeqa discover <path>` funciona end-to-end y produce la salida de texto documentada;
- `codeqa --help` documenta el nuevo subcomando.

## 9. Dependencias

Fase 1 (fundación técnica: `codeqa::core::Diagnostic`, `ExitCode`, `RunApplication`, infraestructura de tests con doctest).
