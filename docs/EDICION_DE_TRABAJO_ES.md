# La edición de trabajo: forma, cuerpos, paletas y escucha

**Estado revisado: 22 de septiembre de 2026.** Este documento describe funciones
presentes en la copia local de desarrollo, que es posterior al código público
de partida. Publicar esta explicación no publica automáticamente esas funciones
ni actualiza un ZIP anterior. El funcionamiento compartido con la base pública
se explica en [Cómo funciona Partitura del Juego](COMO_FUNCIONA_ES.md).

## Una forma que organiza tiempos largos

La edición de trabajo añade una escala por encima de los capítulos de cada
pantalla: la **sección**. Una sección determina un vocabulario de materiales,
un recorrido de tempo, rangos de intensidad y densidad, duraciones y eventos
permitidos. Así el sistema puede cambiar de carácter durante varios minutos.

| Sección | Función compositiva |
|---|---|
| Calibración | Presenta un estado escaso, lento y contenido. |
| Codificación | Da protagonismo a estructuras discretas, matrices y datos. |
| Acumulación | Construye actividad mediante incremento de capas y densidad. |
| Expansión dimensional | Abre relaciones espaciales y capas de material. |
| Saturación | Concentra actividad y presión dentro de límites programados. |
| Ruptura | Interrumpe el desarrollo con una puntuación de conjunto. |
| Recursión | Reorganiza el recorrido y prepara nuevos comienzos. |

Los nombres describen funciones musicales. «Expansión dimensional» no añade
necesariamente dimensiones físicas a los datos, y «saturación» no ordena
sobrecargar la salida de audio.

No se recorren siempre en el orden de la tabla. Una matriz de transición asigna
preferencias a la siguiente sección; otras reglas evitan repetición inmediata,
favorecen contraste y recuperan secciones que llevan tiempo sin aparecer.
Ruptura tiene restricciones de entrada y salida. Dentro de la sección, varios
parámetros se interpolan entre un valor inicial y uno final.

En términos sencillos, la selección funciona así:

```text
leer la sección actual y su historia
obtener los pesos de sus posibles continuaciones
aplicar restricciones de contraste, repetición y recorrido
sortear una continuación entre las opciones válidas
preparar la separación y comenzar la nueva sección
```

El pseudocódigo resume la intención del algoritmo; los detalles están en
`chooseNextSection()` de `src/VisualComposer.cpp` de esta edición.

### La cesura: hacer perceptible el cambio

La transición de sección tiene tres fases: **colapso → vacío → emergencia**.
El conjunto converge, se retira durante un intervalo y presenta el nuevo
material. La cesura permite percibir una separación formal. El estado de clear
y su amplitud también llegan al audio para reducir la presencia sonora.

Las siete secciones no deben confundirse con los nueve estados del análisis
colectivo. La sección fija un marco de larga duración; el estado colectivo
describe y modula lo que ocurre dentro de él.

## Una bolsa de técnicas en lugar de una lista fija

`GraphicScore` recibe los modos permitidos por la sección o por un momento
especial de vídeo. Los baraja mediante Fisher–Yates: recorre la lista desde
el final e intercambia cada elemento con uno elegido entre las posiciones
restantes. Intercala `BwClean` después de cada técnica y rellena la bolsa al
agotarla. Los modos de una bolsa aparecen antes de volver a sortear la siguiente.

Esto introduce variación sin permitir cualquier técnica en cualquier momento.
El cambio de paleta reinicia la selección de modos para que la nueva sección
se haga visible. `SlitScan` sigue siendo manual; `Flash` se gestiona como una
superposición de evento.

También existen momentos de conjunto dedicados a nube de puntos y a vídeo
gráfico. Un **momento** organiza una situación interna; una **sección** gobierna
la forma más larga; un **capítulo** es el episodio de un canal.

## Personas, superficie de juego y límites de la visión

Esta edición incorpora YOLOX-S para detectar personas. La detección se filtra
según confianza, superficie de juego y apoyo de la zona de los pies. Los clips
cuyo nombre contiene `hinchada` quedan fuera del seguimiento de jugadores.
Si faltan el modelo o la evidencia de superficie, el sistema evita sustituir
personas por simples manchas de movimiento.

Las detecciones se relacionan temporalmente y se suavizan para estabilizar sus
trayectorias. Una caja sigue siendo una localización aproximada: puede incluir
fondo, fragmentarse, perderse por oclusión o cambiar de identidad. Una persona
aceptada dentro del campo también puede ser un árbitro.

| Modo | Qué hace visible |
|---|---|
| `SurfaceScan` | Contornos de movimiento dentro de personas aceptadas. |
| `PlayerIDs` | Cajas y etiquetas temporales de seguimiento. |
| `MotionTrails` | Historia reciente de posiciones seguidas. |
| `FieldMap` | Organización espacial de las detecciones; la proyección al campo depende de geometría válida. |

Los contornos no constituyen segmentación anatómica y las etiquetas no son
identidades deportivas. El candidato a balón requiere filtros geométricos,
velocidad y confirmación temporal. Su presencia sigue siendo una estimación,
no una garantía de seguimiento correcto.

## Analizar antes para reproducir con menos carga

El análisis previo guarda resultados al lado de cada vídeo, por ejemplo
`partido.mp4.pdjcv/`. Al reproducirlo, la aplicación comprueba si hay datos
compatibles, consulta su tiempo e interpola posiciones de etiquetas coincidentes.
Con una caché válida evita repetir la detección neuronal y el flujo óptico.
La decodificación del vídeo y el dibujo de efectos siguen siendo necesarios.

Las marcas de tiempo guardadas son la referencia: no debe suponerse que todas
las muestras tienen exactamente el mismo intervalo. Si se modifica el vídeo,
la caché puede quedar invalidada y debe regenerarse. Si no hay resultados
compatibles o falla su lectura, existe una ruta de análisis en vivo.

Hay dos formatos con propósitos diferentes:

| Formato | Uso |
|---|---|
| `.pdjcv` | Caché del análisis de clips para esta aplicación de vídeo. |
| PDJV | Paquetes de datos para reconstrucción y usos volumétricos; tienen especificación y herramientas propias. |

No son intercambiables. Tampoco hay que ejecutar el runtime independiente
`volumetric/` para usar la caché `.pdjcv`.

## El color participa en la composición

El director de paletas utiliza cinco familias: Monochrome, Ember, Glacier,
Verdant e Iris. Cada una contiene una rampa de seis colores. La intensidad de
la imagen selecciona posiciones de esa rampa; exposición y mutación modifican
su uso dentro de límites.

La elección automática depende de la sección y de la historia reciente de
paletas. Los cambios pueden aprovechar el vacío de una cesura; los cambios
manuales fuera de ella se funden desde los colores realmente visibles. Los
pesos de las familias también intervienen en el tratamiento sonoro, de modo que
un cambio de color puede acompañar una transformación del timbre.

Una paleta no sustituye a la forma ni garantiza una emoción concreta. Es otro
conjunto de restricciones y relaciones sobre los materiales existentes.

## Leer la franja de actividad sonora

El prototipo de visor muestra tres filas: **sonido**, **pulsos** y **textura**.
La derecha marca **AHORA**; hacia la izquierda quedan seis segundos de historia.
Los pulsos se dibujan con puntos o marcas cortas, los sonidos continuos con
trazos y la textura con trazos más gruesos. Un indicador resume actividad
reciente y puede aparecer el nombre de un instrumento recibido.

El audio devuelve medidas de tres capas instrumentadas: voz principal, motor
de pulsos y cama de ruido. La aplicación valida canal, sesión y secuencia,
descarta mensajes antiguos y deja de señalar actividad actual cuando los datos
pierden vigencia. El dibujo consulta ese estado; no genera sonido.

**El alcance sigue siendo parcial.** Las tres filas no explican necesariamente
todas las voces, acentos ni efectos de la mezcla. Una medida de señal digital
tampoco confirma que el altavoz esté encendido o que ese sonido sea perceptible
en sala. La latencia física entre imagen y sonido requiere calibración; el
prototipo usa el instante local de recepción de los datos.

Para comprobar su lectura, conviene aislar una fuente, observar su comienzo,
su duración y su final, y después escucharla en el conjunto. Una pantalla
representa una contribución sonora, no la salida exclusiva de un altavoz.

## Qué revisar para relacionar explicación y versión

En la copia que se vaya a ejecutar, comprobar `src/VisualComposer.cpp`,
`src/GraphicScore.cpp`, `src/CVPipeline.cpp`, `src/ClipAnalysisCache.cpp`,
`src/CompositionPalette.h`, `src/AudioScoreState.h`, `src/AudioScoreOverlay.h`
y el motor `supercollider/pdj_datamatics.scd`. La ausencia de esos módulos o de
sus funciones indica que esta ampliación no corresponde íntegramente a esa
versión. La validación de una sesión o un prototipo no certifica por sí sola
todo el sistema ni un paquete diferente.
