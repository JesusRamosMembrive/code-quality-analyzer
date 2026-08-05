# Construcciones soportadas por el MVP

Este documento es el contrato que define "terminado" para el frontend (Fase 4).
Cada construcción tiene un identificador estable. Cada fixture del corpus declara,
en el campo `constructs` de su `.parsing.json`, qué identificadores ejercita.

Si el soporte de una construcción cambia, `grep` de su ID indica exactamente qué
fixtures hay que revisar.

## C

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

## C++ (además de todo lo anterior)

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
| `CPP-TYPE-06` | Clase derivada (cláusula base reconocida) |
| `CPP-NS-01` | `namespace` |
| `CPP-NS-02` | `namespace` anónimo |
| `CPP-PARAM-01` | Valores de parámetro por defecto |
| `CPP-PARAM-02` | Parámetros por referencia y por puntero |

`CPP-TYPE-06` cubre únicamente que la cláusula base (`: public Base`) se
reconozca como parte de la sintaxis de la clase derivada — que el frontend
encuentre `D` en `class D : public B { ... }` sin que la cláusula base rompa
el reconocimiento. La semántica de la herencia (miembros heredados, `virtual`,
resolución de la jerarquía, slicing, etc.) permanece fuera de alcance: eso es
`OUT-05`, sin cambios.

## Control de flujo (común a C y C++)

Estas construcciones no las usa la Fase 4 para extraer estructura, pero son
exactamente lo que mide la Fase 6 (complejidad ciclomática, ramas, anidamiento,
retornos). Se identifican para que los fixtures de `complexity/` y `nesting/`
puedan declarar qué ejercitan.

Que un `CTRL-*` esté listado significa que el corpus lo cubre y que la Fase 6
debe decidir explícitamente cómo lo cuenta. **No** implica que sume a la
complejidad ciclomática: esa decisión pertenece a la Fase 6.

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

## Fuera de alcance

Estas construcciones están explícitamente **excluidas** del MVP. Se identifican
para poder referenciarlas: que el frontend no las reconozca es una decisión
documentada, no un defecto.

Los IDs `OUT-*` nunca aparecen en el campo `constructs` de un fixture.

| ID | Construcción excluida |
|---|---|
| `OUT-01` | Instanciación de plantillas |
| `OUT-02` | Overload resolution |
| `OUT-03` | Evaluación del preprocesador (`#ifdef` no se razona; el texto se parsea tal cual) |
| `OUT-04` | Macros que generan funciones |
| `OUT-05` | Semántica de herencia |
| `OUT-06` | Deducción y resolución de tipos |
| `OUT-07` | Declaraciones estilo K&R |
