# Code Quality Analyzer — Idea general del proyecto

## 1. Propósito

Este proyecto tiene como objetivo desarrollar una herramienta de análisis estático de código fuente orientada a medir y mejorar la calidad, mantenibilidad y evolución de proyectos software.

La herramienta deberá detectar problemas que normalmente no impiden compilar o ejecutar el programa, pero que pueden degradar progresivamente su diseño y dificultar su ampliación, revisión y mantenimiento.

Entre estos problemas se encuentran:

- funciones excesivamente largas;
- complejidad ciclomática elevada;
- profundidad de anidamiento excesiva;
- nombres poco descriptivos;
- duplicación de código;
- reutilización insuficiente de funciones ya existentes;
- dependencias arquitectónicas incorrectas;
- clases o módulos con demasiadas responsabilidades;
- inconsistencias en el tratamiento de errores;
- patrones peligrosos o desaconsejados específicos de un lenguaje;
- degradaciones introducidas por cambios recientes.

La herramienta estará especialmente orientada a revisar código producido o modificado mediante agentes de programación, aunque su funcionamiento no dependerá de inteligencia artificial.

---

## 2. Problema que se quiere resolver

Los agentes de programación pueden generar código funcional con rapidez, pero no siempre respetan completamente la arquitectura, las convenciones o las decisiones ya existentes en un proyecto.

Algunos ejemplos habituales son:

- implementar una utilidad que ya existe en otro archivo;
- introducir una función con demasiadas ramas;
- usar nombres genéricos o variables de una letra;
- aumentar el acoplamiento entre módulos;
- añadir una abstracción innecesaria;
- duplicar bloques de código;
- mezclar responsabilidades dentro de una misma función;
- resolver un problema local sin considerar el diseño global;
- introducir un estilo de gestión de errores distinto al utilizado en el resto del proyecto.

Estos defectos pueden ser menores de forma aislada, pero acumulados producen deuda técnica y reducen la capacidad de mantener y ampliar el sistema.

La herramienta debe proporcionar una revisión determinista y repetible después de cada implementación.

---

## 3. Visión del producto

La visión del proyecto es construir una herramienta que pueda:

1. analizar un proyecto de código fuente;
2. reconocer su estructura básica;
3. calcular métricas objetivas;
4. ejecutar reglas configurables;
5. generar hallazgos concretos y localizables;
6. comparar los resultados con análisis anteriores;
7. distinguir problemas nuevos, existentes y resueltos;
8. mostrar tendencias de calidad;
9. admitir distintos lenguajes mediante frontends especializados;
10. funcionar sin depender de servicios externos ni modelos de inteligencia artificial.

La herramienta deberá poder utilizarse inicialmente desde línea de comandos y, en fases posteriores, desde una interfaz gráfica desarrollada con Qt/QML.

---

## 4. Principios del proyecto

### 4.1. Determinismo

El mismo código y la misma configuración deben producir siempre el mismo resultado.

La herramienta no debe depender de respuestas probabilísticas para calcular métricas o decidir si una regla se incumple.

### 4.2. Explicabilidad

Cada hallazgo debe indicar:

- qué se ha detectado;
- dónde se ha detectado;
- qué métrica o patrón lo provoca;
- cuál es el valor calculado;
- cuál es el umbral configurado;
- por qué puede ser un problema;
- qué tipo de corrección se podría considerar.

No se aceptarán diagnósticos opacos como:

```text
Code quality: 63/100
```

sin información adicional.

### 4.3. Bajo número de falsos positivos

La utilidad de la herramienta dependerá de que los avisos sean relevantes.

Se priorizará la precisión frente a la cantidad de reglas. Es preferible disponer de pocas reglas fiables antes que producir numerosos avisos irrelevantes.

### 4.4. Métricas separadas

No se utilizará una única puntuación global como fuente principal de decisión.

La calidad se analizará por dimensiones independientes, por ejemplo:

- complejidad;
- tamaño;
- duplicación;
- legibilidad;
- acoplamiento;
- arquitectura;
- riesgos;
- mantenibilidad.

Una puntuación agregada podrá añadirse como resumen, pero nunca sustituirá a las métricas individuales ni a los hallazgos.

### 4.5. Evolución histórica

La herramienta no solo debe indicar el estado actual del proyecto. También debe mostrar cómo cambia.

Debe poder responder preguntas como:

- ¿ha aumentado la complejidad de una función?;
- ¿se ha introducido nueva duplicación?;
- ¿se ha resuelto algún hallazgo anterior?;
- ¿qué problemas pertenecen al código legado?;
- ¿qué problemas han sido introducidos por el último cambio?;
- ¿qué módulos están empeorando con el tiempo?

### 4.6. Independencia de la interfaz

El motor de análisis no dependerá de Qt, QML ni de ninguna interfaz gráfica.

La línea de comandos y la futura aplicación Qt/QML serán adaptadores que consumirán el mismo backend.

### 4.7. Extensibilidad por lenguaje

La arquitectura deberá permitir incorporar nuevos lenguajes sin reescribir:

- el sistema de informes;
- el histórico;
- la comparación de análisis;
- el modelo de hallazgos;
- la configuración;
- el sistema de severidades;
- la persistencia;
- la interfaz gráfica;
- las métricas realmente independientes del lenguaje.

---

## 5. Alcance inicial

La primera versión estará orientada a:

- C puro;
- C++11;
- C++14;
- C++17.

La propia herramienta se desarrollará preferentemente en C++17.

El soporte inicial se centrará en un subconjunto estructural suficientemente útil para calcular métricas básicas. No se intentará implementar desde el principio un compilador completo de C o C++.

El analizador inicial deberá ser tolerante:

- reconocerá las construcciones soportadas;
- registrará diagnósticos cuando encuentre construcciones que no comprenda;
- evitará bloquearse ante código incompleto o complejo;
- mantendrá la sincronización del análisis siempre que sea posible.

---

## 6. Ampliación futura a otros lenguajes

Se contempla añadir soporte para Python después de estabilizar el análisis de C y C++.

También podrán considerarse otros lenguajes en el futuro, como:

- Java;
- C#;
- JavaScript;
- TypeScript;
- Rust.

Añadir un lenguaje no será una operación trivial, porque cada lenguaje necesita comprender su propia sintaxis y semántica. Sin embargo, el trabajo deberá quedar localizado en componentes específicos.

Para incorporar un nuevo lenguaje será necesario implementar principalmente:

- detección del lenguaje;
- tokenización o análisis léxico;
- análisis sintáctico o estructural;
- adaptación al modelo intermedio común;
- reglas específicas;
- corpus de pruebas;
- validación de resultados.

El resto del sistema deberá reutilizarse.

---

## 7. Arquitectura general

La arquitectura se dividirá en capas.

```text
┌─────────────────────────────────────────────────────────────┐
│                         Aplicaciones                        │
│                                                             │
│                CLI                    Qt/QML                 │
└─────────────────────────────┬───────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────┐
│                     Casos de uso                            │
│                                                             │
│  Analizar proyecto                                          │
│  Comparar análisis                                          │
│  Generar informe                                            │
│  Crear baseline                                             │
└─────────────────────────────┬───────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────┐
│                  Frontends de lenguaje                      │
│                                                             │
│  CFrontend       CppFrontend       PythonFrontend futuro     │
└─────────────────────────────┬───────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────┐
│                 Modelo intermedio común                     │
│                                                             │
│  Proyecto, archivo, función, clase, parámetro, variable,     │
│  scope, control de flujo, llamada, dependencia y ubicación  │
└─────────────────────────────┬───────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────┐
│                    Motor de análisis                        │
│                                                             │
│  Métricas comunes                                           │
│  Reglas comunes                                             │
│  Reglas específicas por lenguaje                            │
│  Hallazgos                                                  │
│  Baseline e histórico                                       │
└─────────────────────────────┬───────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────┐
│                      Infraestructura                        │
│                                                             │
│  Sistema de archivos                                        │
│  Configuración                                              │
│  Persistencia                                               │
│  Informes                                                   │
└─────────────────────────────────────────────────────────────┘
```

---

## 8. Frontends específicos por lenguaje

No se desarrollará un parser universal.

Cada lenguaje tendrá un frontend encargado de interpretar su código y convertirlo al modelo intermedio común.

### 8.1. Frontend de C

Será responsable de reconocer, entre otros elementos:

- funciones;
- estructuras;
- enums;
- declaraciones;
- variables;
- bloques;
- control de flujo;
- llamadas;
- directivas del preprocesador;
- includes.

### 8.2. Frontend de C++

Compartirá determinadas utilidades con C, pero deberá reconocer además:

- clases;
- namespaces;
- métodos;
- constructores;
- destructores;
- referencias;
- templates;
- sobrecarga;
- operadores;
- lambdas;
- excepciones;
- qualifiers;
- elementos propios de C++11, C++14 y C++17.

Los frontends de C y C++ se apoyan en tree-sitter (gramáticas `tree-sitter-c` y `tree-sitter-cpp`, vendorizadas en el repositorio) en vez de un lexer/parser propio: cada lenguaje tiene su propia gramática tolerante a errores, y ambos frontends comparten únicamente el adaptador que traduce el árbol concreto (CST) de tree-sitter al modelo intermedio común, no el parsing en sí.

### 8.3. Frontend futuro de Python

Necesitará componentes diferentes debido a características como:

- bloques definidos por indentación;
- saltos de línea significativos;
- decorators;
- funciones anidadas;
- comprehensions;
- generators;
- `async` y `await`;
- tipado dinámico;
- imports;
- asignaciones múltiples.

Python no reutilizará el parser de C++, pero producirá el mismo tipo de modelo intermedio cuando represente conceptos comunes.

---

## 9. Modelo intermedio común

El modelo intermedio será la frontera entre los frontends y el motor de análisis.

Los frontends no deberán generar directamente avisos de calidad. Su responsabilidad será describir la estructura encontrada.

Conceptos comunes previstos:

- proyecto;
- archivo;
- módulo;
- símbolo;
- función;
- método;
- clase;
- estructura;
- parámetro;
- variable;
- scope;
- llamada a función;
- dependencia;
- sentencia de control;
- rango de código fuente;
- diagnóstico de parsing.

Ejemplo conceptual:

```text
FunctionModel
├── nombre
├── nombre cualificado
├── lenguaje
├── ubicación
├── parámetros
├── variables locales
├── llamadas
├── nodos de control de flujo
├── métricas estructurales
└── detalles específicos del lenguaje
```

El modelo común no debe limitarse al mínimo denominador entre lenguajes. También admitirá extensiones específicas.

Ejemplos:

```text
CppFunctionDetails
├── const
├── noexcept
├── virtual
├── override
└── template

PythonFunctionDetails
├── async
├── generator
└── decorators
```

---

## 10. Métricas y reglas

Se distinguirán tres conceptos.

### 10.1. Métrica

Valor objetivo calculado sobre el código.

Ejemplos:

```text
Complejidad ciclomática = 14
Número de parámetros = 7
Profundidad máxima = 5
Líneas de código = 83
```

### 10.2. Regla

Condición configurable aplicada a una métrica o a una estructura.

Ejemplo:

```text
Generar un hallazgo cuando la complejidad ciclomática supere 10.
```

### 10.3. Hallazgo

Instancia concreta de una regla incumplida.

Ejemplo:

```text
ProtocolRouter::detectProtocol tiene una complejidad ciclomática de 14.
El límite configurado es 10.
```

---

## 11. Reglas comunes y específicas

### 11.1. Reglas potencialmente comunes

- función demasiado larga;
- demasiados parámetros;
- complejidad ciclomática elevada;
- anidamiento excesivo;
- demasiados retornos;
- demasiadas variables locales;
- nombre de variable demasiado corto;
- código duplicado;
- dependencia arquitectónica prohibida;
- aumento de complejidad respecto al histórico.

### 11.2. Reglas específicas de C

- uso de funciones inseguras;
- gestión incorrecta de memoria;
- problemas con arrays y tamaños;
- macros peligrosas;
- falta de validación de punteros;
- declaraciones antiguas;
- patrones incorrectos con `sizeof`.

### 11.3. Reglas específicas de C++

- casts de estilo C;
- gestión incorrecta de ownership;
- problemas con `new` y `delete`;
- destructor base no virtual;
- incumplimiento de Rule of Three, Five o Zero;
- uso incorrecto de `std::move`;
- slicing;
- `using namespace` en headers;
- ausencia de `override`;
- abuso de `shared_ptr`;
- constructores convertidores no explícitos.

### 11.4. Reglas futuras de Python

- argumentos mutables por defecto;
- `except` demasiado amplio;
- imports con `*`;
- ocultación de built-ins;
- uso de `eval` o `exec`;
- bloques `try` demasiado amplios;
- complejidad excesiva en comprehensions;
- imports circulares.

---

## 12. Flujo general de análisis

```text
Seleccionar proyecto
        │
        ▼
Descubrir archivos
        │
        ▼
Detectar lenguaje
        │
        ▼
Seleccionar frontend
        │
        ▼
Tokenizar y analizar
        │
        ▼
Construir modelo intermedio
        │
        ▼
Calcular métricas
        │
        ▼
Evaluar reglas
        │
        ▼
Aplicar exclusiones y supresiones
        │
        ▼
Comparar con baseline o histórico
        │
        ▼
Generar hallazgos e informes
```

---

## 13. Histórico y baseline

Una versión mínima de esta capacidad (snapshot como JSON versionado, comparación solo contra el snapshot inmediatamente anterior, clasificación en nuevo/existente/resuelto) ya forma parte del primer MVP — ver Fase 10 en `01-cpp-mvp-roadmap.md`. Lo que describe el resto de esta sección (comparación entre snapshots arbitrarios, categorías "modificado"/"no comparable", tendencias de métricas a lo largo de múltiples snapshots) es la versión completa prevista para después del MVP.

La herramienta almacenará snapshots de análisis.

Un snapshot deberá incluir al menos:

- versión del esquema;
- identificador del proyecto;
- fecha;
- revisión opcional;
- archivos analizados;
- símbolos detectados;
- métricas;
- hallazgos;
- diagnósticos del parser.

La comparación entre snapshots permitirá clasificar los hallazgos como:

- nuevos;
- existentes;
- resueltos;
- modificados;
- no comparables.

También permitirá medir variaciones como:

```text
Complejidad: 8 → 14
Líneas de función: 42 → 71
Duplicación: 3,1 % → 4,6 %
Dependencias: 5 → 8
```

La baseline evitará que los problemas históricos oculten los defectos introducidos por cambios recientes.

---

## 14. Salidas previstas

La primera aplicación será una herramienta de línea de comandos.

Ejemplo conceptual:

```bash
codeqa analyze ./project --config codeqa.json
```

Salidas previstas:

- informe legible en texto;
- informe estructurado en JSON;
- diagnósticos de parsing;
- métricas por archivo y función;
- lista de hallazgos;
- resultado del quality gate;
- comparación con baseline.

En una fase posterior, la interfaz Qt/QML consumirá la misma información.

---

## 15. Validación de la herramienta

Durante el desarrollo, las conclusiones del analizador se contrastarán mediante:

1. fixtures con resultados conocidos;
2. pruebas unitarias;
3. pruebas de integración;
4. proyectos reales;
5. revisión humana;
6. revisión temporal mediante agentes.

Los agentes se utilizarán únicamente como apoyo durante la validación. No serán la fuente de verdad ni una dependencia del producto final.

La validación principal será determinista.

Para cada regla se podrán registrar:

- verdaderos positivos;
- falsos positivos;
- falsos negativos;
- precisión;
- recall;
- ejemplos aceptados;
- ejemplos rechazados;
- motivos de rechazo.

Se priorizará una precisión alta antes de habilitar una regla por defecto.

---

## 16. Elementos fuera del alcance inicial

No se abordarán en el primer MVP:

- una interfaz gráfica;
- comprensión semántica completa de C++;
- resolución completa de templates;
- análisis exhaustivo del preprocesador;
- refactoring automático;
- generación automática de código corregido;
- análisis mediante inteligencia artificial;
- soporte simultáneo de numerosos lenguajes;
- sistema dinámico de plugins;
- integración con todos los IDE;
- puntuación global como criterio principal;
- sustitución de un compilador o de herramientas como Clang.

Estos elementos podrán evaluarse en fases posteriores.

---

## 17. Criterios generales de éxito

El proyecto se considerará técnicamente bien encaminado cuando:

- el análisis sea reproducible;
- el parser no se bloquee ante código no soportado;
- los hallazgos sean localizables y explicables;
- las métricas puedan verificarse mediante tests;
- cada lenguaje esté aislado en su frontend;
- las métricas comunes no dependan de detalles sintácticos particulares;
- añadir Python no requiera reescribir el histórico ni los informes;
- el número de falsos positivos sea controlado;
- los cambios de calidad puedan compararse entre versiones;
- la herramienta sea útil sin conexión a servicios externos.

---

## 18. Organización de la documentación

Este documento define únicamente la visión general.

Los detalles de desarrollo se dividirán en documentos independientes y secuenciales.

Estructura inicial propuesta:

```text
docs/
├── 00-project-overview.md
├── 01-cpp-mvp-roadmap.md
├── 02-phase-01-project-foundation.md
├── 03-phase-02-source-discovery.md
├── 04-phase-03-language-detection.md
├── 05-phase-04-treesitter-frontend.md
├── 06-phase-05-common-analysis-model.md
├── 07-phase-06-basic-metrics.md
├── 08-phase-07-rule-engine.md
├── 09-phase-08-configuration.md
├── 10-phase-09-reporting.md
├── 11-phase-10-minimal-history.md
├── 12-phase-11-validation.md
├── 13-phase-12-mvp-integration.md
└── future/
    └── python-support.md
```

Cada fase deberá describir:

- objetivo;
- alcance;
- elementos fuera de alcance;
- requisitos previos;
- decisiones de diseño;
- pasos pequeños de implementación;
- clases o componentes afectados;
- pruebas necesarias;
- resultados esperados;
- criterios de aceptación;
- riesgos;
- entregable final.

---

## 19. Estrategia de ejecución

El proyecto se desarrollará mediante incrementos verticales pequeños.

No se solicitará a un agente que implemente de una vez un analizador completo de C++.

Cada tarea deberá ser:

- concreta;
- limitada;
- verificable;
- respaldada por tests;
- compatible con la arquitectura general;
- suficientemente pequeña para revisar todo el código generado.

Ejemplo de incremento inicial:

```text
Archivo fuente
    ↓
Lectura
    ↓
Tokenización básica
    ↓
Listado de tokens
    ↓
Pruebas unitarias
```

Solo después de validar ese flujo se añadirá el siguiente nivel.

---

## 20. Resumen de decisiones iniciales

- La herramienta se implementará preferentemente en C++17.
- Los primeros lenguajes analizados serán C y C++11–C++17.
- Python se añadirá en una fase futura.
- Cada lenguaje tendrá un frontend especializado.
- Los frontends producirán un modelo intermedio común.
- Las métricas, reglas, informes e histórico se separarán del parsing.
- La primera interfaz será CLI.
- Qt/QML se incorporará después como capa de presentación.
- No se dependerá de inteligencia artificial.
- El histórico y la comparación serán partes fundamentales del producto.
- El desarrollo se dividirá en fases y tareas pequeñas, testeables y revisables.
