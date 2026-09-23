# Leer la partitura sonora visible

Cada pantalla presenta una franja que representa actividad sonora recibida
por la aplicación. Permite relacionar duración, acontecimientos breves y
textura con una parte de la contribución de ese canal.

## Orientarse en la franja

La cabecera indica **PANTALLA** y su número, puede mostrar el nombre de un
instrumento recibido y contiene un indicador de actividad reciente. La línea
de **AHORA**, a la derecha, marca el presente. Hacia la izquierda quedan seis
segundos de historia. Las marcas se desplazan conforme pasa el tiempo.

| Fila | Marca | Lectura |
|---|---|---|
| sonido | Trazo | Continuidad de la voz principal medida. |
| pulsos | Punto o marca corta | Actividad breve de la capa instrumentada; un punto representa un pulso contado. |
| textura | Trazo más grueso | Actividad de la cama de ruido medida. |

Una marca aislada puede aparecer sin un trazo largo. El dibujo conecta muestras
próximas de una misma fuente y evita unir intervalos con huecos amplios.
El indicador de nivel resume actividad digital reciente; no está calibrado
como sonómetro de la sala.

## De dónde procede la información

El motor de SuperCollider mide tres capas: voz principal, motor de pulsos y
cama de ruido. Envía nivel, identidad de fuente, canal y datos de actividad.
La aplicación valida la sesión y la secuencia, conserva una historia limitada
y dibuja el resultado una vez recibido. Dibujar las marcas no dispara sonidos.

El estado distingue datos recientes de datos caducados. Si se interrumpe la
comunicación, deja de señalar actividad actual. El espacio sin marca no debe
interpretarse automáticamente como silencio confirmado de toda la mezcla.

## Qué representa y qué queda fuera

Las tres filas no constituyen una transcripción de todas las voces, eventos,
subgraves y efectos. Una cola compartida puede escucharse sin una marca propia.
La actividad medida tampoco confirma que el amplificador esté encendido o que
un sonido sea perceptible por encima de los demás.

La posición temporal del visor utiliza la recepción local de mensajes.
La latencia física de sonido y pantalla requiere ajuste en la instalación;
la coincidencia visual no es una medición exacta del instante de escucha.
Una pantalla representa una contribución compositiva y puede distribuir su
audio entre varias salidas.

## Propuesta de observación

Elegir una fila y seguir una entrada, su duración y su retirada. Después escuchar
el conjunto y preguntar qué acontecimientos tienen una marca reconocible y
cuáles no. Se puede dibujar una notación propia y compararla con el visor.
La [guía de mediación](MEDIACION_MUSEOS_ES.md) desarrolla esta actividad.

Para revisar el funcionamiento: `src/AudioScoreState.h`,
`src/AudioScoreOverlay.h`, `src/ControlApp.cpp` y
`supercollider/pdj_datamatics.scd` en el código de `Source/current-source.tgz`.
