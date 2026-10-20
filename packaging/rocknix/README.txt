Castlevania: Aria of Sorrow — port de doble pantalla para ROCKNIX
=================================================================

Instalación
-----------
1. Copia "Aria of Sorrow DS.sh" y la carpeta "ariaofsorrowds" a
   /storage/roms/ports/ (carpeta "ports" de la tarjeta de juegos).
2. Copia TU volcado legal de la ROM a la carpeta "ariaofsorrowds" con el
   nombre "Castlevania - Aria of Sorrow (USA).gba".
   (Solo vale la versión USA, SHA-1 abd71fe01ebb201bcc133074db1dd8c5253776c7.)
3. Reinicia EmulationStation (o actualiza la lista de juegos) y lanza
   "Aria of Sorrow DS" desde Ports.

Pantallas
---------
- Superior: el juego en 4:3 (240x180, o 256x192 en AJUSTES > Formato). Las
  filas y columnas extra son escenario y enemigos reales de la sala, no
  relleno. Solo se muestra lo que la cámara del juego puede llegar a enseñar:
  en salas de una sola pantalla de alto (muchos pasillos) no hay filas extra
  y quedan franjas negras.
- Inferior: estado (vida, magia, nivel, oro...), mapa en tiempo real y
  ajustes. Se maneja tocando la pantalla (el táctil se asocia a todo el
  escritorio de sway, como hace RetroArch en ROCKNIX, para que la calibración
  de ROCKNIX lo lleve al panel inferior; ARIA_DS_TOUCH_OUTPUT lo cambia).

Controles
---------
- Los botones del juego son los de la GBA (A, B, L, R, Start, Select).
- Salir del juego: L1 + Start + Select.
- Menú completo (opciones, guardar/cargar estados, trucos): L + R + Start,
  o L3 / R3 / tecla de función si la consola la tiene, o "Más opciones" en
  la pestaña AJUSTES de la pantalla inferior.

Escalado
--------
Por defecto "Suavizado": escala entera 4x y un último paso lineal muy corto
hasta 1024x768, para que no se vean columnas de píxeles de distinto ancho.
"Sin filtro" usa vecino más próximo y "Entero 4x" deja un borde negro.

Archivos
--------
- aria_config.ini: ajustes (sección [Handheld]).
- log.txt: registro de la última ejecución.
- *.sav junto a la ROM: partida guardada.
