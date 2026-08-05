# Dependencias vendorizadas

Todas las dependencias externas se copian como fuente dentro de este directorio.
No se usa `FetchContent` ni submodules: la compilación debe ser reproducible sin
acceso a red.

Ningún archivo de este directorio se edita a mano. Para actualizar una
dependencia se sustituyen sus archivos por los de la nueva versión y se
actualiza su entrada aquí.

## doctest

| Campo | Valor |
|---|---|
| Versión | 2.4.11 |
| Origen | https://github.com/doctest/doctest |
| Licencia | MIT |
| Archivos | `doctest/doctest.h` |
| Para qué | Framework de tests unitarios |

## tree-sitter (runtime)

| Campo | Valor |
|---|---|
| Versión | v0.26.11 |
| Origen | https://github.com/tree-sitter/tree-sitter |
| Licencia | MIT |
| Archivos | `tree-sitter/lib/src/`, `tree-sitter/lib/include/` |
| Para qué | Motor de parseo incremental y tolerante a errores |

## tree-sitter-c

| Campo | Valor |
|---|---|
| Versión | v0.24.2 |
| Origen | https://github.com/tree-sitter/tree-sitter-c |
| Licencia | MIT |
| Archivos | `tree-sitter-c/src/parser.c`, `tree-sitter-c/src/tree_sitter/parser.h` |
| LANGUAGE_VERSION | 15 |
| Para qué | Gramática de C |

## tree-sitter-cpp

| Campo | Valor |
|---|---|
| Versión | v0.23.4 |
| Origen | https://github.com/tree-sitter/tree-sitter-cpp |
| Licencia | MIT |
| Archivos | `tree-sitter-cpp/src/parser.c`, `tree-sitter-cpp/src/scanner.c`, `tree-sitter-cpp/src/tree_sitter/parser.h`, `tree-sitter-cpp/src/tree_sitter/alloc.h` |
| LANGUAGE_VERSION | 14 |
| Para qué | Gramática de C++ |

El runtime vendorizado admite `LANGUAGE_VERSION` en el rango
`[13, 15]`; ambas gramáticas están dentro.

Nota: `scanner.c` de tree-sitter-cpp incluye `tree_sitter/alloc.h`, que no
formaba parte de la lista original de archivos a copiar; se añadió porque es
un requisito de compilación de ese `scanner.c` en esta versión.
