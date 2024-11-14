// StringUtils.h
#ifndef STRINGUTILS_H
#define STRINGUTILS_H

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
int endsWith(char *str, char *suffix);

#endif // STRINGUTILS_H
