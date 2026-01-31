#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/msg.h>
#include <time.h>

FILE *f;
time_t t;
struct tm *t_broken;

volatile sig_atomic_t CLOSE_GENTLY = 0;

int typ;

struct msgbuf_urzednik // wiadomość od urzednika
{
    long mtype;
    char mtext[30]; // TODO: przemyslec jak ma wygladac wiadomosc od urzednika
    pid_t pid;
} __attribute__((packed));

void SIGUSR1_handle(int sig);
void SIGUSR2_handle(int sig);
void SIGINT_handle(int sig);
void install_handler(int signo, void (*handler)(int));

void handle_petent();
void cleanup();
void log_msg(char *msg);

int main(int argc, char **argv)
{
    install_handler(SIGUSR1, SIGUSR1_handle);
    install_handler(SIGUSR2, SIGUSR2_handle);
    install_handler(SIGINT, SIGINT_handle);
    srand(time(NULL));
    printf("urzednik\n");
    raise(SIGSTOP);

    key_t key = atoi(argv[1]);
    typ = atoi(argv[2]);

    char filename[50];
    sprintf(filename, "./Logi/urzednik_%d", typ);
    f = fopen(filename, "w");
    if (!f)
    {
        perror("urzednik fopen");
        cleanup();
        exit(1);
    }
    log_msg("urzednik uruchomiony");

    handle_petent();

    cleanup();
    return 0;
}

void install_handler(int signo, void (*handler)(int))
{
    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(signo, &sa, NULL) == -1)
    {
        perror("sigaction");
        exit(1);
    }
}

void SIGUSR1_handle(int sig)
{
    // log_msg("urzednik przechwycil SIGUSR1");
    CLOSE_GENTLY = 1;
}

void SIGUSR2_handle(int sig)
{
    // log_msg("urzednik przechwycil SIGUSR2");
    cleanup();
    exit(0);
}

void SIGINT_handle(int sig)
{
    // log_msg("urzednik przechwycil SIGINT");
    cleanup();
    exit(0);
}

void handle_petent()
{
    log_msg("urzednik uruchamia handle_petent");
    // tworzymy klucz z maską pid rejestru
    log_msg("urzednik tworzy klucz");
    key_t key = typ == 5 ? ftok(".", getpid() - 1) : ftok(".", getpid());
    if (key == -1)
    {
        perror("urzednik ftok");
        log_msg("error ftok self");
        exit(1);
    }
    char s_klucz[50];
    sprintf(s_klucz, "urzednik tworzy klucz o wartosci=%d", key);
    log_msg(s_klucz);

    // dostajemy sie do kolejki
    log_msg("urzednik dostaje sie do kolejki");
    int msgid = msgget(key, IPC_CREAT | 0666);
    if (msgid == -1)
    {
        perror("urzednik msgget");
        log_msg("error msgget self");
        exit(1);
    }

    int n = 0;
    log_msg("urzednik zaczyna glowna petle");
    while (1)
    {
        sprintf(s_klucz, "wartosc CLOSE_GENTLY=%d", CLOSE_GENTLY);

        struct msgbuf_urzednik msg;
        msg.mtype = 1;
        if (CLOSE_GENTLY)
        {
            log_msg("urzednik obsluguje petentow i sie zamyka");
            while (1)
            {
                if (msgrcv(msgid, &msg, sizeof(struct msgbuf_urzednik) - sizeof(long), 1, IPC_NOWAIT) == -1)
                {
                    if (errno == ENOMSG)
                        break;
                    else
                        break;
                }
                sprintf(msg.mtext, "%s", "jestes przetworzony\n");
                msg.mtype = msg.pid;
                msg.pid = -1;
                msgsnd(msgid, &msg, sizeof(struct msgbuf_urzednik) - sizeof(long), 0);
            }

            log_msg("urzednik sie zamyka");
            cleanup();
            exit(0);
        }
        log_msg("urzednik odbiera wiadomosc");
        if (msgrcv(msgid, &msg, sizeof(struct msgbuf_urzednik) - sizeof(long), 1, 0) == -1) // TODO: obsłużyć jeśli kolejka pusta itd.
        {
            if (errno == EINTR)
                continue;
            else
            {
                perror("urzednik msgrcv");
                log_msg("error msgrcv");
                cleanup();
                exit(1);
            }
        }
        pid_t temp = msg.pid;
        sprintf(s_klucz, "urzednik odebral wiadomosc od pid=%d", temp);
        log_msg(s_klucz);
        msg.mtype = temp > 0 ? temp : 3; // nie powinien być == -1, to był bug
        if (typ < 4)
        {
            int l = rand() % 10;
            if (l == 1)
            {
                sprintf(msg.mtext, "%s", "prosze udac sie do kasy\n");
                msg.pid = 12; // placeholder
            }
            else
            {
                sprintf(msg.mtext, "%s", "jestes przetworzony\n");
                msg.pid = -1;
            }
        }
        else
        {
            int l = rand() % 10;
            if (l < 4)
            {
                sprintf(msg.mtext, "%s", "prosze udac sie do urzednika\n");
                msg.pid = typ == 4 ? getpid() - rand() % 4 - 1 : getpid() - rand() % 4 - 2;
            }
            else
            {
                sprintf(msg.mtext, "%s", "jestes przetworzony\n");
                msg.pid = -1;
            }
        }
        log_msg("urzednik wysyla wiadomosc z powrotem");
        msgsnd(msgid, &msg, sizeof(struct msgbuf_urzednik) - sizeof(long), 0);
    }
}

void cleanup()
{
    log_msg("urzednik uruchamia cleanup");
    key_t key = typ == 5 ? ftok(".", getpid() - 1) : ftok(".", getpid());
    if (key == -1)
    {
        perror("urzednik ftok");
        // exit(1);
    }

    int msgid = msgget(key, 0);
    if (msgid == -1)
    {
        perror("urzednik msgget");
        // exit(1);
    }

    if (msgctl(msgid, IPC_RMID, NULL) == -1)
    {
        perror("urzednik msgctl");
        // exit(1);
    }
    fclose(f);
}

void log_msg(char *msg)
{
    t = time(NULL);
    t_broken = localtime(&t);
    fprintf(f, "<%02d:%02d:%02d> %s\n", t_broken->tm_hour, t_broken->tm_min, t_broken->tm_sec, msg);
    fflush(f);
}