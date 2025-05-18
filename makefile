##################################################
# @Fitxer: makefile
# @Autors: Pau Olea Reyes (pau.olea), Alfred Chávez Fernández (alfred.chavez)
# @Estudis: Enginyeria Electrònica de Telecomunicacions
# @Universitat: Universitat Ramon Llull - La Salle
# @Assignatura: Sistemes Operatius
# @Curs: 2024-2025
# 
# @Descripció: Makefile per a la compilació i execució dels processos
# distribuïts amb configuracions específiques per màquines.
##################################################

# Variables
CC = gcc
CFLAGS = -Wall -Wextra -pthread -lrt -IFileReader -IStringUtils -IDataConversion -INetworking -IFrameUtils -ILogging -IMD5SUM -IFrameUtilsBinary -IGestorTramas -IMessageQueue -ICleanFIles -IShared_Memory -ISemafors

# Comunes
COMMON = FileReader/FileReader.c StringUtils/StringUtils.c DataConversion/DataConversion.c \
         GestorTramas/GestorTramas.c Networking/Networking.c FrameUtils/FrameUtils.c \
         Logging/Logging.c MD5SUM/md5Sum.c FrameUtilsBinary/FrameUtilsBinary.c \
         CleanFiles/CleanFiles.c Shared_Memory/Shared_memory.c \
		 Semafors/semaphore_v2.c

# Objectius per compilar cada executable
all: Fleck_Montserrat.exe Fleck_Puigpedros.exe Fleck_Matagalls.exe \
	 Harley_Matagalls.exe Harley_Matagalls1.exe \
     Enigma_Puigpedros.exe \
	 Gotham_Montserrat.exe \

# Compilació de Gotham a Montserrat
Gotham_Montserrat.exe: Gotham.c arkham $(COMMON)
	$(CC) $(CFLAGS) Gotham.c $(COMMON) -o Gotham_Montserrat.exe

# Compilació de Fleck a Montserrat
Fleck_Montserrat.exe: Fleck.c $(COMMON)
	$(CC) $(CFLAGS) Fleck.c $(COMMON) -o Fleck_Montserrat.exe

Fleck_Puigpedros.exe: Fleck.c $(COMMON)
	$(CC) $(CFLAGS) Fleck.c $(COMMON) -o Fleck_Puigpedros.exe

Fleck_Matagalls.exe: Fleck.c $(COMMON)
	$(CC) $(CFLAGS) Fleck.c $(COMMON) -o Fleck_Matagalls.exe

# Compilació de Harley a Matagalls
Harley_Matagalls.exe: Harley.c $(COMMON) Compression/so_compression.o HarleyCompression/compression_handler.c HarleySync/HarleySync.c
	$(CC) $(CFLAGS) Harley.c $(COMMON) Compression/so_compression.o HarleyCompression/compression_handler.c HarleySync/HarleySync.c -o Harley_Matagalls.exe -lm -lpthread

# Compilació de Enigma a Puigpedros
Enigma_Puigpedros.exe: Enigma.c $(COMMON) EnigmaCompress/EnigmaCompress.c EnigmaSync/EnigmaSync.c
	$(CC) $(CFLAGS) Enigma.c $(COMMON) EnigmaCompress/EnigmaCompress.c EnigmaSync/EnigmaSync.c -o Enigma_Puigpedros.exe -lm -lpthread

arkham: Arkham.c $(COMMON)
	$(CC) $(CFLAGS) Arkham.c $(COMMON) -o arkham

# Regles per executar automàticament cada programa amb el fitxer de configuració corresponent
gm: Gotham_Montserrat.exe
	./Gotham_Montserrat.exe fitxers_configuració/config_gotham.dat

vgm: Gotham_Montserrat.exe
	valgrind --dsymutil=yes --track-origins=yes --leak-check=full --track-fds=yes --show-reachable=yes -s ./Gotham_Montserrat.exe fitxers_configuració/config_gotham.dat

fm: Fleck_Montserrat.exe
	./Fleck_Montserrat.exe fitxers_configuració/config_fleck.dat

fp: Fleck_Puigpedros.exe
	./Fleck_Puigpedros.exe fitxers_configuració/config_fleck_pp.dat

fa: Fleck_Matagalls.exe
	./Fleck_Matagalls.exe fitxers_configuració/config_fleck_ma.dat

vfm: Fleck_Montserrat.exe
	valgrind --dsymutil=yes --track-origins=yes --leak-check=full --track-fds=yes --show-reachable=yes -s ./Fleck_Montserrat.exe fitxers_configuració/config_fleck.dat

fm1: Fleck_Montserrat.exe
	./Fleck_Montserrat.exe fitxers_configuració/config_fleck1.dat

hma: Harley_Matagalls.exe
	./Harley_Matagalls.exe fitxers_configuració/config_harley.dat
	
hma1: Harley_Matagalls.exe
	./Harley_Matagalls.exe fitxers_configuració/config_harley1.dat

vhma: Harley_Matagalls.exe
	valgrind --dsymutil=yes --track-origins=yes --leak-check=full --track-fds=yes --show-reachable=yes -s ./Harley_Matagalls.exe fitxers_configuració/config_harley.dat

vhma1: Harley_Matagalls.exe
	valgrind --dsymutil=yes --track-origins=yes --leak-check=full --track-fds=yes --show-reachable=yes -s ./Harley_Matagalls.exe fitxers_configuració/config_harley1.dat

ep: Enigma_Puigpedros.exe
	./Enigma_Puigpedros.exe fitxers_configuració/config_enigma.dat
	
ep1: Enigma_Puigpedros.exe
	./Enigma_Puigpedros.exe fitxers_configuració/config_enigma1.dat

vep: Enigma_Puigpedros.exe
	valgrind --dsymutil=yes --track-origins=yes --leak-check=full --track-fds=yes --show-reachable=yes -s ./Enigma_Puigpedros.exe fitxers_configuració/config_enigma.dat

vep1: Enigma_Puigpedros.exe
	valgrind --dsymutil=yes --track-origins=yes --leak-check=full --track-fds=yes --show-reachable=yes -s ./Enigma_Puigpedros.exe fitxers_configuració/config_enigma1.dat

# Neteja els fitxers generats
clean:
	rm -f *.exe *.o
