# Partitura del Juego — Manual diario

Guía de apertura, comprobación y cierre para el personal de sala. Utilizar
siempre los lanzadores de la misma carpeta de instalación. Mantener abiertas
las Terminales de audio y visual; se pueden minimizar.

## Abrir la instalación

1. Encender el equipo de sala y comprobar las conexiones previstas por el museo.
2. Abrir **Start Audio.command**. Si DANTE está disponible se selecciona
   automáticamente. Si aparece un menú, escribir el número de salida y Enter.
   Si la sala necesita DANTE y no aparece, detener con Control+C y avisar al
   personal técnico.
3. Escuchar los avisos y responder a esta pregunta:

   ```text
   Did you hear the ping? [y/n] then Enter:
   ```

   Si se oye por la salida esperada, escribir **y** y Enter. Si no se oye,
   escribir **n** y Enter; revisar las indicaciones y salir con Control+C para
   corregir la conexión. Se utiliza **y**, no la letra s.
4. Esperar **PDJ Datamatics — listening OSC UDP :9001**. Con DANTE, escuchar
   la prueba de las ocho salidas y comprobar que llegan a la sala.
5. Abrir **Start Visual.command** una vez. Esperar la carga de la biblioteca.
6. Confirmar imágenes, orientación y sonido de la obra. El panel de control
   muestra **Portrait: 48** y **Horizontal: 3**.

El ping usa la salida de macOS: oírlo en el portátil no confirma las rutas de
sala. La prueba posterior del motor comprueba las salidas una a una.

Si el ordenador del recinto ya abre los lanzadores al iniciar sesión, continuar
con las preguntas pendientes en Terminal. No iniciar componentes duplicados.
El paquete se inicia mediante sus lanzadores; la apertura al iniciar sesión
depende de la configuración del ordenador del museo.

## Durante la jornada

Las pantallas alternan vídeo, generadores y respiraciones. También pueden
converger, quedar escasas o atravesar una cesura. Un intervalo con poca
actividad no demuestra por sí solo un fallo.

Para ocultar o mostrar los controles, pulsar **U**. No cerrar la ventana de
control durante la exposición: su cierre termina la presentación. Mantener
el volumen y la disposición acordados con el personal técnico.

## Si falta el audio

Revisar primero si Terminal está esperando una respuesta. Si el motor está
iniciado, comprobar volumen, silenciamientos y amplificación con el técnico.
**Check Audio.command** informa de los dispositivos; no inicia ni repara audio.

Para reiniciar: pulsar **Control+C** en la Terminal de audio, esperar a que
termine y abrir **Start Audio.command** una vez. Completar selección y prueba.
El visual puede permanecer abierto. No iniciar **Start Mix Desk.command**
sobre un motor en funcionamiento.

## Si falta el visual o queda detenido

El lanzador permite tres minutos de carga antes de evaluar un bloqueo global.
Para reiniciar manualmente, abrir **Stop Visual.command**, esperar el cierre
incluida la Terminal visual y abrir **Start Visual.command** una vez.

Si el problema se repite, anotar canal, nombre del vídeo, hora y mensaje de error.
El [manual de instalación](MANUAL_ES.md) contiene el diagnóstico técnico.

## Cerrar al final del día

1. Abrir **Stop Visual.command**. Esperar a que se cierren las ventanas; pulsar
   Enter cuando aparezca **Press Return to close**.
2. Pulsar **Control+C** en la Terminal de Audio Launcher. Comprobar que cesa el
   sonido. La tecla es Control, no Command.
3. Cerrar las Terminales que terminan y apagar el Mac según el procedimiento
   del recinto. Desconectar el sistema de audio después de detener su motor.

**Stop Visual.command detiene solo el visual.** El audio se detiene por separado.
Si no responde a Control+C, acudir al personal técnico y al apartado de
recuperación del [manual de instalación](MANUAL_ES.md).
