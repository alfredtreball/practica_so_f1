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
CFLAGS = -Wall -Wextra -IFileReader -IStringUtils -IDataConversion -INetworking -IFrameUtils -ILogging -IMD5SUM -IFrameUtilsBinary -IGestorTramas

# Comunes
COMMON = FileReader/FileReader.c StringUtils/StringUtils.c DataConversion/DataConversion.c GestorTramas/GestorTramas.c Networking/Networking.c FrameUtils/FrameUtils.c Logging/Logging.c MD5SUM/md5Sum.c FrameUtilsBinary/FrameUtilsBinary.c
# Objectius per compilar cada executable
all: Fleck_Montserrat.exe Fleck_Matagalls.exe Harley_Montserrat.exe Harley_Matagalls.exe \
     Harley_Puigpedros.exe Enigma_Puigpedros.exe Gotham_Montserrat.exe

# Compilació de Gotham a Montserrat
Gotham_Montserrat.exe: Gotham.c $(COMMON)
	$(CC) $(CFLAGS) Gotham.c $(COMMON) -o Gotham_Montserrat.exe

# Compilació de Fleck a Montserrat
Fleck_Montserrat.exe: Fleck.c $(COMMON)
	$(CC) $(CFLAGS) Fleck.c $(COMMON) -o Fleck_Montserrat.exe

# Compilació de Fleck a Matagalls
Fleck_Matagalls.exe: Fleck.c $(COMMON)
	$(CC) $(CFLAGS) Fleck.c $(COMMON) -o Fleck_Matagalls.exe

# Compilació de Harley a Montserrat
Harley_Montserrat.exe: Harley.c $(COMMON) Compression/so_compression.o HarleyCompression/compression_handler.c
	$(CC) $(CFLAGS) Harley.c $(COMMON) Compression/so_compression.o HarleyCompression/compression_handler.c -o Harley_Montserrat.exe -lm

# Compilació de Harley a Matagalls
Harley_Matagalls.exe: Harley.c $(COMMON) Compression/so_compression.o HarleyCompression/compression_handler.c
	$(CC) $(CFLAGS) Harley.c $(COMMON) Compression/so_compression.o HarleyCompression/compression_handler.c -o Harley_Matagalls.exe -lm

# Compilació de Harley a Puigpedros
Harley_Puigpedros.exe: Harley.c $(COMMON) Compression/so_compression.o HarleyCompression/compression_handler.c
	$(CC) $(CFLAGS) Harley.c $(COMMON) Compression/so_compression.o HarleyCompression/compression_handler.c -o Harley_Puigpedros.exe -lm

# Compilació de Enigma a Puigpedros
Enigma_Puigpedros.exe: Enigma.c $(COMMON)
	$(CC) $(CFLAGS) Enigma.c $(COMMON) -o Enigma_Puigpedros.exe

# Regles per executar automàticament cada programa amb el fitxer de configuració corresponent
gm: Gotham_Montserrat.exe
	./Gotham_Montserrat.exe fitxers_configuració/config_gotham.dat

vgm: Gotham_Montserrat.exe
	valgrind --dsymutil=yes --track-origins=yes --leak-check=full --track-fds=yes --show-reachable=yes -s ./Gotham_Montserrat.exe fitxers_configuració/config_gotham.dat

fm: Fleck_Montserrat.exe
	./Fleck_Montserrat.exe fitxers_configuració/config_fleck.dat

fma: Fleck_Matagalls.exe
	./Fleck_Matagalls.exe fitxers_configuració/config_fleck1.dat

hm: Harley_Montserrat.exe
	./Harley_Montserrat.exe fitxers_configuració/config_harley.dat

hma: Harley_Matagalls.exe
	./Harley_Matagalls.exe fitxers_configuració/config_harley1.dat

hpp: Harley_Puigpedros.exe
	./Harley_Puigpedros.exe fitxers_configuració/config_harley_pp.dat

ep: Enigma_Puigpedros.exe
	./Enigma_Puigpedros.exe fitxers_configuració/config_enigma.dat

# Neteja els fitxers generats
clean:
	rm -f *.exe *.o
