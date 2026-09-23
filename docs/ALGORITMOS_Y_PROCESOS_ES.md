# Partitura del Juego — Algoritmos y procesos

Esta guía explica cómo el código transforma vídeo y datos en composición.
Las rutas de archivos se refieren al código incluido en `Source/current-source.tgz`
del paquete. Para una lectura sin fórmulas, consultar
[Cómo funciona la partitura](COMO_FUNCIONA_ES.md).

## 1. Selección de clips y fragmentos

`ClipPool` mantiene bibliotecas vertical y horizontal. En selección independiente
prefiere clips que no están activos en otro canal del mismo grupo ni en la
historia reciente del canal. Relaja restricciones si faltan candidatos.
`VideoDirector` elige un fragmento corto, uno largo o un vídeo completo; los
pesos y duraciones se leen de la configuración. Un acontecimiento compartido
coordina preparación, disponibilidad y tiempo de los participantes.

Una selección ponderada suma los pesos, sortea un número entre cero y esa suma
y recorre los intervalos acumulados hasta encontrar la opción. Por ejemplo,
pesos 5, 3 y 2 producen preferencias del 50 %, 30 % y 20 % en un sorteo sin
otras restricciones. No garantizan esos porcentajes de tiempo de pantalla:
las duraciones y condiciones de cada opción también intervienen.

## 2. Preparación y medidas de imagen

El análisis trabaja con imágenes reducidas y puede utilizar ajuste de contraste.
Un trabajador independiente publica resultados que el dibujo consume sin
ejecutar la detección en cada llamada de presentación. La cadencia de análisis
y la de dibujo pueden diferir. Con caché válida, las medidas se consultan por
tiempo de vídeo; siguen preparándose imágenes necesarias para los efectos.

### Energía de cambio

```text
energía = media(|gris_actual − gris_anterior|) / 255
```

Una diferencia media de 12,75 niveles de gris produce 0,05. La medida registra
cambio de imagen: puede aumentar por cuerpos, cámara, iluminación o montaje.
No representa esfuerzo físico.

### Flujo óptico

Farneback estima desplazamientos entre imágenes. El programa calcula:

```text
vector_medio = promedio del campo de desplazamientos
magnitud = longitud(vector_medio)
dirección = atan2(vector_medio.y, vector_medio.x)
```

La longitud del vector medio difiere de la media de longitudes: movimientos
opuestos pueden cancelarse aunque haya mucha actividad. El resultado depende
del tamaño e intervalo de las imágenes y no mide metros por segundo.

### Fondo y bordes

MOG2 mantiene un modelo estadístico adaptativo y separa cambios respecto al
fondo. Operaciones morfológicas limpian y agrupan la máscara; los contornos
aportan geometría y material visual. Canny detecta bordes sobre gris. Ninguno
de estos resultados, por sí solo, identifica una persona. La admisión de
personas usa la detección y el filtro de superficie descritos más abajo.

Las posiciones normalizadas van de 0 a 1; `(0,0)` está arriba a la izquierda.
Áreas y longitudes de contorno dependen de las unidades de análisis y no
equivalen a medidas físicas de campo.

### Eventos estimados

| Evento | Cálculo y alcance |
|---|---|
| Colisión | Solapamiento de cajas mediante IoU o caída brusca del recuento; no confirma contacto. |
| Balón | Candidatos pequeños, filtros de forma y velocidad, asociación y confirmación temporal; requiere la ruta de campo habilitada. |
| Concentración | Mayor grupo de detecciones próximas dentro de un radio normalizado. |
| `legDistance` | Proxy de elongación basado en altura y área; no mide separación anatómica de piernas. |

IoU es el área de intersección de dos cajas dividida por el área de su unión.
Las condiciones de validez del análisis pueden inhibir los eventos.

## 3. Forma y capítulos

La composición tiene una escala por encima de los capítulos de cada
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
`chooseNextSection()` de `src/VisualComposer.cpp` del programa.

### La cesura: hacer perceptible el cambio

La transición de sección tiene tres fases: **colapso → vacío → emergencia**.
El conjunto converge, se retira durante un intervalo y presenta el nuevo
material. La cesura permite percibir una separación formal. El estado de clear
y su amplitud también llegan al audio para reducir la presencia sonora.

Las siete secciones no deben confundirse con los nueve estados del análisis
colectivo. La sección fija un marco de larga duración; el estado colectivo
describe y modula lo que ocurre dentro de él.

Cada canal presenta capítulos de vídeo, generador, respiración o transición.
La duración se sortea dentro de rangos y tiene una permanencia mínima. Un
capítulo atraviesa aparición, desarrollo, umbral, transformación y disolución.
Su envolvente regula la presencia; la fase normalizada indica el avance entre
cero y uno. Para el vídeo también se considera el final del fragmento.

Las relaciones entre canales son unísono, propagación, contrapunto y grupos
4+4. Comparten reloj y decisiones de organización; el rol de cada canal puede
introducir diferencias dentro de un material común.

## 4. Análisis colectivo y estabilidad temporal

`VisualComposer` combina energía, flujo, concentración y cantidad de detecciones
para obtener actividad por canal. Resume también coherencia direccional,
diversidad de materiales, convergencia y tensión. Son índices compositivos,
no interpretaciones psicológicas ni tácticas del partido.

La coherencia compara el vector resultante de las direcciones ponderadas con
la suma de sus pesos: direcciones alineadas dan una lectura mayor; direcciones
opuestas la reducen. La diversidad considera el reparto del material visible.
La convergencia mide cuánto domina un generador en el conjunto.

Los datos se suavizan en el tiempo y proponen uno de nueve estados: suspensión,
codificación, acumulación, propagación, convergencia, fragmentación, saturación,
ruptura y residuo. El candidato debe persistir antes de adoptarse; hay permanencia
mínima y espera entre eventos. Los estados urgentes tienen una espera menor y
pueden superar la permanencia protegida. Las siete secciones mantienen su marco
de larga duración sobre estas decisiones.

Parte del azar utiliza una semilla. La ejecución completa depende además del
tiempo de carga, los datos, la intervención manual y otros sorteos. Una semilla
no garantiza por sí sola una repetición audiovisual exacta.

## 5. Técnicas visuales y bolsa barajada

`GraphicScore` recibe los modos permitidos por la sección o por un momento
especial de vídeo. Los baraja mediante Fisher–Yates: recorre la lista desde
el final e intercambia cada elemento con uno elegido entre las posiciones
restantes. Intercala `BwClean` después de cada técnica y rellena la bolsa al
agotarla. Los modos de una bolsa aparecen antes de volver a sortear la siguiente.

Esto introduce variación sin permitir cualquier técnica en cualquier momento.
El cambio de paleta reinicia la selección de modos para que la nueva sección
se haga visible. `SlitScan` se activa manualmente; `Flash` se gestiona como una
superposición de evento.

También existen momentos de conjunto dedicados a nube de puntos y a vídeo
gráfico. Un **momento** organiza una situación interna; una **sección** gobierna
la forma más larga; un **capítulo** es el episodio de un canal.

Los modos transforman distintos datos: `VideoNumbers` convierte brillo de
celdas en cifras; `Barcode` resume primer plano por columnas; `Waveform`
mantiene historia de energía; `VideoLines` combina recursos de líneas y
contornos. Los generadores dibujan estructuras como `BitMatrix`, `PhaseLines`,
`Pulse`, `BarScan` o `GranularRaster` con parámetros del estado compositivo.

La nube de puntos muestrea el vídeo en una rejilla GPU. La profundidad puede
derivarse de luminancia o de un paquete PDJV disponible, según configuración.
El relieve de luminancia no es reconstrucción métrica. El tratamiento térmico
es una transformación de intensidad y color, no una lectura de temperatura.

## 6. Personas, superficie de juego y seguimiento

El análisis utiliza YOLOX-S para detectar personas. La detección se filtra
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
velocidad y confirmación temporal. Su presencia es una estimación,
no una garantía de seguimiento correcto.

## 7. Análisis guardado y reproducción

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

## 8. Color y tratamiento sonoro

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

## 9. OSC, síntesis y espacialización

La aplicación envía datos y estados mediante OSC; SuperCollider los interpreta
en UDP 9001. `/pdj/clock/state` comunica el reloj, `/pdj/collective/state` el
resumen colectivo y `/pdj/channel/N/...` los controles de cada canal. OSC
transporta números y mensajes, no el vídeo ni la señal de audio.

Los perfiles asocian modos y generadores con voces, articulaciones y capas.
La síntesis combina tonos, impulsos, ruido filtrado, subgraves y acentos. Los
datos modulan esos materiales dentro del marco compositivo. Revisiones de
capítulo y de modo distinguen cambios de escena de variaciones continuas.
Las fases, envolventes y relaciones de organización llegan al motor junto a
las medidas del vídeo.

El envío reserva hasta ocho huecos de detecciones por canal y vacía los que no
se utilizan. La geometría agregada aporta posición, extensión y actividad; un
hueco no es una identidad permanente. El reloj compartido coordina controles,
pero UDP no garantiza alineación física exacta de sonido e imagen.

En las ocho salidas, DBAP calcula:

```text
distancia_i = máximo(distancia entre fuente y altavoz_i, 0,001)
peso_i = distancia_i ^ (−rolloff) + spread × 0,5
ganancia_i = peso_i / raíz(media de los pesos al cuadrado)
```

`rolloff` regula la caída con la distancia y `spread` añade una contribución
común. La normalización RMS no significa que las ganancias sumen uno. La
mezcla, sus niveles y efectos también determinan el resultado. La ruta estéreo
utiliza panoramización izquierda/derecha. Las posiciones virtuales deben
corresponder al mapa de salidas que configura el recinto.

## 10. Retorno de actividad sonora

El visor muestra tres filas: **sonido**, **pulsos** y **textura**.
La derecha marca **AHORA**; hacia la izquierda quedan seis segundos de historia.
Los pulsos se dibujan con puntos o marcas cortas, los sonidos continuos con
trazos y la textura con trazos más gruesos. Un indicador resume actividad
reciente y puede aparecer el nombre de un instrumento recibido.

El audio devuelve medidas de tres capas instrumentadas: voz principal, motor
de pulsos y cama de ruido. La aplicación valida canal, sesión y secuencia,
descarta mensajes antiguos y deja de señalar actividad actual cuando los datos
pierden vigencia. El dibujo consulta ese estado; no genera sonido.

**La representación comprende tres capas.** Las tres filas no explican necesariamente
todas las voces, acentos ni efectos de la mezcla. Una medida de señal digital
tampoco confirma que el altavoz esté encendido o que ese sonido sea perceptible
en sala. La latencia física entre imagen y sonido requiere calibración; el
visor usa el instante local de recepción de los datos.

Para comprobar su lectura, conviene aislar una fuente, observar su comienzo,
su duración y su final, y después escucharla en el conjunto. Una pantalla
representa una contribución sonora, no la salida exclusiva de un altavoz.
