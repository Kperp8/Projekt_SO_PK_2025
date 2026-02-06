#ifndef COMMON_H
#define COMMON_H

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/wait.h>

#define ILE_SEMAFOROW 6
#define SEMAFOR_MAIN 0
#define SEMAFOR_DYREKTOR 1
#define SEMAFOR_GENERATOR 2
#define SEMAFOR_REJESTR 3
#define SEMAFOR_REJESTR_DWA 4
#define SEMAFOR_PETENCI 5
#define ILE_PROCESOW 8 // 0-5 urzednicy, 6-rejestr, 7-generator, 8-dyrektor

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

// ----ZMIENNE STERUJĄCE----
// int N w dyrektor.c - liczba petentów, którzy mogą znajdować się w budynku
// int K w dyrektor.c - "cutoff" w stosunku do którego otwieramy i zamykamy dodatkowe automaty biletowe, musi być K>=N/3
// tab_X w rejestr.c - limity dzienne urzędników, tzn. ilu petentów mogą przetworzyć

#endif