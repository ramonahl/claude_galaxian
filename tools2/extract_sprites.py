#!/usr/bin/env python3
"""
extract_sprites.py
------------------
Extrae sprites individuales de una imagen con fondo negro.
Genera PNGs con fondo transparente y tamaño normalizado (canvas igual).

USO BÁSICO:
    python extract_sprites.py imagen.png

OPCIONES:
    --threshold N         Umbral para eliminar fondo negro del resultado final (default 6).
                          Bajo = conserva negros del sprite. Solo negro casi puro se elimina.
    --detect-threshold N  Umbral solo para detectar/separar sprites (default 20).
                          Puede ser alto sin afectar el color final.
    --padding N       Píxeles de margen alrededor de cada sprite (default 6).
    --normalize       Iguala el canvas de todos los sprites al tamaño del mayor.
    --size WxH        Canvas fijo para cada sprite (ej: 500x500). Implica --normalize.
                      Si el sprite es más grande que el canvas, se escala para encajar.
    --erode N         Erosión del alpha en píxeles para eliminar halo de borde (default 1).
                      Sube a 2-3 si queda halo blanco/gris en los bordes. 0 = desactivado.
    --min-area N      Área mínima en píxeles para detectar un sprite (default 100).
    --output-dir DIR  Directorio de salida (default: mismo directorio que la imagen).

EJEMPLOS:
    # Extraer sprites con canvas de 500x500 y fondo transparente
    python extract_sprites.py enemyCangrejoTile.png --threshold 20 --size 500x500

    # Extraer y normalizar al tamaño del sprite más grande
    python extract_sprites.py spritesheet.png --normalize --output-dir ../sprites_new/

    # Si quedan restos de fondo, sube el threshold
    python extract_sprites.py spritesheet.png --threshold 40 --size 500x500

NOTAS:
    - La imagen fuente debe tener fondo negro (o casi negro).
    - Los sprites se ordenan de izquierda a derecha.
    - La salida siempre tiene canal alpha (fondo transparente).
"""

import argparse
import sys
from pathlib import Path

import cv2
import numpy as np
from PIL import Image


def remove_black_background(img_bgra: np.ndarray, threshold: int = 15) -> np.ndarray:
    """
    Elimina el fondo negro usando componentes conectados.
    Solo borra los grupos de píxeles oscuros que tocan el borde de la imagen,
    preservando los negros interiores del sprite.
    """
    b, g, r, a = cv2.split(img_bgra)
    brightness = np.maximum(np.maximum(r, g), b)

    # Máscara binaria: 1 = oscuro (candidato a fondo), 0 = color
    dark = (brightness <= threshold).astype(np.uint8)

    # Componentes conectados sobre los píxeles oscuros (4-conectividad = igual que varita mágica de Paint)
    num_labels, labels = cv2.connectedComponents(dark, connectivity=4)

    # Identificar qué componentes tocan el borde de la imagen
    border_labels = set()
    border_labels.update(labels[0, :].tolist())    # borde superior
    border_labels.update(labels[-1, :].tolist())   # borde inferior
    border_labels.update(labels[:, 0].tolist())    # borde izquierdo
    border_labels.update(labels[:, -1].tolist())   # borde derecho
    border_labels.discard(0)  # 0 = píxeles no oscuros, no es fondo

    # Borrar solo los componentes conectados al exterior
    if border_labels:
        background_mask = np.isin(labels, list(border_labels))
        a[background_mask] = 0

    return cv2.merge([b, g, r, a])


def find_sprite_bboxes(alpha: np.ndarray, min_area: int = 100):
    """
    Encuentra bounding boxes de sprites usando análisis de componentes conectados.
    Devuelve lista de (x, y, w, h) ordenados de izquierda a derecha.
    """
    _, binary = cv2.threshold(alpha, 0, 255, cv2.THRESH_BINARY)
    # Pequeña dilatación para unir píxeles sueltos del mismo sprite
    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (5, 5))
    dilated = cv2.dilate(binary, kernel, iterations=2)

    num_labels, labels, stats, _ = cv2.connectedComponentsWithStats(dilated, connectivity=8)

    bboxes = []
    for i in range(1, num_labels):  # 0 = fondo
        x, y, w, h, area = stats[i]
        if area >= min_area:
            bboxes.append((x, y, w, h))

    # Ordenar de izquierda a derecha
    bboxes.sort(key=lambda b: b[0])
    return bboxes


def crop_with_padding(img_rgba: np.ndarray, bbox: tuple, padding: int = 6):
    """Recorta un sprite con padding, respetando los bordes de la imagen."""
    x, y, w, h = bbox
    ih, iw = img_rgba.shape[:2]
    x1 = max(0, x - padding)
    y1 = max(0, y - padding)
    x2 = min(iw, x + w + padding)
    y2 = min(ih, y + h + padding)
    return img_rgba[y1:y2, x1:x2]


def normalize_canvas(sprites: list[np.ndarray], fixed_size: tuple[int, int] | None = None) -> list[np.ndarray]:
    """
    Pone todos los sprites en un canvas del mismo tamaño, centrados, con fondo transparente.
    Si fixed_size=(w, h), usa ese tamaño fijo; si no, usa el mayor sprite.
    """
    if fixed_size:
        max_w, max_h = fixed_size
    else:
        max_w = max(s.shape[1] for s in sprites)
        max_h = max(s.shape[0] for s in sprites)
    result = []
    for s in sprites:
        canvas = np.zeros((max_h, max_w, 4), dtype=np.uint8)
        sh, sw = s.shape[:2]
        # Escalar si el sprite es mayor que el canvas
        if sw > max_w or sh > max_h:
            scale = min(max_w / sw, max_h / sh)
            new_w, new_h = int(sw * scale), int(sh * scale)
            s = cv2.resize(s, (new_w, new_h), interpolation=cv2.INTER_AREA)
            sh, sw = s.shape[:2]
        off_x = (max_w - sw) // 2
        off_y = (max_h - sh) // 2
        canvas[off_y:off_y + sh, off_x:off_x + sw] = s
        result.append(canvas)
    return result


def main():
    parser = argparse.ArgumentParser(description="Extrae sprites de imagen con fondo negro")
    parser.add_argument("image", help="Imagen fuente (PNG, BMP, etc.)")
    parser.add_argument("--threshold", type=int, default=6,
                        help="Umbral para eliminar fondo negro (0-255, default 6). "
                             "Bajo = conserva negros del sprite. Solo puro negro se elimina.")
    parser.add_argument("--detect-threshold", type=int, default=20,
                        help="Umbral para DETECTAR/separar los sprites (default 20). "
                             "Puede ser más alto que --threshold sin afectar al color final.")
    parser.add_argument("--padding", type=int, default=6,
                        help="Píxeles de padding alrededor de cada sprite (default 6)")
    parser.add_argument("--normalize", action="store_true",
                        help="Igualar el canvas de todos los sprites al tamaño del mayor")
    parser.add_argument("--size", type=str, default=None,
                        help="Canvas fijo WxH para cada sprite, ej: 500x500 (implica --normalize)")
    parser.add_argument("--fill-holes", type=int, default=15,
                        help="Radio en píxeles para rellenar huecos oscuros interiores del sprite (default 15). "
                             "Sube si quedan agujeros negros dentro del sprite.")
    parser.add_argument("--erode", type=int, default=0,
                        help="Erosión del alpha en píxeles para eliminar halo de borde (default 0 = desactivado)")
    parser.add_argument("--min-area", type=int, default=100,
                        help="Área mínima en píxeles para considerar un sprite (default 100)")
    parser.add_argument("--output-dir", default=None,
                        help="Directorio de salida (default: mismo directorio que la imagen)")
    args = parser.parse_args()

    src_path = Path(args.image)
    if not src_path.exists():
        print(f"❌ No se encontró el archivo: {src_path}")
        sys.exit(1)

    out_dir = Path(args.output_dir) if args.output_dir else src_path.parent
    out_dir.mkdir(parents=True, exist_ok=True)

    # --- Cargar imagen ---
    img_bgr = cv2.imread(str(src_path))
    if img_bgr is None:
        print(f"❌ No se pudo leer la imagen: {src_path}")
        sys.exit(1)

    img_bgra = cv2.cvtColor(img_bgr, cv2.COLOR_BGR2BGRA)

    # --- Paso 1: Detectar forma del sprite con threshold alto ---
    detect_threshold = max(args.threshold, args.detect_threshold)
    img_bgra_detect = remove_black_background(img_bgra.copy(), threshold=detect_threshold)
    alpha_detect = img_bgra_detect[:, :, 3]

    # --- Paso 2: Rellenar huecos interiores (zonas oscuras dentro del sprite que quedaron transparentes) ---
    fill_k = args.fill_holes * 2 + 1
    fill_kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (fill_k, fill_k))
    alpha_filled = cv2.morphologyEx(alpha_detect, cv2.MORPH_CLOSE, fill_kernel)

    # --- Paso 3: Aplicar máscara rellena sobre imagen original (preserva colores negros del sprite) ---
    b, g, r, _ = cv2.split(img_bgra)
    img_bgra = cv2.merge([b, g, r, alpha_filled])

    bboxes = find_sprite_bboxes(alpha_filled, min_area=args.min_area)

    if not bboxes:
        print("⚠️  No se encontraron sprites. Prueba bajar --min-area o subir --threshold.")
        sys.exit(1)

    print(f"✅ Encontrados {len(bboxes)} sprite(s) en '{src_path.name}'")

    # --- Recortar ---
    sprites = [crop_with_padding(img_bgra, bb, padding=args.padding) for bb in bboxes]

    # --- Erosionar alpha para eliminar halo de borde (fringe) ---
    if args.erode > 0:
        kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE,
                                           (args.erode * 2 + 1, args.erode * 2 + 1))
        eroded = []
        for s in sprites:
            alpha = s[:, :, 3]
            alpha_eroded = cv2.erode(alpha, kernel, iterations=1)
            s = s.copy()
            s[:, :, 3] = alpha_eroded
            eroded.append(s)
        sprites = eroded

    # --- Normalizar (opcional) ---
    fixed_size = None
    if args.size:
        try:
            fw, fh = map(int, args.size.lower().split('x'))
            fixed_size = (fw, fh)
        except ValueError:
            print(f"❌ Formato de --size inválido: '{args.size}'. Usa WxH, ej: 500x500")
            sys.exit(1)

    if args.normalize or fixed_size:
        sprites = normalize_canvas(sprites, fixed_size=fixed_size)
        print(f"   Canvas normalizado a {sprites[0].shape[1]}x{sprites[0].shape[0]} px")

    # --- Guardar ---
    stem = src_path.stem
    saved = []
    for i, sprite in enumerate(sprites):
        out_path = out_dir / f"{stem}_sprite_{i+1:02d}.png"
        # Convertir de BGRA a RGBA para PIL
        sprite_rgb = cv2.cvtColor(sprite, cv2.COLOR_BGRA2RGBA)
        Image.fromarray(sprite_rgb).save(str(out_path))
        h, w = sprite.shape[:2]
        saved.append(str(out_path))
        print(f"   💾 {out_path.name}  ({w}x{h} px)")

    print(f"\n🎮 Sprites guardados en: {out_dir}")


if __name__ == "__main__":
    main()
