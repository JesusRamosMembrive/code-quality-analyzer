# Proyectos reales de validación

Proyectos públicos usados para validación exploratoria manual del analizador.
Su código **no se copia** a este repositorio: se listan aquí con su procedencia
exacta para que cualquiera pueda obtener la misma versión y reproducir los
resultados.

No forman parte de la suite automatizada. Los casos diseñados a propósito viven
en el corpus (`tests/corpus/`); estos proyectos aportan lo que ningún fixture
puede aportar: código que nadie escribió pensando en este analizador.

El proceso que revisa hallazgos del analizador contra estos proyectos y
produce el informe de precisión por regla está en
`tests/corpus/VALIDATION-METHODOLOGY.md`.

## SQLite (amalgamación)

| Campo | Valor |
|---|---|
| Lenguaje | C |
| Licencia | Dominio público |
| Origen | https://www.sqlite.org/download.html (`sqlite-amalgamation-*.zip`) |
| Versión fijada | 3.46.0 |
| Archivo de interés | `sqlite3.c` |

**Por qué:** la amalgamación es un único archivo de aproximadamente 250 000
líneas. Es la prueba de estrés del descubrimiento y del parsing, con código C
real de altísima calidad y una licencia sin fricción alguna.

**Qué debe ejercitar:** volumen extremo en un solo archivo, funciones `static`
en cantidad, macros densas, y el comportamiento del analizador cuando un archivo
es mucho mayor que cualquier fixture.

## fmt

| Campo | Valor |
|---|---|
| Lenguaje | C++ |
| Licencia | MIT |
| Origen | https://github.com/fmtlib/fmt |
| Versión fijada | 10.2.1 |
| Directorios de interés | `include/fmt/`, `src/` |

**Por qué:** C++ moderno cargado de plantillas, lambdas y sobrecarga de
operadores — exactamente las tres construcciones de la zona gris que el MVP
declara soportadas (`CPP-FUNC-06`, `CPP-FUNC-07`, `CPP-FUNC-08`, `CPP-TYPE-04`).

**Qué debe ejercitar:** plantillas anidadas, `constexpr` intensivo, cabeceras
grandes, y la distinción entre declaración e instanciación (que el MVP
explícitamente no hace, `OUT-01`).

## Criterios de éxito sobre estos proyectos

- El análisis completa sin caídas ni cuelgues sobre ambos.
- La tasa de regiones `ERROR` se **mide y se documenta** la primera vez en la
  Fase 4, y ese valor se trata como línea base a no empeorar.

No se fija hoy un umbral numérico: no existe ningún dato que lo respalde, e
inventarlo sería precisamente el tipo de certeza falsa que este proyecto rechaza
por principio.
