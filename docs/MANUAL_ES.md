# Partitura del Juego — Manual de instalación y operación

Este manual describe el paquete de instalación de `dist/`: aplicación visual,
vídeos y análisis, motor sonoro, SuperCollider y lanzadores. Está dirigido al
personal técnico y de sala. Para preparar visitas educativas, consultar la
[guía de mediación](MEDIACION_MUSEOS_ES.md).

## 1. Preparar el equipo

La aplicación se ejecuta en un Mac Apple Silicon. Para la presentación de sala
se necesitan dos salidas de vídeo utilizables, dos controladores ICUIXIAN
0104-XZ, ocho pantallas, su cableado y un sistema de audio conectado a las
salidas previstas por el recinto. El montaje mixto utiliza cuatro pantallas
verticales y cuatro horizontales.

Para DANTE se necesita Ethernet, Dante Virtual Soundcard activo y con licencia,
Dante Controller y las suscripciones de audio del recinto. Estos controladores
se preparan en el ordenador; no forman parte del entorno SuperCollider incluido.
Para una escucha estéreo se utiliza una salida disponible del Mac.

SuperCollider está en `Runtime/SuperCollider.app`. No hace falta instalar
openFrameworks, Python ni un entorno de desarrollo para ejecutar la entrega.
Mantener alimentación, ventilación y los ajustes de energía de exposición que
establezca el museo. Configurar las pantallas como escritorio extendido.

## 2. Contenido del paquete

Conservar toda la carpeta, con permiso de escritura y fuera del ZIP:

```text
Carpeta de Partitura del Juego/
├── Partitura_del_Juego.app
├── Start Audio.command
├── Start Visual.command
├── Stop Visual.command
├── Start Mix Desk.command
├── Check Audio.command
├── README.md
├── Documentacion/
├── Videos/
│   ├── Portrait/       48 vídeos y sus carpetas .pdjcv
│   └── Horizontal/      3 vídeos y sus carpetas .pdjcv
├── SuperCollider/      módulos de síntesis y mezcla
├── Runtime/
│   └── SuperCollider.app
├── Support/            utilidades de dispositivos y configuración
├── State/              estado y copias de ajustes creados por los lanzadores
├── Source/
│   ├── current-source.tgz
│   └── source-manifest.json
├── VERSION.txt
└── PACKAGE_MANIFEST.json
```

`Partitura_del_Juego.app` incluye shaders, modelo de detección y configuración
inicial. Los vídeos están fuera de la aplicación. No mover solo la `.app` ni
modificar sus recursos internos. `Source/` permite consultar el código; no es
necesario descomprimirlo para ejecutar la instalación.

| Lanzador | Función |
|---|---|
| `Start Audio.command` | Selecciona salida, pide confirmar un aviso sonoro e inicia el motor. |
| `Start Visual.command` | Prepara rutas de vídeo e inicia la aplicación con vigilancia. |
| `Stop Visual.command` | Detiene la aplicación visual y su vigilante. |
| `Check Audio.command` | Informa de los dispositivos; no inicia el motor ni repara conexiones. |
| `Start Mix Desk.command` | Inicia el motor con la mesa de mezcla; se utiliza en lugar del lanzador de audio habitual. |

Para detener el audio se usa **Control+C** en su Terminal. Los lanzadores de
mezcla y de audio no deben iniciar dos motores simultáneos.

## 3. Primera apertura

1. Descomprimir el paquete y situar la carpeta completa en su ubicación de uso.
2. Conectar pantallas y audio. Preparar DANTE si corresponde al montaje.
3. Abrir los lanzadores desde esa carpeta. Si macOS pide autorización para la
   aplicación o los lanzadores, utilizar control-clic → Abrir y seguir su diálogo.
4. Mantener las Terminales de audio y visual abiertas; se pueden minimizar.

La aplicación tiene firma local. Las autorizaciones de macOS se gestionan en
el equipo de instalación con el personal responsable.

## 4. Arranque y comprobación de audio

1. Abrir **Start Audio.command**.
2. Si aparece **DANTE detected**, el dispositivo se selecciona automáticamente.
   Si aparece **No DANTE detected. Select audio output:**, escribir el número
   de salida y pulsar Enter. Si el montaje requiere DANTE y no se detecta,
   detener con Control+C y revisar la conexión antes de continuar.
3. El lanzador reproduce dos avisos y pregunta:

   ```text
   Did you hear the ping? [y/n] then Enter:
   ```

   Escribir **y** y pulsar Enter si se escucha por la salida esperada.
   Escribir **n** si no se escucha. Se muestran indicaciones y se permite salir
   con Control+C para corregir la configuración. La respuesta afirmativa es
   la letra **y**, no la letra s.
4. Esperar el mensaje:

   ```text
   PDJ Datamatics — listening OSC UDP :9001
   ```
5. En DANTE, comprobar el mensaje **8-channel DANTE output active** y la prueba
   secuencial de las ocho salidas. Confirmar físicamente qué zona suena con cada
   canal; **All 8 channels tested** solo indica que terminó la secuencia.

El aviso inicial usa la salida de sonido de macOS. Oírlo en el portátil no
confirma el enrutamiento hacia la sala. La prueba multicanal del motor permite
comprobar las ocho rutas. Ver [Audio multicanal](AUDIO_MULTICHANNEL_ES.md).

## 5. Arranque visual y ajustes guardados

Abrir **Start Visual.command** cuando el audio está listo. La configuración
inicial muestra ocho canales en una ventana de 1920×1080. La configuración
personal guardada tiene prioridad sobre esos valores iniciales.

Los ajustes están en:

```text
~/Library/Application Support/PartituraDelJuego/settings.json
```

El lanzador guarda una copia de los ajustes existentes en `State/` y prepara
las rutas hacia `Videos/Portrait` y `Videos/Horizontal` del paquete. Conserva
los demás valores. Después de mover la carpeta, volver a iniciar mediante el
lanzador para que las rutas correspondan a la nueva ubicación.

La ventana **PDJ Control** muestra `Portrait: 48` y `Horizontal: 3`. La carga
inicial puede tardar; el vigilante concede tres minutos antes de evaluar un
bloqueo global. No iniciar otra copia mientras carga.

## 6. Montaje de las ocho pantallas

| Muro | Canales internos | Controlador | Distribución |
|---|---|---|---|
| A | 0–3 | ICUIXIAN A | Cuatro pantallas verticales, 4×1, rotación 90°. |
| B | 4–7 | ICUIXIAN B | Cuatro pantallas horizontales, 2×2, sin rotación. |

Conectar la salida Thunderbolt/USB-C, mediante el adaptador correspondiente,
a la entrada de A; conectar HDMI a la entrada de B. Cada controlador reparte
sus cuatro salidas a las pantallas de su muro. La señal de entrada prevista
es 1920×1080 a 60 Hz; el tamaño de cada segmento no equivale a la resolución
física completa de su panel.

En **PDJ Control**, usar **Configure Mixed Wall (4V + 2x2H)** para el montaje
mixto. **Enable dual output** activa el modo de dos ventanas. Si aparece
**Restart required**, detener con **Stop Visual.command** y volver a iniciar
con **Start Visual.command**. El audio puede permanecer funcionando.

Usar **Identify A**, **Identify B** o **Identify both** para comprobar muro,
canal y salida física. **Swap A/B** intercambia la asignación de los muros.
Comprobar la orientación con las marcas visibles y ajustar la rotación del
controlador si el panel queda invertido.

**Configure ICUIXIAN outputs (4V + 4V)** corresponde a dos muros de retratos;
no es el ajuste del montaje mixto descrito aquí.

## 7. Controles durante la operación

| Panel | Uso |
|---|---|
| Overview | Comprobar canales, reproducción y clips; acceder a Next, Play/Pause y Stop por canal. |
| Global Director | Activar cámara lenta, avance rápido y clear; habilitar sus disparos automáticos. |
| Video Director | Ajustar selección de fragmentos y solicitar un acontecimiento compartido. |
| Visual Composer | Consultar forma, sección, momento, organización y paleta; intervenir en la composición. |
| Channel Editor | Ajustar vídeo, nube de puntos, imagen, visión y modo de partitura por canal. |
| Performance | Medir funcionamiento y exportar informes. |
| Analisis | Preparar los datos de una biblioteca de vídeos. |

La barra superior incluye volumen maestro, configuración OSC y controles de
salida. La tecla **U** oculta o muestra la interfaz. Cerrar normalmente la
ventana de control cierra la presentación; para el cierre diario se recomienda
el lanzador **Stop Visual.command**.

**Hide H-wall video** retira el vídeo de los canales horizontales y permite
mantener generadores en ese muro. No apaga físicamente las pantallas.

Los controles de sesión no tienen todos la misma persistencia. Guardar la
paleta mediante **Save palette settings** cuando se quiere conservarla.
**Trail seconds** y **Surface spacing** ajustan la sesión de los overlays.
El personal de mediación puede desarrollar sus actividades sin intervenir en
estos controles.

## 8. Comprobación de apertura al público

- Confirmar 48 clips verticales y 3 horizontales en la interfaz.
- Comprobar imagen, orientación y orden de las ocho pantallas.
- Escuchar la salida de cada ruta DANTE durante la prueba y después el sonido
  de la obra con el visual en ejecución.
- Comprobar que los canales presentan actividad y cambian de material con el
  tiempo; una respiración, una cesura o un generador pueden mostrar poca actividad.
- Dejar las Terminales abiertas y anotar la ubicación del paquete y los ajustes
  del recinto en el registro de sala.

La [partitura sonora visible](PARTITURA_SONORA_ES.md) ayuda a observar tres
capas de audio, pero no sustituye la escucha de las salidas físicas.

## 9. Cierre diario

1. Abrir **Stop Visual.command**. Esperar a que desaparezcan las ventanas y
   pulsar Enter cuando aparezca **Press Return to close**.
2. En la Terminal de Audio Launcher, pulsar **Control+C**. Comprobar que cesa
   el audio. Si se utiliza la mesa de mezcla, detener su Terminal del mismo modo.
3. Cerrar las Terminales que terminan y apagar el ordenador según el procedimiento
   del recinto. Detener el audio antes de desconectar su dispositivo o la red.

**Stop Visual.command solo detiene el visual.** No detiene el motor de sonido
ni apaga el Mac.

## 10. Recuperación y diagnóstico

| Situación | Comprobación y acción |
|---|---|
| El audio parece no iniciar | Revisar si Terminal espera el número de salida o la respuesta al ping. |
| DANTE no aparece | Detener audio; abrir Check Audio.command; revisar DVS, Ethernet y las suscripciones con el técnico. |
| El ping suena en el Mac, no en sala | Revisar la salida del sistema y después la prueba de ocho canales del motor. |
| Hay imagen pero no sonido | Revisar salida, volumen maestro, silenciamientos, amplificación y que el motor escuche en 9001. |
| El visual no aparece | Dar margen a la carga; comprobar permisos y que toda la carpeta esté presente. |
| Un canal queda detenido | Distinguir pausa o material escaso de una detención; anotar canal, clip y hora. |
| Se necesita reiniciar el visual | Abrir Stop Visual.command, esperar a que termine su Terminal y abrir Start Visual.command una vez. |
| Se necesita reiniciar el audio | Control+C en su Terminal, esperar y abrir Start Audio.command una vez. |
| El análisis no muestra personas | Revisar el tipo de imagen y la evidencia de campo; la ausencia de marcas puede ser una condición del análisis. |

El vigilante visual relanza la aplicación tras una caída o una pérdida
prolongada de su pulso, respetando el margen inicial. Un cierre normal se
respeta. La vigilancia de cada reproductor distingue pausas solicitadas de
falta inesperada de imágenes y puede solicitar otro clip.

Si se pierde la Terminal de audio o no responde, el personal técnico puede
cerrar los procesos de SuperCollider en el Mac dedicado a la obra:

```sh
pkill -TERM -x sclang
pkill -TERM -x scsynth
```

Estas órdenes afectan a todas las sesiones de SuperCollider de ese usuario.
No utilizarlas si el equipo mantiene otra sesión que deba conservarse. Guardar
el error y la hora si el fallo se repite.

## 11. Biblioteca y documentación

La entrega contiene los vídeos y sus carpetas `.pdjcv`. No separar esas parejas.
Para preparar otra biblioteca, seguir [Análisis de la biblioteca](OFFLINE_ANALYSIS_ES.md).
No iniciar el análisis durante la apertura al público: comparte recursos con
la reproducción.

[Manual diario](MANUAL_DIARIO_COMANDOS_ES.md) reúne las acciones de cada jornada.
[Cómo funciona la partitura](COMO_FUNCIONA_ES.md) y la
[guía de mediación](MEDIACION_MUSEOS_ES.md) apoyan al equipo educativo.
