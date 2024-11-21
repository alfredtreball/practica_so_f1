##################################################
# @Fitxer: makefile
# @Autors: Pau Olea Reyes (pau.olea), Alfred Chávez Fernández (alfred.chavez)
# @Estudis: Enginyeria Electrònica de Telecomunicacions
# @Universitat: Universitat Ramon Llull - La Salle
# @Assignatura: Sistemes Operatius
# @Curs: 2024-2025
# 
# @Descripció: Aquest makefile defineix les regles per a la compilació
# dels diferents executables del projecte, incloent-hi Fleck, Harley,
# Enigma i Gotham, així com les dependències comunes (FileReader.c, StringUtils.c, etc.),
# i els nous mòduls (FrameUtils.c).
##################################################

# Variables
CC = gcc
CFLAGS = -Wall -Wextra -IFileReader -IStringUtils -IDataConversion -INetworking -IFrameUtils

# Llista de fitxers comuns
COMMON = FileReader/FileReader.c StringUtils/StringUtils.c DataConversion/DataConversion.c Networking/Networking.c FrameUtils/FrameUtils.c

# Objectius per compilar cada executable
all: Fleck.exe Harley.exe Enigma.exe Gotham.exe

# Compilació de Fleck
Fleck.exe: Fleck.c $(COMMON)
	$(CC) $(CFLAGS) Fleck.c $(COMMON) -o Fleck.exe

# Compilació de Harley
Harley.exe: Harley.c $(COMMON)
	$(CC) $(CFLAGS) Harley.c $(COMMON) -o Harley.exe

# Compilació de Enigma
Enigma.exe: Enigma.c $(COMMON)
	$(CC) $(CFLAGS) Enigma.c $(COMMON) -o Enigma.exe

# Compilació de Gotham
Gotham.exe: Gotham.c $(COMMON)
	$(CC) $(CFLAGS) Gotham.c $(COMMON) -o Gotham.exe

# Regles per executar automàticament cada programa amb el fitxer de configuració corresponent
Fleck: Fleck.exe
	./Fleck.exe fitxers_configuració/config_fleck.dat

Harley: Harley.exe
	./Harley.exe fitxers_configuració/config_harley.dat

Enigma: Enigma.exe
	./Enigma.exe fitxers_configuració/config_enigma.dat

Gotham: Gotham.exe
	./Gotham.exe fitxers_configuració/config_gotham.dat

# Neteja els fitxers generats
clean:
	rm -f *.exe
