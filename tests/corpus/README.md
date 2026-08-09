# Corpus de validación

El corpus contiene pares de fuentes C/C++ y expectativas declarativas para el
frontend. No es el conjunto de `tests/integration/fixtures/`: las expectativas
de este directorio prueban la extracción estructural de cada archivo fuente.

## Estructura

| Directorio | Contenido |
|---|---|
| `parsing/` | Casos mínimos de parsing |
| `functions/` | Funciones, métodos, operadores, lambdas y plantillas |
| `classes/` | Clases, structs, enums, namespaces y herencia sintáctica |
| `parameters/` | Valores por defecto, referencias y punteros |
| `complexity/`, `nesting/` | Construcciones que la Fase 6 medirá |
| `malformed/` | Fuentes rotas con diagnósticos estables |
| `mixed/` | Combinaciones de varias categorías |

## Esquema de `<nombre>.parsing.json`

Cada fuente `.c`, `.cpp`, `.h` o `.hpp` necesita un archivo hermano
`.parsing.json`. Todos los archivos de expectativa declaran estas cinco claves
de primer nivel:

```json
{
  "constructs": ["CPP-FUNC-01"],
  "functions": [
    {
      "name": "add",
      "kind": "free",
      "startLine": 3,
      "endLine": 6,
      "parameters": [
        {
          "name": "left",
          "passing": "value",
          "hasDefaultValue": false,
          "type": "int"
        }
      ],
      "isStatic": false,
      "isVariadic": false
    }
  ],
  "classes": [],
  "includes": [
    {
      "target": "vector",
      "delimiter": "system",
      "startLine": 1,
      "endLine": 1
    }
  ],
  "diagnostics": []
}
```

`constructs` contiene IDs no vacíos de `SUPPORTED-CONSTRUCTS.md`.

Cada función usa `kind` con uno de `free`, `method`, `constructor`,
`destructor`, `operator`, `lambda` o `prototype`. Cada parámetro conserva su
posición, incluidos los sin nombre mediante `"name": ""`; `passing` es
`value`, `pointer`, `reference` o `function-pointer`. La elipsis de una
función variádica no es un parámetro y se representa con `isVariadic`.
Cada parámetro también declara `type`, el texto fuente de su tipo declarado
sin los marcadores del declarador (`*`, `&`), que `passing` ya identifica.

Cada entrada de `classes` usa `kind` (`class`, `struct`, `union`, `enum` o
`namespace`), `isTemplate`, `isScopedEnum` y `bases`. Cada entrada de
`includes` usa `delimiter` `local` o `system`. Los diagnósticos comparan
`line` y `severity`; pueden declarar `column` cuando sea relevante, pero nunca
el texto del mensaje.

## Reglas de comparación

- Las líneas son 1-based. `startLine` incluye el tipo de retorno o la línea de
  `template`, y `endLine` incluye el cierre; un prototipo tiene el mismo inicio
  y fin.
- En fixtures bien formados, `functions`, `classes` e `includes` son secuencias
  exhaustivas y ordenadas por fuente.
- En `malformed/`, funciones y clases son una subsecuencia mínima ordenada: la
  recuperación de tree-sitter puede aportar entradas adicionales. Los includes y
  diagnósticos se comparan de forma exacta en todos los fixtures.
- Antes de comparar estructura, el runner exige que el archivo se haya parseado.
  Un error de esquema, JSON ilegible o una fuente sin expectativa hermana falla
  la prueba con la ruta del fixture.

## Límites de Fase 5

La Fase 5 está completa: además de archivos, diagnósticos, funciones, tipos,
dependencias, variables de ámbito de archivo y aliases `typedef`, el modelo
común expone datos tipados de cuerpo de función para variables locales, flujo
de control y llamadas. Esta frontera sigue siendo sintáctica: no incluye
resolución de llamadas o tipos, CFG, AST genérico ni texto de cuerpo, métricas,
reglas ni informes.

`classes/c-types.c` contiene `typedef struct Point PointAlias;`. La gramática
actual también entrega el `struct Point` de la línea 24 como una captura
sintáctica, y la expectativa exhaustiva la conserva para fijar la salida real
del extractor. Esa entrada no representa `PointAlias`; la capa de modelo común
lo adapta por separado como un alias de tipo.

## Límites de Fase 6a

`CPP-PARAM-01` y `CPP-PARAM-02` tienen ahora aserción exacta de `type` en
`functions/cpp-functions.parsing.json`, `parameters/cpp-parameters.parsing.json`
y `mixed/mixed-constructs.parsing.json`. `CPP-TYPE-03` se cierra en el mismo
trabajo porque dependía de la misma ampliación de esquema.

`C-PP-01`, `C-DECL-01`, `C-TYPE-04`, `C-FUNC-03`, `C-FUNC-04` y `C-FUNC-05`
siguen sin aserción exacta: cubren typedefs, variables globales, storage
class, la función `static` y la elipsis variádica, ninguno de los cuales
tiene relación con parámetros, comentarios o sentencias. Cerrarlos no aporta
nada a las métricas de la Fase 6b, así que quedan deliberadamente abiertos.

## Métricas (Fase 6b)

Los directorios `complexity/`, `nesting/` y `parameters/` tienen además un
archivo hermano `<nombre>.metrics.json`, opcional por fixture (solo estos
tres lo declaran). Cada uno describe, por función, las métricas de tamaño,
firma y complejidad definidas en
`docs/superpowers/specs/2026-08-08-fase-6b-motor-metricas-design.md`:

```json
{
  "functions": [
    {
      "name": "classify",
      "size": {"physicalLines": 13, "codeLines": 12, "blankLines": 1, "commentLines": 0, "statementCount": 5},
      "signature": {"parameterCount": 2, "signatureLength": 22, "booleanParameterCount": 1},
      "complexity": {"cyclomaticComplexity": 5, "maxConditionComplexity": 1, "maxNestingDepth": 2, "branchCount": 2, "returnCount": 3}
    }
  ]
}
```

`functions` es una secuencia exhaustiva y ordenada, igual que en
`*.parsing.json` — un `name` por entrada, sin duplicar `kind` ni rangos (ya
cubiertos por el `.parsing.json` hermano). Los prototipos no tienen entrada:
`MeasureProject` los excluye porque no tienen cuerpo. El runner
(`tests/unit/CorpusMetricsTest.cpp`) compara con igualdad exacta.

## Integridad del corpus

`CheckCorpusIntegrity.cmake`, ejecutado por CTest, verifica que los pares de
fuente y expectativa existan en ambos sentidos, que cada expectativa declare
`functions`, `classes`, `includes` y `diagnostics`, y que los IDs citados sean
IDs documentados no `OUT-*` con cobertura completa. El runner C++ ejecuta el
frontend y valida los valores estructurales del esquema.
