# Corpus de validacion

El corpus contiene pares de fuentes C/C++ y expectativas declarativas para el
frontend. No es el conjunto de `tests/integration/fixtures/`: las expectativas
de este directorio prueban la extraccion estructural de cada archivo fuente.

## Estructura

| Directorio | Contenido |
|---|---|
| `parsing/` | Casos minimos de parsing |
| `functions/` | Funciones, metodos, operadores, lambdas y plantillas |
| `classes/` | Clases, structs, enums, namespaces y herencia sintactica |
| `parameters/` | Valores por defecto, referencias y punteros |
| `complexity/`, `nesting/` | Construcciones que la Fase 6 medira |
| `malformed/` | Fuentes rotas con diagnosticos estables |
| `mixed/` | Combinaciones de varias categorias |

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
          "hasDefaultValue": false
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

`constructs` contiene IDs no vacios de `SUPPORTED-CONSTRUCTS.md`.

Cada funcion usa `kind` con uno de `free`, `method`, `constructor`,
`destructor`, `operator`, `lambda` o `prototype`. Cada parametro conserva su
posicion, incluidos los sin nombre mediante `"name": ""`; `passing` es
`value`, `pointer`, `reference` o `function-pointer`. La elipsis de una
funcion variadica no es un parametro y se representa con `isVariadic`.

Cada entrada de `classes` usa `kind` (`class`, `struct`, `union`, `enum` o
`namespace`), `isTemplate`, `isScopedEnum` y `bases`. Cada entrada de
`includes` usa `delimiter` `local` o `system`. Los diagnosticos comparan
`line` y `severity`; pueden declarar `column` cuando sea relevante, pero nunca
el texto del mensaje.

## Reglas de comparacion

- Las lineas son 1-based. `startLine` incluye el tipo de retorno o la linea de
  `template`, y `endLine` incluye el cierre; un prototipo tiene el mismo inicio
  y fin.
- En fixtures bien formados, `functions`, `classes` e `includes` son secuencias
  exhaustivas y ordenadas por fuente.
- En `malformed/`, funciones y clases son una subsecuencia minima ordenada: la
  recuperacion de tree-sitter puede aportar entradas adicionales. Los includes y
  diagnosticos se comparan de forma exacta en todos los fixtures.
- Antes de comparar estructura, el runner exige que el archivo se haya parseado.
  Un error de esquema, JSON ilegible o una fuente sin expectativa hermana falla
  la prueba con la ruta del fixture.

## Limites de Fase 5

Los modelos de esta fase no representan variables globales ni aliases `typedef`.
Por ello `C-DECL-01` y `C-TYPE-04` siguen siendo capacidades estructurales
pendientes de Fase 5: esa sera la primera fase que pueda demostrar su modelo y
su analisis propios.

`classes/c-types.c` contiene `typedef struct Point PointAlias;`. La gramatica
actual tambien entrega el `struct Point` de la linea 24 como una captura
sintactica, y la expectativa exhaustiva la conserva para fijar la salida real
del extractor. Esa entrada no representa `PointAlias`, no demuestra que se
reconozca un typedef y no cambia el limite de `C-TYPE-04`.

## Integridad del corpus

`CheckCorpusIntegrity.cmake`, ejecutado por CTest, verifica que los pares de
fuente y expectativa existan en ambos sentidos, que cada expectativa declare
`functions`, `classes`, `includes` y `diagnostics`, y que los IDs citados sean
IDs documentados no `OUT-*` con cobertura completa. El runner C++ ejecuta el
frontend y valida los valores estructurales del esquema.
