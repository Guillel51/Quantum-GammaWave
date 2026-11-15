# La página ARP y su reloj

La página ARP ya dibujaba el teclado con las teclas pulsadas y la nota que suena, y la ruta que el arp construye
(los ocho primeros pasos, el actual encendido, `actual/total`). Faltaba ver **el tiempo**: dónde va el paso dentro
del paso, y dónde cierra el GATE. Eso es lo que se añadió en la Fase 8.

## Qué se dibuja

- **La ruta** (`ROUTE`): los pasos que el arp ha construido con las teclas mantenidas, en el orden de la lista
  (ORDER o el que se pulsó), con las octavas de OCT y las reglas de cada modo (`REPEAT`, `UP8`, `CHORD`). Se ven
  ocho; si el paso actual pasa del octavo, la ventana se desplaza (`+n` en el contador `actual/total`).
- **Las notas que está tocando**: sus nombres junto a `ROUTE`, en color de acento (`E4`, y en CHORD los
  acompañantes de `arp_ch`), más la tecla encendida en el teclado de dos octavas. Todo sale de `arp_note` y
  `arp_ch`, que es lo que el motor está sonando de verdad, no de lo que debería sonar.
- **El reloj**: una franja de 2 px encima de cada paso visible. El paso que suena **se llena** de izquierda a
  derecha con el tiempo transcurrido del paso, y un **tick** de 1 px marca dónde cierra el GATE (el resto hasta el
  final del paso es silencio). Con `CHORD` se llena en todos porque suenan todos.
- Si el transporte está parado, la franja se queda donde se paró: no hay reloj que seguir.

## De dónde salen los números

Nada nuevo en el motor: `arp_pos` (muestras dentro del paso) y `arp_off` (muestras que le quedan al note-off) son
los contadores que `seq.c` (`arp_step`) ya mueve, y el paso es `div_samples(P_ARATE)`, el mismo que usa el arp para
decidir cuándo avanza. La franja es `arp_pos / paso` píxeles sobre 24 y el tick cae en
`(arp_pos + arp_off) / paso`, así que **el swing y el GATE se ven tal como suenan**, sin recalcular nada en la
interfaz ni guardar estado propio. `arp_pos` se recorta al paso por si el centinela de arranque (`0xFFFFFFF`) llega
a la pantalla en el primer bloque.

## Verificación

- **Host, píxeles**: `ui_render` dibuja la pantalla `arp` con el reloj a un tercio del paso y el GATE abierto:
  190 pantallas × 22 paletas con **0 hallazgos de lint**, **0 desalineaciones** en 3 551 691 mediciones. La franja
  se ve sobre la nota actual y el tick dentro del paso.
- **Host, motor**: `midi_control_test` pasa; es el test que ya comprueba que el arp avanza con el reloj y que el
  GATE termina en tiempo real (`arp_off` bajando bloque a bloque), o sea, que lo que la franja lee se mueve.
- **No verificado**: no se ha visto en el FM-1 físico (no está conectado y no se escribe en él) ni en el emulador
  WASM (falta `emcc`), así que el aspecto en la pantalla real y el coste de los píxeles extra en el AC791N no están
  medidos. Lo que sí está medido es el host.

## Lo que no se hizo

- No se dibuja la **velocidad** ni la probabilidad de cada paso: la ruta es de notas, y PROB decide si el paso
  suena (si no suena, el paso se ve apagado pero la franja sigue corriendo, que es lo que hace el reloj).
- No se añadió una vista de la rejilla del secuenciador: la página ROLL ya la tiene.
