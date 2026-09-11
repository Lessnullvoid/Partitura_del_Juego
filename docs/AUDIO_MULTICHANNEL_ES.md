# Partitura del Juego — Audio Multicanal
## Motor SuperCollider · DANTE 8 canales · Sala Abierta

Este documento cubre la configuración completa de audio: el motor SuperCollider,
el sistema espacial DBAP, la conexión a la red DANTE 5 de la Sala Abierta y los
procedimientos de arranque, prueba y apagado. Incluye rescate automático en estéreo
para pruebas sin red DANTE.

---

## 1. Estructura del motor de audio

El motor comprende cuatro archivos en `supercollider/`:

| Archivo | Función |
|---|---|
| `pdj_launcher.scd` | Punto de entrada del paquete de distribución. Encadena `pdj_audio_config.scd` y `pdj_datamatics.scd`, imprime un informe de configuración completo y lanza un monitor de salud periódico cada 2 minutos. |
| `pdj_audio_config.scd` | Detecta Dante Virtual Soundcard. DANTE presente → 8 canales de salida a 48 kHz; ausente → rescate estéreo en dispositivo por defecto. Reinicia el servidor con las opciones correctas. **Evaluar siempre antes de `pdj_datamatics.scd`.** |
| `pdj_datamatics.scd` | Motor principal: buses de control, SynthDefs, motor espacial DBAP, conductor de datos (4 Hz), secuenciador binario en cuadrícula, manejadores OSC, vigilancia de telemetría. Carga `pdj_mode_voices.scd` automáticamente. |
| `pdj_mode_voices.scd` | Un SynthDef por cada modo de `GraphicScore` (13 modos + `pdjKick`). Todos leen los mismos buses de control y usan la misma envoltura de vida para crossfades suaves. |

### Señal OSC recibida desde openFrameworks

El motor escucha en el puerto UDP **9001**. Para cada canal 0–7:

```
/pdj/channel/{N}/motion/energy         — diferencia absoluta media de fotograma
/pdj/channel/{N}/flow/magnitude|angle  — magnitud y dirección del flujo óptico
/pdj/channel/{N}/blobs/count           — número de blobs activos
/pdj/channel/{N}/blob/{0-7}/state      — posición, velocidad y área por blob
/pdj/channel/{N}/contour/length        — longitud total de contornos
/pdj/channel/{N}/event/collision|ball|crowd|leg_distance
/pdj/channel/{N}/score/mode|revision   — modo activo de GraphicScore
/pdj/channel/{N}/generator/mode|stage|organization|beat_phase|...
/pdj/channel/{N}/director/temporal|speed|clear|clear_alpha
/pdj/channel/{N}/program/moment|takeover|group_video_count
/pdj/clock/state                        — BPM y fase de pulso compartido
```

### Perfiles de escena sonora

El motor define dos tablas de perfiles que asignan la escena visual a parámetros
de síntesis. Los valores son puntos base; el CV en vivo se mueve *dentro* de la
escena, no la reemplaza.

**`~modeProfiles`** — una entrada por modo de `GraphicScore` (0–13):
cada perfil fija `voice` (familia de drone 0–3), `tension`, `drone`, `air`,
`pulse`, `color`, `space` y `crush`. Ejemplo: `thermal` → drone alto (0.88),
espacio abierto (0.88), tensión baja (0.20) — inmóvil y caliente.

**`~generatorProfiles`** — una entrada por modo de `VisualGenerator` (0–14):
cada perfil fija `family` (`pulse`, `noise`, `data`, `temporal` o `rupture`),
`legacyMode` (voz cinematográfica de respaldo) y los mismos parámetros de CV.

### Frecuencias base por canal

Cada canal tiene su propia fundamental (`~channelBases`) derivada de
proporciones de entonación justa. No hay octavas ni dobles — cada canal
es espectralmente distinto:

```supercollider
~channelBases = [31.7, 33.1, 35.9, 38.3, 41.1, 43.7, 47.3, 50.9];
//               ch0   ch1   ch2   ch3   ch4   ch5   ch6   ch7
```

### Movimiento colectivo

`VisualComposer` (openFrameworks) infiere un estado de `CollectiveMovement`
de los ocho flujos de datos CV y lo envía como `/pdj/channel/{N}/program/...`.
El motor de audio lo lee en `~collective` y lo usa para reorganizar densidad,
sincronización, registro, espacio y silencio. Los nueve estados son:

```
suspension  codification  accumulation  propagation
convergence  fragmentation  saturation  rupture  residue
```

Un candidato se adopta solo si permanece estable `collectiveDecisionHold`
segundos. Cada estado tiene permanencia mínima protegida (`collectiveMinimumDwell`).

---

## 2. Motor espacial DBAP

Distance-Based Amplitude Panning distribuye las fuentes sonoras de los
ocho canales sobre los ocho altavoces físicos en tiempo real.

```supercollider
// srcX, srcY : posicion de la fuente en coordenadas de sala (0-1)
// rolloff    : exponente de distancia — 1.4 = foco moderado; 2.0 = nitido
// spread     : 0 = fuente puntual; 1 = todos los altavoces iguales
~dbapGains = { |srcX, srcY, rolloff = 1.4, spread = 0.0| ... }
```

| Dato visual de oF | Efecto espacial |
|---|---|
| Centroide X,Y del blob | Posición de la fuente en el plano de la sala |
| Cantidad de blobs | Dispersión: pocos = foco puntual; muchos = difuso |
| Magnitud de flujo óptico | Rolloff: quietud = foco nítido; movimiento = halo |
| `convergence` colectivo | Colapsa todas las fuentes hacia D3-2 (centro frente) |
| `fragmentation` colectivo | Dispersión máxima, explota el cluster D3-6/D3-8 |
| `propagation` colectivo | Barre la fuente X de izquierda a derecha en el tiempo |
| Alpha de clear del director | Reduce todas las ganancias proporcionalmente |

En rescate estéreo (sin DANTE), el mismo algoritmo opera con dos posiciones
virtuales `[0, 0.5]` y `[1, 0.5]`, produciendo paneo estéreo estándar. Sin
cambios de código.

---

## 3. Requisitos de hardware (DANTE)

- **Mac** con puerto Ethernet Gigabit activo (cable, no WiFi).
  Los adaptadores USB-C o Thunderbolt a Ethernet son aceptables.
- **Cable Ethernet** al mismo switch que la unidad DANTE 5 del recinto.
- Conexión a internet para descargar el software de Audinate
  (instalar antes de llegar al recinto).

---

## 4. Software necesario

Instalar **antes** de llegar al recinto:

### Dante Virtual Soundcard (DVS)
- Descarga: https://www.audinate.com/products/software/dante-virtual-soundcard
- Licencia de pago (aprox. USD 30, perpetua por máquina). Activar antes de la visita.
- Compatible con Apple Silicon desde la versión 4.2 en adelante.
- Crea la interfaz de audio virtual `"Dante Virtual Soundcard"` en el sistema.

### Dante Controller
- Descarga: https://www.audinate.com/products/software/dante-controller
- Gratuito. Configura el enrutamiento de canales en la red DANTE.
- Se puede instalar en el mismo Mac o en cualquier equipo de la misma red.

---

## 5. Configuración de macOS (una sola vez)

### Permiso de micrófono
```
Configuración del Sistema → Privacidad y Seguridad → Micrófono
→ Activar para SuperCollider
```
SuperCollider requiere este permiso aunque PDJ no use entrada de micrófono.

### Red
Verificar que el adaptador Ethernet muestre dirección IP al conectarse al switch
del recinto. DHCP la asigna automáticamente. Con auto-IP (169.254.x.x), DVS y
Dante Controller se descubren igualmente vía mDNS.

### Firewall
Agregar **Dante Virtual Soundcard** y **Dante Controller** a la lista de
aplicaciones permitidas, o desactivar el firewall temporalmente.

### Reposo y notificaciones
```
Configuración del Sistema → Batería → Nunca entrar en reposo cuando está enchufado
Configuración del Sistema → Pantallas → Protector de pantalla → Nunca
```
Activar **Modo No Molestar** para evitar que sonidos del sistema lleguen a la
salida DANTE.

---

## 6. Configuración de Dante Virtual Soundcard

1. Abrir la app **Dante Virtual Soundcard** (ícono en la barra de menú).
2. Configurar:

   | Parámetro         | Valor                                          |
   |-------------------|------------------------------------------------|
   | Transmit channels | 8                                              |
   | Receive channels  | 2                                              |
   | Sample rate       | 48000 Hz                                       |
   | Latency           | 1 ms (mismo switch) · 5 ms (switch gestionado) |

3. Hacer clic en **Enable**.
4. Abrir **Configuración de Audio MIDI** (`/Aplicaciones/Utilidades/Audio MIDI Setup`)
   y confirmar que `Dante Virtual Soundcard` muestra **48000.0 Hz · 8 canales de salida**.
   Si el sample rate no es 48000, cambiarlo manualmente.

> Si hay interrupciones (dropouts) durante la instalación, subir latencia DVS
> a 5–10 ms y el `blockSize` a 512 en `supercollider/pdj_audio_config.scd`.

---

## 7. Enrutamiento en Dante Controller

1. Abrir **Dante Controller**.
2. Esperar a que aparezcan en la vista de red:
   - `Dante Virtual Soundcard` (el Mac)
   - El dispositivo DANTE 5 del recinto
   Si no aparecen en 30 segundos: verificar cable Ethernet, encendido de DANTE 5
   y dirección IP del adaptador en Configuración del Sistema.

3. En la pestaña **Routing** crear las suscripciones (receptor ← transmisor):

   | Receptor DANTE 5 | Transmisor DVS | Altavoces físicos              |
   |------------------|----------------|-------------------------------|
   | D3-1             | DVS Out 1      | Derecha, frente (2 unidades)  |
   | D3-2             | DVS Out 2      | Centro, frente                 |
   | D3-3             | DVS Out 3      | Izquierda, frente (2 unidades)|
   | D3-4             | DVS Out 4      | Muro izquierdo, media prof.   |
   | D3-5             | DVS Out 5      | Izquierda, fondo              |
   | D3-6             | DVS Out 6      | Agrupación central             |
   | D3-7             | DVS Out 7      | Centro, fondo                 |
   | D3-8             | DVS Out 8      | Agrupación central (D3-6)     |

4. En **Device Info** de DANTE 5: establecer **Clock Master = Yes**.
5. En **Device Info** de DVS: confirmar **Sync to External**.
6. Ambos dispositivos deben mostrar **candado verde**. Candado amarillo o rojo
   indica desajuste de reloj — revisar que DVS y Audio MIDI Setup estén a 48000 Hz.

> Esta configuración se guarda en la red DANTE. No es necesario repetirla en
> cada sesión; solo al cambiar de ordenador o reconfigurar el sistema.

---

## 8. Arranque (paquete de distribución)

El paquete incluye `Start Audio.command`. Doble clic para arrancar.

```
[1/3] Detectando dispositivos de audio (pdj_audio_config.scd)
      -> "PDJ audio: DANTE detected [...] -> 8ch @ 48kHz"
         o "PDJ audio: DANTE not found -> stereo fallback on default device"
[2/3] Servidor de audio listo — imprime informe completo:
      modo DANTE, dispositivo, canales, sample rate, posiciones de altavoces
[3/3] Cargando motor (pdj_datamatics.scd)
      -> "PDJ Datamatics — listening OSC UDP :9001"
```

Dejar la ventana de Terminal abierta durante toda la instalación.
Cada 2 minutos imprime una línea de salud:
```
[PDJ health] server:running  mode:DANTE  speakers:8  channels:8
```

Después lanzar `Partitura_del_Juego.app`.

---

## 9. Arranque desde el IDE de SuperCollider (solo desarrollo)

**No presionar Boot Server (Cmd+B) en ningún momento.**
`pdj_audio_config.scd` arranca el servidor con el dispositivo y canal count correctos.

```
Paso 1. Conectar cable Ethernet al switch del recinto.
        Verificar que DVS esté habilitado (ícono activo en la barra de menú).

Paso 2. Abrir SuperCollider IDE.

Paso 3. Evaluar pdj_audio_config.scd
        (seleccionar todo → Cmd+Return)
        Esperar uno de estos mensajes en Post Window:

        "PDJ audio: DANTE detected [...] -> 8ch @ 48kHz"
            Sistema operará en modo 8 canales.

        "PDJ audio: DANTE not found -> stereo fallback on default device"
            Sistema operará en estéreo. Revisar DVS y Ethernet si se
            esperaba DANTE.

        Esperar hasta ver "Server ready".

Paso 4. Evaluar pdj_datamatics.scd
        (seleccionar todo → Cmd+Return)
        Esperar: "PDJ Datamatics — listening OSC UDP :9001"

Paso 5. Lanzar Partitura_del_Juego.

Paso 6. Opcional — verificar audio sin oF:
        ~testOsc.play;     // genera OSC sintético de prueba
        ~testOsc.stop;     // detener el OSC de prueba
```

---

## 10. Prueba de altavoces

Ejecutar con `~testOsc.play` activo o con oF en ejecución:

```supercollider
~speakerTest.();
```

La rutina activa cada altavoz 2 segundos en orden (D3-1 a D3-8) e imprime el
número en Post Window. Recorrer la sala y confirmar que cada altavoz físico
suena en el paso correcto.

Si un altavoz suena en el paso incorrecto, hay dos opciones:
- Reordenar las suscripciones en Dante Controller (DVS Out N → D3-M).
- Actualizar `~speakerPositions` en `pdj_datamatics.scd` para que el índice
  corresponda a la posición física correcta.

---

## 11. Calibración de posiciones de altavoces en sitio

Las posiciones iniciales están derivadas del plano de la Sala Abierta.
Medirlas físicamente y normalizarlas (dividir entre ancho y profundidad de sala):

```supercollider
// Coordenadas [x, y] normalizadas 0-1
// x: izquierda (0) -> derecha (1)
// y: frente (0) -> fondo (1)
// Indices: 0=D3-1, 1=D3-2, ..., 7=D3-8
~speakerPositions = [
    [0.82, 0.12],  // D3-1: derecha, frente (2 unidades fisicas)
    [0.50, 0.08],  // D3-2: centro, frente
    [0.14, 0.16],  // D3-3: izquierda, frente (2 unidades fisicas)
    [0.04, 0.50],  // D3-4: muro izquierdo, media profundidad
    [0.10, 0.84],  // D3-5: izquierda, fondo
    [0.40, 0.58],  // D3-6: agrupacion central
    [0.56, 0.74],  // D3-7: centro trasero
    [0.44, 0.54]   // D3-8: agrupacion central (cerca de D3-6)
];
```

Ejecutar `~speakerTest.()` después de actualizar para verificar el comportamiento
del DBAP con las nuevas coordenadas.

> D3-6 y D3-8 están físicamente muy cerca. Si el DBAP no les asigna cobertura
> distinta, separar sus coordenadas Y unos 0.05 puntos.

---

## 12. Apagado

```
Paso 1. Detener SuperCollider:
        ~shutdown.();     (o Cmd+.)

Paso 2. Esperar confirmación de shutdown en Post Window.

Paso 3. Cerrar Partitura_del_Juego.

Paso 4. Esperar 5 segundos antes de deshabilitar DVS o desconectar Ethernet.

IMPORTANTE: nunca deshabilitar DVS ni desconectar Ethernet mientras el
            servidor de SuperCollider esté activo. Provoca caída del servidor.
```

---

## 13. Diagnóstico de problemas frecuentes

| Síntoma | Causa probable | Solución |
|---|---|---|
| No aparece dispositivo DANTE en SC | DVS no está habilitado | Abrir app DVS → Enable |
| Candado amarillo en Dante Controller | Desajuste de sample rate | Establecer 48000 Hz en DVS y Audio MIDI Setup |
| Interrupciones de audio (dropouts) | Latencia DVS muy baja | Subir latencia a 5–10 ms; subir `blockSize` a 512 en `pdj_audio_config.scd` |
| Audio en SC pero no salen altavoces | Enrutamiento incompleto o DANTE 5 apagado | Verificar suscripciones en Dante Controller; confirmar encendido de DANTE 5 |
| Caída del servidor SC al salir | DVS deshabilitado antes de cerrar SC | Detener siempre SC con `~shutdown.()` primero |
| Altavoz incorrecto en la prueba | Desfase entre canal DANTE y posición física | Reordenar suscripciones en Dante Controller o actualizar `~speakerPositions` |
| Sistema arranca en estéreo inesperadamente | Mac no conectado a la red DANTE | Verificar cable Ethernet e IP del adaptador |
| Servidor SC no arranca | `pdj_audio_config.scd` no evaluado primero | Seguir secuencia del § 9 exactamente |
