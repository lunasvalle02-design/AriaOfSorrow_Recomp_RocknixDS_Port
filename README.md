# Aria of Sorrow DS — port de doble pantalla para ROCKNIX

Port de **Castlevania: Aria of Sorrow (USA, GBA)** a **ROCKNIX** pensado para la
**Anbernic RG DS Plus** (dos pantallas IPS de 4,3" a 1024×768, RK3568).
Parte de [AriaRecomp](https://github.com/sergiomanzur/ariaOfSorrow-recomp), una
**recompilación estática**: el código ARM del cartucho se traduce a C++ nativo
de antemano, así que en la consola se ejecuta código ARM64 nativo, sin emulador.

> No contiene ROM, BIOS ni código traducido del juego. Necesitas tu propio
> volcado legal de la versión USA (SHA-1 `abd71fe01ebb201bcc133074db1dd8c5253776c7`),
> y el binario que compiles con él es solo para uso personal. Consulta el
> apartado [Licencias](#licencias).

## Qué añade este port

| | |
|---|---|
| **Dos pantallas reales** | Una ventana ocupa las dos salidas de sway: el juego se dibuja en el panel superior (`DSI-2`) y la interfaz en el inferior táctil (`DSI-1`). El táctil se asocia a todo el escritorio para que la calibración de ROCKNIX lo lleve al panel inferior. |
| **Pantalla superior en 4:3 de verdad** | El juego se renderiza a **240×180** en lugar de 240×160. Las 20 filas extra no son relleno: salen del mapa de bloques real de la sala, con todas sus capas, y los sprites que el juego ya mantiene vivos fuera de su cámara (enemigos, proyectiles, objetos) aparecen en esas franjas en vez de entrar de golpe. |
| **Cámara 4:3 ajustada a la sala** | Solo se muestra lo que la cámara del juego puede llegar a enseñar de cada capa; fuera de eso, negro, nunca datos repetidos de la VRAM. Las filas extra se reparten cada fotograma entre arriba y abajo (junto al techo o al suelo, toda la ampliación pasa al otro lado). En pasillos de una pantalla de alto no hay filas extra y quedan franjas negras. Menús, mapa del juego y escenas: bandas negras. |
| **Pantalla inferior** | Pestañas táctiles **ESTADO** (vida, magia, nivel, EXP, oro, atributos, último enemigo y si tienes su alma, y el mapa en tiempo real alrededor del jugador), **MAPA** (castillo completo o ampliado; toca para cambiar) y **AJUSTES**. |
| **HUD original sustituido** | Las barras de vida/magia y el contador de PV desaparecen de la pantalla superior (opcional). |
| **Escalado** | *Suavizado* (por defecto): escalado entero 4× y un último paso lineal muy corto hasta 1024×768, para disimular que 768/180 = 4,27 no es entero. *Sin filtro*: vecino más próximo. *Entero 4×*: 960×720 con borde. |
| **Otros formatos** | `4:3 256×192` (amplía también 8 px por lado y encaja exacto a 4× en 1024×768) y `Original 3:2`. |

## Instalación en la consola

1. Compila el paquete (siguiente apartado) o usa el que hayas generado:
   `out/rocknix/AriaOfSorrowDS-rocknix-aarch64.zip`.
2. Descomprime su carpeta `ports/` en `/storage/roms/ports/` de la consola.
3. Copia tu ROM a `ports/ariaofsorrowds/Castlevania - Aria of Sorrow (USA).gba`.
4. Actualiza la lista de juegos y abre **Aria of Sorrow DS** desde *Ports*.

Controles: los de la GBA. Menú completo (estados de guardado, trucos, opciones):
**L + R + Start**, L3/R3 o la tecla de función, o *Más opciones* en AJUSTES.
**Salir del juego: L1 + Start + Select** (la partida en SRAM se guarda al salir).

### Rendimiento

- La composición de la imagen se hace en otro núcleo del RK3568 mientras el
  juego avanza al siguiente fotograma (`asyncRender`, un fotograma de latencia),
  también en 256×192: ahí el hilo del juego solo compone las columnas y filas
  de margen.
- El compositor de líneas trabaja por tiles (≈27 % menos de tiempo por línea,
  resultado idéntico píxel a píxel).
- La pantalla inferior se redibuja a 20 Hz desde una textura en caché (al
  instante al tocarla).
- Colchón de audio de 100 ms (`audioLatencyMs`) para que un fotograma lento no
  provoque chasquidos.

Si algún efecto gráfico se viera mal, pon `asyncRender = false` en la sección
`[Handheld]` de `aria_config.ini`.

## Compilar

Todo se compila en Docker (en Windows, con Docker Desktop), con Debian bookworm
y su compilador cruzado `aarch64-linux-gnu`:

```sh
git clone --recurse-submodules <este repo>
cd <repo>
mkdir rom && cp "/ruta/Castlevania - Aria of Sorrow (USA).gba" rom/
docker build -t aria-rocknix-build tools/docker
docker volume create aria-build
# 1. Traduce TU ROM a C++ (recomp_out/, nunca se sube al repo)
docker run --rm -v "$PWD:/src" -v aria-build:/build aria-rocknix-build bash /src/tools/docker/recompile.sh
# 2. Compila para ROCKNIX (aarch64) y empaqueta
docker run --rm -v "$PWD:/src" -v aria-build:/build aria-rocknix-build bash -c \
  "bash /src/tools/docker/build-rocknix.sh && bash /src/tools/docker/package-rocknix.sh"
```

El binario depende solo de `libSDL2-2.0.so.0`, `libGLESv2.so.2` y glibc ≥ 2.36,
todo presente en ROCKNIX; libstdc++ va enlazada estáticamente.

Para probar en el PC hay una compilación x86_64 y utilidades en `tools/dev/`:
`build-host.sh` compila, `headless.sh` ejecuta sin ventana con entradas
grabadas y vuelca fotogramas, y `windowed.sh` arranca el modo de doble pantalla
en un X virtual de 1024×1536 y hace capturas. El parámetro `--handheld` activa
el modo de doble pantalla en cualquier plataforma.

## Cómo funciona

Detalle técnico en [docs/ROCKNIX_DS_PORT.md](docs/ROCKNIX_DS_PORT.md). En resumen:

- `third_party/gbarecomp` (PPU): admite filas por encima de la línea 0 y por
  debajo de la 159, con reparto arriba/abajo variable por fotograma y filtros de
  presentación para ocultar el HUD. `src/runtime/dual_screen.*` detecta los
  paneles con `swaymsg` y presenta el juego en uno de ellos.
- `src/graphics/handheld_view.*`: la política del juego (cuándo hay sala, dónde
  está la cámara, qué filas existen, qué es HUD) y el proveedor de tiles leído
  del mapa de bloques de la sala.
- `src/handheld/handheld_ui.*`: la pantalla inferior (Dear ImGui, táctil).

## Estado

Probado en PC (x86_64, Xvfb) y arrancado en ARM64 emulado. **No se ha probado
aún en una RG DS Plus real**: el nombre de las salidas de sway, el táctil del
panel inferior y el rendimiento en el Cortex-A55 deben confirmarse en el
dispositivo (activa *Contador de FPS* en AJUSTES). Ver la lista de puntos
pendientes en [docs/ROCKNIX_DS_PORT.md](docs/ROCKNIX_DS_PORT.md#pendiente-de-verificar-en-el-dispositivo).

## Licencias

- `third_party/gbarecomp` (motor de recompilación y runtime): **PolyForm
  Noncommercial 1.0.0**, de Matthew Stan. Por enlazarlo, todo el proyecto es
  **solo para uso no comercial**.
- `third_party/recomp-ui` y `arm-recomp-core`: MIT. `third_party/cvaos`
  (submódulo, descompilación de referencia): MIT.
- *Castlevania: Aria of Sorrow* es propiedad de Konami. Proyecto de fans sin
  relación con Konami ni Nintendo.
- El README original de AriaRecomp está en [docs/UPSTREAM_README.md](docs/UPSTREAM_README.md).
