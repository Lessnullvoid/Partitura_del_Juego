# Partitura del Juego — Configuración de Audio Multicanal
## Sistema de 8 canales / DANTE 5 / Sala Abierta

Este documento describe todos los pasos necesarios para conectar SuperCollider
al sistema de bocinas de la Sala Abierta (D3-1 a D3-8, DANTE 5) y arrancar
la instalación correctamente. Incluye un modo de respaldo en estéreo automático
para pruebas sin red DANTE.

> **Paquete de distribución:** si se usa el paquete macOS compilado, el arranque
> de audio se simplifica a doble clic en `Start Audio.command` (diagnóstico previo
> con `Check Audio.command`). Los pasos del § 7 aplican únicamente al IDE de
> SuperCollider en desarrollo. El lanzador ejecuta `pdj_launcher.scd`, que encadena
> automáticamente `pdj_audio_config.scd` y `pdj_datamatics.scd` en el orden correcto.

---

## 1. Requisitos de hardware

- **Mac** con puerto Ethernet Gigabit activo (cable, no WiFi).
  Los adaptadores USB-C o Thunderbolt a Ethernet son aceptables.
- **Cable Ethernet** conectado al mismo switch o segmento de red que la unidad
  DANTE 5 del recinto. No es posible usar WiFi para audio DANTE.
- Conexión a internet para descargar el software de Audinate
  (puede hacerse antes de llegar al recinto).

---

## 2. Software necesario

Instalar ambas aplicaciones **antes** de llegar al recinto:

### 2.1 Dante Virtual Soundcard (DVS)
- Descarga: https://www.audinate.com/products/software/dante-virtual-soundcard
- Requiere licencia de pago (aproximadamente USD 30, uso perpetuo por máquina).
- Comprar y activar la licencia antes de la visita al recinto.
- Compatible con Apple Silicon (M1/M2/M3/M4) desde la versión 4.2 en adelante.
- Al instalarse, crea una interfaz de audio virtual en el Mac que aparece en
  el sistema como `"Dante Virtual Soundcard"`.

### 2.2 Dante Controller
- Descarga: https://www.audinate.com/products/software/dante-controller
- Gratuito. Se usa para configurar el enrutamiento de canales en la red DANTE.
- Puede instalarse en el mismo Mac o en cualquier computadora de la misma red.

---

## 3. Configuración del sistema macOS

Realizar los siguientes ajustes **una sola vez** antes del primer uso:

### 3.1 Permisos de micrófono
```
Configuración del Sistema → Privacidad y Seguridad → Micrófono
→ Activar el permiso para SuperCollider
```
Aunque PDJ no usa entrada de micrófono, el servidor de audio de SuperCollider
requiere este permiso para inicializarse correctamente.

### 3.2 Red
```
Configuración del Sistema → Red
```
Verificar que el adaptador Ethernet muestre una dirección IP cuando esté
conectado al switch del recinto. Si la red usa DHCP, la dirección se asigna
automáticamente. Si usa auto-IP (169.254.x.x), DVS y Dante Controller se
descubren entre sí igualmente vía mDNS.

### 3.3 Firewall
```
Configuración del Sistema → Red → Firewall
```
Si el firewall está activo, agregar **Dante Virtual Soundcard** y
**Dante Controller** a la lista de aplicaciones permitidas.
También es posible desactivar el firewall temporalmente durante la instalación.

### 3.4 Modo de No Molestar y reposo
```
Configuración del Sistema → Pantallas → Protector de pantalla / reposo → Nunca
Configuración del Sistema → Batería → Nunca entrar en reposo cuando está enchufado
```
Habilitar el **Modo No Molestar** para evitar que sonidos del sistema lleguen
a la salida DANTE.

---

## 4. Configuración de Dante Virtual Soundcard

1. Abrir la aplicación **Dante Virtual Soundcard** (ícono en la barra de menú).
2. Configurar los siguientes parámetros:

   | Parámetro             | Valor                                       |
   |-----------------------|---------------------------------------------|
   | Transmit channels     | 8                                           |
   | Receive channels      | 2                                           |
   | Sample rate           | 48000 Hz                                    |
   | Latency               | 1 ms (red local directa, mismo switch)      |
   |                       | 5 ms (si hay switch gestionado entre medio) |

3. Hacer clic en **Enable**.
4. Abrir **Configuración de Audio MIDI** (`/Aplicaciones/Utilidades/Audio MIDI Setup`)
   y verificar que `Dante Virtual Soundcard` aparece con:
   - Formato: **48000.0 Hz**, **8 canales de salida**
   - Si el sample rate no es 48000, cambiarlo manualmente desde esta ventana.

> Si se escuchan interrupciones (dropouts) durante la instalación, aumentar
> la latencia en DVS a 5 ms o 10 ms y el `blockSize` a 512 en
> `supercollider/pdj_audio_config.scd`.

---

## 5. Configuración de Dante Controller (enrutamiento)

1. Abrir **Dante Controller**.
2. Esperar a que aparezcan dos dispositivos en la vista de red:
   - `Dante Virtual Soundcard` (el Mac)
   - El dispositivo DANTE 5 del recinto
   Si no aparecen después de 30 segundos, revisar:
   - Que el cable Ethernet esté conectado y el indicador de enlace esté encendido.
   - Que la unidad DANTE 5 esté encendida.
   - Que el Mac tenga dirección IP en el adaptador Ethernet.

3. En la pestaña **Routing**, crear las siguientes suscripciones
   (fila = receptor, columna = transmisor):

   | Receptor DANTE 5 | Transmisor DVS | Bocinas físicas         |
   |------------------|----------------|--------------------------|
   | D3-1             | DVS Out 1      | Derecha, frente (x2)    |
   | D3-2             | DVS Out 2      | Centro, frente           |
   | D3-3             | DVS Out 3      | Izquierda, frente (x2)  |
   | D3-4             | DVS Out 4      | Pared izquierda, media   |
   | D3-5             | DVS Out 5      | Izquierda, fondo         |
   | D3-6             | DVS Out 6      | Cluster central          |
   | D3-7             | DVS Out 7      | Centro, fondo            |
   | D3-8             | DVS Out 8      | Cluster central (junto a D3-6) |

4. En la pestaña **Device Info** de la unidad DANTE 5:
   - Establecer **Clock Master** en **Yes**.
5. En **Device Info** de Dante Virtual Soundcard:
   - Verificar que muestra **Sync to External** (no debe ser master).
6. Ambos dispositivos deben mostrar un **candado verde** en Dante Controller.
   Un candado amarillo o rojo indica desajuste de reloj — revisar que DVS
   esté configurado a 48000 Hz y que Audio MIDI Setup también muestre 48000.

> Esta configuración de enrutamiento se guarda en la red DANTE y no necesita
> repetirse en cada sesión, solo al cambiar de computadora o reconfigurarse
> el sistema.

---

## 6. Verificar el nombre del dispositivo en SuperCollider

Antes de la primera sesión, ejecutar esta línea en el IDE de SuperCollider
(Post Window) para ver el nombre exacto del dispositivo DVS:

```supercollider
ServerOptions.devices.do({ |d| d.postln });
```

Buscar la línea que contiene "Dante". El archivo `pdj_audio_config.scd`
detecta el dispositivo automáticamente con una búsqueda sin distinción de
mayúsculas, pero conviene anotar el nombre exacto para diagnóstico.

---

## 7. Secuencia de arranque (cada sesión)

Seguir este orden **exacto** en cada inicio de la instalación:

```
Paso 1. Conectar el cable Ethernet al switch del recinto.
        Verificar que DVS esté habilitado (ícono en la barra de menú activo,
        no en gris).

Paso 2. Abrir SuperCollider IDE.
        NO iniciar el servidor manualmente (no presionar Cmd+B, no usar
        Language → Boot Server).

Paso 3. Abrir y evaluar pdj_audio_config.scd
        (seleccionar todo el bloque → Cmd+Return).

        Esperar en la ventana Post uno de estos mensajes:

        "PDJ audio: DANTE detected [...] -> 8ch @ 48kHz"
            El sistema operará en modo 8 canales multicanal.

        "PDJ audio: DANTE not found -> stereo fallback on default device"
            DANTE no está disponible; el sistema operará en estéreo.
            Revisar DVS y la conexión Ethernet si se esperaba el modo DANTE.

        El servidor se reinicia automáticamente dentro de este bloque.
        Esperar hasta ver "Server ready" en la ventana Post.

Paso 4. Abrir y evaluar pdj_datamatics.scd
        (seleccionar todo → Cmd+Return).

        Esperar el mensaje:
        "PDJ Datamatics — listening OSC UDP :9001"

Paso 5. Lanzar la aplicación de openFrameworks (Partitura_del_Juego).

Paso 6. Opcional: ejecutar ~testOsc.play; en SuperCollider para
        verificar el audio sin la aplicación de oF.
```

---

## 8. Prueba de bocinas

Para verificar que cada bocina física corresponde al canal DANTE correcto,
ejecutar esta rutina en SuperCollider (con `~testOsc.play` activo o con
oF en ejecución):

```supercollider
~speakerTest.();
```

La rutina activa cada bocina durante 2 segundos en orden (D3-1 a D3-8)
e imprime el número de bocina en la ventana Post. Caminar por la Sala Abierta
y confirmar que cada bocina suena en el orden correcto.

Si una bocina suena en el paso incorrecto, hay dos opciones:
- Corregir el enrutamiento en Dante Controller (cambiar qué DVS Out va a qué D3-x).
- Reordenar las posiciones en `~speakerPositions` dentro de `pdj_datamatics.scd`
  para que el índice corresponda a la posición física correcta.

---

## 9. Posiciones de bocinas — calibración en sitio

Las posiciones iniciales están derivadas del plano arquitectónico. Una vez
en el recinto, medirlas físicamente (en metros) y normalizarlas dividiendo
entre el ancho y la profundidad de la sala. Actualizar en `pdj_datamatics.scd`:

```supercollider
// Coordenadas [x, y] normalizadas 0-1
// x: izquierda (0) -> derecha (1)
// y: frente (0) -> fondo (1)
// Indices: 0=D3-1, 1=D3-2, 2=D3-3, 3=D3-4, 4=D3-5, 5=D3-6, 6=D3-7, 7=D3-8
~speakerPositions = [
    [0.82, 0.12],  // D3-1: derecha, frente (2 bocinas fisicas)
    [0.50, 0.08],  // D3-2: centro, frente
    [0.14, 0.16],  // D3-3: izquierda, frente (2 bocinas fisicas)
    [0.04, 0.50],  // D3-4: pared izquierda, profundidad media
    [0.10, 0.84],  // D3-5: izquierda, fondo
    [0.40, 0.58],  // D3-6: cluster central
    [0.56, 0.74],  // D3-7: centro, fondo
    [0.44, 0.54]   // D3-8: cluster central (cercano a D3-6)
];
```

Después de actualizar las posiciones, ejecutar `~speakerTest.()` nuevamente
para verificar el comportamiento del algoritmo DBAP con las nuevas coordenadas.

> Nota: D3-6 y D3-8 están físicamente muy cerca. Si el DBAP no les asigna
> cobertura distinta, separar sus coordenadas Y unos 0.05 puntos para que
> el algoritmo las trate como zonas independientes.

---

## 10. Mapeado de datos a espacio (referencia)

El algoritmo DBAP convierte datos visuales en tiempo real a ganancias por bocina:

| Dato visual              | Efecto espacial                                      |
|--------------------------|------------------------------------------------------|
| Centroide X,Y del blob   | Posición de la fuente sonora en el plano de la sala  |
| Cantidad de blobs        | Dispersión: pocos = foco puntual; muchos = difuso    |
| Magnitud de flujo óptico | Rolloff: escena quieta = foco; movimiento = halo     |
| Convergencia colectiva   | Colapsa hacia D3-2 (centro frente)                   |
| Fragmentación colectiva  | Dispersión máxima, explota el cluster D3-6/D3-8      |
| Propagación colectiva    | Barre la fuente X de izquierda a derecha en el tiempo|
| Alpha de clear           | Reduce todas las ganancias proporcionalmente          |

En modo estéreo de respaldo (sin DANTE), el mismo algoritmo opera con dos
posiciones virtuales `[0, 0.5]` y `[1, 0.5]`, produciendo un paneo estéreo
estándar desde el X del blob — sin cambios de código requeridos.

---

## 11. Secuencia de apagado

```
Paso 1. Detener SuperCollider primero:
        Ejecutar  ~shutdown.();  en SC, o presionar Cmd+.

Paso 2. Esperar a que la ventana Post confirme el shutdown.

Paso 3. Cerrar la aplicación de openFrameworks.

Paso 4. Esperar 5 segundos, luego deshabilitar DVS si es necesario.

IMPORTANTE: no deshabilitar DVS ni desconectar el Ethernet
            mientras el servidor de SuperCollider esté activo.
            Esto provoca una caída del servidor.
```

---

## 12. Diagnóstico de problemas frecuentes

| Síntoma | Causa probable | Solución |
|---|---|---|
| No aparece dispositivo DANTE en SC | DVS no está activo o habilitado | Abrir la app DVS y hacer clic en Enable |
| Candado amarillo en Dante Controller | DVS no está a 48000 Hz | Cambiar sample rate en DVS y en Audio MIDI Setup |
| Interrupciones de audio (dropouts) | Latencia DVS muy baja o blockSize pequeño | Subir latencia DVS a 5–10 ms; subir `blockSize` a 512 en `pdj_audio_config.scd` |
| Audio en SC pero no salen las bocinas | Enrutamiento en Dante Controller incompleto, o DANTE 5 sin alimentación | Verificar suscripciones en Dante Controller; confirmar que DANTE 5 está encendido |
| Caída del servidor SC al salir | DVS deshabilitado antes de cerrar SC | Siempre detener SC primero con `~shutdown.()` |
| Bocina incorrecta suena en la prueba | Desfase entre canal DANTE y posición física | Reordenar suscripciones en Dante Controller o actualizar `~speakerPositions` |
| Sistema arranca en estéreo inesperadamente | Mac no conectado a la red DANTE | Revisar cable Ethernet; confirmar IP del adaptador en Configuración del Sistema |
| El servidor SC no arranca | `pdj_audio_config.scd` no fue evaluado primero | Seguir la secuencia de arranque exacta del paso 7 |
