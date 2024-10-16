# Variables
CC = gcc
CFLAGS = -Wall -Wextra

# Objectius per compilar cada executable
all: Fleck.exe Harley.exe Enigma.exe Gotham.exe

# Compilació de Fleck
Fleck.exe: Fleck.c Utils.c
	$(CC) $(CFLAGS) Fleck.c Utils.c -o Fleck.exe

# Compilació de Harley
Harley.exe: Harley.c Utils.c
	$(CC) $(CFLAGS) Harley.c Utils.c -o Harley.exe

# Compilació de Enigma
Enigma.exe: Enigma.c Utils.c
	$(CC) $(CFLAGS) Enigma.c Utils.c -o Enigma.exe

# Compilació de Gotham
Gotham.exe: Gotham.c Utils.c
	$(CC) $(CFLAGS) Gotham.c Utils.c -o Gotham.exe

# Neteja els fitxers generats
clean:
	rm -f *.exe