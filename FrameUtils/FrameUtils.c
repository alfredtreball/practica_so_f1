#include "FrameUtils.h"
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>

// Serializa un frame en un buffer
void serialize_frame(const Frame *frame, char *buffer) {
    if (!frame || !buffer) return;

    memset(buffer, 0, FRAME_SIZE);

    // Truncar datos si exceden el tamaño permitido
    char truncated_data[DATA_MAX_SIZE + 1] = {0};
    strncpy(truncated_data, frame->data, DATA_MAX_SIZE);

    // Serializar los campos en el formato correcto
    int written = snprintf(buffer, FRAME_SIZE, "%02x&%04x&%08x&%04x&",
                            frame->type, frame->data_length,
                            frame->timestamp, frame->checksum);

    if (written < 0 || written >= FRAME_SIZE) {
        fprintf(stderr, "[ERROR][Serialize] Buffer insuficiente al serializar\n");
        return;
    }

    // Concatenar los datos al buffer solo si DATA_LENGTH > 0
    if (frame->data_length > 0) {
        strncat(buffer, truncated_data, FRAME_SIZE - written - 1);
    }
}

// Deserializa un buffer en un frame
int deserialize_frame(const char *buffer, Frame *frame) {
    if (!buffer || !frame) return -1;

    memset(frame, 0, sizeof(Frame)); // Limpiar la estructura del frame

    char data[DATA_MAX_SIZE + 1] = {0}; // Buffer temporal para los datos

    // Extraer los campos del frame
    int fields_read = sscanf(buffer, "%2hhx&%4hx&%8x&%4hx&%1023s",
                             &frame->type, &frame->data_length,
                             &frame->timestamp, &frame->checksum,
                             data);

    // Validar que se hayan leído los campos obligatorios
    if (fields_read < 4) { // DATA es opcional si DATA_LENGTH = 0
        fprintf(stderr, "[ERROR][Deserialize] Formato inválido en el frame\n");
        return -1;
    }

    // Validar longitud de los datos
    if (frame->data_length > DATA_MAX_SIZE) {
        fprintf(stderr, "[ERROR][Deserialize] Longitud de datos excede el máximo permitido\n");
        return -1;
    }

    // Si DATA_LENGTH > 0, copiar los datos al frame
    if (frame->data_length > 0) {
        strncpy(frame->data, data, frame->data_length);
        frame->data[frame->data_length] = '\0'; // Asegurar terminación nula
    } else {
        frame->data[0] = '\0'; // DATA vacío
    }
    return 0; // Deserialización exitosa
}


// Envía un frame a través de un socket
int send_frame(int socket_fd, const Frame *frame) {
    if (!frame) return -1;

    char buffer[FRAME_SIZE];
    serialize_frame(frame, buffer);

    if (write(socket_fd, buffer, FRAME_SIZE) < 0) {
        perror("Error enviando el frame");
        return -1;
    }

    return 0;
}

// Recibe un frame desde un socket
int receive_frame(int socket_fd, Frame *frame) {
    if (!frame) {
        fprintf(stderr, "[ERROR][ReceiveFrame] Frame no válido (puntero NULL).\n");
        return -1;
    }

    char buffer[FRAME_SIZE];
    ssize_t bytesRead = read(socket_fd, buffer, FRAME_SIZE);

    if (bytesRead < 0) {
        // Caso de error al leer
        perror("[ERROR][ReceiveFrame] Error recibiendo el frame");
        return -1;
    } else if (bytesRead == 0) {
        // Caso de conexión cerrada ordenadamente
        fprintf(stderr, "[INFO][ReceiveFrame] Conexión cerrada por el peer (socket %d).\n", socket_fd);
        return -1;
    }

    if (bytesRead != FRAME_SIZE) {
        // Un frame debería ser exactamente de tamaño `FRAME_SIZE`
        fprintf(stderr, "[ERROR][ReceiveFrame] Frame incompleto recibido. Bytes leídos: %zd\n", bytesRead);
        return -1;
    }

    // Intentar deserializar el frame
    if (deserialize_frame(buffer, frame) != 0) {
        fprintf(stderr, "[ERROR][ReceiveFrame] Error deserializando el frame recibido\n");
        return -1;
    }

    return 0; // Éxito
}


// Calcula el checksum de un conjunto de datos
uint16_t calculate_checksum(const char *data, size_t length, int include_null) {
    uint32_t sum = 0;

    for (size_t i = 0; i < length; i++) {
        sum += (uint8_t)data[i];
    }

    // Solo incluir el nulo final si el flag está habilitado
    if (include_null && length < DATA_MAX_SIZE) {
        sum += '\0';
    }

    return (uint16_t)(sum % CHECKSUM_MODULO);
}

// Obtiene el timestamp actual
void get_timestamp(char *timestamp) {
    if (!timestamp) return;

    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    snprintf(timestamp, TIMESTAMP_SIZE, "%04d-%02d-%02d %02d:%02d:%02d",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec);
}
