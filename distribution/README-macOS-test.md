# Partitura del Juego — Paquete macOS (arm64)

Este paquete es para pruebas y distribución directa en Mac Apple Silicon. Contiene la aplicación visual, sus shaders, la biblioteca de clips y los archivos de SuperCollider con los scripts de arranque de audio listos para doble clic. No requiere openFrameworks instalado.

## Requisitos

- Mac Apple Silicon (M1 o posterior)
- macOS 11 o posterior
- SuperCollider, solo si se requiere audio

El paquete está firmado ad-hoc pero no notarizado por Apple. Está destinado a pruebas privadas y distribución controlada, no a distribución pública.

---

## Primera ejecución

1. Copiar el ZIP al Mac de prueba y descomprimirlo.
2. Mover la carpeta extraída a un lugar con permisos de escritura, como el Escritorio.
3. Control-clic sobre `Partitura_del_Juego.app`, elegir **Abrir** y confirmar **Abrir** en el diálogo de macOS.
4. Autorizar cualquier permiso solicitado (cámara o micrófono).

Si macOS sigue bloqueando la aplicación, abrir Terminal y ejecutar:

```sh
xattr -dr com.apple.quarantine "/ruta/a/Partitura_del_Juego.app"
```

Después control-clic sobre la app y elegir **Abrir** nuevamente.

El paquete arranca en una disposición de ventana única 1920×1080. Muestra los cuatro canales uno al lado del otro, lo que permite probar el programa sin la instalación completa de varios monitores.

La configuración de visualización se guarda fuera del bundle firmado en:

`~/Library/Application Support/PartituraDelJuego/settings.json`

Este archivo se crea a partir de los valores predeterminados del paquete en el primer arranque. Para restablecer todos los ajustes, cerrar la aplicación, eliminar ese archivo y volver a abrirla.

---

## Agregar vídeos

Los vídeos actuales están incluidos en dos carpetas modificables junto a la app:

- `Videos/Portrait` — suministra los canales 0–3 (clips en formato retrato).
- `Videos/Horizontal` — suministra los canales 4–7 (clips en formato horizontal).

Para agregar material, cerrar la aplicación visual, copiar archivos `.mp4`, `.mov` o `.avi` directamente en la carpeta correspondiente y volver a abrir la app. La carpeta completa se escanea en cada arranque; no se requiere importación ni cambio de ajustes. Mantener la app, `Videos` y `SuperCollider` juntos dentro de la carpeta del paquete extraído.

---

## Audio con SuperCollider

1. Instalar SuperCollider en `/Aplicaciones`.
2. **Opcional — verificar configuración de audio primero:** doble clic en `Check Audio.command`.
   Escanea el sistema en busca de Dante Virtual Soundcard (DVS), lista dispositivos de audio, verifica conectividad de red e imprime un informe de estado en texto plano sin arrancar el motor. Usar para diagnosticar la configuración DANTE antes de la función.
3. Doble clic en `Start Audio.command`. Si macOS lo bloquea, control-clic, elegir **Abrir** y confirmar.
4. Esperar a que el servidor de audio arranque. La ventana de Terminal imprimirá un informe de configuración con el dispositivo detectado, número de altavoces, tasa de muestreo y coordenadas de sala de cada altavoz. Después lanzar `Partitura_del_Juego.app`.

El lanzador ejecuta una secuencia de arranque de tres pasos automáticamente:

```
[1/3] Detectando dispositivos de audio (pdj_audio_config.scd)
      -> imprime "PDJ audio: DANTE detected [...] -> 8ch @ 48kHz"
         o "PDJ audio: DANTE not found -> stereo fallback"
[2/3] Servidor de audio listo — imprime informe de configuración completo
[3/3] Cargando motor (pdj_datamatics.scd)
      -> imprime "PDJ Datamatics — listening OSC UDP :9001"
```

Una vez en marcha, la Terminal imprime una línea de salud cada dos minutos
(`[PDJ health] server:running  mode:DANTE  speakers:8  channels:8`).
Dejar la ventana abierta durante toda la instalación. Presionar Ctrl+C para detener.

### Modo DANTE de 8 canales (Sala Abierta)

Cuando Dante Virtual Soundcard está activo en la red, el lanzador usa automáticamente **8 canales de salida** enrutados por DANTE 5 (D3-1 a D3-8). Las ganancias por altavoz se calculan en tiempo real mediante un motor DBAP (Distance-Based Amplitude Panning) impulsado por las posiciones de blobs detectados, el flujo óptico y los datos de multitud.

Para habilitar DANTE:

1. Instalar y licenciar **Dante Virtual Soundcard** (audinate.com, aprox. USD 30).
2. Abrir la app DVS en la barra de menú: TX channels = 8, RX = 2, sample rate = 48000 Hz, latency = 1 ms. Hacer clic en **Enable**.
3. Conectar el Mac al switch Ethernet del recinto (mismo switch que la unidad DANTE 5).
4. Abrir **Dante Controller** y enrutar DVS Out 1–8 a D3-1 hasta D3-8.
   Establecer DANTE 5 como clock master. Confirmar icono de candado verde en ambos dispositivos.
5. Ejecutar `Check Audio.command` para confirmar la detección, luego `Start Audio.command`.

Ver `docs/AUDIO_MULTICHANNEL_ES.md` para la configuración completa, enrutamiento de sala y calibración de altavoces.

### Modo de rescate estéreo

Si DVS no se detecta, el motor arranca automáticamente en estéreo en la salida de audio por defecto de macOS. Toda la lógica de enrutamiento de datos visuales, OSC y modos es idéntica; solo cambia el número de canales (2 en lugar de 8). No se requieren cambios de código.

### Arranque desde el IDE (solo desarrollo)

Si se usa el IDE de SuperCollider en lugar de `Start Audio.command`:

1. Evaluar `SuperCollider/pdj_audio_config.scd` primero (Cmd+Return).
   Esperar "Server ready" en Post Window.
2. Evaluar `SuperCollider/pdj_datamatics.scd` (Cmd+Return).
   Esperar "PDJ Datamatics — listening OSC UDP :9001".

**No presionar Boot Server (Cmd+B) manualmente**; `pdj_audio_config.scd` arranca el servidor con el dispositivo y el número de canales correctos.

---

## Prueba básica de funcionamiento

Confirmar lo siguiente:

- La app abre sin requerir openFrameworks ni el repositorio fuente.
- Aparece una ventana de presentación y una ventana de control.
- Las cuatro áreas de canal reproducen vídeo.
- El panel de control reporta 48 clips disponibles.
- Los shaders B&W y los modos visuales renderizan sin errores de shader.
- Los modos rotan automáticamente.
- Si SuperCollider está en marcha, la actividad OSC produce audio en todos los canales.

---

## Prueba de rendimiento

Abrir la página **Performance** en la ventana de control y seleccionar **Start 10-minute stress test**. Mantener las ventanas de presentación visibles y evitar mover ventanas o lanzar otras aplicaciones durante la prueba.

La prueba calienta durante 10 segundos y luego ejercita vídeo normal, los modos costosos de líneas y números, slit-scan con cambios de clip y los generadores procedimentales. Una ejecución de producción requiere ritmo estable de 30 FPS sin ningún fotograma superior a 100 ms.

Al completarse, la página muestra PASS o FAIL con los umbrales específicos que no se alcanzaron. Los archivos JSON y CSV se guardan en `~/Documents/PartituraDelJuego/performance_reports`. Presionar **Stop test** para terminar antes y guardar un informe parcial.

El layout de cuatro canales del paquete es útil como prueba básica. El ordenador de instalación debe probarse también en `dualWindow8` con ambas salidas ICUIXIAN conectadas; solo esa ejecución representa la carga de producción real.

---

## Resolución de problemas

### No hay vídeos

Mantener la carpeta `Videos` junto a `Partitura_del_Juego.app`. Confirmar que los archivos están directamente dentro de `Videos/Portrait` o `Videos/Horizontal`; las subcarpetas anidadas no se escanean.

### No hay audio

- Iniciar SuperCollider (`Start Audio.command`) antes de la aplicación visual.
- Esperar el paso `[3/3]` y "PDJ Datamatics — listening OSC UDP :9001" en el Terminal.
- Asegurarse de que ningún otro proceso usa ya el puerto 9001.

### Sin DANTE / solo estéreo cuando se espera DANTE

- Ejecutar `Check Audio.command` y revisar la línea de detección DANTE.
- Si DVS no aparece en la lista: abrir la app de Dante Virtual Soundcard en la barra de menú y hacer clic en **Enable**. Verificar que el cable Ethernet está conectado.
- Si el dispositivo aparece en el sistema pero no en SC: confirmar que DVS está a 48000 Hz / 8 canales TX. Relanzar `Start Audio.command`.
- Si Dante Controller muestra candado amarillo/rojo: la tasa de muestreo de DVS no coincide con DANTE 5. Establecer ambos a 48000 Hz y volver a habilitar DVS.

### Altavoz incorrecto suena en la prueba

Ejecutar la rutina de prueba de altavoces en el IDE de SuperCollider:

```supercollider
~speakerTest.();
```

Recorrer la sala y anotar qué altavoz físico se activa en cada paso. Reordenar las suscripciones en Dante Controller (DVS Out N → D3-M) o actualizar `~speakerPositions` en `SuperCollider/pdj_datamatics.scd` para que coincida.

### Layout completo de instalación

Este paquete usa intencionalmente una ventana única segura para portátil. Para la instalación completa con dos muros ICUIXIAN, configurar la geometría de visualización con el panel de control o editar directamente `settings.json` en `~/Library/Application Support/PartituraDelJuego/`. Los cambios en tiempo de ejecución se guardan en esa ubicación y no modifican el `.app` firmado.
