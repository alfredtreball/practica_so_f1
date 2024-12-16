#include "GestorTramas.h"
#include "../Logging/Logging.h"
#include <unistd.h>
#include <time.h>
#include <string.h>

int leerTrama(int socket_fd, Frame *frame) {
    if (receive_frame(socket_fd, frame) != 0) {
        logError("[GestorTramas] Error al leer la trama normal.");
        enviarTramaError(socket_fd);
        return -1;
    }

    uint16_t calculated_checksum = calculate_checksum(frame->data, frame->data_length, 0);
    if (calculated_checksum != frame->checksum) {
        logError("[GestorTramas] Checksum inválido en trama normal.");
        enviarTramaError(socket_fd);
        return -1;
    }
    return 0;
}

int escribirTrama(int socket_fd, const Frame *frame) {
    if (send_frame(socket_fd, frame) != 0) {
        logError("[GestorTramas] Error al escribir la trama normal.");
        return -1;
    }

    return 0;
}

int enviarTramaError(int socket_fd) {
    Frame errorFrame = {0};
    errorFrame.type = 0x09;
    errorFrame.data_length = 0;
    errorFrame.timestamp = (uint32_t)time(NULL);
    errorFrame.checksum = calculate_checksum(errorFrame.data, errorFrame.data_length, 0);

    return escribirTrama(socket_fd, &errorFrame);
}

int leerTramaBinaria(int socket_fd, BinaryFrame *frame) {
    if (receive_frame_binary(socket_fd, frame) != 0) {
        logError("[GestorTramas] Error al leer la trama binaria.");
        enviarTramaError(socket_fd);
        return -1;
    }

    uint16_t calculated_checksum = calculate_checksum_binary(frame->data, frame->data_length, 1);
    if (calculated_checksum != frame->checksum) {
        logError("[GestorTramas] Checksum inválido en trama binaria.");
        enviarTramaError(socket_fd);
        return -1;
    }
    return 0;
}

int escribirTramaBinaria(int socket_fd, const BinaryFrame *frame) {
    if (send_frame_binary(socket_fd, frame) != 0) {
        logError("[GestorTramas] Error al escribir la trama binaria.");
        return -1;
    }
    return 0;
}