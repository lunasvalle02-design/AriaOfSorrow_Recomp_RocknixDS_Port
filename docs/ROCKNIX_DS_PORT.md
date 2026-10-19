# Port de doble pantalla para ROCKNIX (RG DS Plus): diseño técnico

## 1. Presentación en dos paneles

En ROCKNIX la RG DS / RG DS Plus ejecuta **sway**, y cada panel es una salida
Wayland (`DSI-1` es el superior). El port no usa dos ventanas: abre **una
ventana sin bordes** y, al arrancar, le pide a sway `fullscreen enable global`
(pantalla completa repartida entre todas las salidas) mediante una regla
`for_window [pid=…]` y una orden directa.

`third_party/gbarecomp/src/runtime/dual_screen.cpp` obtiene los rectángulos de
cada panel por este orden:

1. `ARIA_DS_GAME_RECT` / `ARIA_DS_AUX_RECT` = `x,y,w,h` (píxeles de ventana).
2. `swaymsg -t get_outputs -r`: el panel del juego es `ARIA_DS_GAME_OUTPUT`
   (por defecto `DSI-1`) y el otro es el auxiliar; se calculan relativos al
   rectángulo que engloba todas las salidas.
3. Si no hay sway: la ventana se parte por la mitad (en horizontal si es más
   ancha que ~5:3, apilada en vertical si no).

`HostWindow::present()` dibuja el fotograma en el panel del juego con el modo de
escalado elegido, y la pasada de ImGui dibuja la interfaz de Aria en el otro.
El menú de recomp-ui (estados de guardado, trucos, opciones) queda confinado al
panel auxiliar con `recomp_runtime_ui_set_imgui_viewport()`.

### Escalado

`240×180 → 1024×768` es ×4,267. Los modos son:

- **Suavizado** (*sharp bilinear*): copia con vecino más próximo a una textura
  intermedia de 960×720 (×4 exacto) y de ahí una sola pasada lineal a 1024×768.
  Los bordes siguen nítidos y no aparecen columnas de 4 y 5 píxeles alternas.
- **Sin filtro**: vecino más próximo directo (píxeles de ancho desigual).
- **Entero 4×**: 960×720 centrado, borde negro de 32/24 px.

El formato `256×192` cae en ×4 exacto, sin filtrado.

## 2. Filas extra en el PPU (4:3 real)

`GbaPpu` ya admitía columnas extra (pantalla panorámica). Este port añade
**filas**:

- `kMaxExtraY = 32`. El total de filas extra es fijo por sesión
  (`--view-height`, `RunOptions::max_view_height`), pero el **reparto
  arriba/abajo** se puede cambiar cada fotograma con
  `gbarecomp_set_view_vertical_split(top)`. Se aplica en la línea 0, así que un
  fotograma nunca queda partido.
- Las líneas 0–159 se dibujan como siempre, desplazadas `top` filas. Justo
  después de la línea 159 se dibujan las filas de margen: las de arriba con los
  registros capturados en la línea 0 y las de abajo con los de la línea 159. El
  desplazamiento de los fondos y las referencias afines se extrapolan.
- Las filas de margen quedan fuera de WIN0/WIN1 (se aplica WINOUT) y no tienen
  ventana de OBJ.
- **OAM**: la Y de un sprite es de 8 bits. Con `B` filas por debajo, los valores
  `160…160+B-1` se colocan abajo en lugar de envolver hacia arriba (un sprite
  de ≤64 px en `-96…-90` sería invisible de todas formas).
- `g_ws_letterbox_top/bottom` pintan los márgenes de negro (menús, escenas).
- `g_ws_bg_entry_filter` / `g_ws_obj_filter`: filtros solo de presentación que
  se usan para ocultar el HUD. La memoria del juego no se toca nunca.
- El tilemap de las filas de margen se pide a `g_ws_tilemap_provider`, que ahora
  también se consulta en filas fuera de 0–159.

Con todos los márgenes a 0 se sigue usando el renderizador original sin cambios.

## 3. Política del juego (`src/graphics/handheld_view.cpp`)

Datos sacados de la descompilación (`third_party/cvaos`):

- **Partida en curso**: `EWRAM 0x10 == 4` (`GAME_MODE_IN_GAME`) y
  `EWRAM 0x64 == 1` (subestado *field*; 4 = mapa, 6 = menú de pausa).
  *Ojo*: las cabeceras de cvaos dan offsets **absolutos** (`unk_60.unk_64` está
  en `0x64`, no en `0x60+0x64`); AriaRecomp leía y escribía `0xC4`, dentro del
  mapa de salas visitadas. La función "puertas rápidas" que lo hacía queda
  desactivada.
- **Capas**: `bgInfo[n]` (`0x0A078 + n·0x1C`) describe la capa hardware BG n:
  metadatos (ancho/alto en pantallas de 256 px, mapa de bloques, tabla de
  metatiles) y posición de cámara. La posición se alinea con el registro de
  scroll real (`AlignToScroll`) porque la variable del juego puede ir un
  fotograma por delante.
- **Tiles de los márgenes**: se reproduce `sub_08001800`/`sub_08001994`: bloque
  de 32×32 → metatile 4×4 con volteos → entrada de tilemap, con la tabla de
  metatiles en ROM o en RAM (`0x0A108` para BG1, `0x0C0EC` para las demás). BG0
  (HUD, diálogos, rótulos) nunca se amplía.
- **Banda dibujable**: cada pantalla del mapa mide 256 px, pero la cámara nunca
  baja de y=48 ni pasa de (alto−208). Las filas justo fuera de ese rango
  continúan la sala; solo el borde del bloque más externo es relleno sin
  terminar. La ventana 4:3 se limita a `[32, alto−16)` (16 filas de margen
  posible por cada lado; el 240×180 usa 10) y el reparto arriba/abajo se ajusta
  para no salir de esa banda: una cámara 4:3 real limitada a la sala.
- **Enemigos**: el juego crea entidades a ±80 px de la cámara
  (`sub_0800F4F8`/`sub_0800F5BC`) y las dibuja con y entre −32 y 224
  (`sub_0803AC40`). Las que caen en las franjas nuevas ya existen y ya están en
  la OAM: el decodificador ampliado de OAM las coloca en su sitio.
- **HUD**: BG0, filas 0–4 y columnas 0–12 del mapa, paleta 13, tiles
  0x200–0x24F (marco y barras), más los dígitos de PV: OBJ de 8×8, paleta 7,
  tiles 0x80–0x8F, en la esquina superior izquierda.

## 4. Pantalla inferior (`src/handheld/handheld_ui.cpp`)

- **ESTADO**: nivel, PV/PM con barras, EXP, oro, FUE/CON/INT/SUE, último enemigo
  derrotado y si ya tienes su alma (`QolSystem`), y un mapa de 13×11 casillas
  centrado en el jugador.
- **MAPA**: el castillo completo (caja de las casillas existentes de la tabla de
  salas de la ROM, `0x08116650`) o 25×17 casillas alrededor del jugador; un
  toque cambia de modo. Se dibujan las casillas vistas o visitadas (planos de
  `EWRAM 0xB4`), en rojo las de guardado y en amarillo las de teletransporte, con
  paredes donde la casilla vecina pertenece a otra sala. Posición del jugador:
  `roomMapXPos/YPos` (`0x8C`) + (posición en la sala ≫ 8).
- **AJUSTES**: escalado, formato (al reiniciar), HUD original, pantalla del
  juego (intercambiar), contador de FPS, *Más opciones* (menú de recomp-ui) y
  *Salir*.

La fuente es DejaVu Sans (`fonts/DejaVuSans.ttf`, incluida en el paquete y
cargada con `GBARECOMP_UI_FONT`).

## 5. Compilación para ROCKNIX

- `cmake/aarch64-linux-gnu.cmake` + `tools/docker/build-rocknix.sh`: Debian
  bookworm, `-march=armv8.2-a -mtune=cortex-a55`, libstdc++/libgcc estáticas.
- `RECOMP_UI_GLES2=ON`: el lanzador de recomp-ui, que este port no usa pero se
  compila igualmente, enlaza GLES2 en vez de `libGL.so.1`, que los drivers Mali
  de ROCKNIX no traen.
- `GBARECOMP_SELFHEAL_RECOMPILE_DEFAULT=OFF`: en la consola no hay compilador.
  Si se cae en una dirección no traducida (prácticamente solo tras cargar un
  estado guardado a mitad de una función), la ejecuta el intérprete.

## Pendiente de verificar en el dispositivo

Nada de esto se ha podido comprobar sin una RG DS Plus:

1. **Nombres y disposición de las salidas de sway.** Si el juego sale en el
   panel equivocado, usa *Pantalla del juego → Inferior* o exporta
   `ARIA_DS_GAME_OUTPUT=<nombre>` en el lanzador (`swaymsg -t get_outputs` lo
   muestra).
2. **`fullscreen enable global`** con la configuración de sway de ROCKNIX. Si la
   ventana no se extiende a los dos paneles, fija los rectángulos con
   `ARIA_DS_WINDOW`, `ARIA_DS_GAME_RECT` y `ARIA_DS_AUX_RECT`.
3. **Táctil del panel inferior.** Debe estar asociado al seat de sway y mapeado a
   su salida (`input … map_to_output`). Sin táctil, todo lo de AJUSTES está
   también en el menú de recomp-ui (L+R+Start).
4. **Rendimiento en el Cortex-A55.** En PC el modo 4:3 cuesta ~15 % más que el
   original (3000 fotogramas: 10,0 s frente a 8,7 s). Activa el contador de FPS;
   el objetivo es 59,7.
5. **Mapeo de botones** con la base de datos de mandos de ROCKNIX.
