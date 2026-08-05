# Fase 0 — Corpus y criterios de validación: diseño

Fecha: 2026-08-05
Estado: aprobado para pasar a plan de implementación

## 1. Objetivo

Construir el corpus de validación del analizador y definir explícitamente qué subconjunto de C y C++ soporta el MVP, para que "el frontend funciona" deje de ser una afirmación infalsificable.

Esta fase no produce código de producción: produce datos de prueba, documentación de contrato y criterios de éxito.

## 2. Contexto: por qué ahora

La Fase 0 es la única del roadmap con `Dependencias: Ninguna` — estaba pensada para hacerse antes que todo lo demás, pero se saltó y las Fases 1–3 se construyeron sin ella. Ahora bloquea de verdad:

- La Fase 4 (frontend tree-sitter) no tiene definición de "terminado" sin una lista explícita de construcciones soportadas.
- El criterio de aceptación de la Fase 6 dice literalmente *"las métricas coinciden con los fixtures"* — fixtures que no existen.
- Hoy el único fixture del repositorio es `tests/integration/fixtures/discover-sample/` (4 archivos triviales), que existe solo para probar el descubrimiento de archivos de la Fase 2. Ninguno contiene código pensado para ser parseado.

## 3. Alcance

- lista explícita de construcciones C/C++ soportadas, con identificadores estables;
- lista explícita de construcciones **fuera de alcance**, también identificadas;
- fuentes de fixtures para todas las categorías, incluidas las que solo consumirá la Fase 6;
- expectativas de parsing (`*.parsing.json`) para los fixtures cuyo comportamiento define la Fase 4;
- documentación del formato de expectativas y de cómo se usa el corpus;
- selección y documentación de proyectos públicos de validación;
- criterios de éxito verificables.

### Fuera de alcance (diferido, con motivo)

- **Valores esperados de métricas (`*.metrics.json`)** → Fase 6. Afirmar hoy "complejidad ciclomática = 7" exige haber decidido antes si `&&` cuenta como rama, si cada `case` suma, si el ternario cuenta — decisiones que pertenecen a la Fase 6. Los **fuentes** de los fixtures de `complexity/` y `nesting/` sí se escriben ahora, porque son estables: un `for` dentro de un `if` dentro de un `while` no cambia según la fórmula que se elija después.
- **El runner que lee las expectativas** → Fase 4. Esta fase define el formato y escribe los datos; el código que los consume se construye cuando exista algo que ejecutar.
- **La librería JSON vendorizada** → Fase 4, por el mismo motivo.
- **Modificar `tests/integration/fixtures/discover-sample/`** — se queda intacto.

## 4. Decisiones de diseño

1. **Fase 0 se completa entera ahora salvo los valores numéricos de métricas.** El riesgo de "hacerlo todo de golpe" no está en las fixtures, está en los valores esperados; separando ambos, el corpus se completa hoy y la parte volátil llega como adición, no como refactorización.
2. **Fuente y expectativas viven en archivos separados**, siguiendo la convención que el roadmap ya proponía (`sample.cpp` + `sample.expected.json`).
3. **Las expectativas se parten por concern, un tipo de archivo por fase consumidora**, en vez de un `.expected.json` monolítico: `*.parsing.json` (Fase 4), `*.metrics.json` (Fase 6). Un cambio en una fórmula de métrica toca solo archivos `*.metrics.json` — nunca los fuentes, nunca las expectativas de parsing. Este aislamiento es el mecanismo que hace barato el cambio futuro.
4. **Cada construcción soportada tiene un identificador estable, y cada fixture declara qué IDs cubre** (campo `constructs` en su `.parsing.json`). Da trazabilidad explícita: si cambia el soporte de una construcción, un `grep` del ID dice exactamente qué fixtures revisar, sin releerlas todas.
5. **Formato JSON, con `nlohmann/json` vendorizada en la Fase 4.** El roadmap ya dice `.json`, y el proyecto necesita JSON de todas formas para la Fase 9 (informes) y la Fase 10 (snapshots = informe JSON guardado): no es una dirección de dependencia nueva, es la misma llegando antes. Se vendoriza en `third_party/` como doctest y tree-sitter, nunca vía `FetchContent`.
6. **El corpus vive en `tests/corpus/`, separado de `tests/integration/fixtures/`.** Son conceptos distintos: `discover-sample` es el fixture de un test concreto de la CLI; el corpus es el conjunto de validación del analizador. Nombres distintos evitan confusión y cero riesgo de romper los cuatro tests de integración que hoy pasan.
7. **Los proyectos reales se referencian, no se copian.** Un documento lista nombre, URL, commit fijado, licencia y motivo de elección; su código no entra al repositorio. Se usan para validación exploratoria manual, no en la suite automatizada.
8. **Se descarta crear un "proyecto dummy" como objetivo de validación.** El valor de un proyecto real es que nadie diseñó ese código pensando en este analizador; un proyecto sintético reproduce los sesgos de quien lo escribe y no descubre nada que los fixtures no cubran ya.
9. **En `malformed/`, las expectativas afirman la existencia y ubicación de un diagnóstico, nunca su texto exacto.** El texto proviene de tree-sitter y puede cambiar entre versiones; la línea es lo que importa y lo que debe permanecer estable.

## 5. Estructura del corpus

```text
tests/corpus/
├── SUPPORTED-CONSTRUCTS.md      Contrato: qué soporta el MVP, con IDs
├── REAL-PROJECTS.md             Proyectos públicos de validación
├── README.md                    Cómo se usa el corpus y formato de expectativas
├── parsing/                     Estructura básica reconocible
├── functions/                   Funciones libres, métodos, ctors/dtors, lambdas, operadores
├── classes/                     Clases, structs, enums, namespaces, plantillas declaradas
├── parameters/                  Firmas: cantidad, defaults, referencias, variádicas
├── complexity/                  Fuentes con ramificación conocida (valores → Fase 6)
├── nesting/                     Fuentes con anidamiento conocido (valores → Fase 6)
├── malformed/                   Código incompleto o roto → debe producir diagnósticos
└── mixed/                       Archivos que combinan varias categorías
```

`tests/integration/fixtures/discover-sample/` no se toca.

Cada fixture es un par de archivos (un trío cuando llegue la Fase 6):

```text
functions/free-functions.cpp
functions/free-functions.parsing.json      ← esta fase
functions/free-functions.metrics.json      ← Fase 6
```

Los fixtures de `complexity/` y `nesting/` se crean en esta fase **con su fuente y su `.parsing.json`** — su estructura (qué funciones contienen, en qué líneas) sí es conocible hoy. Lo único que falta en ellos es el `.metrics.json`, que llega en la Fase 6.

## 6. Formato de expectativas: `*.parsing.json`

```json
{
  "constructs": ["CPP-FUNC-01", "CPP-FUNC-06"],
  "functions": [
    {
      "name": "add",
      "startLine": 3,
      "endLine": 6,
      "parameters": ["a", "b"]
    }
  ],
  "classes": [],
  "diagnostics": []
}
```

Campos:

| Campo | Significado |
|---|---|
| `constructs` | IDs de `SUPPORTED-CONSTRUCTS.md` que este fixture ejercita. Obligatorio y no vacío. |
| `functions` | Funciones que el frontend debe encontrar: nombre, línea inicial, línea final, nombres de parámetros en orden. |
| `classes` | Clases/structs/enums/namespaces que debe encontrar: nombre, tipo, línea inicial, línea final. |
| `diagnostics` | Diagnósticos esperados: solo `line` y `severity`. **Nunca el texto del mensaje.** |

Reglas para eliminar ambigüedad:

- **Líneas 1-based**, consistentes con `codeqa::core::SourceLocation` (Fase 1).
- **`startLine`** es la línea donde empieza la declaración de la función (la del tipo de retorno o el `template<...>` si lo lleva), no la de la llave de apertura. **`endLine`** es la línea de la llave de cierre, incluida.
- **Parámetros sin nombre** (`void f(int)`, legal y frecuente en C++) se representan con la cadena vacía `""`, preservando la posición. Una función sin parámetros lleva `"parameters": []`.
- **Un fixture sin errores lleva `"diagnostics": []` explícito**, no ausente — la ausencia de diagnósticos es una afirmación deliberada, no un descuido.
- Los IDs de `constructs` deben existir en `SUPPORTED-CONSTRUCTS.md`; un ID inventado o mal escrito es un error del corpus, detectable con el cruce descrito en los criterios de éxito.

## 7. Lista de construcciones: `SUPPORTED-CONSTRUCTS.md`

### C

| ID | Construcción |
|---|---|
| `C-FUNC-01` | Definición de función libre |
| `C-FUNC-02` | Declaración de función (prototipo sin cuerpo) |
| `C-FUNC-03` | Función `static` |
| `C-FUNC-04` | Función variádica (`...`) |
| `C-FUNC-05` | Puntero a función como parámetro |
| `C-TYPE-01` | `struct` |
| `C-TYPE-02` | `union` |
| `C-TYPE-03` | `enum` |
| `C-TYPE-04` | `typedef` |
| `C-DECL-01` | Variable global |
| `C-PP-01` | `#include` (con `<>` y con `""`) |

### C++ (además de todo lo anterior)

| ID | Construcción |
|---|---|
| `CPP-FUNC-01` | Función libre |
| `CPP-FUNC-02` | Método definido dentro de la clase |
| `CPP-FUNC-03` | Método definido fuera de la clase (`Foo::bar()`) |
| `CPP-FUNC-04` | Constructor |
| `CPP-FUNC-05` | Destructor |
| `CPP-FUNC-06` | Sobrecarga de operador |
| `CPP-FUNC-07` | Lambda con cuerpo |
| `CPP-FUNC-08` | Plantilla de función (declarada, no instanciada) |
| `CPP-TYPE-01` | `class` |
| `CPP-TYPE-02` | `struct` |
| `CPP-TYPE-03` | `enum` / `enum class` |
| `CPP-TYPE-04` | Plantilla de clase (declarada, no instanciada) |
| `CPP-TYPE-05` | Clase anidada |
| `CPP-NS-01` | `namespace` |
| `CPP-NS-02` | `namespace` anónimo |
| `CPP-PARAM-01` | Valores de parámetro por defecto |
| `CPP-PARAM-02` | Parámetros por referencia y por puntero |

### Control de flujo (común a C y C++)

Estas construcciones no las consume la Fase 4 para extraer estructura, pero son exactamente lo que la Fase 6 mide (complejidad ciclomática, ramas, anidamiento, retornos). Se identifican aquí para que los fixtures de `complexity/` y `nesting/` puedan declarar qué ejercitan, y para que la Fase 6 tenga un contrato explícito en vez de improvisarlo.

| ID | Construcción |
|---|---|
| `CTRL-01` | `if` / `else if` / `else` |
| `CTRL-02` | `while` |
| `CTRL-03` | `for` (y `for` basado en rango, solo C++) |
| `CTRL-04` | `do`-`while` |
| `CTRL-05` | `switch` / `case` |
| `CTRL-06` | Operador ternario (`?:`) |
| `CTRL-07` | Operadores de cortocircuito (`&&`, `\|\|`) |
| `CTRL-08` | `return` (incluido el caso de retornos múltiples) |
| `CTRL-09` | `break` / `continue` |
| `CTRL-10` | `try` / `catch` (solo C++) |
| `CTRL-11` | `goto` |

Que un `CTRL-*` esté listado significa que el corpus lo cubre y que la Fase 6 debe decidir explícitamente cómo lo cuenta — **no** implica de antemano que sume a la complejidad ciclomática. Esa decisión pertenece a la Fase 6 y se documentará allí.

### Fuera de alcance (identificado, para poder referenciarlo)

| ID | Construcción excluida |
|---|---|
| `OUT-01` | Instanciación de plantillas |
| `OUT-02` | Overload resolution |
| `OUT-03` | Evaluación del preprocesador (`#ifdef` no se razona; el texto se parsea tal cual) |
| `OUT-04` | Macros que generan funciones |
| `OUT-05` | Semántica de herencia |
| `OUT-06` | Deducción y resolución de tipos |
| `OUT-07` | Declaraciones estilo K&R |

Esta tabla de exclusiones tiene el mismo peso que la de inclusiones: convierte "el parser no reconoce esto" en una decisión documentada en vez de un defecto.

## 8. Proyectos reales de validación: `REAL-PROJECTS.md`

Dos proyectos públicos, elegidos por contraste:

| Proyecto | Lenguaje | Licencia | Por qué |
|---|---|---|---|
| SQLite (amalgamación) | C | Dominio público | Un único archivo de ~250k líneas: prueba de estrés de descubrimiento y parsing, con código C real de altísima calidad. Licencia sin fricción alguna. |
| fmt | C++ | MIT | C++ moderno cargado de plantillas, lambdas y sobrecarga de operadores — exactamente las tres construcciones de la zona gris que este spec declara soportadas. |

El documento registra para cada uno: nombre, URL, commit fijado, licencia, motivo de elección y qué se espera que ejercite. Su código no se copia al repositorio.

## 9. Criterios de éxito

### Sobre el corpus (verificables automáticamente a partir de la Fase 4)

- Todo ID de `SUPPORTED-CONSTRUCTS.md` está cubierto por al menos un fixture — comprobable cruzando los IDs del documento con los campos `constructs` de los `.parsing.json`.
- Cero falsos negativos: el frontend encuentra todas las construcciones declaradas en cada `.parsing.json`.
- Cero funciones fantasma: ninguna llamada normal se reporta como definición de función.
- Determinismo: dos ejecuciones sobre el mismo fixture producen resultados idénticos.
- Cada fixture de `malformed/` produce al menos un diagnóstico y no aborta el análisis del resto del archivo.

### Sobre los proyectos reales (validación exploratoria manual)

- El análisis completa sin caídas ni cuelgues sobre ambos proyectos.
- La tasa de regiones `ERROR` se **mide y documenta** en la Fase 4, y ese primer valor se trata como línea base a no empeorar. No se fija hoy un umbral numérico: no existe ningún dato que lo respalde, e inventarlo sería exactamente el tipo de certeza falsa que este proyecto rechaza por principio.

## 10. Criterios de aceptación de esta fase

- existe una lista explícita de construcciones soportadas, con IDs estables;
- existe una lista explícita de construcciones fuera de alcance;
- existen fixtures para los casos básicos de todas las categorías;
- existen fixtures para errores y código incompleto;
- las expectativas son verificables y están en un formato documentado;
- se han seleccionado y documentado al menos dos proyectos públicos de prueba;
- los criterios de éxito están definidos y son comprobables.

## 11. Dependencias

Ninguna. Esta fase no consume nada de las Fases 1–3 y no modifica código de producción.
