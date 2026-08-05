# Fase 3 — Detección inicial del lenguaje: diseño

Fecha: 2026-08-05
Estado: aprobado para pasar a plan de implementación

## 1. Objetivo

Asignar a cada archivo descubierto (Fase 2) un lenguaje probable (`C`, `C++` o `Unknown`), con un nivel de confianza y una razón explicable, dando prioridad a overrides explícitos del usuario sobre la detección automática por extensión. Exponer el resultado como parte de la salida de `codeqa discover`.

Esta fase **no** lee el contenido de los archivos ni aplica heurísticas de contenido. La detección se basa únicamente en la extensión (ya clasificada por `codeqa::infrastructure::ExtensionCategory` en la Fase 2) y en overrides de ruta pasados por CLI.

## 2. Alcance

- traducir `ExtensionCategory` (Fase 2) a un `Language` resuelto, con confianza y razón;
- overrides de lenguaje por patrón de ruta, vía flag `--lang-override <pattern>=<lang>` repetible en `codeqa discover`;
- prioridad de overrides sobre la detección automática, incluso si la extensión era inequívoca;
- desempate cuando varios overrides coinciden con el mismo archivo (gana el último especificado);
- listado por archivo en la salida de texto de `codeqa discover`, con el lenguaje detectado.

### Fuera de alcance (explícitamente diferido)

- Heurísticas de contenido para desambiguar `.h` (p. ej. buscar `class`/`namespace`/`template`) — la herramienta no inventa certeza cuando no existe; un `.h` sin override queda como `Unknown`.
- Overrides vía `codeqa.json` (Fase 8) — en esta fase solo existen como flag `--lang-override` de la CLI, reutilizando el mismo motor de patrones que expondrá Fase 8 más adelante.
- Soporte para lenguajes distintos de C/C++ (Python, Java, etc. — fuera del MVP).
- Cualquier lectura de contenido de archivo (sigue diferido desde la Fase 2).

## 3. Decisiones de diseño

1. **Overrides vía flag CLI `--lang-override` repetible**, no diferido a la Fase 8. Mismo patrón que `--exclude` en la Fase 2: la Fase 8 reutilizará este mecanismo para exponerlo también vía `codeqa.json`.
2. **Confianza numérica 0.0–1.0**, con valores fijos por tipo de detección (no un cálculo estadístico): `1.0` para extensión inequívoca o override; `0.5` para `.h` ambiguo sin override. El `Confidence: 0.80` del roadmap es solo un ejemplo ilustrativo, no un valor exigido; este spec fija `1.0` para un override explícito, porque no hay ninguna incertidumbre real en una decisión que el usuario tomó a propósito.
3. **Sin heurística de contenido para `.h` ambiguos.** Quedan siempre como `Language::Unknown` con confianza `0.5` si no hay override que los cubra. Coherente con el criterio de aceptación "la herramienta no inventa certeza cuando no existe" y con que esta fase sigue sin leer contenido de archivos.
4. **Los overrides tienen prioridad absoluta**, incluso sobre una extensión inequívoca (`.c`/`.cpp` explícitamente reasignados por un override siguen el override, no la extensión).
5. **`Language`/`LanguageDetectionResult` viven en `codeqa::core`** (tipos de valor puros, sin I/O) — no en `infrastructure`, donde vive `ExtensionCategory` (la señal en bruto por extensión, sin cambios desde la Fase 2). Esto separa la señal cruda (extensión) de la decisión resuelta (lenguaje + confianza + razón), y da a `Language` un hogar compartido por el resto del pipeline (Fases 4+).
6. **La lógica de detección vive en `codeqa::application`, en un caso de uso nuevo y separado** (`DetectFileLanguage`), no integrado dentro de `DiscoverProjectSources`. No se modifica el código ya revisado de la Fase 2; la CLI compone ambos casos de uso.
7. **Los overrides reutilizan `codeqa::infrastructure::matchesExcludePattern`** (el motor de glob de la Fase 2), con la misma semántica: patrón con `/` compara contra la ruta relativa completa; sin `/` compara contra el nombre de archivo a cualquier profundidad.
8. **Formato de salida por archivo:** se omite el sufijo de confianza/razón cuando la razón es `"unambiguous extension"` (siempre confianza 1.0, el caso normal y esperado). En cualquier otro caso — ambiguo *o* override — se añade `(confidence X.XX) - <reason>`, porque ahí sí hay algo que explicar al usuario.

## 4. Arquitectura y modelo de datos

```
include/codeqa/core/
├── Language.h                  (enum class: C, Cpp, Unknown)
└── LanguageDetectionResult.h   (struct)

include/codeqa/application/
└── DetectFileLanguage.h        (caso de uso + LanguageOverride)

src/application/
└── DetectFileLanguage.cpp
```

### Modelo de datos

```cpp
// codeqa::core
enum class Language { C, Cpp, Unknown };

struct LanguageDetectionResult
{
    Language language{Language::Unknown};
    double confidence{0.0};
    std::string reason;
};
```

```cpp
// codeqa::application
struct LanguageOverride
{
    std::string pattern;              // mismo motor de glob que --exclude (Fase 2)
    codeqa::core::Language language;
};

class DetectFileLanguage
{
public:
    [[nodiscard]] codeqa::core::LanguageDetectionResult execute(
        const codeqa::infrastructure::DiscoveredFile& file,
        const std::vector<LanguageOverride>& overrides) const;
};
```

### Reglas de resolución (en orden de evaluación)

1. Si algún `LanguageOverride` de `overrides` coincide con `file.path` (vía `matchesExcludePattern(pattern, file.path)`), el resultado es `{override.language, 1.0, "path override"}`. Si varios coinciden, se usa el **último** de la lista (el orden de la lista es el orden en que se especificaron los flags `--lang-override` en la CLI).
2. Si no hay override y `file.category == ExtensionCategory::C`, el resultado es `{Language::C, 1.0, "unambiguous extension"}`.
3. Si no hay override y `file.category == ExtensionCategory::Cpp`, el resultado es `{Language::Cpp, 1.0, "unambiguous extension"}`.
4. Si no hay override y `file.category == ExtensionCategory::AmbiguousHeader`, el resultado es `{Language::Unknown, 0.5, "ambiguous header extension"}`.

`ExtensionCategory` (Fase 2, `infrastructure`) no cambia — sigue siendo la señal en bruto usada por el resumen `discover` (`C files:`, `C++ files:`, `Ambiguous headers:`). `Language`/`LanguageDetectionResult` es la capa de decisión nueva de esta fase, construida encima.

## 5. CLI

```
codeqa discover <path> [--exclude <glob>]... [--lang-override <pattern>=<lang>]...
```

- `--lang-override` sin valor tras él → `Error [CLI006]: Missing value for '--lang-override'.`, `ExitCode::InvalidArguments`.
- `--lang-override` con valor mal formado (sin `=`, patrón vacío, lenguaje vacío, o `<lang>` distinto de `c`/`cpp`) → `Error [CLI007]: Invalid --lang-override value '<valor>'. Expected <pattern>=c|cpp.`, `ExitCode::InvalidArguments`. El valor se separa por el **primer** signo `=` que aparezca (un patrón no debería contener `=`, pero si lo tuviera, todo lo posterior al primer `=` se interpreta como `<lang>`). `<lang>` es sensible a mayúsculas: solo `c`/`cpp` en minúscula son válidos, igual que `classifyExtension` de la Fase 2 es sensible a mayúsculas para las extensiones.
- La validación de `--lang-override` ocurre en el mismo bucle de parseo que ya procesa `--exclude` (mismo nivel que `CLI004`/`CLI005` de la Fase 2).
- `--help` se actualiza para documentar `--lang-override`.

**Salida de texto** — tras el resumen ya existente (sin cambios), una línea en blanco y luego una línea por archivo descubierto, en el mismo orden determinista que `DiscoveryResult.files`:

```text
Files discovered: 3
C files: 1
C++ files: 1
Ambiguous headers: 1
Excluded files: 1
Read errors: 0

main.c    C
util.cpp  C++
util.h    Unknown (confidence 0.50) - ambiguous header extension
```

Con override activo, un archivo con extensión inequívoca también muestra el sufijo, porque su razón ya no es `"unambiguous extension"`:

```text
include/network/socket.h  C (confidence 1.00) - path override
```

## 6. Manejo de errores

| Situación | Resultado |
|---|---|
| `--lang-override` sin valor | `CLI006`, `ExitCode::InvalidArguments` |
| `--lang-override` mal formado (sin `=`, valores vacíos, lenguaje desconocido) | `CLI007`, `ExitCode::InvalidArguments` |
| Ningún override coincide con un archivo | Se usa la detección por extensión, sin error |
| Varios overrides coinciden con el mismo archivo | Gana el último especificado en la línea de comandos |
| `.h` sin override que coincida | `Language::Unknown`, confianza `0.5`, sin error |

Los errores ya existentes de la Fase 2 (`CLI001`–`CLI005`) no cambian.

## 7. Tests

- **Unitarios** (`tests/unit/DetectFileLanguageTest.cpp`): extensión C inequívoca → `{C, 1.0, "unambiguous extension"}`; extensión C++ inequívoca → `{Cpp, 1.0, "unambiguous extension"}`; `.h` sin override → `{Unknown, 0.5, "ambiguous header extension"}`; override con patrón sin `/` (nombre de archivo) y con `/` (ruta completa) — reutilizando los mismos casos ya probados del `GlobMatcher` de la Fase 2; override gana incluso sobre una extensión inequívoca; dos overrides que coinciden con el mismo archivo — gana el último de la lista.
- **Unitarios** (`tests/unit/RunApplicationTest.cpp`, ampliado): `--lang-override` válido cambia la salida; `--lang-override` sin valor → `CLI006`; `--lang-override` mal formado (sin `=`, lenguaje inválido) → `CLI007`; el listado por archivo aparece con el formato exacto, con y sin sufijo de confianza/razón.
- **Integración** (CTest, mismo patrón que la Fase 2): el fixture existente `tests/integration/fixtures/discover-sample/` ya contiene un `.h` ambiguo — un test comprueba que la salida de `codeqa discover` incluye `util.h` y `Unknown` sin necesidad de fixtures nuevos; un segundo test añade `--lang-override "util.h=cpp"` sobre el mismo fixture y comprueba que la salida refleja el override (`C++ (confidence 1.00) - path override`) contra el binario compilado real.

## 8. Criterios de aceptación

- `.c` se clasifica como `Language::C` con confianza `1.0`;
- `.cpp`, `.cc` y `.cxx` se clasifican como `Language::Cpp` con confianza `1.0`;
- `.h` sin override queda como `Language::Unknown` con confianza `0.5`;
- los overrides tienen prioridad sobre la detección automática, incluso ante una extensión inequívoca;
- la herramienta no inventa certeza cuando no existe (ningún `.h` sin override recibe confianza `1.0`, ninguna heurística de contenido);
- `codeqa discover <path>` muestra el lenguaje detectado por archivo, con confianza y razón cuando no es el caso trivial;
- `codeqa discover <path> --lang-override <pattern>=<lang>` funciona end-to-end;
- `codeqa --help` documenta `--lang-override`.

## 9. Dependencias

Fase 2 (`codeqa::infrastructure::{DiscoveredFile, ExtensionCategory, DiscoveryResult, matchesExcludePattern}`, el subcomando `discover` y su motor de parseo de argumentos).
