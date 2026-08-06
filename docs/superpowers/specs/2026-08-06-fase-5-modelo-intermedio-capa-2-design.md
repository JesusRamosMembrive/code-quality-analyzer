# Fase 5, capa 2 — cuerpos de función: diseño

Fecha: 2026-08-06
Estado: aprobado para revisión de especificación

## 1. Objetivo

Completar el modelo intermedio común de Fase 5 con la estructura interna de
cada función: variables locales, control de flujo y llamadas. El resultado
sigue siendo tipado, determinista e independiente de tree-sitter, y prepara
la Fase 6 sin implementar todavía métricas.

## 2. Límites

Esta capa cubre C y C++ y conserva solo información sintáctica. No resuelve
símbolos ni tipos, no construye un CFG, no guarda AST genéricos ni texto de
cuerpos, y no calcula métricas ni emite reglas o informes. Los parámetros
permanecen exclusivamente en `CallableModel`; no se duplican como locales.

Quedan fuera `goto`, `throw`, `try`/`catch`, corutinas, macros, preprocesado,
evaluación de expresiones y resolución del destino de las llamadas.

## 3. Modelo público

`CallableModel` gana estas colecciones ordenadas establemente por rango:

- `localVariables`, de `LocalVariableModel`: identidad, tipo textual,
  `hasInitializer` y `lexicalDepth`.
- `controlFlow`, de `ControlFlowModel`: `ControlFlowKind`, rango,
  `nestingDepth` y `conditionRange` opcional.
- `calls`, de `CallModel`: texto del callee, `CallKind`, número de argumentos
  y rango.

`ControlFlowKind` distingue `If`, `Switch`, `For`, `While`, `DoWhile`, `Case`,
`Default`, `Conditional`, `Return`, `Break` y `Continue`. `conditionRange`
solo existe para las construcciones que poseen condición; permite a la Fase 6
calcular complejidad booleana sin consultar el parser.

`CallKind` distingue `Direct`, `Member` e `Indirect`. Una llamada directa usa
un identificador; una de miembro usa `.` o `->`; la indirecta abarca callbacks
u otras expresiones invocables. `calleeText` conserva la grafía fuente, pero
ningún modelo pretende identificar una función destino.

Los prototipos no tienen estas colecciones pobladas. Funciones, métodos,
operadores y lambdas con cuerpo sí pueden tenerlas. Un cuerpo de lambda se
atribuye solo a su propia callable, nunca se duplica en la función envolvente.

## 4. Flujo de datos

`codeqa::frontend` añade los agregados paralelos a `FunctionModel` y
`StructuralExtractor` extrae los candidatos desde cada cuerpo de función.
Las queries capturan los nodos relevantes; el adaptador frontend utiliza la
estructura CST solo para confirmar pertenencia, clasificar la forma de llamada,
calcular profundidades y obtener rangos/texto fuente.

`ProjectModelBuilder` transforma esos valores mediante conversiones explícitas
en `codeqa::model`. Ningún header público de `model` expone tree-sitter.
`AnalyzeProject` no cambia: continúa obteniendo `ParsedFile` y construyendo el
modelo común mediante el builder.

## 5. Errores y determinismo

En archivos parciales se conservan por separado los elementos cuyo subárbol no
contenga nodos `ERROR` ni `MISSING`. Un candidato ambiguo se descarta, sin
inventar estructura; los diagnósticos y el estado `Partial` del archivo se
mantienen. Un error en una función no descarta los elementos fiables de otra.

Las colecciones de cada callable se ordenan por inicio y fin de rango, con
empates estables. El orden de parámetros, callables y archivos ya establecido
en la capa 1 no cambia.

## 6. Verificación

Las pruebas unitarias del extractor cubrirán C y C++ con locales anidados e
inicializadores de `for`, los once tipos de flujo, condiciones, llamadas
directas/de miembro/indirectas y lambdas sin duplicación. Las pruebas del
builder comprobarán el mapeo tipado, orden estable y pertenencia a la callable.
Las pruebas de integración verificarán recuperación parcial y el modelo del
corpus. El corpus documentará que esta capa completa locales, flujo y llamadas
sin cambiar la salida de `codeqa parse`.

La validación final incluye compilación Debug, pruebas focalizadas, CTest,
`codeqa parse tests/corpus` con el resumen `13/4/9/2` y `git diff --check`.
