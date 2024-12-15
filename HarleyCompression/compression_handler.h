#ifndef COMPRESSION_HANDLER_H
#define COMPRESSION_HANDLER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../Compression/so_compression.h"
    
// Constantes para extensiones válidas
#define VALID_IMAGE_EXTENSIONS {".png", ".jpg", ".jpeg", ".bmp", ".tga"}
#define VALID_AUDIO_EXTENSIONS {".wav"}

// Prototipos de funciones
/**
 * Procesa la compresión de un archivo en función de su extensión.
 *
 * @param filepath Ruta completa del archivo a procesar.
 * @param factor Factor de compresión o escalado.
 * @return 0 si el procesamiento es exitoso, -1 en caso de error.
 */
int process_compression(const char *filepath, int factor);

/**
 * Verifica si un archivo tiene una extensión válida para compresión.
 *
 * @param filepath Ruta del archivo a verificar.
 * @return 1 si la extensión es válida, 0 en caso contrario.
 */
int is_valid_extension(const char *filepath);

/**
 * Extrae la extensión de un archivo.
 *
 * @param filepath Ruta del archivo.
 * @return Puntero a la extensión (incluyendo el punto), o NULL si no tiene extensión.
 */
const char *get_file_extension(const char *filepath);

#endif // COMPRESSION_HANDLER_H
