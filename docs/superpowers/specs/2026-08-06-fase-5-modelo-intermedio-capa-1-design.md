# Fase 5, capa 1 — modelo intermedio común: diseño

Fecha: 2026-08-06
Estado: aprobado para revisión de especificación

## Objetivo

Convertir el resultado estructural de C y C++ en un modelo estable, tipado e
independiente de tree-sitter. La Fase 6 consumirá exclusivamente este modelo
para calcular métricas comunes.

## Alcance de esta capa

Se crea `codeqa::model` con `ProjectModel`, `FileModel` y agregados tipados
para funciones, métodos, constructores, destructores, operadores, lambdas,
prototipos, tipos, namespaces, parámetros, dependencias, variables globales y
alias `typedef`. Todo símbolo conserva nombre, nombre cualificado cuando existe
y `core::SourceRange`; el modelo reutiliza `core::Diagnostic` para no duplicar
el contrato de diagnósticos.

`FileModel` contiene el lenguaje, estado del análisis, diagnósticos y
colecciones ordenadas de dependencias, callables, tipos, namespaces, variables
globales y alias. Los detalles exclusivos de C++ son opcionales y tipados; por
ejemplo bases sintácticas, plantilla y enum scoped. No hay una colección de
nodos genéricos ni tipos de tree-sitter en la API pública del modelo.

El estado del archivo distingue análisis completo, parcial y no disponible. Un
archivo con errores conserva diagnósticos y cualquier estructura recuperada; un
fallo de lectura o de parser produce un archivo no disponible sin inventar
símbolos.

## Arquitectura y flujo

`frontend::ParsedFile` sigue siendo el resultado privado del parser. Un
`ProjectModelBuilder` en `codeqa::model` recibe los `ParsedFile` ya obtenidos y
construye un `ProjectModel`; no lee el CST, tokens ni tipos de tree-sitter. La
adaptación es unidireccional y determinista: conserva el orden fuente de cada
colección y no resuelve referencias, herencia ni alias.

La primera capa ampliará el frontend solo para extraer variables de ámbito de
archivo y alias `typedef`, pendientes de la Fase 4b. Cada adaptación debe ser
explícita: una función estructural se convierte en un callable, un tipo o
namespace en su agregado correspondiente, y un include en dependencia. El CLI
actual no cambia su salida ni pasa a exponer el modelo.

## Límites

Esta capa no modela variables locales, cuerpos, control de flujo, llamadas,
resolución de símbolos o tipos, métricas, reglas ni informes. Esos elementos
forman la segunda capa de la Fase 5 o fases posteriores. Tampoco asigna IDs
persistentes entre ejecuciones: la identidad disponible es contextual al
archivo, nombre y rango fuente.

## Errores y determinismo

El builder nunca descarta un archivo por tener diagnósticos y nunca crea
estructura a partir de una región de error. Conserva los diagnósticos en el
orden de entrada. Las colecciones se ordenan por offset inicial y final de su
rango, con desempate estable, para que C y C++ produzcan el mismo resultado
ante la misma entrada.

## Pruebas y aceptación

Las pruebas unitarias verificarán los valores por defecto y contratos de cada
agregado; las de adaptación cubrirán C, C++, detalles C++ y globales/typedefs.
Los fixtures malformados comprobarán que el `FileModel` parcial conserva tanto
diagnósticos como símbolos recuperados. Se verificará que los headers públicos
de `model` no incluyen tree-sitter y que el modelo preserva el orden fuente.

La capa queda aceptada cuando el frontend produce el modelo común para todos
los archivos analizados, las métricas futuras puedan depender solo de
`codeqa::model`, C y C++ compartan los mismos conceptos comunes y no exista una
representación basada en nodos genéricos.
