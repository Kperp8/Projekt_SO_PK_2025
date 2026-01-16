# Makefile dla main.c i programów w katalogu Procesy
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L

# Plik main.c
MAIN_SRC = main.c
MAIN_PROGRAM = main

# Pliki źródłowe w katalogu Procesy
PROCESY_SOURCES := $(wildcard Procesy/*.c)

# Pliki wykonywalne w katalogu Procesy (ten sam katalog co źródła)
PROCESY_PROGRAMS := $(PROCESY_SOURCES:.c=)

# Wszystkie programy
PROGRAMS = $(MAIN_PROGRAM) $(PROCESY_PROGRAMS)

# Domyślny cel
all: $(PROGRAMS)

# Kompilacja main.c (do katalogu głównego)
$(MAIN_PROGRAM): $(MAIN_SRC)
	$(CC) $(CFLAGS) $< -o $@

# Kompilacja każdego pliku w Procesy/ do tego samego katalogu
Procesy/%: Procesy/%.c
	$(CC) $(CFLAGS) $< -o $@

# Sprzątanie
clean:
	rm -f $(MAIN_PROGRAM) $(PROCESY_PROGRAMS)