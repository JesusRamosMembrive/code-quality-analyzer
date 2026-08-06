# Corpus de validación

Conjunto de datos de prueba del analizador: archivos fuente C/C++ con expectativas
declaradas, usados para verificar que el frontend (Fase 4) y las métricas (Fase 6)
producen los resultados correctos.

No confundir con `tests/integration/fixtures/`, que contiene fixtures de tests
concretos de la CLI. El corpus es el conjunto de validación del analizador.

## Estructura

| Directorio | Contenido |
|---|---|
| `parsing/` | Estructura básica reconocible (casos mínimos) |
| `functions/` | Funciones libres, métodos, constructores, destructores, lambdas, operadores |
| `classes/` | Clases, structs, enums, namespaces, plantillas declaradas |
| `parameters/` | Firmas: cantidad, valores por defecto, referencias, punteros |
| `complexity/` | Fuentes con ramificación conocida |
| `nesting/` | Fuentes con anidamiento conocido |
| `malformed/` | Código incompleto o roto: debe producir diagnósticos |
| `mixed/` | Archivos que combinan varias categorías |

## Formato de expectativas

Cada fixture es un par de archivos:

```text
functions/cpp-functions.cpp
functions/cpp-functions.parsing.json
```

En la Fase 6 se añadirá un tercer archivo por fixture, `<nombre>.metrics.json`,
con los valores esperados de las métricas. **No existe todavía**: escribir hoy
"complejidad ciclomática = 7" exigiría haber decidido antes si `&&` cuenta como
rama o si cada `case` suma, y esas decisiones pertenecen a la Fase 6.

### `<nombre>.parsing.json`

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

| Campo | Significado |
|---|---|
| `constructs` | IDs de `SUPPORTED-CONSTRUCTS.md` que este fixture ejercita. Obligatorio y no vacío. |
| `functions` | Funciones que el frontend debe encontrar. |
| `classes` | Clases, structs, enums y namespaces que debe encontrar: `name`, `kind`, `startLine`, `endLine`. |
| `diagnostics` | Diagnósticos esperados: solo `line` y `severity`. |

### Reglas

- Las líneas son **1-based**, consistentes con `codeqa::core::SourceLocation`.
- `startLine` es la línea donde empieza la declaración (el tipo de retorno, o la
  línea `template<...>` si la lleva), **no** la de la llave de apertura.
- `endLine` es la línea de la llave de cierre, incluida. Para un prototipo sin
  cuerpo, `startLine == endLine`.
- Un parámetro sin nombre (`void f(int)`) se representa con la cadena vacía `""`,
  preservando su posición. Una función sin parámetros lleva `"parameters": []`.
- El `...` de una función variádica **no** es un parámetro: `int f(int n, ...)`
  lleva `"parameters": ["n"]`.
- Una lambda se registra en `functions` con el nombre `"<lambda>"`. Si hay varias
  en un mismo fixture, se distinguen por su rango de líneas.
- `diagnostics` **nunca** incluye el texto del mensaje: proviene de tree-sitter y
  puede cambiar entre versiones. La línea es lo que debe permanecer estable.
- Un fixture sin errores lleva `"diagnostics": []` explícito. La ausencia de
  diagnósticos es una afirmación deliberada, no un descuido.
- Los valores de `kind` en `classes` son: `class`, `struct`, `union`, `enum`,
  `namespace`.

### Exhaustividad vs. mínimo

- En los fixtures bien formados, `functions` y `classes` son **exhaustivos**: el
  frontend debe encontrar exactamente estas entradas, ni más ni menos.
- En los fixtures de `malformed/`, `functions` y `classes` son un **conjunto
  mínimo**: el frontend debe encontrar al menos estas entradas, y puede recuperar
  entradas adicionales de la región rota como resultado de la recuperación de
  errores de tree-sitter. Un runner que exija igualdad de conjuntos ahí fallaría
  sobre un comportamiento correcto del parser, no sobre un error real.
- Las `functions` (y `classes`) de los fixtures de `malformed/` siguen siendo
  **provisionales, pendientes de la extracción real de funciones y clases en la
  Fase 4b**: el frontend de la Fase 4a solo confirma parseo y diagnósticos —
  `ParsedFile` todavía no expone `functions` ni `classes`. Las líneas de
  `diagnostics`, en cambio, ya no son provisionales: la Fase 4a las verificó
  contra la salida real de tree-sitter (`tests/unit/CorpusParsingTest.cpp`),
  y coinciden con lo que la recuperación de errores produce hoy.

### IDs no verificables: cobertura solo por presencia

El formato de expectativas tiene cuatro campos: `constructs`, `functions`,
`classes` y `diagnostics`. Ninguno de ellos puede expresar todo lo que
`SUPPORTED-CONSTRUCTS.md` declara soportado. Los siguientes IDs están citados
por al menos un fixture — pasan el check 5 de `CheckCorpusIntegrity.cmake` —
pero **no dejan ningún rastro verificable** en el `.parsing.json`: nada en
`functions` ni en `classes` distingue un fixture donde el frontend maneja
correctamente la construcción de uno donde la ignora por completo.

| ID | Por qué no es verificable |
|---|---|
| `C-PP-01` | El `#include` no aparece en ningún campo de la expectativa. |
| `C-DECL-01` | Las variables globales no tienen campo propio; no están en `functions` ni en `classes`. |
| `C-TYPE-04` | `typedef` no tiene valor de `kind` propio: no hay forma de afirmar que se reconoció el alias. |
| `C-FUNC-03` | `static` no se registra en ninguna parte de la entrada de `functions`. |
| `C-FUNC-04` | El formato descarta explícitamente la elipsis (`...`) de `parameters`: una función variádica y una que no lo sea son indistinguibles. |
| `C-FUNC-05` | Un parámetro de tipo puntero a función solo deja su nombre en `parameters`, igual que cualquier otro parámetro. |
| `CPP-PARAM-01` | Los valores por defecto de los parámetros no se registran; `parameters` solo lleva nombres. |
| `CPP-PARAM-02` | Referencia y puntero son indistinguibles en `parameters`: ambos dejan solo el nombre del parámetro. |

`CPP-TYPE-03` es un caso a medias: `enum` y `enum class` colapsan al mismo
`kind: "enum"`, así que un fixture prueba que el frontend reconoce *un* enum
en esa línea, pero no si distingue la variante con ámbito (`enum class`) de la
que no lo tiene.

La consecuencia práctica: un frontend de Fase 4 que ignorara por completo
`typedef`, las variables globales, `static`, los parámetros variádicos y la
distinción referencia/puntero pasaría igualmente los ocho IDs de esta lista,
porque el check 5 solo prueba **citación** (el ID aparece en `constructs` de
algún fixture), no **ejercicio** (el frontend produjo el resultado correcto
para esa construcción). La Fase 4 debe validar estos IDs por algún otro medio
— no basta con que el corpus los cite.

## Verificación

`CheckCorpusIntegrity.cmake` se ejecuta como test de CTest (`codeqa_corpus_integrity`)
y comprueba, sin necesidad del analizador:

1. Todo fixture fuente tiene su `.parsing.json`, y viceversa.
2. Todo `.parsing.json` declara los campos `functions`, `classes` y `diagnostics`.
3. Todo ID citado en un `constructs` existe en `SUPPORTED-CONSTRUCTS.md`.
4. Ningún fixture cita un ID `OUT-*`.
5. Todo ID no-`OUT-*` de `SUPPORTED-CONSTRUCTS.md` está cubierto por al menos un fixture.

La verificación de que el analizador produce realmente estos resultados llega en
la Fase 4, con el runner que lee estos archivos.
