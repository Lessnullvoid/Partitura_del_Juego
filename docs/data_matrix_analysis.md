# Análisis técnico de `19 data.matrix.flac`

> Análisis funcional realizado sobre el archivo suministrado. No es una
> transcripción destinada a duplicar la grabación: identifica los procesos
> audibles y los convierte en un vocabulario reutilizable para una obra nueva.

## Fuente

- Duración: 601 s (10:01)
- Formato: FLAC, estéreo, 44.1 kHz, 16 bit
- Nivel RMS medio por ventanas de 1 s: -17.6 dBFS
- RMS percentiles 10/50/90: -29.0 / -16.1 / -6.2 dBFS
- Factor de cresta mediano: 12.5 dB
- Sonoridad integrada: -12.3 LUFS; rango de sonoridad: 8.0 LU
- True peak medido por sobremuestreo: +4.6 dBTP (inter-sample peaks severos)
- Correlación estéreo percentiles 10/50/90: 0.022 / 0.814 / 0.997

La gran variación de correlación indica que la imagen alterna entre material
casi monofónico y campos laterales muy abiertos. El estéreo no es una capa
decorativa constante: forma parte de la articulación.

El true peak positivo no debe copiarse al nuevo motor. Es preferible conservar
la sensación de presión mediante saturación controlada y limitar por debajo de
-1 dBFS, especialmente al trabajar con impulsos y frecuencias ultrasónicas.

## Gramática espectral

### A. Columna subgrave

El pico persistente principal aparece en 45.76 Hz. Se observan componentes
relacionados en 91.52, 134.58 y 183.03 Hz, aproximadamente los primeros
armónicos de esa base. El sonido no funciona como una melodía; es una columna
espectral que cambia de peso, saturación y envolvente.

Implementación: seno/triángulo a 45.76 Hz, parciales 2–4, ligera inestabilidad,
LPF y saturación suave.

### B. Marcadores de alta frecuencia

El espectrograma muestra líneas y ataques persistentes aproximadamente en
2.0, 6.3, 10.0 y 12.6 kHz. Tienen dos funciones:

1. puntos de calibración estables;
2. resonadores de impulsos que hacen audible la cuadrícula.

Implementación: `Ringz`, `BPF` y senos extremadamente bajos de amplitud. Las
frecuencias deben desplazarse algunos cents para evitar una copia literal.

### C. Bandas de ruido

Se alternan ruido de banda estrecha, ruido casi blanco y acumulaciones densas
de transitorios. La mediana del centroide es baja (204 Hz), pero el percentil
90 alcanza 2.36 kHz: la forma depende de saltos entre masa grave y exposición
aguda, no de una evolución espectral uniforme.

Implementación: `PinkNoise`, `GrayNoise`, bancos `BPF`, `HPF`, `Latch` y
sample-rate reduction controlada.

### D. Líneas y peines

Entre los bloques rítmicos aparecen estratos horizontales estables y familias
de parciales. Pueden modelarse como bancos sinusoidales, resonadores excitados
por ruido y peines con tiempos cortos.

## Gramática temporal

La periodicidad dominante medida es aproximadamente 161.5 BPM:

- negra: 0.3715 s;
- corchea: 0.1858 s;
- semicorchea: 0.0929 s;
- tresillo de semicorchea: 0.0619 s.

La densidad media detectada es 7.7 transitorios/s y los bloques activos llegan
a 10.6–10.7/s. Por eso se perciben simultáneamente pulso y textura: una capa
mantiene el periodo de 0.3715 s mientras otras subdivisiones se acercan a la
fusión granular.

El ritmo se produce mediante:

- trenes regulares de impulsos;
- máscaras binarias que eliminan pasos;
- ráfagas cortas dentro de un pulso principal;
- silencios duros en fronteras de bloque;
- cambios de registro sin cambiar necesariamente el reloj;
- acumulación de subdivisiones, no groove humano ni swing.

## Segunda pasada: procesos que faltaban

Un análisis con ventanas de 2 s y seguimiento de centroide y cresta espectral
revela más variedad que la división inicial entre pulso y ruido.

### Barridos espectrales

Hay desplazamientos ascendentes o descendentes sostenidos de 8–12 s. Los más
claros aparecen alrededor de 00:49–00:59, 01:35–01:45, 02:21–02:31,
03:32–03:42, 06:18–06:28 y 06:44–06:54. Algunos desplazan una banda audible;
otros retiran energía aguda y dejan expuesto el subgrave. Por eso se modelan
por separado como `noiseRise` y `noiseFall`.

### Barras de frecuencia

Además de barridos continuos aparecen bloques horizontales: ruido de banda
estrecha que salta entre centros discretos. Este comportamiento necesita
`LFNoise0`/escalones, no un filtro que se desplace suavemente.

### Desfase de relojes

La periodicidad de 161.5 BPM convive con múltiplos cercanos a 3, 4 y 5 veces
el pulso. Pequeñas diferencias entre estos relojes producen ciclos de
coincidencia y separación: el patrón cambia aunque cada reloj siga siendo
regular. Esta es una fuente de complejidad más adecuada que el azar en cada
ataque.

### Microcortes y espacio negativo

La densidad cae de unos 10.7 ataques/s a regiones casi vacías. El silencio
funciona como un estado activo: conserva uno o dos puntos de referencia y
elimina temporalmente la máquina. `microCuts` trabaja con partículas aisladas;
`negativeSpace` con pausas largas y referencias mínimas.

### Presión sin pulso

En varios retornos domina el rango 20–250 Hz sin que la rejilla sea el elemento
principal. Esta masa grave merece una escena propia (`pressure`) para evitar
que todo aumento de energía se traduzca automáticamente en más ritmo.

## Mapa formal observado

Las fronteras son aproximadas; describen cambios de régimen, no compases.

| Tiempo | Función dominante | Componentes |
|---|---|---|
| 00:00–00:54 | Exposición/calibración | subgrave, líneas fijas, clicks escasos |
| 00:54–01:12 | Primera activación | pulsos graves, ruido ancho, marcadores HF |
| 01:12–02:22 | Máquina rítmica I | tren principal, subdivisiones, sub armónico |
| 02:22–02:36 | Corte/reconfiguración | caída de energía y cambio de espectro |
| 02:36–03:38 | Máquina rítmica II | ataques densos, mayor peso de 45.8 Hz |
| 03:38–04:02 | Vacío y reinicio | corte, tono/ruido de transición |
| 04:02–05:45 | Expansión | pulsos, peines, líneas medias, acumulación |
| 05:45–06:10 | Interferencia | masa de ruido y pérdida momentánea de rejilla |
| 06:10–07:09 | Máquina fragmentada | grupos rítmicos separados por huecos |
| 07:09–07:44 | Ruptura | discontinuidades y bloques aislados |
| 07:44–08:20 | Retorno de presión | subgrave y pulso condensado |
| 08:20–08:30 | Corte | transición abrupta |
| 08:30–09:00 | Desactivación | desaparece el motor de impulsos |
| 09:00–10:01 | Coda/residuo | parciales medios, ruido tenue y fade largo |

Microfronteras medidas: 00:54, 01:12, 01:33, 01:52, 02:05, 02:22, 02:36,
02:52, 03:05, 03:18, 03:35, 03:48, 04:02, 04:15, 04:34, 04:52, 05:10,
05:27, 05:47, 06:10, 06:26, 06:45, 07:09, 07:31, 07:44, 08:07, 08:20 y
09:55.

## Componentes del estudio SuperCollider

El archivo `supercollider/data_matrix_functional_study.scd` traduce el análisis
a módulos independientes:

| Módulo | Función |
|---|---|
| `dmSubColumn` | fundamental grave y armónicos |
| `dmCalibrationLines` | líneas sinusoidales estables de referencia |
| `dmPulseVoice` | impulso resonante individual |
| `dmPulseMachine` | rejilla, máscara y subdivisiones |
| `dmNoiseField` | bandas de ruido y apertura estéreo |
| `dmCombField` | estratos de peine/interferencia |
| `dmBurst` | ruptura y ráfaga digital |
| `dmResidue` | coda tonal de baja energía |
| `dmNoiseSweep` | barrido continuo ascendente o descendente |
| `dmBandBars` | ruido filtrado en bandas escalonadas |
| `dmPhaseGrid` | tres relojes próximos que entran y salen de fase |
| `dmMicroCuts` | partículas breves separadas por silencio real |
| `dmBeacons` | puntos pulsados de alta frecuencia |
| `dmNegativeSpace` | vacío articulado con referencias mínimas |
| `dmPressureMass` | presión grave continua sin beat explícito |
| `dmNoiseToPulse` | transformación progresiva de ruido a rejilla rítmica |
| `dmDroneRhythmInterlock` | alternancia complementaria de drone y ritmo |
| `dmTonalInterference` | contorno afinado producido por batimientos y peines |
| `dmMaster` | filtro, saturación, limitación y control de salida |

El código incluye controles para `density`, `subdivision`, `mask`, `brightness`,
`width`, `noise`, `pressure` y `space`. Son los puntos adecuados para conectar
los datos de *Partitura del Juego*.

## Escenas disponibles

`calibration`, `beacons`, `machineA`, `machineB`, `machineC`, `machineD`,
`phaseGrid`, `noiseRise`, `noiseFall`, `bandBars`, `microCuts`, `interference`,
`interferenceTonal`, `interferenceDrift`, `negativeSpace`, `pressure`,
`fragmented`, `rupture` y `residue`.

`machineC` comienza como una banda de ruido ancha. Durante unos 20 s el filtro
se estrecha, el ruido se fragmenta con una rejilla y finalmente dominan los
resonadores rítmicos. Es una metamorfosis dentro de una escena, no un crossfade
entre dos presets.

`machineD` distribuye drone y ritmo en ventanas temporales complementarias. En
las transiciones aparece ruido filtrado; una interferencia afinada secundaria
conserva continuidad entre ambas mitades.

`interferenceTonal` recorre lentamente una retícula de proporciones simples y
produce batimientos alrededor de cada altura. `interferenceDrift` usa cambios
más lentos, mayor desafinación y feedback más largo, de modo que el contorno se
percibe como memoria espectral en lugar de línea melódica frontal.

`~dmPlayStudy` recorre todas las familias en una forma condensada. La función
`~dmPlayGenerative` escoge escenas y duraciones con probabilidades ponderadas,
evita repeticiones inmediatas y conserva `rupture` y `residue` como estados
raros. Esto permite escuchar una forma abierta antes de conectarla al vídeo.

## Traducción recomendada al proyecto

- Movimiento acumulado → `density`, nunca amplitud instantánea.
- Número de blobs → máscara binaria persistente durante 2–8 pulsos.
- Dispersión espacial → `width` y separación entre resonadores.
- Velocidad → probabilidad de aumentar `subdivision`.
- Colisión → `dmBurst`, con periodo refractario.
- Crowd → `pressure` del subgrave.
- Posición vertical → `brightness`.
- Estado del generador visual → elección de régimen formal.

La lección principal es que el dato no debería sintetizar cada sonido. Debe
seleccionar y transformar regímenes que continúan viviendo durante un tiempo.
