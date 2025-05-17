#include "compression_handler.h"
#include "../DataConversion/DataConversion.h"

int process_compression(const char *filepath, int factor) {
    // Obtener la extensión del archivo
    const char *extension = get_file_extension(filepath);
    if (!extension) {
        fprintf(stderr, "[ERROR]: El archivo no tiene una extensión válida.\n");
        return -1;
    }

    int result = 0;

    // Procesar según la extensión
    if (strcmp(extension, "png") == 0 || strcmp(extension, "jpg") == 0 || 
        strcmp(extension, "jpeg") == 0 || strcmp(extension, "bmp") == 0 || 
        strcmp(extension, "tga") == 0) {
        // Comprimir imagen
        result = SO_compressImage((char *)filepath, factor);
        if (result != 0) {
            fprintf(stderr, "[ERROR]: Error al comprimir la imagen '%s'. Código de error: %d\n", filepath, result);
        }
    } else if (strcmp(extension, "wav") == 0) {
        // Comprimir audio
        result = SO_compressAudio((char *)filepath, factor);
        if (result != 0) {
            fprintf(stderr, "[ERROR]: Error al comprimir el audio '%s'. Código de error: %d\n", filepath, result);
        } else {
            customPrintf("[INFO]: Audio comprimido correctamente: '%s'.\n", filepath);
        }
    } else {
        fprintf(stderr, "[ERROR]: Extensión no soportada para compresión: '%s'.\n", extension);
        result = -1;
    }

    return result;
}

int is_valid_extension(const char *filepath) {
    const char *extension = get_file_extension(filepath);
    if (!extension) return 0;

    const char *validImageExtensions[] = {"png", "jpg", "jpeg", "bmp", "tga"};
    const char *validAudioExtensions[] = {"wav"};

    // Verificar extensiones de imagen
    for (size_t i = 0; i < sizeof(validImageExtensions) / sizeof(validImageExtensions[0]); ++i) {
        if (strcmp(extension, validImageExtensions[i]) == 0) return 1;
    }

    // Verificar extensiones de audio
    for (size_t i = 0; i < sizeof(validAudioExtensions) / sizeof(validAudioExtensions[0]); ++i) {
        if (strcmp(extension, validAudioExtensions[i]) == 0) return 1;
    }

    return 0;  // No válida
}

const char *get_file_extension(const char *filepath) {
    const char *dot = strrchr(filepath, '.');
    if (dot && *(dot + 1)) {  // Verifica si hay un punto y que no sea el último carácter
        return dot + 1;
    }
    return NULL;  // No tiene extensión
}
