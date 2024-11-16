// StringUtils.h
#ifndef STRINGUTILS_H
#define STRINGUTILS_H

// Colors ANSI
#define RESET "\033[0m"
#define BOLD "\033[1m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define BLUE "\033[34m"
#define CYAN "\033[36m"
#define MAGENTA "\033[35m"
#define YELLOW "\033[33m"

#define ANSI_COLOR_RESET  "\033[0m"
#define ANSI_COLOR_RED    "\033[1;31m"
#define ANSI_COLOR_GREEN  "\033[1;32m"
#define ANSI_COLOR_YELLOW "\033[1;33m"
#define ANSI_COLOR_BLUE   "\033[1;34m"
#define ANSI_COLOR_MAGENTA "\033[1;35m"
#define ANSI_COLOR_CYAN   "\033[1;36m"
#define ANSI_COLOR_WHITE  "\033[1;37m"

/***********************************************
* @Finalitat: Escriu la cadena de text 'x' a la sortida estàndard (file descriptor 1).
* @Paràmetres:
*   in: x = cadena de text a imprimir.
* @Retorn: ----
************************************************/
#define printF(x) write(1, x, strlen(x))

char *trim(char *str);
void removeChar(char *string, char charToRemove);
int esTipoValido(const char *filename, const char *workerType);
void trimCommand(char *command);
int endsWith(char *str, char *suffix);

#endif // STRINGUTILS_H
