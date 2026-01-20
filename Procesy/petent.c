#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <errno.h>

struct msgbuf_rejestr // wiadomość od rejestru
{
    long mtype;
    pid_t pid;
} __attribute__((packed));

struct msgbuf_urzednik // wiadomość od urzednika
{
    long mtype;
    char mtext[21]; // TODO: przemyslec jak ma wygladac wiadomosc od urzednika
    pid_t pid;
} __attribute__((packed));

void SIGUSR2_handle(int sig);

pid_t recieve_rejestr(pid_t r_pid);
void handle_urzednik(pid_t u_pid);

int main(int argc, char **argv)
{
    signal(SIGUSR2, SIGUSR2_handle);
    printf("%s %s %d pid %d\n", argv[2], argv[3], atoi(argv[4]), getpid());
    // idziemy do rejestru
    pid_t r_pid = atoi(argv[1]);
    printf("petent r_pid=%d\n", r_pid);
    // odbieramy pid urzednika
    pid_t u_pid = recieve_rejestr(r_pid);
    // idziemy do urzednika
    handle_urzednik(u_pid);
    return 0;
}

pid_t recieve_rejestr(pid_t r_pid)
{
    // tworzymy klucz z maską pid rejestru
    key_t key = ftok(".", r_pid); // TODO: rejestr ma key, może lepiej go przekazywać tutaj
    if (key == -1)
    {
        perror("petent ftok");
        exit(1);
    }

    // dostajemy sie do kolejki
    int msgid = msgget(key, IPC_CREAT | 0666);
    if (msgid == -1)
    {
        perror("petent msgget");
        exit(1);
    }

    int shm_id = shmget(key, sizeof(int), 0); // pamiec
    if (shm_id == -1)
    {
        perror("petent shmget");
        exit(1);
    }

    int *shared_mem = (int *)shmat(shm_id, NULL, 0); // podlaczamy pamiec
    if (shared_mem == (int *)-1)
    {
        perror("petent shmat");
        exit(1);
    }

    struct msgbuf_rejestr msg;
    msg.mtype = 1;
    msg.pid = getpid();
    msgsnd(msgid, &msg, sizeof(pid_t), 0); // TODO: obsługa błędów
    (*shared_mem)++;
    msgrcv(msgid, &msg, sizeof(pid_t), getpid(), 0); // TODO: obsłużyć jeśli kolejka pusta itd.
    (*shared_mem)--;
    return msg.pid;
    printf("%d pid otrzymal - %d\n", msg.pid);
}

void handle_urzednik(pid_t u_pid)
{
    printf("petent dostal pid - %d\n", u_pid);
    // tworzymy klucz z maską pid rejestru
    key_t key = ftok(".", u_pid);
    if (key == -1)
    {
        perror("petent ftok");
        exit(1);
    }

    // dostajemy sie do kolejki
    int msgid = msgget(key, IPC_CREAT | 0666);
    if (msgid == -1)
    {
        perror("petent msgget");
        exit(1);
    }

    struct msgbuf_urzednik msg;
    msg.mtype = 1;
    msg.pid = getpid();
    msgsnd(msgid, &msg, sizeof(struct msgbuf_urzednik) - sizeof(long), 0); // TODO: obsługa błędów
    msgrcv(msgid, &msg, sizeof(struct msgbuf_urzednik) - sizeof(long), getpid(), 0); // TODO: obsłużyć jeśli kolejka pusta itd.
    printf("pid %d otrzymal - %s", getpid(), msg.mtext);
}

void SIGUSR2_handle(int sig)
{
    int i = 0;
    do
    {
        sleep(10);
        printf("PID %d - JESTEM SFRUSTROWANY\n", getpid());
        i+=10;
    } while (i < 120); // TODO: fajnie by byłoby to zrandowmizować, żeby nie mówili wszyscy naraz
    // może różne wiadomości
    exit(0);
}