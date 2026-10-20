# Port de doble pantalla para ROCKNIX (RG DS Plus): diseño técnico

## 1. Presentación en dos paneles

En ROCKNIX la RG DS / RG DS Plus ejecuta **sway**, y cada panel es una salida
Wayland: **`DSI-2` es el superior y `DSI-1` el inferior táctil** (pantalla Goodix;
así lo configura la `sway-config.theme` de ROCKNIXDS). El port no usa dos ventanas: abre **una
ventana sin bordes** y, al arrancar, le pide a sway `fullscreen enable global`
(pantalla completa repartida entre todas las salidas) mediante una regla
`for_window [pid=…]` y una orden directa.

`third_party/gbarecomp/src/runtime/dual_screen.cpp` obtiene los rectángulos de
cada panel por este orden:

1. `ARIA_DS_GAME_RECT` / `ARIA_DS_AUX_RECT` = `x,y,w,h` (píxeles de ventana).
2. `swaymsg -t get_outputs -r`: el panel del juego es `ARIA_DS_GAME_OUTPUT`
   (por defecto `DSI-2`) y el otro es el auxiliar; se calculan relativos al
   rectángulo que engloba todas las salidas.
3. Si no hay sway: la ventana se parte por la mitad (en horizontal si es más
   ancha que ~5:3, apilada en vertical si no).

### Táctil

ROCKNIX instala para la RG DS / RG DS Plus una regla udev
(`quirks/devices/Anbernic RG DS Plus/touchcontrol`) con
`LIBINPUT_CALIBRATION_MATRIX="0.5 0 0.5 0 1 0"`: la calibración ya lleva el
táctil a la mitad derecha del escritorio, es decir, a `DSI-1`. Solo cae en su
sitio si el dispositivo está asociado a **todo** el escritorio
(`map_to_output *`, lo mismo que hace ROCKNIXDS para RetroArch). La primera
versión del port lo asociaba a `DSI-1` y la calibración lo comprimía otra vez:
un toque en x llegaba a 512 + x/2, unos 2,5 cm a la derecha en el centro. Ahora
`dual_screen.cpp` ejecuta una vez `swaymsg 'input type:touch map_to_output *'`
(o la salida de `ARIA_DS_TOUCH_OUTPUT`). El digitalizador va pegado al panel
inferior, así que el mapeo no depende de *Pantalla del juego*.

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
- **Extensión con contenido**: cada pantalla del mapa mide 256 px, pero la
  cámara nunca sube de y=48 ni baja de (alto−208), así que cada capa solo
  garantiza contenido en `x ∈ [0, ancho)`, `y ∈ [48, pantallas·256−48)`. Fuera
  de eso hay datos que el juego nunca enseña: el fondo fijo de una pantalla
  (240×208) repite basura del anillo de VRAM pasada la columna 240 y fuera de
  las filas 48–207 (la luna duplicada y los bloques cian de la primera versión),
  y el mapa de bloques de la sala tiene relleno sin terminar bajo el suelo. El
  proveedor devuelve `kWsTilemapVoid` en esas muestras y el PPU pinta la capa
  de negro opaco a su prioridad: los sprites y capas delanteras siguen encima y
  no se cuela nada de detrás.
- **Ventana 4:3**: limitada a la extensión de BG1. Junto al borde, todas las
  filas extra pasan al otro lado. Si la sala es más baja que la ventana (un
  pasillo de una pantalla de alto mide justo 160 filas), se muestra lo que hay
  y el resto queda en negro, centrado. Las columnas extra del 256×192 usan la
  misma regla, con barras negras en el lado que se sale de la sala.
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

## 6. Rendimiento

Medido en PC (3000 fotogramas, sin ventana): componer las 160 líneas era la
mitad del tiempo total. Cambios, todos con salida idéntica píxel a píxel
(comprobado con 99 fotogramas en ambos modos):

- **Compositor por tiles**: la entrada del mapa, volteos, banco de paleta y fila
  del tile se resuelven una vez cada 8 px; la ventana de capas, una vez por
  línea. 8,7 → 6,3 µs por línea.
- **Las 160 líneas nativas por el compositor original** cuando no hay columnas
  extra; el compositor ancho solo para las 20 filas de margen, con caché por
  tile de las consultas al mapa de la sala.
- **Composición asíncrona** (`GbaPpu::set_async_render`, `RunOptions::async_ppu`,
  `[Handheld] asyncRender`): cada línea guarda sus registros y, si cambiaron
  desde la línea anterior, una copia de OAM y paleta. Al empezar el VBlank se
  copia la VRAM y otro hilo compone las 240 columnas nativas. Las columnas y
  filas de margen se componen en el hilo del juego, porque consultan estado
  vivo. No reproduce escrituras a VRAM a mitad de fotograma.
- **256×192**: las 240 columnas nativas de cada línea van por el compositor
  nativo, escritas en su sitio dentro de la fila de 256, y por tanto también
  fuera del hilo del juego. El compositor ancho solo hace las 8+8 columnas
  laterales (`margins_only`) y las filas de margen. En PC, 3000 fotogramas:
  11,1 → 8,8 µs por línea en síncrono y 2,5 µs en el hilo del juego con la
  composición asíncrona (antes no podía usarse en este formato).
- **Pantalla inferior a 20 Hz** desde una textura en caché
  (`GBARECOMP_AUX_UI_DIVIDER`, por defecto 3).
- **Audio**: colchón de 100 ms (`[Handheld] audioLatencyMs`,
  `GBARECOMP_AUDIO_TARGET_MS`).

Lo que queda en el hilo del juego es el propio código recompilado y la
contabilidad de ciclos del runtime (≈10 % de las instrucciones), que es parte
del núcleo de emulación y no se ha tocado.

## Pendiente de verificar en el dispositivo

Nada de esto se ha podido comprobar sin una RG DS Plus:

1. **Nombres de las salidas de sway.** Corregido a `DSI-2` (arriba) tras la
   primera prueba en la consola. Si aun así sale al revés, usa *Pantalla del
   juego → Inferior* o exporta `ARIA_DS_GAME_OUTPUT=<nombre>` en el lanzador; el
   `log.txt` muestra las salidas que detecta.
2. **`fullscreen enable global`** con la configuración de sway de ROCKNIX. Si la
   ventana no se extiende a los dos paneles, fija los rectángulos con
   `ARIA_DS_WINDOW`, `ARIA_DS_GAME_RECT` y `ARIA_DS_AUX_RECT`.
3. **Táctil del panel inferior.** Corregido tras la segunda prueba (ver
   *Táctil*). Si sigue desplazado en tu sistema, prueba
   `ARIA_DS_TOUCH_OUTPUT=DSI-1` en el lanzador. Sin táctil, todo lo de AJUSTES
   está también en el menú de recomp-ui (L+R+Start).
4. **Rendimiento en el Cortex-A55.** En PC el modo 4:3 cuesta ~15 % más que el
   original (3000 fotogramas: 10,0 s frente a 8,7 s). Activa el contador de FPS;
   el objetivo es 59,7.
5. **Mapeo de botones** con la base de datos de mandos de ROCKNIX.
