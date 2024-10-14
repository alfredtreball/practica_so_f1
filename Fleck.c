#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>  // Per treballar amb directoris
#include <sys/types.h>
#include <ctype.h>

#define BUFFER_SIZE 512  // Augmentat per a noms de fitxers llargs
#define COMMAND_SIZE 128

typedef struct {
    char *user;
    char *directory;
    char *ip;
    int port;
} Config;

char *trim(char *str) {
    char *end;

    // Elimina espais al principi
    while(isspace((unsigned char)*str)) str++;

    if(*str == 0)  // Totes les lletres eren espais
        return str;

    // Elimina espais al final
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;

    // Afegeix el caràcter nul al final de la cadena
    *(end + 1) = '\0';

    return str;
}
// Funció per escriure missatges a la consola sense printf
void writeMessage(const char *message) {
    write(STDOUT_FILENO, message, strlen(message));
}

// Funció per llegir fins a un caràcter delimitador
char *readUntil(int fd, char cEnd) {
    int i = 0;
    ssize_t chars_read;
    char c = 0;
    char *buffer = NULL;

    while (1) {
        chars_read = read(fd, &c, sizeof(char));  
        if (chars_read == 0) {         
            if (i == 0) {              
                return NULL;
            }
            break;                     
        } else if (chars_read < 0) {   
            free(buffer);
            return NULL;
        }

        if (c == cEnd) {              
            break;
        }

        buffer = (char *)realloc(buffer, i + 2);  // Assignar més espai
        buffer[i++] = c;                
    }

    buffer[i] = '\0';  // Finalitzar la cadena amb '\0'
    return buffer;
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

// Funció per llegir el fitxer de configuració de Fleck
void readConfigFile(const char *configFile, Config *fleckConfig) {
    int fd = open(configFile, O_RDONLY);
    if (fd == -1) {
        writeMessage("Error obrint el fitxer de configuració\n");
        exit(1);
    }

    fleckConfig->user = readUntil(fd, '\n');
    fleckConfig->directory = strdup(readUntil(fd, '\n'));  // Còpia modificable
    fleckConfig->directory = trim(fleckConfig->directory);  // Elimina espais al principi i final
    fleckConfig->ip = readUntil(fd, '\n');

    char *portStr = readUntil(fd, '\n');
    fleckConfig->port = stringToInt(portStr);
    free(portStr);

    close(fd);

    writeMessage("File read correctly:\n");
    writeMessage("User - ");
    writeMessage(fleckConfig->user);
    writeMessage("\nDirectory - ");
    writeMessage(fleckConfig->directory);
    writeMessage("\nIP - ");
    writeMessage(fleckConfig->ip);
    writeMessage("\nPort - ");
    char portMsg[BUFFER_SIZE];
    snprintf(portMsg, BUFFER_SIZE, "%d\n", fleckConfig->port);
    write(STDOUT_FILENO, portMsg, strlen(portMsg));
}

// Funció per determinar si una cadena acaba amb una extensió específica
int endsWith(const char *str, const char *suffix) {
    if (!str || !suffix) return 0;
    size_t lenStr = strlen(str);
    size_t lenSuffix = strlen(suffix);
    if (lenSuffix > lenStr) return 0;
    return strncmp(str + lenStr - lenSuffix, suffix, lenSuffix) == 0;
}

// Funció per llistar fitxers de media (amb extensions .wav, .png, .jpg)
void listMedia(char *directory) {
    DIR *dir;
    struct dirent *entry;
    int count = 0;
   // printf("Intentant obrir el directori: %s\n", directory);
    if (access(directory, F_OK) != 0) {
        perror("access error");
        return;
    }
    if ((dir = opendir(directory)) == NULL) {
        writeMessage("Error obrint el directori\n");
        return;
    }

    writeMessage("Media files available:\n");
    while ((entry = readdir(dir)) != NULL) {
        if (endsWith(entry->d_name, ".wav") || endsWith(entry->d_name, ".png") || endsWith(entry->d_name, ".jpg")) {
            char buffer[BUFFER_SIZE];
            snprintf(buffer, BUFFER_SIZE, "%d. %.250s\n", ++count, entry->d_name);  // Limit de 250 caràcters
            writeMessage(buffer);
        }
    }

    if (count == 0) {
        writeMessage("No media files found\n");
    }

    closedir(dir);
}

// Funció per llistar fitxers de text (amb extensió .txt)
void listText(char *directory) {
    DIR *dir;
    struct dirent *entry;
    int count = 0;

    printf("Intentant obrir el directori: %s\n", directory);
    directory[strcspn(directory, "\n")] = 0;  // Elimina qualsevol salt de línia extra

    if ((dir = opendir(directory)) == NULL) {
        writeMessage("Error obrint el directori\n");
        return;
    }

    writeMessage("Text files available:\n");
    while ((entry = readdir(dir)) != NULL) {
        if (endsWith(entry->d_name, ".txt")) {
            char buffer[BUFFER_SIZE];
            snprintf(buffer, BUFFER_SIZE, "%d. %.250s\n", ++count, entry->d_name);  // Limit de 250 caràcters
            writeMessage(buffer);
        }
    }

    if (count == 0) {
        writeMessage("No text files found\n");
    }

    closedir(dir);
}

// Funció per processar les comandes de l'usuari
void processCommand(char *command, char *directory) {
    char *cmd = strtok(command, " ");

    if (strcasecmp(cmd, "CONNECT") == 0) {
        writeMessage("Comanda OK: CONNECT\n");
    } else if (strcasecmp(cmd, "LOGOUT") == 0) {
        writeMessage("Comanda OK: LOGOUT\n");
    } else if (strcasecmp(cmd, "DISTORT") == 0) {
        char *file = strtok(NULL, " ");
        char *factorStr = strtok(NULL, " ");
        if (file != NULL && factorStr != NULL) {
            int factor = stringToInt(factorStr);
            if (factor > 0) {
                writeMessage("Distorsion started!\n");
            } else {
                writeMessage("Comanda KO: factor incorrecte\n");
            }
        } else {
            writeMessage("Comanda KO: arguments incorrectes\n");
        }
    } else if (strcasecmp(cmd, "LIST") == 0) {
        char *listType = strtok(NULL, " ");
        if (listType != NULL) {
            if (strcasecmp(listType, "MEDIA") == 0) {
                listMedia(directory);  // Llistar fitxers media
            } else if (strcasecmp(listType, "TEXT") == 0) {
                listText(directory);  // Llistar fitxers de text
            } else {
                writeMessage("Comanda KO: tipus de llista desconegut\n");
            }
        } else {
            writeMessage("Comanda KO: falta especificar MEDIA o TEXT\n");
        }
    } else {
        writeMessage("ERROR: Please input a valid command.\n");
    }
}

int main(int argc, char *argv[]) {
    Config *fleckConfig = (Config *)malloc(sizeof(Config));
    
    if (argc != 2) {
        writeMessage("Ús: ./fleck <fitxer de configuració>\n");
        exit(1);
    }

    readConfigFile(argv[1], fleckConfig);

    char command[COMMAND_SIZE];
    while (1) {
        writeMessage("$ ");
        if (!fgets(command, COMMAND_SIZE, stdin)) {
            break;
        }
        command[strcspn(command, "\n")] = 0;
        processCommand(command, fleckConfig->directory);
    }

    free(fleckConfig->user);
    free(fleckConfig->directory);
    free(fleckConfig->ip);
    free(fleckConfig);

    return 0;
}
