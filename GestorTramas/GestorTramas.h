#ifndef GESTOR_TRAMAS_H
#define GESTOR_TRAMAS_H

#include "../FrameUtils/FrameUtils.h"
#include "../FrameUtilsBinary/FrameUtilsBinary.h"

// Funciones para tramas normales
int leerTrama(int socket_fd, Frame *frame);
int escribirTrama(int socket_fd, const Frame *frame);
int enviarTramaError(int socket_fd);

// Funciones para tramas binarias
int leerTramaBinaria(int socket_fd, BinaryFrame *frame);
int escribirTramaBinaria(int socket_fd, const BinaryFrame *frame);

#endif
