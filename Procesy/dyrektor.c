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
#include <time.h>

// TODO: pełne działanie petentów
// TODO: typy jak key_t i pid_t są używane niespójnie
// TODO: podzielić kod w main.c na funkcjie
// TODO: w wielu miejscach zła filozofia funkcji asynchronicznych, głównie cleanup() wywoływany w handlerze
// TODO: petenci powinni mieć limit czasu przez jaki istnieją
// TODO: urzędnicy mają niepełną funkcjonalność
// TODO: w rejestrze dużo powtarzającego się kodu
// TODO: głupie nazwy w rejestrze
// TODO: skoro tab_X i tak jest w pamięci dzielonej, równie dobrze dyrektor może go zapisać
// TODO: rejestr jest super brzydki
// TODO: obsługa edge-casuw
// TODO: większość bibliotek się powtarza, można je upchnąć do jednego pliku
// TODO: proces usuwający zombie
// TODO: urzędnicy mogą odebrać wiadomości z pid=-1, generalnie przemyśleć ich logikę

#define ILE_SEMAFOROW 9
#define SEMAFOR_MAIN 0
#define SEMAFOR_DYREKTOR 1
#define SEMAFOR_GENERATOR 2
#define SEMAFOR_REJESTR 3
#define ILE_PROCESOW 8

time_t Tp, Tk;
int N = 27, K = 9; // do generator, rejestr

FILE *f;
time_t t;
struct tm *t_broken;

key_t key;
key_t p_id[ILE_PROCESOW];

union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

void cleanup();
void SIGINT_handle(int sig);
int recieve_main(int sems, key_t *shared_mem);
int send_generator(int sems, key_t *shared_mem);
int send_rejestr(int sems, key_t *shared_mem);
void log_msg(char *msg);

int main(int argc, char **argv)
{
    signal(SIGINT, SIGINT_handle);

    time_t now, tp, tk;
    struct tm tm_tp, tm_tk;

    now = time(NULL);
    srand(now);

    int which = rand() % 2;

    tp = now + 10 + rand() % 20;
    tk = now + 30 + rand() % 60;

    localtime_r(&tp, &tm_tp);
    localtime_r(&tk, &tm_tk);

    f = fopen("./Logi/dyrektor", "w");
    if (!f)
    {
        perror("dyrektor fopen");
        cleanup();
        exit(1);
    }

    log_msg("dyrektor uruchomiony");

    char message[128];
    snprintf(
        message,
        sizeof(message),
        "program uruchomi sie o %02d:%02d:%02d, "
        "zakonczy sie o %02d:%02d:%02d, "
        "wysle sygnal=%d",
        tm_tp.tm_hour, tm_tp.tm_min, tm_tp.tm_sec,
        tm_tk.tm_hour, tm_tk.tm_min, tm_tk.tm_sec,
        which + 1);

    log_msg(message);

    key = atoi(argv[1]); // odbieramy klucz
    printf("dyrektor\n");

    sleep(tp - now);
    log_msg("dyrektor uruchamia reszte procesow");
    kill(0, SIGCONT); // TODO: może uruchamiać po pidach, trochę niżej

    int sems = semget(key, ILE_SEMAFOROW, 0); // semafory
    if (sems == -1)
    {
        perror("dyrektor semget");
        log_msg("error semget main");
        cleanup();
        exit(1);
    }

    int shm_id = shmget(key, sizeof(key_t), 0); // pamiec
    if (shm_id == -1)
    {
        perror("dyrektor shmget");
        log_msg("error shmget main");
        cleanup();
        exit(1);
    }

    key_t *shared_mem = (key_t *)shmat(shm_id, NULL, 0); // podlaczamy pamiec
    if (shared_mem == (key_t *)-1)
    {
        perror("dyrektor shmat");
        log_msg("error shmat main");
        cleanup();
        exit(1);
    }

    if (recieve_main(sems, shared_mem) != 0)
    {
        perror("dyrektor recieve_main");
        log_msg("error recieve_main");
        cleanup();
        exit(1);
    }
    log_msg("dyrektor odebral od main");
    // printf("dyrektor odebral pidy\n");

    // printf("dyrektor otrzymal:\n");
    // for (int i = 0; i < ILE_PROCESOW; i++)
    // {
    //     printf("%d\n", p_id[i]);
    //     // kill(p_id[i], SIGUSR1);
    //     // kill(p_id[i], SIGUSR2);
    // }
    union semun arg;
    arg.val = 1;
    if (semctl(sems, SEMAFOR_DYREKTOR, SETVAL, arg) == -1)
    {
        perror("dyrektor semctl");
        log_msg("error recieve_main");
        cleanup();
        exit(1);
    }

    if (send_generator(sems, shared_mem) != 0)
    {
        perror("dyrektor send_generator");
        log_msg("error send_generator");
        cleanup();
        exit(1);
    }
    log_msg("dyrektor wyslal do generator");

    if (send_rejestr(sems, shared_mem) != 0)
    {
        perror("dyrektor send_rejestr");
        log_msg("error send_rejestr");
        cleanup();
        exit(1);
    }
    log_msg("dyrektor wyslal do rejestr");

    printf("przeslano\n");
    log_msg("dyrektor skonczyl przesyl");

    if (shmdt(shared_mem) != 0)
        perror("dyrektor shmdt");

    // sleep(120);

    while (time(NULL) < tk)
        sleep(1);

    for (int i = 0; i < ILE_PROCESOW; i++)
        kill(p_id[i], which == 0 ? SIGUSR1 : SIGUSR2);

    printf("koniec\n");
    log_msg("dyrektor konczy dzialanie");
    cleanup();

    return 0;
}

void cleanup()
{
    log_msg("dyrektor wykonuje cleanup");
    int semid = semget(key, 0, 0);
    if (semid != -1)
        semctl(semid, 0, IPC_RMID);
    int shmid = shmget(key, sizeof(key_t), 0);
    if (shmid != -1)
    {
        key_t *shared_mem = (key_t *)shmat(shmid, NULL, 0);
        if (shmdt(shared_mem) != 0)
            perror("dyrektor shmdt");
        shmctl(shmid, IPC_RMID, NULL);
    }
    // for (int i = 0; i < ILE_PROCESOW; i++)
    // kill(p_id[i], SIGINT);
    kill(0, SIGINT);
    fclose(f);
}

void SIGINT_handle(int sig)
{
    printf("\nprzechwycono SIGINT\n");
    log_msg("dyrektor przechwycil SIGINT");
    cleanup();
    exit(0);
}

int recieve_main(int sems, key_t *shared_mem)
{
    log_msg("dyrektor odbiera od main");
    struct sembuf P = {.sem_num = SEMAFOR_DYREKTOR, .sem_op = -1, .sem_flg = 0};
    struct sembuf V = {.sem_num = SEMAFOR_MAIN, .sem_op = +1, .sem_flg = 0};

    for (int i = 0; i < ILE_PROCESOW; i++) // odbieramy p_id[]
    {
        log_msg("dyrektor blokuje semafor DYREKTOR");
        while (semop(sems, &P, 1) == -1)
        {
            if (errno == EINTR)
                continue;
            else
            {
                perror("dyrektor semop P");
                log_msg("error semop main");
                return 1;
            }
        }

        p_id[i] = *shared_mem;

        log_msg("dyrektor oddaje semafor MAIN");
        while (semop(sems, &V, 1) == -1)
        {
            if (errno == EINTR)
                continue;
            else
            {
                perror("dyrektor semop V");
                log_msg("error semop main");
                return 1;
            }
        }
    }
    return 0;
}

int send_generator(int sems, key_t *shared_mem)
{
    log_msg("dyrektor wysyla do generator");

    struct sembuf P = {.sem_num = SEMAFOR_DYREKTOR, .sem_op = -1, .sem_flg = 0};
    struct sembuf V = {.sem_num = SEMAFOR_GENERATOR, .sem_op = +1, .sem_flg = 0};

    for (int i = 0; i < 2; i++)
    {
        log_msg("dyrektor blokuje semafor DYREKTOR");
        while (semop(sems, &P, 1) == -1) // czekamy czy można wysyłać
        {
            if (errno == EINTR)
                continue;
            else
            {
                perror("dyrektor semop P");
                log_msg("error semop generator");
                return 1;
            }
        }

        *shared_mem = i == 0 ? N : p_id[6];

        log_msg("dyrektor oddaje semafor GENERATOR");
        while (semop(sems, &V, 1) == -1) // zaznaczamy, że można czytać
        {
            if (errno == EINTR)
                continue;
            else
            {
                perror("dyrektor semop V");
                log_msg("error semop generator");
                return 1;
            }
        }
    }
    return 0;
}

int send_rejestr(int sems, key_t *shared_mem)
{
    log_msg("dyrektor wysyla do rejestr");
    struct sembuf P = {.sem_num = SEMAFOR_DYREKTOR, .sem_op = -1, .sem_flg = 0};
    struct sembuf V = {.sem_num = SEMAFOR_REJESTR, .sem_op = +1, .sem_flg = 0};

    for (int i = 0; i < 8; i++)
    {
        log_msg("dyrektor blokuje semafor DYREKTOR");
        while (semop(sems, &P, 1) == -1) // czekamy czy można wysyłać
        {
            if (errno == EINTR)
                continue;
            else
            {
                perror("main semop P");
                log_msg("error semop rejestr");
                cleanup();
                exit(1);
            }
        }

        switch (i)
        {
        case 5:
            *shared_mem = K;
            break;
        case 6:
            *shared_mem = N;
            break;
        case 7:
            *shared_mem = p_id[7];
            break;
        default:
            *shared_mem = p_id[i];
        }

        log_msg("dyrektor oddaje semafor REJESTR");
        while (semop(sems, &V, 1) == -1) // zaznaczamy, że można czytać
        {
            if (errno == EINTR)
                continue;
            else
            {
                perror("main semop V");
                log_msg("error semop rejestr");
                cleanup();
                exit(1);
            }
        }
    }
    return 0;
}

void log_msg(char *msg)
{
    t = time(NULL);
    t_broken = localtime(&t);
    fprintf(f, "<%02d:%02d:%02d> %s\n", t_broken->tm_hour, t_broken->tm_min, t_broken->tm_sec, msg);
    fflush(f);
}