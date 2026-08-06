# Fase 4b — extracción estructural y validación del corpus: diseño

Fecha: 2026-08-06
Estado: aprobado para pasar a plan de implementación

## 1. Objetivo

Completar el frontend de tree-sitter iniciado en la Fase 4a. A partir del CST ya
parseado, el frontend extraerá funciones, parámetros, tipos estructurales,
namespaces e includes en modelos tipados y verificará el resultado contra el
corpus de validación.

El resultado sigue siendo un modelo propio del frontend. La Fase 5 lo convertirá
en el modelo intermedio común de proyecto, archivo, símbolo y variable.

## 2. Alcance y límites

Esta fase incluye:

- queries tree-sitter .scm para C y C++, embebidas por CMake en el binario;
- FunctionModel, ParameterModel, ClassModel e IncludeDependency;
- integración de las colecciones estructurales en ParsedFile;
- un runner del corpus que lea *.parsing.json con nlohmann/json vendorizada;
- evolución de las expectativas JSON para comprobar atributos estructurales;
- ampliación interna de codeqa parse para ejecutar la extracción, sin cambiar su
  formato de salida.

No incluye variables globales ni typedef (C-DECL-01, C-TYPE-04): se modelarán
en la Fase 5. Tampoco incluye control de flujo, llamadas, métricas, reglas,
informes, resolución de tipos, semántica de herencia, macros ni evaluación del
preprocesador.

## 3. Arquitectura

codeqa::frontend es la única capa que conoce tree-sitter. parseSourceFile
conserva la propiedad del árbol y llama a un extractor interno antes de que este
se destruya. Ningún TSParser*, TSTree*, TSNode ni otro tipo de tree-sitter sale
de ese espacio de nombres; el accessor público al handle de SyntaxTree se
eliminará o hará inaccesible fuera de la implementación del frontend.

    archivo fuente -> TreeSitterParser -> CST + diagnósticos
                                          -> StructuralExtractor
    queries .scm embebidas --------------> modelos en ParsedFile
    ParsedFile --------------------------> runner del corpus (solo tests)

Los modelos no son nodos genéricos. Su contrato mínimo es:

- FunctionModel: nombre cualificado, FunctionKind (libre, método, constructor,
  destructor, operador, lambda o prototipo), rango, parámetros, isStatic e
  isVariadic.
- ParameterModel: nombre, modo de paso (Value, Pointer, Reference o
  FunctionPointer) e indicador de valor por defecto.
- ClassModel: ClassKind (class, struct, union, enum o namespace), nombre,
  rango, plantilla, enum con ámbito y cláusulas base sintácticas. Las cláusulas
  base son texto estructural; no se resuelven semánticamente.
- IncludeDependency: destino incluido, delimitador de sistema o local y rango.

ParsedFile contendrá vectores de estos modelos, además de sus diagnósticos,
gramática y razón de selección actuales. La Fase 5 podrá consumirlos sin leer
tokens ni consultar el CST.

## 4. Queries y extracción

Las consultas se versionarán bajo src/frontend/queries/, separadas por gramática
y dominio (functions, types, includes). CMake las transformará en datos
generados que se compilan con el proyecto, de modo que el ejecutable no dependa
de archivos .scm presentes en el directorio de trabajo o de instalación.

El extractor compila cada query para la gramática correspondiente y usa sus
capturas como candidatos. Código C++ específico del frontend interpreta esos
candidatos y normaliza los modelos. Las queries inválidas son defectos del
producto y se detectan en tests que compilan todas las queries; no son
diagnósticos del código de la persona usuaria.

Los resultados se ordenan por rango fuente y se deduplican por clase de modelo y
rango. En archivos malformados se conservan los nodos que tree-sitter haya
recuperado como estructura reconocible, pero nunca se fabrica estructura a
partir de nodos ERROR o MISSING.

## 5. Runner y expectativas del corpus

nlohmann/json se vendorizará como fuente header-only en third_party/, se
registrará con versión, origen, licencia y archivos en third_party/README.md, y
se enlazará únicamente a las pruebas. Antes de descargarla se solicitará permiso
explícito al propietario del proyecto.

El runner recorre todas las parejas fuente y *.parsing.json, ejecuta el frontend
y valida:

- en fixtures bien formados, igualdad exacta de funciones, tipos/namespaces,
  parámetros e includes, y ausencia de diagnósticos;
- en malformed/, presencia de todas las funciones y tipos esperados, aunque
  tree-sitter pueda recuperar entradas adicionales;
- en todos los casos, diagnósticos exactos por línea, columna cuando se declare
  y severidad; nunca por texto de mensaje.

El esquema JSON conservará las categorías actuales, pero expresará los
parámetros como objetos y añadirá campos para los atributos que hoy no se
pueden comprobar: static, variádica, modo de paso, valor por defecto, plantilla,
enum con ámbito, bases e includes. Con ello todos los IDs de
SUPPORTED-CONSTRUCTS.md quedan comprobados mediante salida estructural salvo
C-DECL-01 y C-TYPE-04, que se documentarán como pendientes de la Fase 5.

## 6. Errores y determinismo

Los errores de lectura siguen generando PARSE003. Los errores sintácticos siguen
siendo PARSE001/PARSE002, no fallos del comando parse; un archivo roto no impide
analizar sus vecinos ni recuperar la estructura válida del mismo archivo.

Cada ejecución debe producir los mismos modelos y diagnósticos para la misma
entrada. El orden explícito y la deduplicación eliminan cualquier dependencia
del orden interno de capturas de tree-sitter.

## 7. Criterios de aceptación

- Las queries de C y C++ se compilan y están embebidas en el binario.
- El frontend extrae los modelos descritos sin filtrar tipos de tree-sitter.
- Los rangos son 1-based y deterministas.
- El runner valida el corpus completo con igualdad exacta en fixtures sanos y
  comparación mínima de estructura en malformed/.
- Los diagnósticos de los fixtures se comparan por ubicación y severidad.
- Solo C-DECL-01 y C-TYPE-04 permanecen expresamente sin verificación
  estructural hasta la Fase 5.
- codeqa parse conserva su salida y devuelve éxito aunque existan diagnósticos.
- La compilación y todas las pruebas se ejecutan sin acceso a red tras vendorizar
  la dependencia.

## 8. Dependencias

Fase 0 aporta el corpus y sus contratos; Fase 2 el descubrimiento; Fase 3 la
detección de lenguaje; y Fase 4a el parser RAII, las gramáticas vendorizadas y
los diagnósticos. La única dependencia nueva es nlohmann/json, sujeta a permiso
explícito de descarga.
