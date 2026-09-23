# Partitura del Juego — Audio y distribución espacial

Guía del motor incluido en `SuperCollider/` y del entorno
`Runtime/SuperCollider.app`. Para las acciones diarias, consultar el
[manual diario](MANUAL_DIARIO_COMANDOS_ES.md).

## 1. Recorrido de la señal

```text
Aplicación visual → mensajes OSC → SuperCollider → dispositivo de audio
    → salidas estéreo o red DANTE → amplificación → altavoces
```

OSC transmite parámetros, no audio. El motor escucha en UDP **9001**;
la aplicación recibe respuestas en **9002** según la configuración incluida.
El motor genera voces, pulsos, texturas, subgraves y acentos, los articula con
el estado compositivo y los envía a la mezcla.

Un canal visual es una fuente de información y composición. No equivale a un
altavoz exclusivo: su contribución puede distribuirse entre varias salidas.

## 2. Iniciar el motor

Abrir **Start Audio.command** desde la carpeta de entrega. El lanzador consulta
los dispositivos, selecciona DANTE si lo detecta y ofrece un menú de salida
cuando corresponde a estéreo. Reproduce un ping y solicita **y/n + Enter**.
El ping utiliza la salida del sistema operativo, que puede ser distinta de
la elegida para el motor.

Esperar **PDJ Datamatics — listening OSC UDP :9001**. En DANTE se realiza una
prueba secuencial; comprobar físicamente el destino de cada salida. El mensaje
**All 8 channels tested** confirma que terminó la secuencia, no la escucha.
Después abrir el visual para recibir los controles de la obra.

El entorno de sonido está incluido. El paquete utiliza salida de audio y no
analiza un micrófono del público. Las autorizaciones que solicite macOS se
resuelven en el ordenador con el personal responsable.

## 3. Preparar DANTE

El técnico del recinto prepara Dante Virtual Soundcard, su licencia, la conexión
Ethernet y Dante Controller. Activar el dispositivo y utilizar **48 kHz**, en
coherencia con los receptores de sala. El motor abre el número de salidas
anunciado por DVS y utiliza ocho para la obra; los valores de su configuración
incluida son bloque de 512 y búfer de hardware de 512 muestras en DANTE.

En Dante Controller, asociar las ocho rutas de la obra con las entradas del
sistema de amplificación. Confirmar el desplazamiento de salida indicado por
el motor si existe una configuración específica. Con el desplazamiento cero,
las rutas corresponden a DVS 1–8.

| Ruta de la obra | Comprobación en sala |
|---|---|
| 1 | Identificar receptor y zona física durante su tono. |
| 2 | Identificar receptor y zona física durante su tono. |
| 3 | Identificar receptor y zona física durante su tono. |
| 4 | Identificar receptor y zona física durante su tono. |
| 5 | Identificar receptor y zona física durante su tono. |
| 6 | Identificar receptor y zona física durante su tono. |
| 7 | Identificar receptor y zona física durante su tono. |
| 8 | Identificar receptor y zona física durante su tono. |

Registrar la correspondencia real del recinto. No deducir el orden físico solo
por el número de pantalla. Una ruta puede alimentar más de un altavoz según el
sistema de amplificación del museo.

## 4. Escucha estéreo

Sin DANTE, el lanzador permite seleccionar un dispositivo estéreo disponible.
El motor utiliza panoramización izquierda/derecha. Esta escucha permite revisar
material, continuidad y mezcla; no reproduce la distribución espacial de ocho
salidas.

Si la instalación necesita DANTE y el dispositivo no aparece, detener el
arranque y revisar la conexión. No tomar una escucha correcta en el portátil
como comprobación del montaje de sala.

## 5. Espacialización DBAP

DBAP calcula ganancias según la distancia entre una fuente virtual y las
posiciones de altavoces configuradas. La posición depende de datos geométricos
y decisiones compositivas. Un parámetro regula la caída con la distancia y
otro añade dispersión. La implementación normaliza las ganancias mediante RMS.

Las coordenadas `~speakerPositions` de `pdj_datamatics.scd` representan el plano
de la sala entre 0 y 1. Deben concordar con la posición y el orden de las salidas
físicas del montaje. El técnico puede revisarlas en ese archivo externo a la
aplicación y reiniciar el audio. Escuchar la prueba y recorrer la sala después
de cualquier ajuste.

[Algoritmos y procesos](ALGORITMOS_Y_PROCESOS_ES.md) explica el cálculo y sus
límites. La espacialización no es una medida de la posición física de los
jugadores ni de las personas visitantes.

## 6. Mezcla

**Start Mix Desk.command** inicia el motor con su interfaz de mezcla. Es una
alternativa a **Start Audio.command**: detener el motor activo antes de usarla.
La mesa permite ajustar niveles y guardar mediante **Save installation mix**.
Su opción de OSC sintético sirve para audicionar; durante la obra se utilizan
los datos de la aplicación visual.

La mezcla del usuario está en:

```text
~/Library/Application Support/PartituraDelJuego/audio_mix.scdcfg
```

El archivo de mezcla incluido en `SuperCollider/` proporciona valores del
paquete. El volumen maestro también se controla desde **PDJ Control**; el
ajuste visual de volumen está en `settings.json`. La configuración inicial
incluye un valor maestro de 0,55. El nivel apropiado en sala se establece
escuchando el sistema de amplificación y las condiciones del museo.

## 7. Archivos y funciones

| Archivo o grupo en `SuperCollider/` | Función |
|---|---|
| `pdj_launcher.scd` | Prepara el arranque, carga el motor y muestra estado y prueba de salida. |
| `pdj_audio_config.scd` | Selección de dispositivo y opciones del servidor. |
| `pdj_datamatics.scd` | Control OSC, estados, síntesis, espacialización y mezcla. |
| `pdj_mode_voices.scd` | Vocabulario de voces asociado a materiales visuales. |
| `pdj_palette_voices.scd`, `pdj_ember_voices.scd`, `pdj_world_voices.scd` | Tratamientos y materiales de los mundos sonoros. |
| `pdj_video_phrases.scd`, `pdj_functional_material.scd` | Articulación sonora de capítulos de vídeo y su material. |
| `pdj_mix_config.scd`, `pdj_mix_desk.scd` | Persistencia e interfaz de mezcla. |
| `pdj_volumetric_compat.scd` | Compatibilidad de mensajes de la ruta volumétrica. |
| `data_matrix_functional_study.scd` | Estudio incluido en la carpeta; no se inicia con la operación habitual. |
| `audio_mix.scdcfg`, `audio_mix.default.json` | Recursos de configuración de mezcla. |

El lanzador carga las dependencias del motor. El personal de sala no necesita
abrir ni evaluar estos archivos individualmente.

## 8. Diagnóstico y cierre

| Síntoma | Acción |
|---|---|
| Terminal parece detenida | Revisar el menú de salida y la pregunta del ping. |
| DANTE no se detecta | Consultar Check Audio.command; revisar DVS, Ethernet y dispositivo activo. |
| Hay prueba local pero no audio de sala | Revisar suscripciones, frecuencia de muestreo, amplificación y silenciamientos. |
| No hay respuesta a la imagen | Comprobar que el visual está activo, OSC apunta a localhost:9001 y no hay un segundo motor. |
| La salida se interrumpe | Conservar mensajes y revisar dispositivo, conexión y carga con el técnico. |
| La franja sonora tiene actividad pero no se oye | Revisar toda la ruta física: la franja mide señal digital, no el altavoz. |

Para cerrar, detener el visual con su lanzador y el audio con **Control+C** en
su Terminal. Confirmar que cesa antes de desactivar DVS o desconectar el equipo.
La recuperación técnica de procesos está en el [manual](MANUAL_ES.md).
