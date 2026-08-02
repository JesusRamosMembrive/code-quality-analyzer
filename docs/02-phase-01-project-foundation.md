# Fase 1 — Fundación técnica del proyecto

## 1. Objetivo

Esta fase establece la base técnica del repositorio sobre la que se desarrollará el analizador de calidad de código.

El objetivo no es analizar todavía archivos C o C++. El resultado debe ser un proyecto C++17:

- compilable;
- ejecutable;
- testeable;
- organizado por responsabilidades;
- preparado para crecer;
- independiente de Qt en su núcleo;
- apto para incorporar posteriormente varios frontends de lenguaje.

Al terminar esta fase deberá existir un ejecutable CLI mínimo, una biblioteca con el núcleo del proyecto y una infraestructura de pruebas funcional.

---

## 2. Motivación

Un analizador estático crecerá en varias direcciones:

- descubrimiento de archivos;
- detección de lenguajes;
- frontend de parsing (tree-sitter);
- modelo intermedio;
- métricas;
- reglas;
- informes;
- histórico;
- interfaz gráfica;
- nuevos lenguajes.

Si el repositorio comienza sin límites claros entre capas, será fácil introducir dependencias incorrectas y mezclar responsabilidades.

Esta fase debe impedir desde el principio problemas como:

- lógica de análisis dentro de `main.cpp`;
- dependencia del motor respecto a Qt;
- acceso directo al sistema de archivos desde cualquier componente;
- uso inconsistente de errores y diagnósticos;
- estructuras duplicadas para representar ubicaciones;
- tests acoplados a detalles internos;
- clases creadas por anticipación sin una necesidad concreta;
- arquitectura excesivamente compleja para el MVP.

La fundación debe ser mínima, pero no improvisada.

---

## 3. Alcance

Esta fase incluye:

1. creación del repositorio y su estructura;
2. configuración de compilación con CMake;
3. definición del estándar C++17;
4. creación de una biblioteca de núcleo;
5. creación de un ejecutable CLI;
6. integración de un framework de pruebas;
7. definición de convenciones básicas;
8. creación de modelos transversales mínimos;
9. creación de una estrategia inicial de errores;
10. configuración de warnings;
11. creación de tests de humo;
12. documentación para compilar y ejecutar.

---

## 4. Fuera de alcance

En esta fase no se implementará:

- descubrimiento recursivo de archivos;
- lectura de proyectos;
- clasificación de C y C++;
- frontend de parsing (tree-sitter);
- modelo de funciones o clases;
- métricas;
- reglas;
- configuración JSON;
- informes JSON;
- histórico;
- baseline;
- integración con Git;
- GUI Qt/QML;
- integración con Clang;
- sistema de plugins;
- soporte para Python.

El agente no debe crear clases vacías para todas las fases futuras. Solo deben existir los contratos mínimos necesarios para esta fase.

---

## 5. Decisiones técnicas iniciales

### 5.1. Lenguaje de implementación

La herramienta se implementará en:

```text
C++17
```

El estándar debe fijarse explícitamente en CMake.

No se utilizarán extensiones específicas del compilador salvo que estén encapsuladas y justificadas.

### 5.2. Sistema de compilación

Se utilizará:

```text
CMake
```

Versión mínima recomendada:

```cmake
cmake_minimum_required(VERSION 3.16)
```

La versión puede elevarse si el entorno real dispone de una versión más moderna, pero no debe elegirse una versión innecesariamente reciente sin comprobar compatibilidad.

### 5.3. Núcleo independiente de Qt

El núcleo no debe depender de:

- `QString`;
- `QFile`;
- `QDir`;
- `QObject`;
- `QVariant`;
- `QJsonDocument`;
- QML.

Se utilizarán tipos de la biblioteca estándar:

- `std::string`;
- `std::filesystem::path`;
- `std::vector`;
- `std::optional`;
- `std::variant`;
- `std::unique_ptr`;
- `std::string_view`.

Qt podrá añadirse en una fase posterior como adaptador de interfaz.

### 5.4. Biblioteca y ejecutable separados

La lógica reutilizable se compilará en una biblioteca.

El ejecutable CLI solo deberá:

1. recoger argumentos;
2. invocar un caso de uso;
3. mostrar un resultado;
4. devolver un código de salida.

`main.cpp` no debe contener lógica de negocio.

### 5.5. Tests automatizados

Framework elegido: **doctest**.

Razón: cabecera única, vendorizable como un solo archivo en el repositorio, compilación rápida, licencia permisiva. Se descartan Catch2 y GoogleTest por tener mayor superficie de vendoring o gestionarse típicamente como dependencia de build en vez de vendorizada.

No se debe descargar ninguna dependencia en tiempo de compilación en el entorno de producción si esto no está permitido.

### 5.6. Dependencias externas

El MVP debe minimizar dependencias.

Toda dependencia deberá:

- estar justificada;
- tener licencia compatible;
- poder incluirse en el repositorio o desplegarse de forma controlada;
- no ser necesaria para el núcleo salvo que aporte valor real.

Política de vendoring: toda dependencia externa (doctest en esta fase; tree-sitter en la Fase 4) se vendoriza como fuente dentro del repositorio (p.ej. `third_party/`), en vez de usar `FetchContent` o submodules. Esto garantiza una compilación reproducible sin acceso a red.

---

## 6. Estructura inicial del repositorio

La estructura propuesta es:

```text
code-quality-analyzer/
├── CMakeLists.txt
├── cmake/
│   └── CompilerWarnings.cmake
├── docs/
│   ├── 00-project-overview.md
│   ├── 01-cpp-mvp-roadmap.md
│   └── 02-phase-01-project-foundation.md
├── include/
│   └── codeqa/
│       ├── core/
│       └── application/
├── src/
│   ├── core/
│   ├── application/
│   └── cli/
│       └── main.cpp
├── tests/
│   ├── CMakeLists.txt
│   ├── unit/
│   └── integration/
├── .gitignore
├── README.md
└── LICENSE
```

### 6.1. `include/codeqa`

Contendrá headers públicos de la biblioteca.

Ejemplo:

```text
include/codeqa/core/SourceLocation.h
include/codeqa/core/Diagnostic.h
include/codeqa/application/ApplicationInfo.h
```

### 6.2. `src/core`

Contendrá implementaciones del núcleo.

### 6.3. `src/application`

Contendrá casos de uso y coordinación de alto nivel.

En esta fase solo será necesario un caso de uso mínimo que demuestre la separación respecto a la CLI.

### 6.4. `src/cli`

Contendrá el ejecutable de línea de comandos.

### 6.5. `tests/unit`

Tests de componentes aislados.

### 6.6. `tests/integration`

Tests que ejecuten un flujo completo del estado actual del proyecto.

---

## 7. Targets de CMake

Se crearán al menos estos targets:

```text
codeqa_core
codeqa_cli
codeqa_tests
```

### 7.1. Biblioteca principal

```cmake
add_library(codeqa_core ...)
```

Responsabilidades:

- modelos compartidos;
- utilidades del núcleo;
- casos de uso iniciales.

La biblioteca no debe depender del ejecutable CLI.

### 7.2. Ejecutable CLI

```cmake
add_executable(codeqa ...)
target_link_libraries(codeqa PRIVATE codeqa_core)
```

### 7.3. Tests

```cmake
add_executable(codeqa_tests ...)
target_link_libraries(codeqa_tests PRIVATE codeqa_core)
```

CTest debe estar habilitado:

```cmake
enable_testing()
add_test(NAME codeqa_tests COMMAND codeqa_tests)
```

---

## 8. Configuración CMake mínima

El `CMakeLists.txt` raíz deberá definir:

```cmake
cmake_minimum_required(VERSION 3.16)

project(
    CodeQualityAnalyzer
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
```

También debe ofrecer una opción para compilar tests:

```cmake
option(CODEQA_BUILD_TESTS "Build CodeQA tests" ON)
```

La activación de warnings debe centralizarse en una función de CMake.

Ejemplo conceptual:

```cmake
codeqa_enable_warnings(codeqa_core)
codeqa_enable_warnings(codeqa_cli)
```

No se deben copiar listas de warnings en cada target.

---

## 9. Warnings del compilador

La herramienta deberá compilar con warnings estrictos.

### 9.1. GCC y Clang

Conjunto inicial recomendado:

```text
-Wall
-Wextra
-Wpedantic
-Wconversion
-Wsign-conversion
-Wshadow
-Wnon-virtual-dtor
-Wold-style-cast
-Woverloaded-virtual
```

Algunos warnings pueden producir ruido dependiendo del código y del framework de testing. Deben evaluarse, no añadirse ciegamente.

### 9.2. MSVC

Conjunto inicial:

```text
/W4
/permissive-
```

### 9.3. Warnings como errores

No se recomienda activar globalmente `-Werror` o `/WX` para todas las configuraciones desde el primer momento.

Se añadirá una opción:

```cmake
option(CODEQA_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
```

Esto permite usar warnings como errores en CI sin bloquear compiladores o entornos distintos durante el desarrollo inicial.

---

## 10. Namespaces

Todo el código del proyecto deberá estar dentro de:

```cpp
namespace codeqa
{
}
```

Subnamespaces recomendados:

```cpp
namespace codeqa::core
{
}

namespace codeqa::application
{
}

namespace codeqa::cli
{
}
```

No se utilizará:

```cpp
using namespace std;
```

ni en headers ni a nivel global en archivos fuente.

---

## 11. Convenciones de nombres

Convención inicial recomendada:

| Elemento | Convención | Ejemplo |
|---|---|---|
| Clases y structs | PascalCase | `SourceLocation` |
| Funciones y métodos | camelCase | `formatVersion()` |
| Variables locales | camelCase | `exitCode` |
| Miembros privados | camelCase con sufijo `_` | `message_` |
| Constantes | camelCase o `k` + PascalCase | `kDefaultVersion` |
| Namespaces | minúsculas | `codeqa::core` |
| Archivos de clases | PascalCase | `Diagnostic.h` |

La convención debe mantenerse estable, pero no es objetivo de esta fase implementar una herramienta que la compruebe.

---

## 12. Headers

Cada header debe:

- utilizar `#pragma once` o include guards de forma consistente;
- incluir directamente aquello que necesita;
- evitar dependencias innecesarias;
- no depender de un orden concreto de includes;
- no contener `using namespace`;
- minimizar implementación no trivial inline.

Orden recomendado de includes en `.cpp`:

1. header correspondiente;
2. headers del proyecto;
3. biblioteca estándar;
4. dependencias externas.

Ejemplo:

```cpp
#include "codeqa/core/Diagnostic.h"

#include "codeqa/core/SourceLocation.h"

#include <string>
#include <utility>
```

---

## 13. Contratos mínimos del núcleo

Esta fase debe definir pocos modelos, pero deben ser estables y reutilizables.

---

### 13.1. `SourceLocation`

Representa una posición dentro de un archivo.

```cpp
#pragma once

#include <cstddef>
#include <filesystem>

namespace codeqa::core
{

struct SourceLocation
{
    std::filesystem::path filePath;
    std::size_t line{1};
    std::size_t column{1};
    std::size_t offset{0};
};

} // namespace codeqa::core
```

Decisiones:

- líneas y columnas comienzan en 1;
- offset comienza en 0;
- `filePath` puede estar vacío para diagnósticos no asociados a un archivo;
- la normalización de rutas se implementará en una fase posterior.

---

### 13.2. `SourceRange`

Representa un rango de código.

```cpp
#pragma once

#include "codeqa/core/SourceLocation.h"

namespace codeqa::core
{

struct SourceRange
{
    SourceLocation begin;
    SourceLocation end;
};

} // namespace codeqa::core
```

Convención:

- `begin` es inclusivo;
- `end` debe documentarse como inclusivo o exclusivo.

Recomendación:

```text
begin inclusivo
end exclusivo
```

Esto facilita trabajar con rangos y substrings.

---

### 13.3. `DiagnosticSeverity`

Se utilizará para errores del propio proceso de análisis, no para hallazgos de calidad.

```cpp
#pragma once

namespace codeqa::core
{

enum class DiagnosticSeverity
{
    Information,
    Warning,
    Error
};

} // namespace codeqa::core
```

Debe mantenerse separado de la futura severidad de reglas:

```text
DiagnosticSeverity != FindingSeverity
```

Un error de parser y una violación de calidad son conceptos diferentes.

---

### 13.4. `Diagnostic`

```cpp
#pragma once

#include "codeqa/core/DiagnosticSeverity.h"
#include "codeqa/core/SourceLocation.h"

#include <string>

namespace codeqa::core
{

struct Diagnostic
{
    std::string code;
    DiagnosticSeverity severity{DiagnosticSeverity::Error};
    std::string message;
    SourceLocation location;
};

} // namespace codeqa::core
```

Ejemplos futuros:

```text
CLI001: Missing command.
FS001: Input path does not exist.
LEX001: Unterminated string literal.
PARSE001: Unmatched closing brace.
```

Los códigos deben ser estables y aptos para tests.

---

### 13.5. `OperationResult`

No se debe diseñar todavía un sistema complejo de manejo de errores.

Se puede utilizar un tipo sencillo para operaciones que devuelven:

- un valor;
- diagnósticos;
- indicador de éxito.

Ejemplo:

```cpp
template <typename T>
struct OperationResult
{
    T value;
    std::vector<Diagnostic> diagnostics;

    [[nodiscard]] bool hasErrors() const;
};
```

Sin embargo, no debe introducirse una plantilla genérica si en esta fase no existe un caso real que la necesite.

Alternativa preferida para esta fase:

- devolver tipos directos;
- utilizar `std::optional` cuando sea suficiente;
- devolver un objeto específico del caso de uso.

Se evita crear infraestructura abstracta prematuramente.

---

## 14. Caso de uso mínimo

Para demostrar la separación entre CLI y núcleo se implementará un caso de uso mínimo.

Nombre propuesto:

```text
GetApplicationInfo
```

Modelo:

```cpp
struct ApplicationInfo
{
    std::string name;
    std::string version;
};
```

Caso de uso:

```cpp
class GetApplicationInfo
{
public:
    [[nodiscard]] ApplicationInfo execute() const;
};
```

El CLI usará este caso de uso para implementar:

```bash
codeqa --version
```

Salida:

```text
Code Quality Analyzer 0.1.0
```

Este caso de uso es deliberadamente pequeño. Su finalidad es validar:

- enlace entre ejecutable y biblioteca;
- separación por capas;
- funcionamiento de tests;
- manejo de argumentos;
- empaquetado de versión.

---

## 15. CLI mínima

La CLI deberá soportar:

```bash
codeqa --help
codeqa --version
```

No deberá aceptar todavía:

```bash
codeqa analyze
codeqa discover
codeqa tokens
```

Esos comandos pertenecen a fases posteriores.

### 15.1. Salida de ayuda

Ejemplo:

```text
Code Quality Analyzer

Usage:
  codeqa [options]

Options:
  -h, --help       Show this help.
  -v, --version    Show application version.
```

### 15.2. Argumentos desconocidos

Ejemplo:

```bash
codeqa --unknown
```

Resultado:

```text
Error [CLI001]: Unknown option '--unknown'.
Use 'codeqa --help' for usage.
```

Código de salida:

```text
2
```

### 15.3. Sin argumentos

Decisión recomendada:

- mostrar ayuda breve;
- devolver `0`.

Esta política debe documentarse y probarse.

---

## 16. Códigos de salida iniciales

Se definirá un conjunto mínimo:

```cpp
enum class ExitCode : int
{
    Success = 0,
    InvalidArguments = 2,
    InternalError = 4
};
```

No se añadirán todavía códigos relacionados con quality gates o análisis.

Los códigos podrán ampliarse en fases posteriores sin modificar los existentes.

---

## 17. Manejo de excepciones

### 17.1. Dentro del núcleo

No se impondrá todavía una prohibición global de excepciones.

Reglas iniciales:

- no usar excepciones para flujo normal;
- no capturar y silenciar excepciones;
- no lanzar strings ni enteros;
- utilizar excepciones estándar cuando sean apropiadas;
- documentar funciones que puedan lanzar si no es evidente.

### 17.2. Frontera de la aplicación

`main.cpp` debe contener una barrera final:

```cpp
try
{
    return runApplication(argc, argv);
}
catch (const std::exception& exception)
{
    // Mostrar error interno.
    return static_cast<int>(ExitCode::InternalError);
}
catch (...)
{
    // Mostrar error interno desconocido.
    return static_cast<int>(ExitCode::InternalError);
}
```

La lógica principal deberá residir en una función testeable:

```cpp
int runApplication(
    const std::vector<std::string>& arguments,
    std::ostream& output,
    std::ostream& errorOutput);
```

Esto permite probar la CLI sin lanzar procesos externos.

---

## 18. Logging

No se implementará un sistema de logging en esta fase.

Para la CLI se utilizarán:

- `std::ostream& output`;
- `std::ostream& errorOutput`.

No se añadirá:

- singleton de logger;
- macros de logging;
- niveles configurables;
- archivos de log;
- timestamps.

Estas capacidades se incorporarán únicamente cuando exista un requisito concreto.

---

## 19. Testing

### 19.1. Tests unitarios mínimos

Se crearán tests para:

- versión de aplicación;
- nombre de aplicación;
- `--help`;
- `--version`;
- argumento desconocido;
- código de salida;
- separación entre stdout y stderr;
- valores por defecto de `SourceLocation`;
- valores por defecto de `Diagnostic`.

### 19.2. Test de integración mínimo

Debe ejecutarse el binario y verificar:

```bash
codeqa --version
```

El test podrá implementarse mediante CTest.

### 19.3. Determinismo

La ayuda y la versión deben ser estables y no incluir:

- fecha de compilación;
- rutas locales;
- nombre del usuario;
- datos del entorno.

Esto permite golden tests reproducibles.

---

## 20. Sanitizers

Se añadirá soporte opcional para sanitizers si el compilador lo permite.

Opciones propuestas:

```cmake
option(CODEQA_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(CODEQA_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
```

No deben activarse automáticamente en compilaciones de producción.

Esta fase solo debe preparar la integración, sin complicar el CMake en exceso.

---

## 21. Formateo y análisis estático del propio proyecto

Se recomienda documentar una configuración futura para:

- `clang-format`;
- `clang-tidy`.

No es obligatorio que estén disponibles en el entorno de trabajo.

Puede añadirse un `.clang-format` al repositorio si existe una configuración acordada.

No se debe convertir esta fase en un proyecto secundario de tooling.

---

## 22. README inicial

El `README.md` deberá incluir:

### Descripción

Resumen del objetivo del proyecto.

### Requisitos

- compilador compatible con C++17;
- CMake;
- framework de testing incluido o disponible.

### Compilación

```bash
cmake -S . -B build
cmake --build build
```

### Tests

```bash
ctest --test-dir build --output-on-failure
```

### Ejecución

```bash
./build/codeqa --help
./build/codeqa --version
```

En Windows se adaptará la ruta del ejecutable según el generador utilizado.

### Estado

Debe indicar que el proyecto se encuentra en fase de fundación y todavía no analiza código.

---

## 23. `.gitignore`

Debe excluir al menos:

```text
build/
cmake-build-*/
.vscode/
.idea/
*.user
*.suo
*.VC.db
*.VC.opendb
Testing/
```

No se deben ignorar indiscriminadamente extensiones que puedan formar parte del código o de fixtures.

---

## 24. Pasos de implementación

Los pasos deben ejecutarse en este orden.

---

### Paso 1 — Crear estructura mínima

Crear:

- `CMakeLists.txt`;
- `src/`;
- `include/`;
- `tests/`;
- `docs/`;
- `README.md`;
- `.gitignore`.

#### Criterio de aceptación

CMake configura el proyecto aunque todavía no existan funcionalidades.

---

### Paso 2 — Crear `codeqa_core`

Crear la biblioteca principal con un archivo fuente mínimo.

#### Criterio de aceptación

La biblioteca compila de forma independiente.

---

### Paso 3 — Crear modelos básicos

Implementar:

- `SourceLocation`;
- `SourceRange`;
- `DiagnosticSeverity`;
- `Diagnostic`.

#### Criterio de aceptación

Los modelos tienen tests de construcción y valores por defecto.

---

### Paso 4 — Crear información de aplicación

Implementar:

- `ApplicationInfo`;
- `GetApplicationInfo`.

#### Criterio de aceptación

El caso de uso devuelve nombre y versión correctos.

---

### Paso 5 — Crear CLI mínima

Implementar:

- `runApplication`;
- `--help`;
- `--version`;
- argumentos desconocidos;
- códigos de salida.

#### Criterio de aceptación

La CLI no contiene lógica de negocio ajena al manejo de argumentos.

---

### Paso 6 — Integrar framework de testing

Crear target de tests y habilitar CTest.

#### Criterio de aceptación

```bash
ctest --test-dir build --output-on-failure
```

finaliza correctamente.

---

### Paso 7 — Añadir warnings

Centralizar configuración para GCC, Clang y MSVC.

#### Criterio de aceptación

El proyecto compila sin warnings en el compilador principal.

---

### Paso 8 — Añadir tests de integración

Probar el ejecutable real mediante CTest.

#### Criterio de aceptación

`--version` y `--help` funcionan desde el binario generado.

---

### Paso 9 — Completar documentación

Actualizar README con instrucciones verificadas.

#### Criterio de aceptación

Una persona puede clonar, compilar, probar y ejecutar el proyecto siguiendo únicamente el README.

---

## 25. División recomendada en tareas para un agente

Esta fase no debe entregarse a un agente como una única orden.

Se recomienda dividirla en tareas como las siguientes.

### Tarea 1

```text
Crear la estructura inicial del repositorio y un CMakeLists.txt que genere
una biblioteca codeqa_core vacía.

No crear todavía el ejecutable ni integrar tests.
```

### Tarea 2

```text
Añadir SourceLocation, SourceRange, DiagnosticSeverity y Diagnostic.

Incluir tests unitarios de construcción y valores por defecto.
No añadir modelos de parser, métricas ni reglas.
```

### Tarea 3

```text
Añadir ApplicationInfo y GetApplicationInfo para devolver nombre y versión.

No leer argumentos de línea de comandos todavía.
```

### Tarea 4

```text
Crear el ejecutable codeqa y soportar --help y --version.

La lógica debe residir en runApplication, que recibirá streams inyectados.
Añadir tests unitarios.
```

### Tarea 5

```text
Añadir gestión de argumentos desconocidos y códigos de salida.

No implementar subcomandos.
```

### Tarea 6

```text
Centralizar warnings de compilador en cmake/CompilerWarnings.cmake.

Añadir una opción para warnings como errores.
No modificar la lógica C++.
```

### Tarea 7

```text
Añadir tests de integración con CTest para --help y --version.

No ejecutar todavía análisis de archivos.
```

### Tarea 8

```text
Completar README y .gitignore con instrucciones comprobadas.
```

Cada tarea deberá revisarse y validarse antes de iniciar la siguiente.

---

## 26. Riesgos

### 26.1. Sobrearquitectura

Riesgo:

Crear interfaces, factories y registries para componentes que todavía no existen.

Corrección:

Implementar solo los contratos transversales necesarios.

### 26.2. Acoplar el núcleo a la CLI

Riesgo:

Utilizar `std::cout`, `argc` o `argv` dentro de casos de uso.

Corrección:

La CLI adapta argumentos y streams; el núcleo devuelve datos.

### 26.3. Acoplar el núcleo a Qt

Riesgo:

Utilizar clases Qt por comodidad inicial.

Corrección:

Mantener la biblioteca principal en C++ estándar.

### 26.4. CMake excesivamente complejo

Riesgo:

Añadir empaquetado, instalación, cobertura, sanitizers, documentación y múltiples plataformas de una vez.

Corrección:

Añadir solo lo necesario para compilar, ejecutar y probar.

### 26.5. Dependencias descargadas automáticamente

Riesgo:

Usar `FetchContent` en un entorno sin acceso a internet.

Corrección:

Permitir una dependencia local o incluida en el repositorio.

### 26.6. Mezclar diagnósticos con hallazgos

Riesgo:

Usar el mismo modelo para errores del analizador y defectos del código analizado.

Corrección:

Mantener separados `Diagnostic` y el futuro `Finding`.

### 26.7. Versiones no reproducibles

Riesgo:

Generar la versión a partir de fecha, hostname o Git sin fallback.

Corrección:

Definir versión estable desde CMake.

---

## 27. Criterios de aceptación de la fase

La fase se considerará terminada cuando se cumplan todos estos puntos:

- [ ] El proyecto configura con CMake.
- [ ] El proyecto compila con C++17.
- [ ] Existe una biblioteca `codeqa_core`.
- [ ] Existe un ejecutable `codeqa`.
- [ ] `codeqa --help` funciona.
- [ ] `codeqa --version` funciona.
- [ ] Los argumentos desconocidos producen un diagnóstico claro.
- [ ] Los códigos de salida están documentados.
- [ ] Existe un framework de tests.
- [ ] CTest ejecuta todos los tests.
- [ ] Existen tests unitarios y de integración.
- [ ] El núcleo no depende de Qt.
- [ ] `main.cpp` no contiene lógica de negocio.
- [ ] El proyecto compila sin warnings en el compilador principal.
- [ ] El README permite reproducir compilación y tests.
- [ ] No se ha adelantado funcionalidad de fases posteriores.

---

## 28. Entregable final

El entregable visible será un repositorio capaz de ejecutar:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Y después:

```bash
./build/codeqa --help
./build/codeqa --version
```

Salida esperada de versión:

```text
Code Quality Analyzer 0.1.0
```

La herramienta todavía no analizará código.

---

## 29. Estado esperado al finalizar

El flujo disponible será:

```text
Argumentos CLI
      │
      ▼
runApplication
      │
      ▼
GetApplicationInfo
      │
      ▼
ApplicationInfo
      │
      ▼
Salida de texto
```

Esta fase valida la infraestructura mínima antes de introducir acceso a archivos o parsing.

---

## 30. Siguiente fase

El siguiente documento será:

```text
03-phase-02-source-discovery.md
```

Esa fase añadirá:

- recepción de una ruta de proyecto;
- recorrido de directorios;
- detección de extensiones;
- exclusiones;
- carga segura de archivos;
- orden determinista;
- diagnósticos del sistema de archivos.

No deberá incluir todavía tokenización ni parsing.
