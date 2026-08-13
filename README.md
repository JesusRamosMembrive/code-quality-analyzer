# Code Quality Analyzer

Static analysis tool for measuring and improving the quality, maintainability, and evolution of C and C++ source projects. It targets issues that don't prevent compilation but progressively degrade design: excessively long functions, high cyclomatic complexity, deep nesting, poor naming, duplication, and more.

This repository is currently in the **foundation phase**: the project builds, runs, and tests itself. It discovers C and C++ source files, detects their language, and parses them with tree-sitter, but does not yet perform static analysis on them.

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
./build/codeqa discover ./ruta/al/proyecto --exclude vendor
./build/codeqa discover ./ruta/al/proyecto --exclude "*_generated.cpp"
```

Recorre recursivamente el directorio indicado y localiza los archivos fuente C/C++ soportados (`.c`, `.cc`, `.cpp`, `.cxx`, `.hpp`, `.hh`, `.hxx`; `.h` se cuenta como ambiguo). Excluye por defecto `.git`, `build/`, `cmake-build-*/` y cualquier directorio oculto; `--exclude <glob>` (repetible) añade exclusiones adicionales. No lee el contenido de los archivos — solo enumera rutas y metadatos.

Reglas de coincidencia de `--exclude`: un patrón sin `/` coincide con el nombre del archivo o directorio a cualquier profundidad (por eso `--exclude vendor` excluye cualquier directorio `vendor`, esté donde esté anidado). Un patrón con `/` debe coincidir con la ruta relativa completa, y `*` nunca cruza un `/` — por ejemplo `*/vendor/*` solo coincide con rutas de exactamente tres segmentos como `src/vendor/foo.cpp`, no con `vendor/foo.cpp` ni con `src/vendor/nested/foo.cpp`. Los patrones siempre usan `/` como separador, incluso en Windows: una ruta como `src\vendor\*` no coincidirá con nada, porque `\` no es un comodín y las rutas de Windows se normalizan internamente a `/` antes de comparar.

### Detección de lenguaje

```bash
./build/codeqa discover ./ruta/al/proyecto --lang-override "include/network/*.h=c"
```

Cada archivo descubierto se lista con su lenguaje detectado (`C`, `C++` o `Unknown`). La detección se basa solo en la extensión: `.c` es siempre `C`, `.cc`/`.cpp`/`.cxx`/`.hpp`/`.hh`/`.hxx` son siempre `C++`, y un `.h` sin override queda como `Unknown` con confianza `0.50` — la herramienta no inventa certeza cuando no la hay, y esta fase sigue sin leer el contenido de los archivos.

`--lang-override <pattern>=<lang>` (repetible, `<lang>` es `c` o `cpp`) fuerza el lenguaje de los archivos cuya ruta coincide con `<pattern>`, con prioridad sobre la detección automática, incluso si la extensión era inequívoca. Si varios overrides coinciden con el mismo archivo, gana el último especificado.

Reglas de coincidencia de `--lang-override`: a diferencia de `--exclude`, un override solo se evalúa contra archivos individuales ya descubiertos, nunca contra directorios. Un patrón sin `/` coincide con el nombre de archivo del fichero clasificado (no con un directorio); un patrón con `/` debe coincidir con la ruta relativa completa del archivo. `*` nunca cruza un `/`, y aplican las mismas mecánicas de traducción de glob (`?`, escapado, separador `/` normalizado) que `--exclude` — pero el objetivo de la coincidencia es siempre un archivo, nunca un directorio.

## Parseo

```bash
./build/codeqa parse ./ruta/al/proyecto
./build/codeqa parse ./ruta/al/proyecto --exclude vendor --lang-override "legacy/*.h=c"
```

Descubre los archivos fuente (mismas reglas que `discover`), detecta su lenguaje
y los parsea con tree-sitter, reportando las regiones que la gramática no ha
podido interpretar.

Acepta los mismos flags que `discover`. Una cabecera `.h` sin `--lang-override`
se parsea con la gramática de C++, que acepta prácticamente todo C válido y
minimiza los errores falsos.

Los errores de parseo son información, no un fallo: `parse` devuelve 0 aunque
encuentre archivos rotos. `analyze`, en cambio, sí puede devolver un código de
salida distinto de cero a través del quality gate (ver más abajo).

## Códigos de salida

| Código | Significado |
|--------|--------------|
| 0 | Éxito |
| 1 | Quality gate no superado |
| 2 | Error de configuración |
| 3 | Argumentos inválidos |
| 4 | Error interno no controlado |

## Quality gate

`analyze` falla (código de salida 1) si aparece un finding nuevo -- respecto al
snapshot anterior -- con severidad igual o superior a `qualityGate.minSeverity`
(por defecto, `warning`).

Si no existe un snapshot anterior (o no se puede leer), el gate es absoluto:
cualquier finding que cumpla la severidad mínima lo hace fallar, no solo los
nuevos. El snapshot se guarda siempre, sin importar el resultado del gate, así
que repetir `analyze` sin cambios en el código tras un fallo pasará una vez
exista ese primer snapshot como referencia.

El campo de configuración acepta `"information"`, `"warning"` o `"error"`, por
ejemplo:

```json
{ "qualityGate": { "minSeverity": "warning" } }
```

## Estado

Proyecto en fase de fundación. Descubre archivos fuente C/C++, detecta su lenguaje y los
parsea con tree-sitter, pero todavía no los analiza.
