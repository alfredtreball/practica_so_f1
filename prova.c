#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>  // Per treballar amb directoris
#include <sys/types.h>







int main() {

    DIR *dir;


    if ((dir = opendir("/users/home/alfred.chavez/SO/carpetaDestino/practica_so/fitxers_prova")) == NULL) {
        printf("Error obrint el directori\n");
        
    } else {
        
        printf("béeee s'ah obert béee\n");

    }

    //printf("Intentant obrir el directori: %s\n", dir);



    return 0;
}