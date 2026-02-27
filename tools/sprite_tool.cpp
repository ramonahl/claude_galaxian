#include "raylib.h"

#include <cmath>
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

struct Options {
    std::string inputPath;
    std::string outputPath;
    std::string previewPath = "sprite_preview.png";
    std::string spriteName = "SPR_IMPORTED";
    int size = 16;
    int previewScale = 24;
    int alphaThreshold = 32;
    bool useColorKey = false;
    Color colorKey = {0, 0, 0, 255};
    int colorKeyTolerance = 24;
    std::string fitMode = "tight";   // tight: maximize inside NxN preserving aspect ratio, source: preserve source-canvas proportion
    bool exactMode = false;          // exact: source is already NxN, skip crop/resize
    bool writePreview = true;
};

static const Color kPalette[] = {
    {0, 0, 0, 0},           //  0 transparente
    {255, 255, 255, 255},   //  1 blanco
    {160, 220, 255, 255},   //  2 cian claro
    {80, 160, 255, 255},    //  3 azul
    {40, 80, 200, 255},     //  4 azul oscuro
    {255, 230, 60, 255},    //  5 amarillo
    {230, 30, 20, 255},     //  6 rojo
    {160, 10, 5, 255},      //  7 rojo oscuro
    {255, 90, 40, 255},     //  8 naranja
    {255, 200, 180, 255},   //  9 rosa highlight
    {50, 200, 50, 255},     // 10 verde
    {140, 255, 140, 255},   // 11 verde claro
    {15, 110, 15, 255},     // 12 verde oscuro
    {255, 240, 80, 255},    // 13 amarillo brillante (ojos)
    {100, 80, 255, 255},    // 14 índigo
    {200, 180, 255, 255},   // 15 lila claro
    {0, 180, 180, 255},     // 16 teal
    {0, 220, 220, 255},     // 17 teal claro
    {0, 100, 120, 255},     // 18 teal oscuro
    {0, 255, 200, 255},     // 19 aqua
    {180, 60, 200, 255},    // 20 magenta
    {255, 120, 200, 255},   // 21 rosa
    {120, 40, 0, 255},      // 22 marrón
    {200, 140, 60, 255},    // 23 marrón claro
    {80, 80, 80, 255},      // 24 gris oscuro
    {160, 160, 160, 255},   // 25 gris
    {220, 220, 220, 255},   // 26 gris claro
    {0, 40, 100, 255},      // 27 azul marino
    {255, 160, 0, 255},     // 28 naranja brillante
    {0, 200, 100, 255},     // 29 verde esmeralda
    {255, 60, 120, 255},    // 30 rojo coral
    {140, 255, 255, 255},   // 31 cian brillante
};

static void printUsage(const char *argv0) {
    std::cout
        << "Uso:\n"
        << "  " << argv0 << " --input <imagen.png> [opciones]\n\n"
        << "Opciones:\n"
        << "  --name <SPRITE_NAME>        Nombre del array C++ (default: SPR_IMPORTED)\n"
        << "  --size <N>                  Tamano NxN del sprite (default: 16)\n"
        << "  --alpha-threshold <0..255>  Alpha minimo para pixel visible (default: 32)\n"
        << "  --colorkey <r,g,b>          Trata ese color como transparente (ej: 0,0,0)\n"
        << "  --colorkey-tolerance <N>    Tolerancia para colorkey (default: 24)\n"
        << "  --fit-mode <source|tight>   source=proporcion del lienzo original, tight=llena mas sin deformar\n"
        << "  --output <archivo.h>        Guarda el array C++ en archivo (si no, imprime stdout)\n"
        << "  --preview <archivo.png>     Ruta de preview escalada (default: sprite_preview.png)\n"
        << "  --preview-scale <N>         Escala de preview por pixel (default: 24)\n"
        << "  --no-preview                No genera preview PNG\n"
        << "Nota:\n"
        << "  El sprite se recorta automaticamente al area visible y se centra en el lienzo final.\n"
        << "  --help                      Muestra ayuda\n";
}

static bool parseIntArg(const std::string &value, int *out) {
    try {
        size_t idx = 0;
        int v = std::stoi(value, &idx);
        if (idx != value.size()) return false;
        *out = v;
        return true;
    } catch (...) {
        return false;
    }
}

static bool parseRgb(const std::string &value, Color *out) {
    int r = 0, g = 0, b = 0;
    if (sscanf(value.c_str(), "%d,%d,%d", &r, &g, &b) != 3) return false;
    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) return false;
    out->r = static_cast<unsigned char>(r);
    out->g = static_cast<unsigned char>(g);
    out->b = static_cast<unsigned char>(b);
    out->a = 255;
    return true;
}

static bool parseArgs(int argc, char **argv, Options *opt) {
    if (argc < 2) return false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto needValue = [&](const char *flag) -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "Falta valor para " << flag << "\n";
                return "";
            }
            return argv[++i];
        };

        if (a == "--help" || a == "-h") {
            return false;
        } else if (a == "--input") {
            opt->inputPath = needValue("--input");
            if (opt->inputPath.empty()) return false;
        } else if (a == "--name") {
            opt->spriteName = needValue("--name");
            if (opt->spriteName.empty()) return false;
        } else if (a == "--output") {
            opt->outputPath = needValue("--output");
            if (opt->outputPath.empty()) return false;
        } else if (a == "--preview") {
            opt->previewPath = needValue("--preview");
            if (opt->previewPath.empty()) return false;
        } else if (a == "--size") {
            int v = 0;
            if (!parseIntArg(needValue("--size"), &v) || v <= 0 || v > 256) {
                std::cerr << "--size debe estar entre 1 y 256\n";
                return false;
            }
            opt->size = v;
        } else if (a == "--preview-scale") {
            int v = 0;
            if (!parseIntArg(needValue("--preview-scale"), &v) || v <= 0 || v > 128) {
                std::cerr << "--preview-scale debe estar entre 1 y 128\n";
                return false;
            }
            opt->previewScale = v;
        } else if (a == "--alpha-threshold") {
            int v = 0;
            if (!parseIntArg(needValue("--alpha-threshold"), &v) || v < 0 || v > 255) {
                std::cerr << "--alpha-threshold debe estar entre 0 y 255\n";
                return false;
            }
            opt->alphaThreshold = v;
        } else if (a == "--colorkey") {
            Color key;
            if (!parseRgb(needValue("--colorkey"), &key)) {
                std::cerr << "--colorkey debe tener formato r,g,b con valores 0..255\n";
                return false;
            }
            opt->useColorKey = true;
            opt->colorKey = key;
        } else if (a == "--colorkey-tolerance") {
            int v = 0;
            if (!parseIntArg(needValue("--colorkey-tolerance"), &v) || v < 0 || v > 255) {
                std::cerr << "--colorkey-tolerance debe estar entre 0 y 255\n";
                return false;
            }
            opt->colorKeyTolerance = v;
        } else if (a == "--fit-mode") {
            opt->fitMode = needValue("--fit-mode");
            if (opt->fitMode != "source" && opt->fitMode != "tight") {
                std::cerr << "--fit-mode debe ser source o tight\n";
                return false;
            }
        } else if (a == "--exact") {
            opt->exactMode = true;
        } else if (a == "--no-preview") {
            opt->writePreview = false;
        } else {
            std::cerr << "Parametro desconocido: " << a << "\n";
            return false;
        }
    }

    return !opt->inputPath.empty();
}

static int nearestPaletteIndex(const Color &px, const Options &opt) {
    if (px.a < opt.alphaThreshold) return 0;
    if (opt.useColorKey) {
        const int drk = static_cast<int>(px.r) - static_cast<int>(opt.colorKey.r);
        const int dgk = static_cast<int>(px.g) - static_cast<int>(opt.colorKey.g);
        const int dbk = static_cast<int>(px.b) - static_cast<int>(opt.colorKey.b);
        const int distk = drk * drk + dgk * dgk + dbk * dbk;
        if (distk <= opt.colorKeyTolerance * opt.colorKeyTolerance) return 0;
    }

    int bestIdx = 1;
    int bestDist = std::numeric_limits<int>::max();
    for (int i = 1; i < static_cast<int>(sizeof(kPalette) / sizeof(kPalette[0])); ++i) {
        const int dr = static_cast<int>(px.r) - static_cast<int>(kPalette[i].r);
        const int dg = static_cast<int>(px.g) - static_cast<int>(kPalette[i].g);
        const int db = static_cast<int>(px.b) - static_cast<int>(kPalette[i].b);
        const int dist = dr * dr + dg * dg + db * db;
        if (dist < bestDist) {
            bestDist = dist;
            bestIdx = i;
        }
    }
    return bestIdx;
}

static bool isVisiblePixel(const Color &px, const Options &opt) {
    if (px.a < opt.alphaThreshold) return false;
    if (opt.useColorKey) {
        const int drk = static_cast<int>(px.r) - static_cast<int>(opt.colorKey.r);
        const int dgk = static_cast<int>(px.g) - static_cast<int>(opt.colorKey.g);
        const int dbk = static_cast<int>(px.b) - static_cast<int>(opt.colorKey.b);
        const int distk = drk * drk + dgk * dgk + dbk * dbk;
        if (distk <= opt.colorKeyTolerance * opt.colorKeyTolerance) return false;
    }
    return true;
}

static std::string buildCppArray(const std::vector<uint8_t> &idx, const std::string &name, int size) {
    std::string out = "static const uint8_t " + name + "[" + std::to_string(size) + "][" + std::to_string(size) + "] = {\n";
    for (int y = 0; y < size; ++y) {
        out += "    {";
        for (int x = 0; x < size; ++x) {
            out += std::to_string(idx[y * size + x]);
            if (x + 1 < size) out += ",";
        }
        out += "}";
        if (y + 1 < size) out += ",";
        out += "\n";
    }
    out += "};\n";
    return out;
}

int main(int argc, char **argv) {
    Options opt;
    if (!parseArgs(argc, argv, &opt)) {
        printUsage(argv[0]);
        return 1;
    }

    Image src = LoadImage(opt.inputPath.c_str());
    if (src.data == nullptr) {
        std::cerr << "No se pudo cargar imagen: " << opt.inputPath << "\n";
        return 2;
    }

    Color *srcPixels = LoadImageColors(src);
    if (srcPixels == nullptr) {
        UnloadImage(src);
        std::cerr << "No se pudieron leer pixeles de entrada\n";
        return 3;
    }

    // En modo exact, si la imagen ya es NxN, saltar crop/resize
    if (opt.exactMode && src.width == opt.size && src.height == opt.size) {
        UnloadImageColors(srcPixels);
    } else {
        int minX = src.width;
        int minY = src.height;
        int maxX = -1;
        int maxY = -1;
        for (int y = 0; y < src.height; ++y) {
            for (int x = 0; x < src.width; ++x) {
                const Color px = srcPixels[y * src.width + x];
                if (!isVisiblePixel(px, opt)) continue;
                if (x < minX) minX = x;
                if (y < minY) minY = y;
                if (x > maxX) maxX = x;
                if (y > maxY) maxY = y;
            }
        }
        UnloadImageColors(srcPixels);

        if (maxX < minX || maxY < minY) {
            UnloadImage(src);
            std::cerr << "No se detectaron pixeles visibles tras alpha/colorkey\n";
            return 4;
        }

        const int cropW = maxX - minX + 1;
        const int cropH = maxY - minY + 1;
        const int srcW = src.width;
        const int srcH = src.height;
        Rectangle cropRect = {static_cast<float>(minX), static_cast<float>(minY), static_cast<float>(cropW), static_cast<float>(cropH)};
        Image cropped = ImageFromImage(src, cropRect);
        UnloadImage(src);

        if (cropped.data == nullptr) {
            std::cerr << "No se pudo recortar el area visible\n";
            return 5;
        }

        int scaledW = 1;
        int scaledH = 1;
        if (opt.fitMode == "source") {
            scaledW = std::max(1, static_cast<int>(std::ceil(static_cast<double>(cropW) * opt.size / srcW)));
            scaledH = std::max(1, static_cast<int>(std::ceil(static_cast<double>(cropH) * opt.size / srcH)));
            if (scaledW > opt.size) scaledW = opt.size;
            if (scaledH > opt.size) scaledH = opt.size;
        } else {
            scaledW = opt.size;
            scaledH = opt.size;
            if (cropW >= cropH) {
                scaledH = std::max(1, static_cast<int>(std::lround(static_cast<double>(cropH) * opt.size / cropW)));
            } else {
                scaledW = std::max(1, static_cast<int>(std::lround(static_cast<double>(cropW) * opt.size / cropH)));
            }
        }
        ImageResizeNN(&cropped, scaledW, scaledH);

        Image canvas = GenImageColor(opt.size, opt.size, BLANK);
        const int offX = (opt.size - scaledW) / 2;
        const int offY = (opt.size - scaledH) / 2;
        ImageDraw(&canvas, cropped, {0, 0, static_cast<float>(scaledW), static_cast<float>(scaledH)},
                  {static_cast<float>(offX), static_cast<float>(offY), static_cast<float>(scaledW), static_cast<float>(scaledH)},
                  WHITE);
        UnloadImage(cropped);
        src = canvas;
    }

    Color *pixels = LoadImageColors(src);
    if (pixels == nullptr) {
        UnloadImage(src);
        std::cerr << "No se pudieron leer pixeles del sprite procesado\n";
        return 6;
    }

    std::vector<uint8_t> indices(static_cast<size_t>(opt.size * opt.size), 0);
    for (int y = 0; y < opt.size; ++y) {
        for (int x = 0; x < opt.size; ++x) {
            const Color px = pixels[y * opt.size + x];
            indices[y * opt.size + x] = static_cast<uint8_t>(nearestPaletteIndex(px, opt));
        }
    }

    const std::string cppArray = buildCppArray(indices, opt.spriteName, opt.size);
    if (!opt.outputPath.empty()) {
        std::ofstream ofs(opt.outputPath);
        if (!ofs) {
            std::cerr << "No se pudo abrir salida: " << opt.outputPath << "\n";
            UnloadImageColors(pixels);
            return 4;
        }
        ofs << cppArray;
        std::cout << "Array guardado en: " << opt.outputPath << "\n";
    } else {
        std::cout << cppArray;
    }

    if (opt.writePreview) {
        Image preview = GenImageColor(opt.size * opt.previewScale, opt.size * opt.previewScale, BLANK);
        for (int y = 0; y < opt.size; ++y) {
            for (int x = 0; x < opt.size; ++x) {
                const uint8_t idx = indices[y * opt.size + x];
                if (idx == 0) continue;
                ImageDrawRectangle(&preview, x * opt.previewScale, y * opt.previewScale, opt.previewScale, opt.previewScale, kPalette[idx]);
            }
        }

        if (ExportImage(preview, opt.previewPath.c_str())) {
            std::cout << "Preview guardado en: " << opt.previewPath << "\n";
        } else {
            std::cerr << "No se pudo guardar preview: " << opt.previewPath << "\n";
        }
        UnloadImage(preview);
    }

    UnloadImageColors(pixels);
    UnloadImage(src);
    return 0;
}
