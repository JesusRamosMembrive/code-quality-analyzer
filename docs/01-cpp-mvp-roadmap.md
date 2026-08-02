# Code Quality Analyzer — Roadmap del MVP para C y C++

## 1. Propósito de este documento

Este documento define el roadmap general para construir el primer MVP del analizador de calidad de código para proyectos C y C++.

Su objetivo no es describir todavía la implementación detallada de cada componente. Cada fase tendrá posteriormente su propio documento con:

- decisiones de diseño;
- pasos de implementación;
- componentes afectados;
- pruebas;
- criterios de aceptación;
- riesgos;
- entregables.

Este roadmap establece el orden de desarrollo, las dependencias entre fases y el resultado esperado de cada una.

---

## 2. Objetivo del MVP

El primer MVP deberá poder analizar un directorio que contenga código C y C++ y producir un informe determinista con métricas básicas y hallazgos configurables.

El MVP deberá incluir:

- descubrimiento de archivos fuente;
- detección inicial del lenguaje;
- lectura segura de archivos;
- frontend C/C++ basado en tree-sitter (parsing tolerante);
- extracción estructural básica;
- detección de funciones;
- métricas sencillas por función;
- motor de reglas;
- histórico mínimo (snapshot y comparación contra el análisis anterior);
- informe de texto;
- informe JSON;
- diagnósticos del analizador;
- pruebas automatizadas;
- corpus de validación.

El MVP no deberá intentar comprender completamente C o C++.

---

## 3. Lenguajes y estándares iniciales

La herramienta se implementará preferentemente en:

```text
C++17
```

Los lenguajes analizados inicialmente serán:

```text
C
C++11
C++14
C++17
```

La herramienta deberá tolerar construcciones posteriores aunque no las interprete completamente.

Encontrar una construcción no soportada no deberá provocar:

- bloqueo;
- excepción no controlada;
- bucle infinito;
- corrupción del estado interno;
- generación silenciosa de resultados incorrectos.

Cuando no pueda interpretar una región, deberá registrar un diagnóstico.

---

## 4. Principios de ejecución del roadmap

### 4.1. Incrementos pequeños

Cada fase se dividirá en pasos reducidos y verificables.

No se pedirá a un agente que implemente una fase completa en una única tarea si contiene varias responsabilidades.

### 4.2. Flujo vertical temprano

Se priorizará construir pronto un flujo completo y pequeño:

```text
Archivo
    ↓
Lectura
    ↓
Análisis básico
    ↓
Métrica
    ↓
Hallazgo
    ↓
Informe
```

Después se ampliará cada parte.

### 4.3. Tests antes de ampliar alcance

Cada construcción soportada deberá disponer de fixtures y pruebas antes de incorporar variantes más complejas.

### 4.4. Parser tolerante

El parser deberá admitir análisis parcial.

Un archivo podrá contener:

- regiones correctamente analizadas;
- regiones parcialmente analizadas;
- regiones no reconocidas.

El informe deberá distinguir estos casos.

### 4.5. Separación de responsabilidades

El pipeline general será:

```text
Descubrimiento
    ↓
Detección de lenguaje
    ↓
Lectura
    ↓
Frontend tree-sitter
    ↓
Modelo intermedio
    ↓
Métricas
    ↓
Reglas
    ↓
Hallazgos
    ↓
Informes
    ↓
Histórico mínimo
```

Ninguna fase deberá asumir responsabilidades pertenecientes a otra capa.

---

# Fase 0 — Definición del corpus y criterios de validación

## Objetivo

Preparar los datos de prueba y definir qué construcciones deberá soportar la primera versión.

## Alcance

- definir el subconjunto inicial de C;
- definir el subconjunto inicial de C++;
- preparar fixtures;
- definir resultados esperados;
- identificar proyectos reales de prueba;
- establecer criterios de éxito.

## Resultados esperados

Una estructura inicial como:

```text
tests/
├── fixtures/
│   ├── discovery/
│   ├── parsing/
│   ├── functions/
│   ├── complexity/
│   ├── nesting/
│   ├── parameters/
│   ├── malformed/
│   └── mixed/
└── projects/
    ├── synthetic-c/
    ├── synthetic-cpp/
    └── real-samples/
```

Cada fixture relevante tendrá un archivo esperado:

```text
sample.cpp
sample.expected.json
```

## Criterios de aceptación

- existe una lista explícita de construcciones soportadas;
- existen fixtures para los casos básicos;
- existen fixtures para errores y código incompleto;
- las expectativas son verificables;
- se han seleccionado al menos dos proyectos de prueba.

## Dependencias

Ninguna.

---

# Fase 1 — Fundación del proyecto

## Objetivo

Crear la estructura técnica mínima del repositorio y un ejecutable CLI funcional.

## Alcance

- estructura de directorios;
- configuración de compilación;
- ejecutable principal;
- framework de testing;
- convenciones de código;
- modelos básicos compartidos;
- sistema inicial de errores y diagnósticos.

## Componentes previstos

```text
src/
├── cli/
├── application/
├── core/
├── frontends/
├── infrastructure/
└── reporting/
```

## Resultado esperado

El programa deberá poder ejecutarse:

```bash
codeqa --help
```

y devolver información básica de uso.

## Criterios de aceptación

- el proyecto compila con C++17;
- existe un ejecutable CLI;
- existe al menos un test automatizado;
- los errores de entrada se gestionan;
- la compilación es reproducible.

## Dependencias

Fase 0.

---

# Fase 2 — Descubrimiento y carga de archivos

## Objetivo

Localizar y cargar archivos fuente desde un directorio de proyecto.

## Alcance

- recorrido recursivo;
- extensiones soportadas;
- exclusión de directorios;
- exclusión mediante patrones;
- lectura segura;
- normalización de rutas;
- orden determinista;
- errores de acceso;
- archivos vacíos;
- archivos grandes.

## Extensiones iniciales

```text
C:
.c

C++:
.cc
.cpp
.cxx
.hpp
.hh
.hxx

Ambiguos:
.h
```

## Resultado esperado

Ejemplo:

```bash
codeqa discover ./project
```

Salida conceptual:

```text
Files discovered: 42
C files: 7
C++ files: 29
Ambiguous headers: 6
Excluded files: 14
Read errors: 0
```

## Criterios de aceptación

- todos los archivos soportados se descubren;
- las exclusiones se respetan;
- el resultado mantiene un orden estable;
- una ruta inexistente produce un error claro;
- un archivo inaccesible no detiene todo el análisis.

## Dependencias

Fase 1.

---

# Fase 3 — Detección inicial del lenguaje

## Objetivo

Asignar a cada archivo un lenguaje probable.

## Alcance

- detección por extensión;
- tratamiento de archivos `.h`;
- overrides de configuración;
- nivel de confianza;
- razón de detección;
- estado desconocido o ambiguo.

## Resultado esperado

Modelo conceptual:

```text
LanguageDetectionResult
├── language
├── confidence
└── reason
```

Ejemplo:

```text
include/network/socket.h
Language: C
Confidence: 0.80
Reason: Configured path override.
```

## Criterios de aceptación

- `.c` se clasifica como C;
- `.cpp`, `.cc` y `.cxx` se clasifican como C++;
- `.h` puede quedar como ambiguo;
- los overrides tienen prioridad;
- la herramienta no inventa certeza cuando no existe.

## Dependencias

Fase 2.

---

# Fase 4 — Frontend C/C++ basado en tree-sitter

## Objetivo

Convertir archivos C y C++ en un árbol sintáctico concreto (CST) tolerante a errores, y extraer de él los elementos estructurales básicos, sin escribir un lexer ni un parser propios.

## Motivación

Escribir a mano un lexer y un parser tolerante para C++ es la parte de mayor riesgo de todo el roadmap: la gramática es notoriamente ambigua (most vexing parse, macros, templates) y esa complejidad no aporta valor diferencial al producto — el valor está en métricas, reglas e histórico, no en el parsing en sí.

tree-sitter resuelve exactamente el problema que necesitamos: es un parser incremental diseñado para editores, tolerante a errores por construcción, con gramáticas ya maduras para C y C++ (licencia MIT). Cuando encuentra una región que no puede interpretar, la representa como nodos `ERROR` / `MISSING` dentro del propio árbol — es decir, ya modela el concepto de "diagnóstico de parsing" que este proyecto necesita, sin tener que construirlo desde cero.

## Alcance

- vendorizar el runtime de tree-sitter (C puro) y las gramáticas `tree-sitter-c` y `tree-sitter-cpp` en `third_party/` del repositorio (sin `FetchContent`, sin submodules — compilación reproducible sin acceso a red);
- invocar el parser sobre cada archivo fuente y obtener el CST;
- convertir los nodos `ERROR` / `MISSING` del árbol en `Diagnostic`s del analizador;
- escribir *queries* tree-sitter (`.scm`) para localizar en el CST:
  - funciones libres;
  - métodos, constructores, destructores;
  - namespaces, clases, structs, enums;
  - includes;
  - parámetros;
  - rangos de código de cada elemento;
- un adaptador que traduzca los resultados de las queries a modelos estructurales intermedios (ver Fase 5).

## Fuera de alcance

- resolución completa de tipos;
- overload resolution;
- instanciación de templates;
- evaluación del preprocesador;
- macros que generan funciones;
- semántica de herencia;
- deducción de tipos;
- parsing incremental (tree-sitter lo soporta, pero no es necesario para el MVP).

## Resultado esperado

Modelos conceptuales (sin cambios respecto al plan original, solo cambia cómo se obtienen):

```text
FileAnalysisModel
FunctionModel
ClassModel
ParameterModel
IncludeDependency
```

## Criterios de aceptación

- el runtime y las gramáticas de tree-sitter compilan vendorizados, sin acceso a red;
- se detectan funciones básicas mediante queries;
- se reconocen métodos definidos fuera de clase, constructores y destructores;
- cada nodo `ERROR`/`MISSING` del árbol genera un `Diagnostic` con su ubicación;
- el parser no genera funciones falsas a partir de llamadas normales;
- repetir el análisis sobre el mismo archivo produce el mismo árbol y los mismos diagnósticos.

## Dependencias

Fase 3.

---

# Fase 5 — Modelo intermedio común

## Objetivo

Convertir el resultado del frontend de C/C++ en una representación independiente de la sintaxis concreta.

## Alcance

- proyecto;
- archivo;
- símbolo;
- función;
- método;
- clase;
- parámetro;
- variable;
- control de flujo;
- llamada;
- dependencia;
- ubicación;
- diagnóstico;
- detalles específicos de lenguaje.

## Resultado esperado

El motor de métricas deberá poder trabajar con el modelo sin consultar directamente los tokens originales salvo cuando una métrica lo requiera expresamente.

## Criterios de aceptación

- el frontend produce un modelo estable;
- las métricas no dependen del parser concreto;
- C y C++ pueden compartir métricas comunes;
- el modelo admite extensiones específicas de C++;
- no se reduce todo a nodos genéricos sin tipado.

## Dependencias

Fase 4.

---

# Fase 6 — Métricas básicas por función

## Objetivo

Calcular las primeras métricas objetivas y verificables.

## Métricas iniciales

### Tamaño

- líneas físicas;
- líneas con código;
- líneas vacías;
- líneas de comentario;
- número de statements.

### Firma

- número de parámetros;
- longitud de la firma;
- número de parámetros booleanos.

### Complejidad

- complejidad ciclomática;
- complejidad de condiciones booleanas;
- profundidad máxima de anidamiento;
- número de ramas;
- número de retornos.

## Resultado esperado

Ejemplo:

```text
Function: ProtocolRouter::detectProtocol
Physical lines: 81
Code lines: 63
Parameters: 4
Cyclomatic complexity: 14
Condition complexity: 5
Maximum nesting: 4
Returns: 6
```

## Criterios de aceptación

- las métricas coinciden con los fixtures;
- la definición de cada métrica está documentada;
- las métricas no generan hallazgos por sí mismas;
- los cálculos son reproducibles;
- las métricas se calculan igualmente en regiones con errores de parsing (con fines de diagnóstico); es en la Fase 7 (motor de reglas) donde se decide si esas regiones generan hallazgos.

## Dependencias

Fase 5.

---

# Fase 7 — Motor de reglas y hallazgos

## Objetivo

Evaluar métricas y patrones mediante reglas configurables.

## Decisiones de diseño

- **Interfaz única de regla.** Todas las reglas implementan el mismo contrato — `Rule::evaluate(context) -> vector<Finding>` — sin distinguir a nivel de tipo entre reglas de umbral numérico (p.ej. complejidad ciclomática) y reglas booleanas o de patrón (p.ej. ausencia de `override`). Cada regla lee del contexto/configuración solo los campos que necesita. No se introduce una jerarquía de tipos (`ThresholdRule` / `PatternRule`): sería una abstracción anticipada sin un segundo caso de uso concreto que la justifique todavía.
- **Confianza ante errores de parsing.** Si el rango de una función se solapa con un nodo `ERROR`/`MISSING` del árbol de tree-sitter (Fase 4), sus métricas se calculan igualmente para diagnóstico, pero no se generan `Finding`s sobre ella — solo el `Diagnostic` de parsing correspondiente. No se introduce un score de confianza numérico por hallazgo en el MVP.
- **Supresión.** Un hallazgo se suprime mediante un comentario inline junto a la línea afectada, p.ej. `// codeqa-disable-next-line <rule-id>`. No hay supresión únicamente por configuración en el MVP.

## Alcance

- identificador de regla;
- severidad;
- activación;
- parámetros (umbral, patrón u otros según la regla);
- alcance por lenguaje;
- descripción;
- recomendación;
- valor observado (cuando aplique);
- valor límite (cuando aplique);
- ubicación;
- símbolo afectado;
- supresión mediante comentario inline.

## Reglas iniciales propuestas

- función demasiado larga;
- demasiados parámetros;
- complejidad ciclomática elevada;
- condición demasiado compleja;
- nesting excesivo;
- demasiados retornos;
- demasiadas variables locales;
- parámetros booleanos consecutivos.

## Resultado esperado

```text
[HIGH] complexity.cyclomatic
src/protocol/ProtocolRouter.cpp:84

Function:
ProtocolRouter::detectProtocol

Measured value: 14
Configured limit: 10
```

## Criterios de aceptación

- métricas y reglas están separadas;
- cada hallazgo tiene una ubicación;
- cada hallazgo identifica su regla;
- los parámetros de cada regla son configurables;
- una regla puede desactivarse;
- una regla específica no se ejecuta sobre lenguajes incompatibles;
- una función que se solapa con una región `ERROR`/`MISSING` no genera hallazgos, solo diagnóstico;
- un hallazgo suprimido mediante comentario inline no aparece en el informe.

## Dependencias

Fase 6.

---

# Fase 8 — Configuración

## Objetivo

Permitir adaptar el análisis a cada proyecto sin recompilar la herramienta.

## Decisiones de diseño

- **Configuración por defecto embebida.** El binario incluye una configuración por defecto para todas las reglas. `codeqa.json` es opcional.
- **Merge por regla.** Si se proporciona `codeqa.json`, se hace un merge superficial por `rule-id` sobre los valores por defecto: una regla mencionada en el archivo del usuario reemplaza completamente esa entrada (no hay merge profundo dentro de los parámetros de una regla individual); las reglas no mencionadas conservan su valor por defecto. Se descarta el reemplazo total de la configuración porque obligaría a repetir toda la config para un solo ajuste.
- **Sin auto-descubrimiento.** `codeqa.json` solo se usa si se pasa explícitamente con `--config`. Si no se pasa, se usan los valores por defecto embebidos — no se busca el archivo en el sistema de ficheros. Esto evita comportamiento implícito y es coherente con el principio de determinismo del proyecto.

## Alcance

- rutas excluidas;
- overrides de lenguaje;
- reglas activas y sus parámetros (esquema flexible por regla, ver Fase 7);
- severidades;
- listas permitidas;
- patrones ignorados;
- formato de salida.

## Resultado esperado

Ejemplo conceptual — nótese que cada regla tiene su propio esquema de parámetros; no todas comparten `warning`/`high`:

```json
{
    "languages": {
        "headerOverrides": [
            {
                "pattern": "include/c_api/**",
                "language": "c"
            }
        ]
    },
    "rules": {
        "cyclomatic-complexity": {
            "enabled": true,
            "params": { "warning": 10, "high": 15 }
        },
        "function-length": {
            "enabled": true,
            "params": { "warning": 50, "high": 90 }
        },
        "missing-override": {
            "enabled": true,
            "severity": "warning"
        }
    }
}
```

## Criterios de aceptación

- existe una configuración por defecto embebida en el binario;
- una regla no mencionada en `codeqa.json` conserva su valor por defecto (merge por regla);
- una regla mencionada en `codeqa.json` reemplaza completamente su entrada por defecto;
- sin `--config`, no se busca ningún archivo de configuración en el sistema de archivos;
- una configuración inválida genera errores claros;
- los valores desconocidos no se ignoran silenciosamente;
- la configuración efectiva puede mostrarse;
- los tests pueden proporcionar configuraciones aisladas.

## Dependencias

Fase 7.

---

# Fase 9 — Informes de texto y JSON

## Objetivo

Generar resultados legibles por personas y consumibles por otras herramientas.

## Alcance

### Texto

- resumen;
- diagnósticos;
- métricas;
- hallazgos;
- agrupación por severidad;
- agrupación por archivo;
- resultado final.

### JSON

- versión del esquema;
- metadatos;
- archivos;
- símbolos;
- métricas;
- hallazgos;
- diagnósticos;
- configuración relevante.

## Resultado esperado

```bash
codeqa analyze ./project
```

y:

```bash
codeqa analyze ./project     --format json     --output report.json
```

## Criterios de aceptación

- el informe de texto es legible;
- el JSON es determinista;
- el esquema incluye versión;
- el informe no depende de Qt;
- los campos variables pueden excluirse de golden tests;
- el JSON puede utilizarse posteriormente por la GUI.

## Dependencias

Fase 8.

---

# Fase 10 — Histórico mínimo

## Objetivo

Guardar snapshots de análisis y compararlos contra el snapshot anterior, para distinguir hallazgos nuevos, existentes y resueltos.

## Motivación

El histórico completo (tendencias multi-snapshot, comparación entre revisiones arbitrarias) queda fuera del MVP, pero una versión mínima es justamente la funcionalidad que más valor da al propósito declarado del proyecto: revisar si un cambio reciente (de un agente o de una persona) introdujo problemas nuevos, sin que la deuda técnica heredada bloquee la revisión. Por eso se adelanta desde "fases posteriores al MVP" a esta fase.

## Alcance

- guardar cada análisis como un snapshot en `.codeqa/snapshots/<timestamp>.json`;
- el snapshot reutiliza el mismo esquema versionado que el informe JSON de la Fase 9 — no se define un segundo formato en paralelo;
- comparar el snapshot actual contra el snapshot anterior más reciente;
- clasificar cada hallazgo como nuevo, existente o resuelto;
- comando para listar/consultar snapshots guardados.

## Fuera de alcance

- comparación entre dos snapshots arbitrarios (no solo "contra el anterior");
- tendencias a lo largo de múltiples snapshots;
- almacenamiento en base de datos (SQLite u otro) — se descarta por ser sobre-ingeniería para una comparación que en el MVP es solo "contra el snapshot anterior";
- interfaz gráfica para navegar el histórico.

## Resultado esperado

```text
Findings: 3 new, 12 existing, 1 resolved
```

## Criterios de aceptación

- cada ejecución de `analyze` puede guardar un snapshot;
- un snapshot es un JSON versionado, reutilizando el esquema de la Fase 9;
- comparar dos snapshots consecutivos clasifica correctamente nuevo/existente/resuelto;
- sin snapshot anterior, la comparación se omite sin error.

## Dependencias

Fase 9.

---

# Fase 11 — Validación sistemática

## Objetivo

Medir la exactitud y utilidad del analizador.

## Alcance

- tests unitarios;
- tests de integración;
- fixtures parametrizados;
- golden tests;
- tests de regresión;
- comparación con revisión humana;
- revisión auxiliar mediante agentes;
- registro de falsos positivos;
- precisión por regla.

## Resultado esperado

Informe interno por regla:

```text
Rule: complexity.cyclomatic
Examples reviewed: 100
Accepted: 98
Rejected: 2
Precision: 98 %
```

## Criterios de aceptación

- toda métrica dispone de fixtures;
- todo bug corregido añade un test de regresión;
- los falsos positivos se registran;
- una regla experimental se distingue de una estable;
- el agente no se considera fuente de verdad;
- las decisiones finales de validación son revisables.

## Dependencias

Todas las fases anteriores.

---

# Fase 12 — Primer MVP completo

## Objetivo

Consolidar todas las fases anteriores en una versión utilizable.

## Comando objetivo

```bash
codeqa analyze ./project     --config codeqa.json     --format json     --output report.json
```

## El MVP deberá producir

- archivos analizados;
- lenguajes detectados;
- funciones detectadas;
- diagnósticos;
- métricas por función;
- hallazgos;
- snapshot guardado y comparación contra el snapshot anterior (Fase 10);
- resumen por severidad;
- informe de texto;
- informe JSON;
- código de salida coherente con el quality gate.

## Quality gate (código de salida 1)

- **Con snapshot anterior disponible:** el gate falla solo si hay hallazgos **nuevos** (que no existían en el snapshot anterior) con severidad igual o superior a la configurada. Los hallazgos heredados de código legado no hacen fallar el gate.
- **Sin snapshot anterior (primera ejecución):** el gate falla en modo absoluto — cualquier hallazgo con severidad igual o superior a la configurada. Esto evita que la primera ejecución quede sin ninguna protección.

## Códigos de salida propuestos

```text
0: análisis completado sin incumplimientos bloqueantes
1: análisis completado con quality gate fallido
2: error de configuración
3: error de entrada
4: fallo interno
```

## Criterios de aceptación

- el análisis completo es reproducible;
- los resultados coinciden con el corpus esperado;
- la herramienta no se bloquea ante archivos problemáticos;
- los hallazgos son accionables;
- el motor no depende de una interfaz gráfica;
- el quality gate es diferencial cuando hay snapshot anterior, y absoluto en la primera ejecución;
- el MVP puede analizar al menos un proyecto real completo.

## Dependencias

Fases 0–11.

---

# Fases posteriores al MVP

Estas fases quedan fuera del primer MVP, pero condicionan algunas decisiones arquitectónicas.

## Histórico avanzado

El histórico mínimo (snapshot + comparación contra el anterior) ya forma parte del MVP (Fase 10). Quedan fuera del MVP:

- comparación entre dos snapshots arbitrarios (no solo contra el anterior);
- tendencias de métricas a lo largo de múltiples snapshots;
- almacenamiento más allá de archivos JSON (p.ej. si el volumen de snapshots lo justificara en el futuro).

## Duplicación

- secuencias normalizadas;
- fingerprints;
- clones exactos;
- clones parciales;
- candidatos a reutilización.

## Arquitectura

- grafo de includes;
- ciclos;
- dependencias prohibidas;
- capas;
- fan-in;
- fan-out.

## Reglas avanzadas de C

- memoria;
- arrays;
- macros;
- API insegura;
- punteros;
- tamaños.

## Reglas avanzadas de C++

- ownership;
- Rule of Five;
- destructores virtuales;
- slicing;
- `std::move`;
- casts;
- `override`;
- excepciones;
- smart pointers.

## Frontend alternativo basado en Clang

- AST real;
- compile commands;
- resolución de tipos;
- análisis semántico;
- adaptación al modelo común.

## Interfaz Qt/QML

- selección de proyecto;
- filtros;
- navegación por hallazgos;
- histórico;
- comparación;
- supresiones;
- exportación.

## Soporte para Python

- tokenizer;
- indentación;
- parser;
- adaptación al modelo intermedio;
- reglas comunes;
- reglas específicas.

---

# Orden recomendado de documentos detallados

Después de este roadmap se crearán los documentos siguientes:

```text
02-phase-01-project-foundation.md
03-phase-02-source-discovery.md
04-phase-03-language-detection.md
05-phase-04-treesitter-frontend.md
06-phase-05-common-analysis-model.md
07-phase-06-basic-metrics.md
08-phase-07-rule-engine.md
09-phase-08-configuration.md
10-phase-09-reporting.md
11-phase-10-minimal-history.md
12-phase-11-validation.md
13-phase-12-mvp-integration.md
```

Cada documento deberá poder transformarse en tareas pequeñas para un agente.

---

# Plantilla obligatoria para cada fase

Cada documento de fase deberá contener:

## Objetivo

Qué capacidad se añade.

## Motivación

Por qué esta fase es necesaria.

## Alcance

Qué se implementará.

## Fuera de alcance

Qué no debe implementar el agente.

## Requisitos previos

Qué fases deben estar terminadas.

## Decisiones de diseño

Contratos y restricciones técnicas.

## Pasos de implementación

Tareas pequeñas y ordenadas.

## Pruebas

Tests unitarios, integración, fixtures y regresión.

## Criterios de aceptación

Condiciones objetivas para cerrar la fase.

## Riesgos

Errores de diseño o implementación previsibles.

## Entregable

Resultado visible y ejecutable.

---

# Definición de una tarea adecuada para un agente

Una tarea será adecuada cuando:

- tenga una única responsabilidad principal;
- modifique un conjunto reducido de archivos;
- incluya criterios de aceptación;
- especifique tests;
- indique expresamente qué no debe hacer;
- pueda revisarse de forma completa;
- no requiera asumir decisiones arquitectónicas no documentadas.

Ejemplo adecuado:

```text
Añadir la query tree-sitter y el adaptador que extraen funciones libres
(sin métodos, sin constructores) desde el CST de C++ hacia FunctionModel.

Requisitos:
- conservar rango de código (SourceRange) de cada función;
- convertir cada nodo ERROR/MISSING solapado en un Diagnostic;
- añadir seis fixtures;
- no implementar todavía métodos, constructores ni destructores.
```

Ejemplo inadecuado:

```text
Implementa el frontend completo de C++.
```

---

# Criterio de finalización del roadmap

El roadmap se considerará completado cuando exista un MVP capaz de realizar este flujo:

```text
Directorio C/C++
        ↓
Descubrimiento
        ↓
Detección de lenguaje
        ↓
Frontend tree-sitter (parsing + extracción estructural)
        ↓
Modelo común
        ↓
Métricas
        ↓
Reglas
        ↓
Hallazgos
        ↓
Informe de texto y JSON
        ↓
Snapshot e histórico mínimo
```

El siguiente paso será redactar `02-phase-01-project-foundation.md`.
