# Partitura del Juego — Paquete macOS arm64

Aplicación compilada para Mac Apple Silicon. No requiere openFrameworks instalado.

## Contenido del paquete

```
Partitura_del_Juego-macOS-arm64/
├── Partitura_del_Juego.app   — aplicación visual compilada (ad-hoc signed)
├── Start Audio.command       — arranca el motor SuperCollider con doble clic
├── README.md                 — este archivo
├── SuperCollider/
│   ├── pdj_launcher.scd      — punto de entrada: encadena audio_config + datamatics
│   ├── pdj_audio_config.scd  — detección DANTE, configura el servidor
│   ├── pdj_datamatics.scd    — motor principal: síntesis, DBAP, OSC, buses
│   ├── pdj_mode_voices.scd   — voces por modo de partitura gráfica
│   └── pdj_volumetric_compat.scd — compatibilidad OSC con runtime volumétrico
└── Videos/
    ├── Portrait/             — clips en formato retrato (canales 0-3)
    └── Horizontal/           — clips en formato horizontal (canales 4-7)
```

## Requisitos

- Mac Apple Silicon (M1 o posterior)
- macOS 11 o posterior
- SuperCollider instalado en `/Aplicaciones` o `~/Aplicaciones`

El paquete está firmado ad-hoc pero no notarizado por Apple.

---

## Primera ejecución

1. Descomprimir el ZIP y mover la carpeta a un lugar con permisos de escritura
   (por ejemplo, el Escritorio).
2. Control-clic sobre `Partitura_del_Juego.app` → **Abrir** → confirmar **Abrir**.
3. Autorizar cualquier permiso de cámara o micrófono solicitado.

Si macOS bloquea la aplicación después de abrirla:

```sh
xattr -dr com.apple.quarantine "/ruta/a/Partitura_del_Juego.app"
```

Después control-clic → **Abrir** nuevamente.

La configuración de visualización se guarda fuera del bundle en:

```
~/Library/Application Support/PartituraDelJuego/settings.json
```

Se crea desde los valores predeterminados en el primer arranque. Para restablecer
todos los ajustes, cerrar la app, eliminar ese archivo y volver a abrirla.

---

## Layout del paquete de prueba

El paquete arranca en modo `dualWindow8`: dos ventanas de presentación de
1920×1080 px cada una.

- **Ventana A** — canales 0–3 en cuatro tiras verticales (layout 4×1, retrato).
- **Ventana B** — canales 4–7 en mosaico 2×2 (layout 2×2, horizontal).
- **Ventana de control** — ControlApp accesible con la tecla `U`.

Para pruebas en portátil sin monitores externos, editar
`~/Library/Application Support/PartituraDelJuego/settings.json` y cambiar
`"outputMode"` a `"singleWindow"`. Esto muestra los ocho canales comprimidos
en una sola ventana.

---

## Agregar vídeos

Los clips incluidos están en las dos carpetas junto a la app:

- `Videos/Portrait` — canales 0–3 (clips 9:16 en vertical).
- `Videos/Horizontal` — canales 4–7 (clips 16:9 en horizontal).

Para agregar material: cerrar la app, copiar archivos `.mp4`, `.mov` o `.avi`
directamente en la carpeta correspondiente y volver a abrir. La carpeta completa
se escanea en cada arranque; no se requiere importación ni cambio de ajustes.

Mantener la app, `Videos` y `SuperCollider` juntos dentro de la carpeta extraída.

---

## Audio — arranque con Start Audio.command

1. Instalar SuperCollider desde https://supercollider.github.io si aún no está.
2. Doble clic en `Start Audio.command`.
   Si macOS lo bloquea: control-clic → **Abrir** → confirmar.
3. Esperar a que la Terminal imprima el informe de configuración y el mensaje
   final `PDJ Datamatics — listening OSC UDP :9001`.
4. Lanzar `Partitura_del_Juego.app`.

El lanzador ejecuta tres pasos automáticamente:

```
[1/3] Detectando dispositivos de audio
      -> "PDJ audio: DANTE detected [...] -> 8ch @ 48kHz"
         o "PDJ audio: DANTE not found -> stereo fallback"
[2/3] Servidor listo — imprime dispositivo, canales, sample rate y altavoces
[3/3] Motor cargado
      -> "PDJ Datamatics — listening OSC UDP :9001"
```

Dejar la Terminal abierta durante toda la instalación. Cada 2 minutos imprime
una línea de salud: `[PDJ health] server:running  mode:DANTE  speakers:8  channels:8`.

### Modo DANTE de 8 canales

Cuando Dante Virtual Soundcard (DVS) está activo en la red, el motor usa
automáticamente 8 canales de salida enrutados por DANTE 5 (D3-1 a D3-8).
Las ganancias por altavoz se calculan en tiempo real mediante el algoritmo
DBAP (Distance-Based Amplitude Panning) impulsado por las posiciones de los
blobs detectados, el flujo óptico y los estados colectivos del VisualComposer.

Para habilitar DANTE:

1. Instalar y licenciar **Dante Virtual Soundcard** (audinate.com, aprox. USD 30).
2. Abrir la app DVS en la barra de menú:
   TX channels = 8, RX = 2, sample rate = 48000 Hz, latency = 1 ms → **Enable**.
3. Conectar el Mac al switch Ethernet del recinto (mismo switch que DANTE 5).
4. Abrir **Dante Controller** y enrutar DVS Out 1–8 a D3-1 hasta D3-8.
   Establecer DANTE 5 como clock master. Confirmar candado verde en ambos dispositivos.
5. Lanzar `Start Audio.command`.

Ver `docs/AUDIO_MULTICHANNEL_ES.md` para la configuración completa de DANTE,
enrutamiento de sala, motor DBAP y calibración de altavoces.

### Rescate estéreo automático

Si DVS no se detecta, el motor arranca en estéreo sobre la salida por defecto
de macOS. Toda la lógica OSC y de modos es idéntica; solo cambia el número de
canales (2 en lugar de 8). No se requiere ningún cambio de código.

### Arranque desde el IDE de SuperCollider (solo desarrollo)

```
1. Evaluar SuperCollider/pdj_audio_config.scd  (Cmd+Return)
   Esperar "Server ready" en Post Window.
2. Evaluar SuperCollider/pdj_datamatics.scd    (Cmd+Return)
   Esperar "PDJ Datamatics — listening OSC UDP :9001".
```

No presionar Boot Server (Cmd+B) manualmente; `pdj_audio_config.scd` arranca
el servidor con el dispositivo y el número de canales correctos.

---

## Prueba básica de funcionamiento

Confirmar lo siguiente tras el arranque completo:

- La app abre sin requerir openFrameworks ni el repositorio fuente.
- Aparecen las dos ventanas de presentación y la ventana de control.
- Los ocho canales reproducen vídeo.
- La ControlApp (tecla `U`) muestra 48 clips disponibles.
- Los shaders B&W y los modos visuales se renderizan sin errores.
- Los modos de partitura gráfica rotan automáticamente.
- Con SuperCollider en marcha, la actividad OSC produce audio.

---

## Prueba de rendimiento

Abrir la página **Performance** en la ventana de control → **Start 10-minute
stress test**. Mantener las ventanas de presentación visibles y no mover
ventanas ni lanzar otras apps durante la prueba.

La prueba ejerce vídeo normal, los modos costosos (`VideoLines`, `VideoNumbers`),
slit-scan con cambios de clip y los generadores procedimentales. El resultado
de producción requiere 30 FPS estables sin fotogramas superiores a 100 ms.

Al completarse: PASS o FAIL con los umbrales incumplidos. Los informes JSON y
CSV se guardan en `~/Documents/PartituraDelJuego/performance_reports`.

> El layout del paquete (una o dos ventanas en el mismo Mac) es útil como
> prueba básica. El ordenador de instalación debe probarse también con ambas
> salidas ICUIXIAN conectadas; solo esa ejecución representa la carga real.

---

## Resolución de problemas

### No hay vídeos
Confirmar que los archivos están directamente dentro de `Videos/Portrait` o
`Videos/Horizontal`. Las subcarpetas anidadas no se escanean.

### No hay audio
- Arrancar `Start Audio.command` antes de la app visual.
- Esperar el paso `[3/3]` y el mensaje `listening OSC UDP :9001` en la Terminal.
- Verificar que ningún otro proceso ocupa el puerto 9001.

### Solo estéreo cuando se espera DANTE
- Verificar que DVS está habilitado (ícono activo en barra de menú).
- Confirmar que el cable Ethernet está conectado y el adaptador tiene dirección IP.
- Si DVS está a muestra tasa incorrecta: establecer 48000 Hz en DVS y en
  Audio MIDI Setup, luego volver a habilitar DVS y relanzar `Start Audio.command`.
- Si Dante Controller muestra candado amarillo/rojo: desajuste de reloj entre
  DVS y DANTE 5. Establecer ambos a 48000 Hz.

### Altavoz incorrecto en la prueba
Desde el IDE de SuperCollider:
```supercollider
~speakerTest.();
```
Recorrer la sala y anotar qué altavoz físico suena en cada paso. Corregir
las suscripciones en Dante Controller o actualizar `~speakerPositions` en
`SuperCollider/pdj_datamatics.scd`.

### Layout completo de instalación
Configurar la geometría de visualización desde la ControlApp (tecla `U`) o
editando `settings.json` en `~/Library/Application Support/PartituraDelJuego/`.
Los cambios se guardan en esa ubicación y no modifican el `.app` firmado.
