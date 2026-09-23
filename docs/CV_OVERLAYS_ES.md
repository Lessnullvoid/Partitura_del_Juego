# Visión de cuerpos, trayectorias y campo

Los modos de visión superponen marcas a los vídeos a partir de detecciones y
geometría. La aplicación utiliza YOLOX-S para localizar personas y un filtro
de superficie de juego para admitir las detecciones. Las marcas son resultados
de ese análisis, no información sobre identidades deportivas.

## Modos y lectura

| Modo | Material visible | Alcance |
|---|---|---|
| SurfaceScan | Contornos de movimiento, trama y puntos dentro de personas admitidas. | No es segmentación anatómica ni medida de superficie corporal. |
| PlayerIDs | Cajas y etiquetas temporales de personas admitidas. | Un ID puede perderse o cambiar con oclusiones; no nombra a la persona. |
| MotionTrails | Estelas de posiciones y del candidato a balón cuando hay datos. | Son coordenadas de imagen e incluyen efectos del movimiento de cámara. |
| FieldMap | Rejilla sobre el cuadrilátero de campo estimado. | Necesita vídeo apaisado y geometría suficiente; no mide metros. |

## Condiciones de detección

Se exige confianza de persona y apoyo de la zona de los pies sobre una región
amplia de césped. El filtro es conservador: puede omitir figuras recortadas,
planos sin superficie visible o imágenes cuyo color no permite inferirla.
Árbitros y personal sobre el campo también pueden cumplir sus condiciones.

Los clips cuyo nombre contiene `hinchada`, sin distinguir mayúsculas, quedan
excluidos del seguimiento de jugadores. Si falta el modelo, la imagen necesaria
para inferencia o la evidencia de superficie, no se utilizan manchas de
movimiento como sustituto de personas.

El seguimiento asocia detecciones y suaviza su presentación. Las estelas tienen
historia limitada; se interrumpen ante saltos o falta de continuidad y se
limpian al cambiar de clip. `FieldMap` muestra **WAITING FOR GEOMETRY** cuando
no tiene las condiciones necesarias. La ausencia de marcas es un resultado
posible del análisis, no por sí sola un fallo de reproducción.

## Uso técnico

En **Channel Editor**, el selector **SCORE** permite elegir un modo. Para
observarlo continuamente, el operador puede desactivar compositor y nube de
puntos del canal, seleccionando así la ruta gráfica del vídeo. Devolver el
selector a **Auto** restituye su selección automática y volver a habilitar
compositor y nube restituye la programación de la instalación.

**Trail seconds** ajusta la duración de las estelas y **Surface spacing** la
separación de trama. Son controles de sesión. La composición también selecciona
estos materiales en capítulos de vídeo gráfico según sus paletas de modos.
No se necesita intervenir en ellos durante una actividad de mediación.

Los contornos, máscaras y trayectorias son representaciones parciales. La
[guía pedagógica](COMO_FUNCIONA_ES.md) ofrece preguntas para observarlas y
[Algoritmos y procesos](ALGORITMOS_Y_PROCESOS_ES.md) explica sus cálculos.
