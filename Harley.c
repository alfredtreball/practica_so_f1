#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define BUFFER_SIZE 256

typedef struct {
    char *gotham_ip;
    int gotham_port;
    char *server_ip;
    int server_port;
    char *directory;
    char *worker_type;
} HarleyConfig;

// Funció per escriure missatges a la consola sense printf
void writeMessage(const char *message) {
    write(STDOUT_FILENO, message, strlen(message));
}

// Funció per llegir línies amb read i memòria dinàmica
ssize_t readLine(int fd, char *buffer, size_t size) {
    ssize_t bytesRead = 0;
    ssize_t totalBytesRead = 0;
    char ch;
    while (totalBytesRead < (ssize_t)(size - 1) && (bytesRead = read(fd, &ch, 1)) > 0) {
        if (ch == '\n') {
            break;
        }
        buffer[totalBytesRead++] = ch;
    }
    buffer[totalBytesRead] = '\0'; // Terminar la cadena
    return totalBytesRead;
}

// Funció per convertir un string en enter
int stringToInt(char *str) {
    int result = 0;
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] >= '0' && str[i] <= '9') {
            result = result * 10 + (str[i] - '0');
        }
    }
    return result;
}

// Funció per llegir el fitxer de configuració utilitzant open, read i memòria dinàmica
void readConfigFile(const char *configFile, HarleyConfig *harleyConfig) {
    int fd = open(configFile, O_RDONLY);
    if (fd == -1) {
        writeMessage("Error obrint el fitxer de configuració\n");
        exit(1);
    }

    // Llegir i assignar memòria per cada camp
    readLine(fd, harleyConfig->gotham_ip, BUFFER_SIZE);
    char portStr[BUFFER_SIZE];
    readLine(fd, portStr, BUFFER_SIZE);
    harleyConfig->gotham_port = stringToInt(portStr);

    readLine(fd, harleyConfig->server_ip, BUFFER_SIZE);
    readLine(fd, portStr, BUFFER_SIZE);
    harleyConfig->server_port = stringToInt(portStr);

    readLine(fd, harleyConfig->directory, BUFFER_SIZE);
    readLine(fd, harleyConfig->worker_type, BUFFER_SIZE);

    close(fd);

    // Mostrar la configuració llegida
    writeMessage("File read correctly:\n");
    writeMessage("Gotham IP - ");
    writeMessage(harleyConfig->gotham_ip);
    writeMessage("\nGotham Port - ");
    
    char portMsg[BUFFER_SIZE];
    snprintf(portMsg, BUFFER_SIZE, "%d\n", harleyConfig->gotham_port);
    write(STDOUT_FILENO, portMsg, strlen(portMsg));

    writeMessage("Server IP - ");
    writeMessage(harleyConfig->server_ip);
    writeMessage("\nServer Port - ");
    
    snprintf(portMsg, BUFFER_SIZE, "%d\n", harleyConfig->server_port);
    write(STDOUT_FILENO, portMsg, strlen(portMsg));

    writeMessage("Directory - ");
    writeMessage(harleyConfig->directory);
    writeMessage("\nWorker Type - ");
    writeMessage(harleyConfig->worker_type);
    writeMessage("\n");
}

int main(int argc, char *argv[]) {
    // Crear la variable local a main()
    HarleyConfig *harleyConfig = (HarleyConfig *)malloc(sizeof(HarleyConfig));
    harleyConfig->gotham_ip = (char *)malloc(BUFFER_SIZE);
    harleyConfig->server_ip = (char *)malloc(BUFFER_SIZE);
    harleyConfig->directory = (char *)malloc(BUFFER_SIZE);
    harleyConfig->worker_type = (char *)malloc(BUFFER_SIZE);

    if (argc != 2) {
        writeMessage("Ús: ./harley <fitxer de configuració>\n");
        exit(1);
    }

    // Llegir la configuració passant harleyConfig com a argument
    readConfigFile(argv[1], harleyConfig);

    // Alliberar memòria dinàmica
    free(harleyConfig->gotham_ip);
    free(harleyConfig->server_ip);
    free(harleyConfig->directory);
    free(harleyConfig->worker_type);
    free(harleyConfig);

    return 0;
}
