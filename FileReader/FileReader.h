// FileReader.h
#ifndef FILEREADER_H
#define FILEREADER_H

#define _GNU_SOURCE
#define printF(x) write(1, x, strlen(x))

typedef struct {
    char *ipGotham;
    int portGotham;
    char *ipFleck;
    int portFleck;
    char *directory;
    char *workerType;
} EnigmaConfig;

typedef struct {
    char *ipGotham;
    int portGotham;
    char *ipFleck;
    int portFleck;
    char *directory;
    char *workerType;
} HarleyConfig;

typedef struct {
    char *ipFleck;
    int portFleck;
    char *ipHarEni;
    int portHarEni;
} GothamConfig;

typedef struct {
    char *user;
    char *directory;
    char *ipGotham;
    int portGotham;
} FleckConfig;

//Enum per identificar el tipus de configuració
typedef enum {
    CONFIG_ENIGMA,
    CONFIG_HARLEY,
    CONFIG_GOTHAM,
    CONFIG_FLECK
} ConfigType;

// Funció genèrica per llegir la configuració
void readConfigFileGeneric(const char *configFile, void *configStruct, ConfigType configType);
char *readUntil(int fd, char cEnd);

#endif // FILEREADER_H
