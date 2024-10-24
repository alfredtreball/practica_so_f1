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
# Enigma i Gotham, així com les dependències comunes (Utils.c). També 
# proporciona un objectiu per a la neteja dels fitxers generats durant
# el procés de compilació.
##################################################

# Variables
CC = gcc
CFLAGS = -Wall -Wextra

# Objectius per compilar cada executable
all: Fleck.exe Harley.exe Enigma.exe Gotham.exe

# Compilació de Fleck
Fleck.exe: Fleck.c FileReader.c StringUtils.c DataConversion.c
	$(CC) $(CFLAGS) Fleck.c  FileReader.c StringUtils.c DataConversion.c -o Fleck.exe

# Compilació de Harley
Harley.exe: Harley.c FileReader.c
	$(CC) $(CFLAGS) Harley.c FileReader.c StringUtils.c -o Harley.exe

# Compilació de Enigma
Enigma.exe: Enigma.c FileReader.c
	$(CC) $(CFLAGS) Enigma.c FileReader.c StringUtils.c -o Enigma.exe

# Compilació de Gotham
Gotham.exe: Gotham.c FileReader.c
	$(CC) $(CFLAGS) Gotham.c FileReader.c StringUtils.c -o Gotham.exe

# Neteja els fitxers generats
clean:
	rm -f *.exe