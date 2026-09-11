# Partitura del Juego — Manual de Instalación y Operación

**Versión:** `pdj-video-pointcloud-v1` · macOS arm64 · Apple Silicon

---

## Índice

1. [Descripción del sistema](#1-descripción-del-sistema)
2. [Requisitos de hardware](#2-requisitos-de-hardware)
3. [Contenido del paquete](#3-contenido-del-paquete)
4. [Instalación inicial](#4-instalación-inicial)
5. [Configuración de los muros de vídeo — ICUIXIAN](#5-configuración-de-los-muros-de-vídeo--icuixian)
6. [Configuración de audio — DANTE 8 canales](#6-configuración-de-audio--dante-8-canales)
7. [Secuencia de arranque](#7-secuencia-de-arranque)
8. [La ControlApp — referencia completa](#8-la-controlapp--referencia-completa)
9. [Ajuste de imagen por canal](#9-ajuste-de-imagen-por-canal)
10. [Funcionamiento normal durante la instalación](#10-funcionamiento-normal-durante-la-instalación)
11. [Secuencia de apagado](#11-secuencia-de-apagado)
12. [Diagnóstico de problemas](#12-diagnóstico-de-problemas)
13. [Referencia rápida](#13-referencia-rápida)

---

## 1. Descripción del sistema

*Partitura del Juego* es una instalación audiovisual generativa. Un solo ordenador
Mac procesa vídeo deportivo pregrabado y genera en tiempo real ocho canales
visuales independientes y ocho canales de audio espacial.

### Flujo de señal

```
Vídeo deportivo pregrabado
    → openFrameworks (visión artificial + renderizado)
        → 8 canales visuales → 2 ventanas → 2 ICUIXIAN → 8 pantallas
        → OSC UDP (localhost:9001)
    → SuperCollider (síntesis + espacialización DBAP)
        → Dante Virtual Soundcard → red DANTE → DANTE 5 → 8 altavoces de sala
```

### Distribución de pantallas

| Muro | Ventana | Canales | ICUIXIAN | Layout | Orientación |
|------|---------|---------|----------|--------|-------------|
| A    | A       | 0 – 3   | Unidad A | 4×1    | Retrato, rotación 90° |
| B    | B       | 4 – 7   | Unidad B | 2×2    | Horizontal, sin rotación |

Cada canal ejecuta su propio pipeline de visión artificial, partitura gráfica,
nube de puntos de vídeo por GPU y transmisión OSC.

---

## 2. Requisitos de hardware

### Mínimo para instalación completa

| Cant. | Equipo |
|-------|--------|
| 1 | Mac Apple Silicon (M1 o posterior) con **dos salidas HDMI independientes** |
| 4 | Pantallas 1080×1920 px (9:16) para montaje en retrato — Muro A |
| 4 | Pantallas 1920×1080 px (16:9) para montaje horizontal — Muro B |
| 1 | ICUIXIAN 0104-XZ (ASIN B0DM98NVSH) — Muro A |
| 1 | ICUIXIAN 0104-XZ (ASIN B0DM98NVSH) — Muro B |
| 1 | Switch Ethernet Gigabit (mismo segmento de red que DANTE 5) |
| 1 | Unidad DANTE 5 del sistema de audio de sala |
| 10 | Cables HDMI (2 Mac→ICUIXIAN + 8 ICUIXIAN→pantallas) |
| 1+ | Cable Ethernet Cat5e/Cat6 |

### Para pruebas sin instalación completa

Un portátil Apple Silicon con SuperCollider instalado es suficiente para
verificar audio y vídeo. Ver sección 4.3.

### Software en el Mac

| Software | Fuente | Notas |
|----------|--------|-------|
| macOS 11 o posterior | Sistema | — |
| SuperCollider | supercollider.github.io | Instalar en `/Aplicaciones` |
| Dante Virtual Soundcard | audinate.com | Licencia aprox. USD 30, perpetua por máquina |
| Dante Controller | audinate.com | Gratuito |

---

## 3. Contenido del paquete

El archivo `Partitura_del_Juego-macOS-arm64.zip` contiene:

```
Partitura_del_Juego-macOS-arm64/
├── Partitura_del_Juego.app        — aplicación visual (ad-hoc signed)
├── Start Audio.command            — arranca SuperCollider con doble clic
├── README.md                      — guía rápida de distribución
├── SuperCollider/
│   ├── pdj_launcher.scd           — punto de entrada: detecta DANTE y carga el motor
│   ├── pdj_audio_config.scd       — configuración del servidor de audio
│   ├── pdj_datamatics.scd         — motor principal: síntesis, DBAP, OSC
│   ├── pdj_mode_voices.scd        — voces por modo de partitura gráfica
│   └── pdj_volumetric_compat.scd  — compatibilidad con runtime volumétrico
└── Videos/
    ├── Portrait/                  — clips para los canales 0–3 (formato retrato)
    └── Horizontal/                — clips para los canales 4–7 (formato horizontal)
```

La configuración del usuario se guarda en:
```
~/Library/Application Support/PartituraDelJuego/settings.json
```
Este archivo se crea automáticamente en el primer arranque. Eliminarlo
restablece todos los ajustes a los valores predeterminados del paquete.

---

## 4. Instalación inicial

### 4.1 Descomprimir y primera ejecución

1. Descomprimir `Partitura_del_Juego-macOS-arm64.zip`.
2. Mover la carpeta resultante a un lugar con permisos de escritura (el Escritorio
   o `~/Aplicaciones`).
3. **No abrir la app directamente desde el ZIP ni desde una carpeta de solo lectura.**
4. Control-clic sobre `Partitura_del_Juego.app` → **Abrir** → confirmar **Abrir**
   en el diálogo de macOS.

Si macOS bloquea la apertura, ejecutar en Terminal:

```sh
xattr -dr com.apple.quarantine "/ruta/a/Partitura_del_Juego.app"
```

Después control-clic → **Abrir** nuevamente.

### 4.2 Instalar SuperCollider

1. Descargar desde https://supercollider.github.io la versión para Apple Silicon.
2. Copiar `SuperCollider.app` a `/Aplicaciones` o `~/Aplicaciones`.
3. Abrirlo una vez manualmente para aceptar los permisos del sistema operativo.

**Permiso de micrófono obligatorio:**
```
Configuración del Sistema → Privacidad y Seguridad → Micrófono
→ Activar para SuperCollider
```
SuperCollider necesita este permiso aunque *Partitura del Juego* no use entrada
de micrófono.

### 4.3 Prueba básica en portátil (sin instalación completa)

Con el paquete descomprimido y SuperCollider instalado:

1. Doble clic en `Start Audio.command`. Esperar `PDJ Datamatics — listening OSC UDP :9001`.
2. Abrir `Partitura_del_Juego.app`.

La app arranca en modo de prueba con dos ventanas de presentación de 1920×1080 px
cada una. Para verlas en una sola ventana, editar
`~/Library/Application Support/PartituraDelJuego/settings.json` y cambiar
`"outputMode"` a `"singleWindow"`.

La tecla `U` abre y cierra la ControlApp. Verificar que los ocho canales
reproducen vídeo y que la ControlApp muestra 48 clips disponibles.

---

## 5. Configuración de los muros de vídeo — ICUIXIAN

### 5.1 Conexiones físicas

**Muro A — cuatro pantallas en retrato:**

1. Conectar la primera salida HDMI del Mac a `HDMI IN` de ICUIXIAN A.
2. Conectar `HDMI OUT 1` → pantalla izquierda (canal 0).
3. Conectar `HDMI OUT 2` → segunda pantalla (canal 1).
4. Conectar `HDMI OUT 3` → tercera pantalla (canal 2).
5. Conectar `HDMI OUT 4` → pantalla derecha (canal 3).
6. En ICUIXIAN A: seleccionar mosaico **4×1** y rotación **90°**.
   Si las imágenes quedan invertidas, usar **270°**.

**Muro B — cuatro pantallas horizontales:**

1. Conectar la segunda salida HDMI del Mac a `HDMI IN` de ICUIXIAN B.
2. Conectar `HDMI OUT 1` → pantalla superior izquierda (canal 4).
3. Conectar `HDMI OUT 2` → pantalla superior derecha (canal 5).
4. Conectar `HDMI OUT 3` → pantalla inferior izquierda (canal 6).
5. Conectar `HDMI OUT 4` → pantalla inferior derecha (canal 7).
6. En ICUIXIAN B: seleccionar mosaico **2×2** y rotación **0°**.

### 5.2 Configuración desde macOS

1. Abrir `Preferencias del Sistema → Pantallas`.
2. Desactivar el modo espejo. Activar **escritorio extendido** con las dos
   unidades ICUIXIAN como pantallas externas independientes.
3. Las dos pantallas externas deben aparecer a **1920×1080 a 60 Hz**.

### 5.3 Configuración desde la ControlApp

1. Abrir la app. Tecla `U` si la ControlApp no es visible.
2. En el panel lateral, pulsar **Configure Mixed Wall (4V + 2x2H)**.
   - La app detecta automáticamente las dos salidas externas a 1080p60.
   - Cambia ambas a 1920×1080 a 60 Hz y guarda sus posiciones en `settings.json`.
   - Muestra un resumen de la configuración guardada para Muro A y Muro B.
3. **Reiniciar la aplicación** para que los cambios de posición de ventana
   tengan efecto.

> Si solo se usan pantallas en retrato (sin mosaico 2×2), usar el botón
> **Configure ICUIXIAN outputs (4V + 4V)** en su lugar.

### 5.4 Verificar el orden de canales

Después de reiniciar con la configuración guardada, en la ControlApp →
pestaña **Overview**: el canal 0 debe estar en la posición física más a la
izquierda del Muro A y el canal 7 en la posición más a la derecha del Muro B
(o inferior derecha en el mosaico 2×2).

Si el orden no coincide, ajustar manualmente las posiciones de ventana en
`~/Library/Application Support/PartituraDelJuego/settings.json` bajo
`"presentationWindows"`.

---

## 6. Configuración de audio — DANTE 8 canales

Para la guía técnica completa del motor de audio, ver `docs/AUDIO_MULTICHANNEL_ES.md`.
Esta sección cubre los pasos esenciales de instalación.

### 6.1 Preparación de macOS (una sola vez)

```
Configuración del Sistema → Batería → Nunca entrar en reposo cuando está enchufado
Configuración del Sistema → Pantallas → Protector de pantalla → Nunca
Modo No Molestar → Activado  (evita que sonidos del sistema lleguen a la salida)
Firewall → Agregar Dante Virtual Soundcard y Dante Controller a las excepciones
```

### 6.2 Configurar Dante Virtual Soundcard

1. Instalar DVS desde https://www.audinate.com/products/software/dante-virtual-soundcard
   (requiere licencia de pago, aprox. USD 30).
2. Abrir la app DVS desde la barra de menú del Mac y configurar:

   | Parámetro         | Valor |
   |-------------------|-------|
   | Transmit channels | 8     |
   | Receive channels  | 2     |
   | Sample rate       | 48000 Hz |
   | Latency           | 1 ms (mismo switch) · 5 ms (switch gestionado) |

3. Hacer clic en **Enable**.
4. Abrir `/Aplicaciones/Utilidades/Audio MIDI Setup` y confirmar que
   `Dante Virtual Soundcard` aparece como **48000.0 Hz · 8 canales de salida**.

### 6.3 Enrutar canales en Dante Controller

1. Instalar Dante Controller desde https://www.audinate.com/products/software/dante-controller (gratuito).
2. Conectar el Mac al switch Ethernet del recinto (mismo switch que DANTE 5).
3. Abrir Dante Controller. Esperar a que aparezcan los dos dispositivos en la vista de red:
   `Dante Virtual Soundcard` y el dispositivo DANTE 5.
4. En la pestaña **Routing** crear las suscripciones:

   | Receptor DANTE 5 | Transmisor DVS | Altavoz |
   |------------------|----------------|---------|
   | D3-1 | DVS Out 1 | Derecha, frente |
   | D3-2 | DVS Out 2 | Centro, frente |
   | D3-3 | DVS Out 3 | Izquierda, frente |
   | D3-4 | DVS Out 4 | Muro izquierdo, media profundidad |
   | D3-5 | DVS Out 5 | Izquierda, fondo |
   | D3-6 | DVS Out 6 | Agrupación central |
   | D3-7 | DVS Out 7 | Centro, fondo |
   | D3-8 | DVS Out 8 | Agrupación central |

5. En **Device Info** de DANTE 5: **Clock Master = Yes**.
6. En **Device Info** de DVS: confirmar **Sync to External**.
7. Ambos dispositivos deben mostrar **candado verde**. Si aparece candado
   amarillo o rojo, revisar que DVS y Audio MIDI Setup estén a 48000 Hz.

> Esta configuración se guarda en la red DANTE y no necesita repetirse
> en cada sesión.

### 6.4 Prueba de altavoces

Con `Start Audio.command` activo y `Partitura_del_Juego.app` en marcha,
abrir el IDE de SuperCollider y ejecutar:

```supercollider
~speakerTest.();
```

Recorrer la sala. Cada altavoz sueña 2 segundos en orden D3-1 a D3-8 mientras
la Post Window imprime su número. Si un altavoz físico suena en el paso
incorrecto, corregir las suscripciones en Dante Controller.

### 6.5 Rescate estéreo automático

Si Dante Virtual Soundcard no está activo o no se detecta en la red, el motor
arranca automáticamente en estéreo sobre la salida de audio por defecto del Mac.
No se requiere ningún cambio de código. El comportamiento espacial es idéntico
pero con dos canales en lugar de ocho.

---

## 7. Secuencia de arranque

Seguir este orden en cada sesión:

```
1. ALIMENTACIÓN ELÉCTRICA
   Encender las regletas/multicontactos. Esperar 5 segundos.

2. PANTALLAS
   Encender las ocho pantallas.

3. ICUIXIAN
   Encender las dos unidades ICUIXIAN.
   Esperar a que las cuatro salidas de cada unidad muestren señal o imagen
   de espera (puede tardar 10–20 segundos).

4. ORDENADOR
   Encender el Mac. Verificar que los dos escritorios ICUIXIAN aparecen
   como pantallas externas en Preferencias del Sistema → Pantallas.

5. DANTE VIRTUAL SOUNDCARD
   Abrir la app DVS desde la barra de menú.
   Confirmar que está habilitada (Enable) y configurada a 48000 Hz / 8 TX.

6. SUPERCOLLIDER — MOTOR DE AUDIO
   Doble clic en Start Audio.command.
   Esperar los tres pasos en la Terminal:
     [1/3] ... "PDJ audio: DANTE detected -> 8ch @ 48kHz"
     [2/3] Servidor listo — imprime informe de altavoces
     [3/3] ... "PDJ Datamatics — listening OSC UDP :9001"
   Dejar la Terminal abierta durante toda la instalación.

7. APLICACIÓN VISUAL
   Abrir Partitura_del_Juego.app.
   Verificar que los ocho canales reproducen vídeo.
   La tecla U abre la ControlApp si es necesario ajustar algo.
```

> **Tiempo estimado de arranque completo:** 3–5 minutos desde encender los
> multicontactos hasta imagen y audio estables.

---

## 8. La ControlApp — referencia completa

La ControlApp se abre y cierra con la tecla **`U`**. Ocultar la ControlApp
antes de la presentación no afecta el funcionamiento.

### Panel lateral — configuración de salida

Siempre visible en la franja izquierda:

| Elemento | Función |
|---|---|
| Campo **OSC host** | IP del Mac que corre SuperCollider (por defecto `localhost`) |
| Campo **OSC port** | Puerto de envío OSC (por defecto 9001) |
| **Total clips** | Número de clips cargados en el pool |
| **Configure ICUIXIAN outputs (4V + 4V)** | Detecta dos salidas externas 1080p60 y guarda ambas como muro de retratos 4×1 con rotación 90° |
| **Configure Mixed Wall (4V + 2x2H)** | Detecta dos salidas externas: Muro A como 4×1 retrato (90°), Muro B como 2×2 horizontal (0°) |
| Resumen Wall A / Wall B | Muestra la configuración guardada activa |

Después de pulsar cualquier botón de configuración, **reiniciar la aplicación**.

### Página Overview

Estado operativo de los ocho canales simultáneamente. Por canal:

| Elemento | Descripción |
|---|---|
| Nombre del clip activo | Clip que se reproduce en este momento |
| **Next** | Carga el siguiente clip inmediatamente |
| **Play / Pause** | Alterna reproducción y pausa |
| **Stop** | Detiene la reproducción |
| Slider de velocidad | Multiplica la velocidad de reproducción (0.1× – 4×) |
| Modo visual activo | Modo actual de GraphicScore (BwClean, ScanLine, etc.) |
| Detectores de eventos | Collision · Ball · Crowd density · Leg distance |
| Telemetría en vivo | Motion energy · Flow magnitude · Flow angle · Blobs · Contour |

### Página Global Director

Controla la manipulación temporal y visual de todos los canales simultáneamente.

**Disparadores manuales:**

| Botón | Acción |
|---|---|
| **Slow Mo** | Cámara lenta en todos los canales (30% de velocidad base, ~2–4 s) |
| **Fast Fwd** | Avance rápido en todos los canales (350% de velocidad base, ~2–4 s) |
| **Clear Black** | Borrado a negro sobre todos los canales |
| **Clear Red** | Borrado a rojo oscuro sobre todos los canales |

**Automatización:**

| Toggle | Acción |
|---|---|
| **Auto Slow** | Activa disparado automático de cámara lenta |
| **Auto Fast** | Activa disparado automático de avance rápido |
| **Auto Clear** | Activa disparado automático de borrado |

Los tres están desactivados por defecto para que el intérprete decida cuándo actuar.

**Parámetros de cada proceso** (bloques expandibles):

- *Slow Motion:* velocidad objetivo, duración de rampa bajada, duración de
  retención, duración de rampa subida, intervalo entre disparos automáticos.
- *Fast Forward:* ídem para el proceso de avance rápido.
- *Screen Clear:* duración de fade-in, duración de retención, duración de
  fade-out, paleta de color del borrado, intervalo entre borrados automáticos.

### Página Video Director

Controla la planificación cinematográfica del material de archivo.

| Elemento | Descripción |
|---|---|
| **Enabled** | Activa / desactiva el VideoDirector (si desactivado, los canales se detienen al terminar el segmento actual) |
| Short / Long / Full weight | Probabilidad de cada tipo de plan (fragmento corto / largo / clip completo) |
| Short min/max | Rango de duración del fragmento corto en segundos (8–25 s por defecto) |
| Long min/max | Rango del fragmento largo (30–120 s) |
| Shared interval | Intervalo entre eventos compartidos (120–300 s) |
| Shared start delay | Retardo de arranque sincronizado (0.35 s) |
| **Next** por canal | Fuerza la carga del siguiente clip en ese canal |

### Página Visual Composer

Controla el `VisualComposer` — el sistema que organiza generadores,
vídeo, respiraciones y transiciones en el tiempo.

**Estado:**

| Elemento | Descripción |
|---|---|
| **Enabled** | Activa / desactiva el compositor |
| Momento activo | PulseSystem · BarScanSystem · Intercalation |
| Tiempo en el momento | Segundos transcurridos en el momento actual |
| **Next moment** | Fuerza el avance al siguiente momento |
| **Takeover** | Fuerza un takeover global (todos los canales al mismo generador) |

**Programa:**

| Parámetro | Descripción |
|---|---|
| Pulse moment duration | Duración del momento PulseSystem en segundos |
| Bar moment duration | Duración del momento BarScanSystem en segundos |
| Takeover min/max | Rango de duración de un takeover global |

**Eventos periódicos durante Intercalation:**

| Evento | Toggle | Descripción |
|---|---|---|
| Noise event | **Noise event** checkbox | Ruido analógico a muro completo; botón **Force** para disparar manualmente |
| Generator invert | **Generator invert** checkbox | Inversión de paleta en canales con generador activo |
| Polarity invert | **Polarity invert** checkbox | Inversión de polaridad de canal completo (generadores + nube de puntos) |

Cada evento tiene un slider de intervalo (segundos entre disparos automáticos)
y un slider de duración, más un botón **Force** para activarlo en cualquier momento.

**Organización espacial:**

| Parámetro | Descripción |
|---|---|
| Organization weights | Pesos de Unison / Propagation / Counterpoint / Group4Plus4 |
| Disabled generators | Lista de generadores excluidos del programa (por defecto: OrbitalRings) |

### Página Channel Editor

Edición detallada de un canal individual. Seleccionar el canal con el selector
de la parte superior.

#### VIDEO POINT CLOUD

| Parámetro | Efecto |
|---|---|
| Enabled | Activa/desactiva la nube de puntos de ese canal |
| Quality tier | Draft / Preview / Installation |
| Grid W / Grid H | Resolución de la cuadrícula UV (por defecto 256×256) |
| Depth source | Luminance · PdjvMask · Hybrid |
| Depth scale | Escala del relieve de profundidad |
| Point size | Tamaño de cada punto en píxeles |
| Feedback | Activa retroalimentación temporal |
| Feedback decay | Factor de desvanecimiento de la retroalimentación (0–1) |
| Preset | Luminance Relief · Player Extraction · Hybrid Stadium Field |

#### IMAGE

Controla el shader B&W aplicado al vídeo base:

| Parámetro | Efecto |
|---|---|
| Contrast / Bright | Ajuste lineal de contraste y brillo |
| Gamma | Curva de gamma |
| S curve | Intensidad de la curva S tonal fílmica |
| Grain | Amplitud del grano analógico animado |
| Vignette | Intensidad del oscurecimiento elíptico de bordes |
| Threshold | Umbral duro B&W (0 = desactivado) |
| Posterize | Cuantización de niveles de brillo (256 = desactivado) |
| CLAHE | Mejora de contraste adaptativa local (desactivada por defecto) |

#### COMPUTER VISION

| Parámetro | Efecto |
|---|---|
| Enabled | Activa/desactiva el análisis CV del canal |
| Blob thr | Umbral de binarización para la detección de contornos |
| Min area | Área mínima en px² para considerar un blob válido |
| Canny low / high | Umbrales del detector de bordes Canny |

#### EVENTS

| Parámetro | Efecto |
|---|---|
| Ball area | Área máxima en px² para identificar un blob como balón |
| Ball speed | Velocidad mínima normalizada para confirmar el balón |
| Crowd N | Número mínimo de blobs para evento de multitud |
| Crowd dist | Radio normalizado de agrupación para multitud |
| Overlap thr | Umbral de IoU para detectar colisión |

#### SCORE

| Parámetro | Efecto |
|---|---|
| Min dur / Max dur | Rango de duración aleatoria de cada modo en segundos |
| Flash dur | Duración del flash blanco en colisión (segundos) |
| Opacity | Opacidad global de las marcas de la partitura gráfica |
| Mode (dropdown) | Auto = secuencia automática; cualquier otro = modo forzado |
| Active | Modo activo en este instante (solo lectura) |
| Square size / count | Tamaño y número máximo de parches en VideoSquares |
| Slit W | Anchura de la franja de muestreo en SlitScan |

La sección **LIVE CV DATA** al final muestra en tiempo real los valores de
flujo, ángulo, energía, blobs y contorno.

### Página Performance

Ejecuta una prueba de carga reproducible de 10 minutos sobre las dos ventanas
y los ocho canales.

**Botones:**

| Botón | Acción |
|---|---|
| **Start** | Inicia la prueba (10 s de calentamiento, luego 10 min de carga) |
| **Stop** | Detiene la prueba y guarda un informe parcial |
| **Reset** | Restablece el estado visual original (disponible tras finalizar) |
| **Export report** | Guarda JSON y CSV en `~/Documents/PartituraDelJuego/performance_reports/` |

**Umbral de aprobación:**

| Métrica | Umbral |
|---|---|
| FPS mínimo | 29 fps (ambas ventanas) |
| p95 de tiempo de fotograma | < 38 ms |
| Fotogramas tardíos | < 0.5% por encima de 50 ms |
| Atasco máximo | < 100 ms |
| Crecimiento de memoria | < 256 MB |

---

## 9. Ajuste de imagen por canal

### 9.1 Puesta a punto general

Al encender por primera vez, los parámetros por defecto están calibrados para
vídeo deportivo en condiciones de estadio (luz diurna, cámara alta). Antes de
la apertura al público:

1. Abrir la ControlApp (tecla `U`).
2. Ir a **Channel Editor** → seleccionar canal 0.
3. Ajustar **Gamma** (recomendado 0.85–0.95) hasta que los grises medios
   tengan peso visual.
4. Ajustar **Contrast** (recomendado 1.1–1.4) para que las marcas sean legibles
   sin saturar las altas luces.
5. **Grain** entre 0.02 y 0.05 da textura fílmica sin ser molesto.
6. **Vignette** entre 0.25 y 0.40 enmarca el canal verticalmente.
7. Repetir para los demás canales. Los valores no tienen por qué ser idénticos
   entre canales — pequeñas variaciones enriquecen la diversidad visual.

### 9.2 Ajuste de visión artificial

Si los blobs detectados son demasiados o demasiado pequeños:

- Aumentar **Min area** (por defecto 500 px²) para ignorar blobs pequeños.
- Ajustar **Blob thr** (umbral de binarización, por defecto 80) si la
  sustracción de fondo genera demasiado ruido.

Si no se detectan eventos de balón:

- Reducir **Ball area** y **Ball speed** para capturar balones más lentos o aparentes.

### 9.3 Forzar un modo visual

En **Channel Editor → SCORE → Mode (dropdown)**: seleccionar cualquier modo
para fijarlo de forma permanente. Seleccionar **Auto** para volver a la
secuencia automática.

### 9.4 Modo SlitScan

`SlitScan` no está en la secuencia automática pero está disponible en el
dropdown. Activarlo fijado en un canal crea un contrapunto temporal útil
frente a los otros modos.

---

## 10. Funcionamiento normal durante la instalación

### Qué esperar en la imagen

La partitura gráfica recorre una secuencia de 18 modos separados por pausas
`BwClean` (imagen en negro fílmico). Cada modo dura un tiempo aleatorio
entre los valores configurados en **Min dur** y **Max dur**. Los modos se
reconocen por su textura:

| Modo | Aspecto visual |
|---|---|
| **BwClean** | Negro con textura fílmica — pausa entre modos |
| **ScanLine** | Líneas horizontales moduladas por el movimiento |
| **BBoxTracker** | Cajas y marcas de esquina sobre los jugadores detectados |
| **BinaryText** | Texto binario denso que codifica posiciones y velocidades |
| **Waveform** | Gráfico de barras del historial de energía de movimiento |
| **GridData** | Cuadrícula de cruces modulada por el movimiento |
| **Barcode** | Barras verticales derivadas de la densidad de primer plano |
| **VideoNormal** | Vídeo en color directo, sin procesado añadido |
| **VideoSquares** | Parches de vídeo en color centrados en los jugadores |
| **VideoNumbers** | Cuadrícula de dígitos 0–9 mapeados al brillo del vídeo |
| **VideoLines** | Dibujo de líneas de contorno + trazos de flujo óptico |
| **ThermalVision** | Mapa de color Ironbow (negro frío → blanco caliente) |
| **SlitScan** | Slit-scan temporal (solo modo manual) |

`ThermalVision` aparece tres veces por ciclo; los demás modos con repetición
funcionan como estribillos de la partitura.

Cada canal puede estar además en modo `VisualComposer`: en ese caso el
`VisualComposer` programa generadores, nube de puntos de vídeo o respiraciones
en lugar de la secuencia de partitura gráfica. El compositor alterna entre
tres momentos: **PulseSystem** (pulsos sincronizados), **BarScanSystem**
(barras de escaneo independientes) e **Intercalation** (régimen libre por canal).

### Qué esperar en el audio

El motor de audio recibe datos CV de los ocho canales vía OSC y los mapea a:

- **Síntesis continua:** drone, ruido, pulso y espacio proporcionales a la
  escena visual activa.
- **Espacialización DBAP:** la posición de cada fuente sonora en la sala
  refleja la posición de los jugadores detectados en el campo.
- **Movimiento colectivo:** los estados Suspension, Accumulation, Convergence,
  Fragmentation, Rupture, etc., reorganizan la densidad y el registro del
  conjunto.

El motor no produce melodía ni armonía. El material es drone, ruido de banda,
impulsos y clics binarios. Las discontinuidades —cortes de modo, colisiones,
eventos de borrado— producen discontinuidades audibles equivalentes.

### Intervenciones durante la instalación

- **Slow Mo / Fast Fwd / Clear Black / Clear Red** — en Global Director.
  Usar con criterio para subrayar momentos del vídeo o crear transiciones.
- **Next moment** — en Visual Composer. Útil para saltar de PulseSystem a
  Intercalation si la pieza necesita más diversidad.
- **Takeover** — todos los canales al mismo generador durante un intervalo.
  Crea unísono dramático.
- **Force noise / Force invert / Force polarity** — eventos de ruptura
  visuales inmediatos.

---

## 11. Secuencia de apagado

```
1. APLICACIÓN VISUAL
   Pulsar Cmd+Q o cerrar la ventana.

2. SUPERCOLLIDER
   En la ventana de Terminal donde corre Start Audio.command:
   Presionar Ctrl+C.
   O desde el IDE: ejecutar ~shutdown.();  (o Cmd+.)
   Esperar la confirmación de shutdown en Post Window.

3. DANTE VIRTUAL SOUNDCARD
   Esperar 5 segundos tras cerrar SuperCollider.
   En la barra de menú del Mac, abrir DVS → Disable.
   NO deshabilitar DVS mientras SuperCollider esté activo.

4. ETHERNET
   Desconectar el cable Ethernet si se va a transportar el equipo.

5. HARDWARE
   Apagar el Mac.
   Apagar las unidades ICUIXIAN.
   Apagar las pantallas.
   Apagar las regletas eléctricas.
```

---

## 12. Diagnóstico de problemas

### Imagen

| Síntoma | Causa probable | Solución |
|---|---|---|
| Las ventanas de presentación no aparecen en las pantallas correctas | Posiciones de ventana desactualizadas | Pulsar **Configure Mixed Wall** en la ControlApp y reiniciar |
| Una ventana aparece en la pantalla integrada del Mac | ICUIXIAN no detectado o no a 1080p60 | Verificar conexión HDMI y modo de pantalla en Preferencias del Sistema |
| Imagen girada o invertida en el Muro A | Rotación incorrecta en ICUIXIAN A | Cambiar rotación de 90° a 270° en ICUIXIAN A |
| Los ocho canales muestran negro | VideoDirector desactivado o clips no encontrados | Verificar la carpeta de clips; activar VideoDirector en la página Video Director |
| Un canal muestra negro constante | Clip no encontrado o canal pausado | En Overview: pulsar **Next** en ese canal |
| Los modos no rotan | Modo forzado activo | En Channel Editor → SCORE → Mode: seleccionar **Auto** |
| Errores de shader al arrancar | Shaders no encontrados | Verificar que la app no está en una carpeta de solo lectura; reinstalar el paquete |

### Audio

| Síntoma | Causa probable | Solución |
|---|---|---|
| No hay audio | SuperCollider no arrancó antes que la app | Detener la app, arrancar `Start Audio.command`, esperar `[3/3]`, relanzar la app |
| Audio estéreo en lugar de 8 canales | DVS no detectado o no habilitado | Abrir app DVS → Enable; verificar cable Ethernet |
| Candado amarillo en Dante Controller | Desajuste de sample rate | Establecer 48000 Hz en DVS y en Audio MIDI Setup |
| Interrupciones de audio (dropouts) | Latencia DVS demasiado baja | Subir latencia a 5–10 ms; reiniciar `Start Audio.command` |
| Altavoz incorrecto en la prueba | Desfase de enrutamiento DANTE | Reordenar suscripciones en Dante Controller |
| El servidor SC se cae al apagar | DVS deshabilitado antes de cerrar SC | Siempre `~shutdown.()` antes de deshabilitar DVS |
| No aparece DANTE en SuperCollider | DVS activo pero sin red DANTE | Verificar que el cable Ethernet está conectado al switch del recinto |

### Hardware

| Síntoma | Causa probable | Solución |
|---|---|---|
| ICUIXIAN no divide la imagen | Modo de mosaico incorrecto | Verificar que ICUIXIAN está en 4×1 (Muro A) o 2×2 (Muro B) |
| ICUIXIAN no recibe señal | HDMI no conectado o resolución incorrecta | Verificar conexión HDMI; macOS debe exponer 1920×1080 a 60 Hz |
| Pantallas parpadeantes | Cables HDMI con interferencia o largos | Usar cables certificados HDMI 2.0; máx. 5 m sin amplificador |

---

## 13. Referencia rápida

### Arranque (orden obligatorio)

```
1. Eléctrico  →  2. Pantallas  →  3. ICUIXIAN
→  4. Mac  →  5. DVS (Enable)
→  6. Start Audio.command  →  esperar [3/3]
→  7. Partitura_del_Juego.app
```

### Apagado (orden obligatorio)

```
1. Cerrar app  →  2. Ctrl+C en Terminal  →  esperar shutdown
→  3. DVS (Disable, tras 5 s)  →  4. Mac
→  5. ICUIXIAN  →  6. Pantallas  →  7. Eléctrico
```

### Teclas durante la instalación

| Tecla | Acción |
|---|---|
| `U` | Abrir/cerrar ControlApp |
| `Cmd+Q` | Cerrar la aplicación |

### Comandos SuperCollider de emergencia

```supercollider
~shutdown.();         // apagado limpio del motor
~testOsc.play;        // OSC sintetico de prueba (sin app visual)
~testOsc.stop;        // detener OSC de prueba
~speakerTest.();      // prueba individual de altavoces
```

### Archivos de configuración

| Archivo | Uso |
|---|---|
| `~/Library/Application Support/PartituraDelJuego/settings.json` | Configuración activa del usuario (ventanas, clips, CV, compositor) |
| `SuperCollider/pdj_datamatics.scd` línea `~speakerPositions` | Coordenadas de altavoces en sala |
| `SuperCollider/pdj_audio_config.scd` línea `blockSize` | Tamaño de bloque del servidor de audio (subir a 512 si hay dropouts) |
