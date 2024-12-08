#include "Logging.h"

void logInfo(const char *msg) {
    printf(CYAN "[INFO]: " RESET "%s\n", msg);
}

void logWarning(const char *msg) {
    printf(YELLOW "[WARNING]: " RESET "%s\n", msg);
}

void logError(const char *msg) {
    printf(RED "[ERROR]: " RESET "%s\n", msg);
}

void logSuccess(const char *msg) {
    printf(GREEN "[SUCCESS]: " RESET "%s\n", msg);
}
