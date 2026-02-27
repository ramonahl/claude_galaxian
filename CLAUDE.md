# Galaxian Clone

## Build
```
g++ -std=c++17 -O2 -o galaxian main.cpp $(pkg-config --cflags --libs raylib) -lm
g++ -std=c++17 -O2 -o sprite_tool tools/sprite_tool.cpp $(pkg-config --cflags --libs raylib) -lm
```

## Run
```
./galaxian
./sprite_tool --input ruta/a/sprite.png --name SPR_NUEVO --output /tmp/spr_nuevo.h --preview /tmp/spr_nuevo.png
./sprite_tool --input ruta/a/sprite.png --name SPR_NUEVO --colorkey 0,0,0 --output /tmp/spr_nuevo.h --preview /tmp/spr_nuevo.png
# fit-mode: source (default) mantiene proporcion del lienzo original; tight lo hace ocupar mas area
./sprite_tool --input ruta/a/sprite.png --name SPR_NUEVO --colorkey 0,0,0 --fit-mode source --output spr_nuevo.h --preview spr_nuevo_preview.png
```

## Project Structure
- `main.cpp` — Single-file game, ~600 lines, C++17 + Raylib 5.5
- All sprites are filled polygons (no textures)
- `tools/sprite_tool.cpp` — Convierte imagen a sprite indexado (`uint8_t[][]`) usando la paleta del juego y genera preview PNG
  - Recorta automaticamente el fondo no visible y centra el sprite en el lienzo final

## Session Log
<!-- Actualizar aquí al final de cada sesión para trackear progreso -->

### 2026-02-25
- Proyecto funcional: enemigos en formación, diving con Bezier, starfield, disparo del jugador
- Se creó este CLAUDE.md para tracking entre sesiones
- Se añadió `sprite_tool` para convertir imágenes a arrays de píxeles indexados con preview visual

### 2026-02-26
- `main.cpp` migrado de sprites por arrays (`uint8_t`) a texturas PNG cargadas en runtime desde `sprites_new/`.
- Sprites activos:
  - `sprites_new/player1.png`
  - `sprites_new/enemy1.png`
  - `sprites_new/enemy2.png`
  - `sprites_new/enemy3.png`
- Enemigos reducidos de tamaño para evitar solape.
- Formación cambiada a 4 filas con menos enemigos arriba:
  - fila 1: 2 (`enemy1`)
  - fila 2: 6 (`enemy1`)
  - fila 3: 8 (`enemy2`)
  - fila 4: 10 (`enemy3`)
- Ataques diving ajustados:
  - sin homing continuo al jugador (objetivo se fija al iniciar el dive con variación aleatoria).
  - balas enemigas ligeramente menos agresivas.
- Ventana ahora redimensionable y render escalado con letterbox (resolución lógica interna 480x720).
- Sistema de power-ups implementado:
  - drops al morir enemigos (probabilidad aproximada 23%).
  - tipos: `FIRE_RATE`, `DOUBLE_SHOT`, `TRIPLE_SHOT`.
  - jugador ahora usa disparo con cooldown y niveles 1/2/3 (single/double/triple).
- HUD muestra nivel de disparo actual (`SHOT xN`).
- Compilación validada: `g++ -std=c++17 -O2 -o galaxian main.cpp $(pkg-config --cflags --libs raylib) -lm`.
- Pendiente para mañana: balancear dificultad (frecuencia/selección de dives, puntería y velocidad de balas enemigas, progresión por ronda y tuning de drops/power-ups).
