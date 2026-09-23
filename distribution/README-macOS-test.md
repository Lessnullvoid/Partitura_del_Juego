# Partitura del Juego — Inicio de la instalación

Conserva esta carpeta completa en un lugar con permiso de escritura. Incluye
la aplicación, 48 vídeos verticales y 3 horizontales con sus análisis, el motor
sonoro y SuperCollider. No necesitas instalar openFrameworks, Python ni
SuperCollider para ejecutarla. El montaje DANTE necesita sus controladores,
licencia y conexiones preparados en el ordenador del recinto.

## Abrir

1. Descomprime el ZIP. Si macOS solicita autorización, utiliza control-clic →
   Abrir y sigue el diálogo del equipo.
2. Abre **Start Audio.command**. Si aparece un menú, elige la salida y pulsa Enter.
3. Escucha los avisos y responde **y + Enter** a **Did you hear the ping?** si se
   oyen por la salida esperada. Si no, responde **n** y revisa las indicaciones;
   puedes salir con Control+C para corregir la conexión.
4. Espera **PDJ Datamatics — listening OSC UDP :9001**. En DANTE, comprueba la
   prueba de las ocho salidas en la sala.
5. Abre **Start Visual.command** una vez. Deja ambas Terminales abiertas.

El ping inicial utiliza la salida de macOS. No sustituye la prueba de las ocho
rutas DANTE. La aplicación parte de una ventana con ocho canales; los ajustes
guardados del recinto tienen prioridad. La disposición de dos muros se configura
desde **PDJ Control**. El lanzador prepara las rutas hacia los vídeos de esta
carpeta y conserva los demás ajustes del usuario.

## Cerrar

1. Abre **Stop Visual.command** y espera el cierre de las ventanas. Pulsa Enter
   cuando lo solicite.
2. Pulsa **Control+C** en la Terminal de audio y comprueba que cesa el sonido.
3. Apaga el Mac según el procedimiento del museo.

**Stop Visual.command solo detiene el visual.** El vigilante respeta un cierre
normal y recupera caídas o pérdida prolongada de actividad, con margen de carga
inicial. **Check Audio.command** informa de los dispositivos disponibles.
**Start Mix Desk.command** inicia audio con mesa de mezcla y se utiliza en lugar
del lanzador de audio habitual, con el otro motor detenido.

## Documentación

Abre [LECTURA.html](../docs/LECTURA.html) en tu navegador para consultar las
guías sin conexión e imprimir cada una. Los mismos textos están en Markdown.

| Uso | Lectura |
|---|---|
| Instalar y configurar | [Manual de instalación](../docs/MANUAL_ES.md) |
| Operación de cada jornada | [Manual diario](../docs/MANUAL_DIARIO_COMANDOS_ES.md) |
| Audio de sala | [Audio multicanal](../docs/AUDIO_MULTICHANNEL_ES.md) |
| Entender la obra | [Cómo funciona la partitura](../docs/COMO_FUNCIONA_ES.md) |
| Preparar visitas y actividades | [Mediación para museos](../docs/MEDIACION_MUSEOS_ES.md) |
| Profundizar en las reglas | [Algoritmos y procesos](../docs/ALGORITMOS_Y_PROCESOS_ES.md) |

## Ubicaciones útiles

- `Videos/Portrait` y `Videos/Horizontal`: medios y carpetas de análisis `.pdjcv`.
- `SuperCollider/`: síntesis y mezcla; `Runtime/SuperCollider.app`: entorno incluido.
- `Support/` y `State/`: utilidades y estado de los lanzadores.
- `Source/current-source.tgz`: código del programa.
- `VERSION.txt`, `Source/source-manifest.json` y `PACKAGE_MANIFEST.json`: identificación y huellas de los archivos.
- `~/Library/Application Support/PartituraDelJuego/`: ajustes personales.
