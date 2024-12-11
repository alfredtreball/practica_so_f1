#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "MD5SUM/MD5_UTILS.h" // Incluye el módulo de MD5

int main() {
    char md5[33];
    const char *testFile = "fitxers_prova/crazier.wav"; // Cambia por cualquier archivo en tu carpeta de pruebas

    calculate_md5(testFile, md5);

    if (strcmp(md5, "ERROR") != 0) {
        char buffer[128];
        int len = snprintf(buffer, sizeof(buffer), "MD5SUM de %s: %s\n", testFile, md5);
        write(STDOUT_FILENO, buffer, len);
    } else {
        write(STDERR_FILENO, "Error al calcular el MD5SUM\n", 28);
    }

    return 0;
}
