#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

#define ILE_SEMAFOROW 6
#define SEMAFOR_PETENCI 5

key_t key;
int sems;
struct sembuf V_free = {.sem_num = SEMAFOR_PETENCI, .sem_op = +1, .sem_flg = 0};

pid_t pid_self;
int vip;

FILE *f;
time_t t;
struct tm *t_broken;

struct msgbuf_rejestr // wiadomość od rejestru
{
    long mtype;
    pid_t pid;
} __attribute__((packed));

struct msgbuf_urzednik // wiadomość od urzednika
{
    long mtype;
    char mtext[30];
    pid_t pid;
} __attribute__((packed));

void SIGUSR2_handle(int sig);
void EMPTY_handle(int sig);

pid_t recieve_rejestr(pid_t r_pid);
void handle_urzednik(pid_t u_pid);
void *opiekun(void *arg);
void log_msg(char *msg);

int main(int argc, char **argv)
{
    key = ftok(".", 1);
    if (key == -1)
    {
        perror("petent ftok");
        log_msg("error ftok");
        exit(1);
    }

    sems = semget(key, ILE_SEMAFOROW, 0);
    if (sems == -1)
    {
        perror("petent semget");
        log_msg("error semget");
        exit(1);
    }

    pid_self = getpid();
    vip = atoi(argv[5]);
    signal(SIGUSR2, SIGUSR2_handle);
    signal(SIGUSR1, EMPTY_handle);
    f = fopen("./Logi/petent", "a");
    if (!f)
    {
        perror("petent fopen");
        semop(sems, &V_free, 1);
        exit(1);
    }
    printf("%s %s %d pid %d vip=%d\n", argv[2], argv[3], atoi(argv[4]), pid_self, vip);
    char message[100];
    sprintf(message, "petent %s %s wiek %d pid %d r_pid %d", argv[2], argv[3], atoi(argv[4]), pid_self, atoi(argv[1]));
    log_msg(message);
    pid_t r_pid = atoi(argv[1]);

    if (atoi(argv[4]) < 18) // jeśli nieletni, uruchamiamy oosbny wątek jako opiekuna prawnego
    {
        sprintf(message, "%d nieletni obslugiwany", pid_self);
        log_msg(message);
        pthread_t tid;
        pthread_create(&tid, NULL, opiekun, &r_pid);
        pthread_join(tid, NULL);
        exit(0);
    }

    pid_t u_pid = recieve_rejestr(r_pid);
    if (u_pid == -1)
        raise(SIGUSR2);
    handle_urzednik(u_pid);
    return 0;
}

pid_t recieve_rejestr(pid_t r_pid)
{
    char message[100];
    sprintf(message, "%d uruchamia recieve_rejestr", pid_self);
    log_msg(message);
    // tworzymy klucz z maską pid rejestru
    key_t key = ftok(".", r_pid);
    if (key == -1)
    {
        perror("petent ftok");
        sprintf(message, "%d error ftok recieve_rejestr", pid_self);
        log_msg(message);
        semop(sems, &V_free, 1); // zwalniamy miejsce w budynku
        exit(1);
    }

    int msgid = msgget(key, 0);
    if (msgid == -1)
    {
        perror("petent msgget");
        sprintf(message, "%d error msgget recieve_rejestr", pid_self);
        log_msg(message);
        semop(sems, &V_free, 1);
        exit(1);
    }

    int shm_id = shmget(key, sizeof(long), 0);
    if (shm_id == -1)
    {
        perror("petent shmget");
        sprintf(message, "%d error shmget recieve_rejestr", pid_self);
        log_msg(message);
        semop(sems, &V_free, 1);
        exit(1);
    }

    long *shared_mem = (long *)shmat(shm_id, NULL, 0);
    if (shared_mem == (long *)-1)
    {
        perror("petent shmat");
        sprintf(message, "%d error shmat recieve_rejestr", pid_self);
        log_msg(message);
        semop(sems, &V_free, 1);
        exit(1);
    }

    int sems = semget(key, 1, 0);
    if (sems == -1)
    {
        perror("petent semget");
        sprintf(message, "%d error semget recieve_rejestr", pid_self);
        log_msg(message);
        semop(sems, &V_free, 1);
        exit(1);
    }

    struct sembuf P = {.sem_num = 0, .sem_op = -1, .sem_flg = 0};
    struct sembuf V = {.sem_num = 0, .sem_op = +1, .sem_flg = 0};

    struct msgbuf_rejestr msg;
    if (vip)
        kill(r_pid, SIGRTMIN);
    msg.mtype = vip == 1 ? 2 : 1;
    msg.pid = getpid();
    sprintf(message, "%d wysyla wiadomosc do rejestr", pid_self);
    log_msg(message);
    msgsnd(msgid, &msg, sizeof(pid_t), 0);
    sprintf(message, "%d blokuje semafor 0 rejestr", pid_self);
    log_msg(message);
    semop(sems, &P, 1);
    (*shared_mem)++;
    sprintf(message, "%d oddaje semafor 0 rejestr", pid_self);
    log_msg(message);
    semop(sems, &V, 1);
    sprintf(message, "%d odbiera wiadomosc", pid_self);
    log_msg(message);
    msgrcv(msgid, &msg, sizeof(pid_t), pid_self, 0);
    sprintf(message, "%d blokuje semafor 0 rejestr", pid_self);
    log_msg(message);
    semop(sems, &P, 1);
    (*shared_mem)--;
    sprintf(message, "%d oddaje semafor 0 rejestr", pid_self);
    log_msg(message);
    semop(sems, &V, 1);
    shmdt(shared_mem);
    return msg.pid != pid_self ? msg.pid : -1;
}

void handle_urzednik(pid_t u_pid)
{
    char message[100];
    sprintf(message, "%d uruchamia handle_urzednik pid=%d", pid_self, u_pid);
    log_msg(message);
    // tworzymy klucz z maską pid rejestru
    key_t key = ftok(".", u_pid);
    if (key == -1)
    {
        perror("petent ftok");
        sprintf(message, "%d error ftok handle_urzednik", pid_self);
        log_msg(message);
        semop(sems, &V_free, 1);
        exit(1);
    }

    int msgid = msgget(key, 0);
    if (msgid == -1)
    {
        perror("petent msgget");
        sprintf(message, "%d error msgget handle_urzednik", pid_self);
        log_msg(message);
        semop(sems, &V_free, 1);
        exit(1);
    }

    struct msgbuf_urzednik msg;
    if (vip)
        kill(u_pid, SIGRTMIN);
    msg.mtype = vip == 1 ? 2 : 1;
    msg.pid = getpid();
    sprintf(message, "%d wysyla wiadomosc do urzednik", pid_self);
    log_msg(message);
    msgsnd(msgid, &msg, sizeof(struct msgbuf_urzednik) - sizeof(long), 0);
    sprintf(message, "%d odbiera wiadomosc", pid_self);
    log_msg(message);
    msgrcv(msgid, &msg, sizeof(struct msgbuf_urzednik) - sizeof(long), getpid(), 0);
    if (msg.pid != -1)
    {
        if (!strcmp(msg.mtext, "prosze udac sie do kasy\n"))
        {
            // proces idzie uiścić zapłate i wraca
            sprintf(message, "%d idzie do kasy", pid_self);
            log_msg(message);
            sleep(2);
            handle_urzednik(u_pid);
        }
        else
        {
            sprintf(message, "%d idzie do urzednika pid=%d", pid_self, msg.pid);
            log_msg(message);
            handle_urzednik(msg.pid);
        }
    }
    printf("pid %d otrzymal - %s", pid_self, msg.mtext);
    sprintf(message, "%d otrzymal wiadomosc %s", pid_self, msg.mtext);
    log_msg(message);
    semop(sems, &V_free, 1);
    exit(0); // żeby dwa razy się nie odzywali jeśli są odesłani
}

void *opiekun(void *arg)
{
    pid_t r_pid = *(pid_t *)arg;
    pid_t u_pid = recieve_rejestr(r_pid);
    if (u_pid == -1)
        raise(SIGUSR2);
    handle_urzednik(u_pid);
}

void SIGUSR2_handle(int sig)
{
    char message[40];
    sprintf(message, "%d przechwycil sygnal SIGUSR2", pid_self);
    log_msg(message);
    int i = 0;
    do
    {
        sleep(10);
        printf("PID %d - JESTEM SFRUSTROWANY\n", pid_self);
        i += 10;
    } while (i < 20);
    semop(sems, &V_free, 1);
    exit(0);
}

void EMPTY_handle(int sig)
{
}

void log_msg(char *msg)
{
    t = time(NULL);
    t_broken = localtime(&t);
    fprintf(f, "<%02d:%02d:%02d> %s\n", t_broken->tm_hour, t_broken->tm_min, t_broken->tm_sec, msg);
    fflush(f);
}