# Fase 4a — tree-sitter vendorizado y parseo: diseño

Fecha: 2026-08-05
Estado: aprobado para pasar a plan de implementación

## 1. Objetivo

Vendorizar tree-sitter y sus gramáticas de C y C++, integrarlas en la compilación sin acceso a red, y convertir cada archivo fuente descubierto en un árbol sintáctico concreto (CST), traduciendo las regiones que la gramática no reconoce en `Diagnostic`s del analizador.

Esta subfase **no extrae estructura**: no busca funciones, clases ni parámetros. Produce el árbol y los diagnósticos de parseo. La extracción es la Fase 4b.

## 2. Contexto: por qué se parte la Fase 4

La Fase 4 del roadmap contiene varios subsistemas independientes: vendorizar tree-sitter, parsear y diagnosticar, escribir queries `.scm` y extraer estructura, y construir el runner del corpus. Es la fase más grande del proyecto y casi todo su riesgo técnico se concentra en el vendorizado.

Se divide en dos subfases, cada una con su spec, su plan y su ciclo completo:

- **Fase 4a** (este documento): vendorizado, parseo, diagnósticos.
- **Fase 4b**: queries `.scm`, modelos estructurales, runner del corpus con `nlohmann/json`.

El corte está donde cambia la naturaleza del trabajo: 4a es integración de terceros y recorrido de árbol; 4b es diseño de queries y modelado.

## 3. Alcance

- vendorizar el runtime de tree-sitter y las gramáticas `tree-sitter-c` y `tree-sitter-cpp` en `third_party/`, sin `FetchContent` ni submodules;
- integrarlas en CMake como bibliotecas estáticas compiladas como C;
- leer el contenido de cada archivo fuente (primera vez en el proyecto);
- seleccionar la gramática a partir del `Language` detectado en la Fase 3;
- parsear y obtener el CST;
- convertir nodos `ERROR` y `MISSING` en `Diagnostic`s con ubicación;
- subcomando `codeqa parse <path>` con los mismos flags que `discover`;
- verificación contra el corpus: cero diagnósticos en fixtures bien formados, al menos uno en cada fixture de `malformed/`;
- resolver el pendiente de la Fase 0: fijar las líneas de `diagnostics` de los dos fixtures de `malformed/` contra la salida real de tree-sitter;
- añadir el encabezado `Detected languages:` al listado por archivo de `discover` (pendiente de la revisión de la Fase 3).

### Fuera de alcance (diferido, con motivo)

- **Queries `.scm` y extracción estructural** (funciones, clases, namespaces, parámetros, includes) → Fase 4b.
- **Modelos `FunctionModel`, `ClassModel`, `ParameterModel`, `IncludeDependency`** → Fase 4b.
- **Vendorizar `nlohmann/json` y construir el runner del corpus** → Fase 4b. Las dos afirmaciones que 4a necesita (cero diagnósticos / al menos uno) no requieren leer JSON.
- **Validar los ocho IDs de cobertura solo por presencia** (`C-PP-01`, `C-DECL-01`, `C-TYPE-04`, `C-FUNC-03/04/05`, `CPP-PARAM-01/02`) → Fase 4b, que es donde existen los campos con los que podrían afirmarse.
- **Resolución de tipos, overload resolution, instanciación de plantillas, evaluación del preprocesador, semántica de herencia, deducción de tipos** — fuera del MVP entero (`OUT-01`..`OUT-07` de `SUPPORTED-CONSTRUCTS.md`).
- **Parsing incremental**: tree-sitter lo soporta, pero el MVP no lo necesita.

## 4. Decisiones de diseño

1. **La Fase 4 se parte en 4a y 4b** por tamaño y por concentración de riesgo (ver sección 2).
2. **Un archivo con `Language::Unknown` se parsea con la gramática de C++.** La gramática de C++ acepta prácticamente todo C válido, así que es la opción que maximiza lo reconocido y minimiza los `ERROR` falsos en cabeceras `.h` ambiguas — que en C son justo donde viven las declaraciones. Se registra qué gramática se usó y por qué, para que el resultado sea explicable. Se descarta parsear con ambas y comparar (dobla el coste por cabecera e introduce una heurística de desempate que habría que justificar) y se descarta saltarse el archivo (dejaría fuera del análisis casi todas las cabeceras de C).
3. **Un `Diagnostic` por nodo `ERROR`/`MISSING` de primer nivel, sin descender dentro de un nodo ya reportado.** Los errores anidados dentro de una región ya marcada son consecuencia del mismo defecto; emitirlos todos convierte un `{` sin cerrar en decenas de diagnósticos que apuntan al mismo sitio. Se descarta reportar solo el primero por archivo: ocultaría defectos independientes en regiones distintas.
4. **Subcomando nuevo `codeqa parse <path>`**, en vez de un flag `--parse` en `discover`. `discover` ya está revisado y estable, y mezclar "descubrir" con "analizar" en un comando cuyo nombre describe lo primero degradaría ambos. La Fase 4b ampliará `parse` con la extracción estructural.
5. **El frontend vive en un espacio de nombres nuevo, `codeqa::frontend`** — no en `infrastructure` (que es acceso a sistema de archivos) ni en `core` (que no tiene dependencias externas). Es la única capa que conoce tree-sitter.
6. **Toda la API C de tree-sitter queda encapsulada en un wrapper RAII.** `TSParser*` y `TSTree*` requieren `ts_parser_delete`/`ts_tree_delete`; se envuelven en tipos no copiables y movibles, y ningún puntero crudo escapa del wrapper.
7. **Se vendorizan solo los archivos necesarios para compilar**, no los repositorios completos (que incluyen tests, bindings de Rust/Node/Python y las `grammar.js`). Los `parser.c` son código generado y se vendorizan sin editar.
8. **Las bibliotecas de terceros quedan excluidas de `codeqa_enable_warnings`.** Son código generado por terceros que no compilaría con `-Werror`; mismo criterio ya aplicado a doctest en la Fase 1.
9. **La lectura del contenido de archivo vive en el frontend**, y su fallo produce un `Diagnostic` (`PARSE003`), no una excepción. Coherente con el principio establecido en la Fase 2: un fallo sobre un archivo no aborta el análisis del resto.

## 5. Vendorizado

```text
third_party/
├── README.md                        versión y origen exactos de cada dependencia
├── doctest/                          (ya existente, Fase 1)
├── tree-sitter/
│   ├── lib/src/*.c, *.h
│   └── lib/include/tree_sitter/api.h
├── tree-sitter-c/
│   └── src/parser.c
│       src/tree_sitter/parser.h
└── tree-sitter-cpp/
    └── src/parser.c
        src/scanner.c
        src/tree_sitter/parser.h
```

`tree-sitter-cpp` incluye un `scanner.c` (escáner externo para casos que la gramática generada no cubre); `tree-sitter-c` no lo necesita.

Los `parser.c` generados son voluminosos — el de C++ ronda varios MB. Es el precio de la compilación reproducible sin red, y es coherente con la política de vendorizado fijada en la Fase 1.

**Obtención:** requiere descargar los repositorios de origen. Se pedirá permiso explícito antes de hacerlo, igual que con doctest en la Fase 1. `third_party/README.md` registra, por dependencia: nombre, URL, versión o commit fijado, licencia, y qué archivos se copiaron.

**CMake:** tres bibliotecas estáticas nuevas (`tree_sitter`, `tree_sitter_c`, `tree_sitter_cpp`), compiladas como C. Esto obliga a añadir `C` a `project(CodeQualityAnalyzer ... LANGUAGES CXX)` → `LANGUAGES C CXX`. Ninguna recibe `codeqa_enable_warnings`.

## 6. Arquitectura

```text
include/codeqa/frontend/
├── Grammar.h                enum class: C, Cpp
├── SelectGrammar.h          Language (Fase 3) → GrammarSelection
├── ParsedFile.h             struct resultado del parseo
├── TreeSitterParser.h       wrapper RAII sobre la API C
└── ParseSourceFile.h        leer + parsear + diagnosticar

src/frontend/
├── SelectGrammar.cpp
├── TreeSitterParser.cpp
└── ParseSourceFile.cpp
```

### Modelo de datos

```cpp
// codeqa::frontend
enum class Grammar { C, Cpp };

struct GrammarSelection
{
    Grammar grammar{Grammar::Cpp};
    std::string reason;
};

struct ParsedFile
{
    std::filesystem::path path;
    Grammar grammar{Grammar::Cpp};
    std::string grammarReason;
    std::vector<codeqa::core::Diagnostic> diagnostics;
    bool parsed{false};          // false si el archivo no pudo leerse
};
```

`ParsedFile` **no expone el `TSTree*`** en esta subfase: nada fuera del frontend lo consume todavía, y exponerlo obligaría a filtrar tipos de tree-sitter a capas superiores. La Fase 4b decidirá cómo entregar el árbol al extractor — probablemente ejecutando las queries dentro del propio frontend y devolviendo modelos, sin que el árbol salga nunca.

### Selección de gramática

| `Language` (Fase 3) | `Grammar` | `reason` |
|---|---|---|
| `C` | `C` | `"detected language"` |
| `Cpp` | `Cpp` | `"detected language"` |
| `Unknown` | `Cpp` | `"ambiguous header, defaulted to C++"` |

## 7. Diagnósticos

Recorrido del árbol con `TSTreeCursor`. Al encontrar un nodo `ERROR` (`ts_node_is_error`) o `MISSING` (`ts_node_is_missing`) se emite un `Diagnostic` y **no se desciende dentro de ese nodo**.

| Código | Severidad | Cuándo |
|---|---|---|
| `PARSE001` | `Error` | Nodo `ERROR`: región que la gramática no supo interpretar |
| `PARSE002` | `Error` | Nodo `MISSING`: token que la gramática esperaba y no está |
| `PARSE003` | `Error` | El archivo no se pudo leer |

`ts_node_start_point()` devuelve `TSPoint{row, column}` **0-based**. Se convierten sumando 1 a cada uno al construir el `SourceLocation`, consistente con la Fase 1 (`SourceLocation` es 1-based) y con el contrato del corpus.

Un archivo con `PARSE003` lleva `parsed == false` y ningún otro diagnóstico: no hubo árbol que recorrer.

## 8. CLI

```
codeqa parse <path> [--exclude <glob>]... [--lang-override <pattern>=<lang>]...
```

Reutiliza sin cambios el descubrimiento (Fase 2) y la detección de lenguaje (Fase 3): mismos flags, misma semántica de patrones, mismos códigos de error `CLI002`–`CLI007`. `discover` no cambia su comportamiento.

Salida de texto:

Ejemplo real sobre `tests/corpus/` (13 fixtures: 4 con extensión de C, 9 de C++ incluyendo la cabecera `.hpp`):

```text
Files parsed: 13
C files: 4
C++ files: 9
Parse errors: 2

malformed/truncated.c
  line 8: error
malformed/unclosed-brace.cpp
  line 13: error
```

- El resumen cuenta archivos por **gramática usada** (no por lenguaje detectado), porque es lo que describe qué hizo realmente el parser. Una cabecera `.h` sin override cuenta como C++, porque con esa gramática se parseó.
- `Files parsed` cuenta los archivos que llegaron a parsearse (`parsed == true`). Un archivo con `PARSE003` no se pudo leer, así que **no** entra en ese recuento ni en el de ninguna gramática; sí aparece en `Parse errors` y en el detalle por archivo.
- `Parse errors` es el número de **archivos** con al menos un diagnóstico, no el número total de diagnósticos.
- El detalle por archivo solo lista los archivos con diagnósticos; un análisis limpio no imprime nada tras el resumen.
- `--help` documenta el subcomando.

### Pendiente saldado de la Fase 3

El listado por archivo de `discover` recibe un encabezado `Detected languages:`, para que el resumen agregado y las filas por archivo no parezcan contradecirse cuando hay un `--lang-override` activo. Orden exacto: la línea en blanco que hoy separa el resumen del listado se mantiene, el encabezado va inmediatamente después, y las filas por archivo siguen sin línea en blanco intermedia:

```text
Read errors: 0

Detected languages:
main.c    C
util.cpp  C++
util.h    Unknown (confidence 0.50) - ambiguous header extension
```

El encabezado se imprime bajo la misma condición que el listado: solo si hay al menos un archivo descubierto.

## 9. Manejo de errores

| Situación | Resultado |
|---|---|
| Ruta raíz inexistente o no directorio | `CLI003`, `ExitCode::InvalidArguments` (igual que `discover`) |
| Archivo inaccesible en el descubrimiento | `Diagnostic` de descubrimiento (`DISC001`), ya existente |
| Archivo ilegible al parsear | `PARSE003`, `parsed == false`, el análisis continúa |
| Región no reconocida por la gramática | `PARSE001`, el análisis del resto del archivo continúa |
| Token esperado ausente | `PARSE002`, el análisis del resto del archivo continúa |
| Cualquier archivo con diagnósticos | No cambia el código de salida: `parse` devuelve `ExitCode::Success` |

Que un archivo tenga errores de parseo **no** es un fallo del análisis: es información. El quality gate que sí afecta al código de salida llega en la Fase 12.

## 10. Verificación contra el corpus

Un test unitario recorre `tests/corpus/` — ruta inyectada por CMake como definición de compilación, con `std::filesystem` (ya disponible desde la Fase 2) — y afirma:

- **Cero diagnósticos en todo fixture fuera de `malformed/`.** Es la afirmación más valiosa de la subfase: si la gramática de C++ atraganta con algo que `SUPPORTED-CONSTRUCTS.md` declara soportado (lambdas, plantillas, la cláusula base añadida en la Fase 0, el `enum` sin ámbito, la cabecera `.hpp`), se detecta aquí y no en la Fase 6 con las métricas ya construidas encima.
- **Al menos un diagnóstico en cada fixture de `malformed/`**, y que el resto del archivo se sigue recorriendo.

No lee los `.parsing.json`: ninguna de las dos afirmaciones lo necesita, y así 4a no arrastra el vendorizado de `nlohmann/json`.

### Fijación de las líneas provisionales

Los `diagnostics` de `malformed/unclosed-brace.parsing.json` (línea 13) y `malformed/truncated.parsing.json` (línea 8) se marcaron como provisionales en la Fase 0, porque no existía parser que confirmara qué produce la recuperación de errores.

Con el parser en marcha se observa la salida real y se actualizan. **Con una cautela explícita:** no se copia la salida a ciegas — eso sería afirmar que el código hace lo que hace. Se comprueba que la línea reportada señala de verdad al defecto (o a su primera manifestación detectable) y solo entonces se fija. Si la línea observada no es defendible, el problema está en el fixture o en el recorrido, no en la expectativa.

## 11. Criterios de aceptación

- el runtime y las gramáticas compilan vendorizados, sin acceso a red;
- `codeqa parse <path>` funciona end-to-end sobre un directorio real;
- todo fixture bien formado del corpus parsea con cero diagnósticos;
- todo fixture de `malformed/` produce al menos un diagnóstico;
- un archivo roto no impide el análisis del resto de archivos, ni del resto del propio archivo;
- las líneas de `diagnostics` de los dos fixtures de `malformed/` quedan fijadas contra la salida real y justificadas;
- repetir el análisis sobre el mismo archivo produce los mismos diagnósticos (determinismo);
- un `.h` sin override se parsea como C++ y el resultado lo indica;
- `codeqa --help` documenta `parse`;
- `discover` muestra el encabezado `Detected languages:` sobre su listado por archivo.

## 12. Dependencias

Fase 2 (descubrimiento: `DiscoveredFile`, `DiscoveryResult`, el motor de glob), Fase 3 (`Language`, `DetectFileLanguage`, `LanguageOverride`), Fase 0 (el corpus contra el que se verifica).
