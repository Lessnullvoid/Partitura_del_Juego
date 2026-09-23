# Preparar el análisis de la biblioteca

El paquete contiene 48 vídeos verticales y 3 horizontales, cada uno junto a su
carpeta de análisis `.pdjcv`. Para utilizar esos medios basta iniciar el visual
con su lanzador. Este procedimiento permite preparar los datos cuando se
incorporan o modifican vídeos.

## Analizar una carpeta

1. En **PDJ Control**, abrir **Analisis**.
2. Pulsar **Elegir carpeta de videos** o **Usar biblioteca actual**. La búsqueda
   de análisis admite MP4, MOV y M4V, también en subcarpetas.
3. Comprobar que la carpeta tiene permiso de escritura y pulsar
   **Analizar biblioteca / Continuar pendientes**.
4. Seguir progreso, errores y vídeos completados. Realizarlo fuera del horario
   de exposición: comparte recursos con reproducción y dibujo.
5. Conservar cada vídeo con su carpeta de resultados. Por ejemplo:
   `partido.mp4` y `partido.mp4.pdjcv/`.

Seleccionar una carpeta de análisis no cambia la biblioteca de reproducción
ni copia los vídeos. La reproducción del paquete utiliza `Videos/Portrait` y
`Videos/Horizontal`. Conservar formatos y distribución adecuados a esas
bibliotecas; el escáner de reproducción admite MP4, MOV y AVI en sus carpetas.
No confundir las extensiones del escáner de análisis con las de reproducción.

**Cancelar analisis** conserva resultados completos. Al continuar, se omiten
los análisis compatibles terminados y se procesa desde el inicio cada vídeo
incompleto. Los errores no se publican como resultados terminados.

## Qué contienen los resultados

El proceso muestrea el vídeo para calcular detección de personas, filtro de
superficie, seguimiento, movimiento y contornos. Guarda tiempos del vídeo,
índice y datos; no necesita incluir fotogramas dentro de la caché. Las marcas
de tiempo son la referencia, no una frecuencia uniforme supuesta.

Durante la reproducción, la aplicación consulta el análisis e interpola
posiciones de identificadores coincidentes. Con resultados compatibles evita
repetir la red neuronal y el flujo óptico. La decodificación, el dibujo y
algunas máscaras de los tratamientos gráficos trabajan durante la ejecución.
El panel indica **analisis guardado** en los canales que utilizan esa ruta.

## Trasladar o modificar medios

Copiar juntos vídeo y `.pdjcv`. La comprobación de compatibilidad utiliza
tamaño y muestras del contenido, no un examen de todos los bytes. Recortar o
recodificar requiere analizar de nuevo. Un nombre que incluya `hinchada`
implica exclusión de seguimiento de jugadores y puede invalidar resultados
que admiten personas.

Si faltan datos compatibles o se produce un error al leerlos, el canal puede
utilizar análisis en vivo. Los [límites de detección](CV_OVERLAYS_ES.md) se
aplican también a los datos guardados. `.pdjcv` es la caché de esta aplicación;
los paquetes PDJV pertenecen a otra ruta de datos y no son intercambiables.
