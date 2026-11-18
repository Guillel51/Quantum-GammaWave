# v1.4.16 no arrancó: qué puede haberlo roto

**Qué pasó.** El dueño instaló v1.4.16 en su FM-1 y la unidad no volvió a arrancar. v1.4.16 se compiló con `-flto`
(Fase 6) y es la única versión publicada con esa bandera. No hay unidad en este entorno: nada de lo que sigue está
confirmado en hardware, y así está dicho.

**Qué se hizo en el acto.** v1.4.17 quita `-flto` y vuelve exactamente a las banderas con las que corren las
unidades (imagen 583 036 B, la misma de 1.4.14/1.4.15). El sitio deja de ofrecer 1.4.16.

## Los mecanismos por los que LTO puede romper este firmware

1. **Direcciones que el arranque estampa.** `crt0` copia `.ram_text`, `.c1_text` y `.hot_text` a RAM usando símbolos
   (`_rt_load`, `_c1t_load`, `_hot_load`) y el núcleo 1 salta a `_c1_start`. El build verifica alineación y no
   solapamiento, pero **no** que esas direcciones sigan siendo las que espera el arranque del SDK. Con LTO el ELF
   cambia de forma: el mapa deja de tener secciones por función (lo medimos con `engine_size.py`: `.text.<fn>`
   desaparece), así que el orden dentro de `.text` no es el de antes. Un salto a una dirección corrida es un cuelgue
   sin USB y sin pantalla: exactamente el síntoma.
2. **Reordenamiento del código que corre desde RAM.** Las funciones de `HOT_FNS` se parten a `.hot_text` por
   atributo de sección; con LTO el optimizador ve el módulo entero y puede duplicar o mover cuerpos. El build
   comprueba que las encuentra todas, no que cada una siga en su sección.
3. **El loader también se compiló con `-flto`.** `ota.bin` es el que escribe el flash; si el loader cambia de
   comportamiento, el estado del flash después del install no es el que el instalador cree.
4. **Un toolchain de 2021 haciendo LTO nuevo.** clang 4.0.1 + GNU ld 2.26 + `LLVMgold.so` de 2021. Cualquier
   comportamiento indefinido del firmware (alias, acceso no alineado, lectura de memoria sin inicializar) que sin
   LTO caía del lado bueno puede caer del malo. No es una hipótesis favorita, pero es la razón por la que un build
   que "compila y pasa los tests host" no dice nada del binario final.

## Lo que está descartado

- **El tamaño no era el problema**: 578 840 B de 583 612, con el pool y la RAM dentro de presupuesto, y el build
  habría fallado antes de empaquetar si algo no cabía.
- **Los tests host no vieron nada porque no podían verlo**: 303 dorados, 3 056 cuadros, la suite entera en verde.
  Nada de eso ejecuta el binario de la unidad; es la lección, no una excusa.
- **La tabla del panel y el arranque**: entre 1.4.15 y 1.4.16 no cambió `lcd.c`, ni `main.c`, ni el loader, ni el
  head de arranque. El candidato es el compilador, no la lógica.

## Lo que falta para confirmarlo

Con una unidad que arranque: instalar un build con `-flto` y leer `_c1_start`, `_hot_load` y `_rt_load` del mapa
contra los del build sin LTO; si difieren del contrato del arranque, el mecanismo 1 queda probado. Sin unidad no se
afirma. Mientras tanto LTO no vuelve.
