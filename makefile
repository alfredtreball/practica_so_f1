# Variables
CC = gcc
CFLAGS = -Wall -Wextra

# Objectius per compilar cada executable
all: Fleck.exe Harley.exe Enigma.exe Gotham.exe

# Compilació de Fleck
Fleck.exe: Fleck.c
	$(CC) $(CFLAGS) Fleck.c -o Fleck.exe

# Compilació de Harley
Harley.exe: Harley.c
	$(CC) $(CFLAGS) Harley.c -o Harley.exe

# Compilació de Enigma
Enigma.exe: Enigma.c
	$(CC) $(CFLAGS) Enigma.c -o Enigma.exe

# Compilació de Gotham
Gotham.exe: Gotham.c
	$(CC) $(CFLAGS) Gotham.c -o Gotham.exe

# Neteja els fitxers generats
clean:
	rm -f *.exe
