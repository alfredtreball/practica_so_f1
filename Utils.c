#include "Utils.h"

void printF(const char *str) {
    write(STDOUT_FILENO, str, strlen(str));
}

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

        buffer = (char *)realloc(buffer, i + 2);
        buffer[i++] = c;
    }

    buffer[i] = '\0';
    return buffer;
}

char* separarParaules(char* string, const char* delimiter, char** context) {
    char* posicio = NULL;

    if (string != NULL) {
        posicio = string;
    } else {
        if (*context == NULL) {
            return NULL;
        }
        posicio = *context;
    }

    while (*posicio && strchr(delimiter, *posicio)) {
        posicio++;
    }

    if (*posicio == '\0') {
        *context = NULL;
        return NULL;
    }

    char *iniciParaula = posicio;

    while (*posicio && !strchr(delimiter, *posicio)) {
        posicio++;
    }

    if (*posicio) {
        *posicio = '\0';
        posicio++;
    }

    *context = posicio;
    return iniciParaula;
}

int endsWith(char *str, char *suffix) {
    if (!str || !suffix) return 0;
    size_t lenStr = strlen(str);
    size_t lenSuffix = strlen(suffix);
    if (lenSuffix > lenStr) return 0;
    return strncmp(str + lenStr - lenSuffix, suffix, lenSuffix) == 0;
}

char *trim(char *str) {
    char *end;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0)
        return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';
    return str;
}

void removeChar(char *string, char charToRemove) {
    char *origen, *dst;
    for (origen = dst = string; *origen != '\0'; origen++) {
        *dst = *origen;
        if (*dst != charToRemove) dst++;
    }
    *dst = '\0';
}
