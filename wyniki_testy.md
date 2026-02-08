# Test 1: Mała skala bez ingerencji

## Specyfikacja:
tab_X - po 20 dla każdego limitu urzędnika, łącznie 100 petentów
N - 9
K - 3

## Wyniki:
### Output summary.sh po zakończeniu programu:
```
petentow stworzonych: 102
petentow przetworzonych: 100
petentow porazek: 0
uruchomione procesy(powinien być tylko bash i ps):
    PID TTY          TIME CMD
   4294 pts/1    00:00:00 bash
  65304 pts/1    00:00:00 ps
pozostałe struktury V(powinny być puste):

------ Message Queues --------
key        msqid      owner      perms      used-bytes   messages    

------ Shared Memory Segments --------
key        shmid      owner      perms      bytes      nattch     status      

------ Semaphore Arrays --------
key        semid      owner      perms      nsems     
```
Jak widać, przetworzono powstało 102 petentów, co jest zgodne ze specyfikacją, przetworzono 100 i 0 zakończyło się porażką.

Ponadto, wszystkie procesy i struktury systemu V zostały zamknięte.

Test został powtórzony dla obu sygnałów  z tym samym efektem.

# Test 2: Średnia skala bez ingerencji:

## Specyfikacja:
tab_X = {1000, 500, 600, 400, 2000}, łącznie 4500
N - 60
K - 20

## Wyniki:
### Output summary.sh po zakończeniu programu:
```
petentow stworzonych: 4506
petentow przetworzonych: 4500
petentow porazek: 0
uruchomione procesy(powinien być tylko bash i ps):
    PID TTY          TIME CMD
   4294 pts/1    00:00:00 bash
  85857 pts/1    00:00:00 ps
pozostałe struktury V(powinny być puste):

------ Message Queues --------
key        msqid      owner      perms      used-bytes   messages    

------ Shared Memory Segments --------
key        shmid      owner      perms      bytes      nattch     status      

------ Semaphore Arrays --------
key        semid      owner      perms      nsems     
```
Jak widać wszystko poprawnie. Test został powtórzony dla obu sygnałów z tym samym rezultatem.

# Test 3: Średnia skala z ctrl+z
## Specyfikacja:
Analogicznie do powyższej
## Wyniki:
1. Przed odczekaniem odliczania:
  * Terminal:
  ```
  ./main
  ^Z
  [1]+  Stopped                 ./main
  ```
  * Output ps:
  ```
    PID TTY          TIME CMD
   6053 pts/1    00:00:00 bash
  34465 pts/1    00:00:00 main
  34466 pts/1    00:00:00 urzednik
  34467 pts/1    00:00:00 urzednik
  34468 pts/1    00:00:00 urzednik
  34469 pts/1    00:00:00 urzednik
  34470 pts/1    00:00:00 urzednik
  34471 pts/1    00:00:00 urzednik
  34472 pts/1    00:00:00 rejestr
  34473 pts/1    00:00:00 generator_peten
  34474 pts/1    00:00:00 dyrektor
  34658 pts/1    00:00:00 ps
  ```
  * Output ipcs:
  ```
  ------ Message Queues --------
  key        msqid      owner      perms      used-bytes   messages    
  0xa402195c 19         e          666        0            0           
  0xa602195c 20         e          666        0            0           
  0xa302195c 21         e          666        0            0           
  0xa202195c 22         e          666        0            0           
  0xa502195c 23         e          666        0            0           

  ------ Shared Memory Segments --------
  key        shmid      owner      perms      bytes      nattch     status      
  0x0102195c 19         e          666        4          3                       

  ------ Semaphore Arrays --------
  key        semid      owner      perms      nsems     
  0x0102195c 11         e          666        7
  ```
  * Terminal po fg:
  ```
  ./main

  ```
Jak widać, program poprawnie inicjalizuje wszystkie procesy i zasoby oraz zatrzymuje odliczanie do uruchomienia symulacji.

2. W trakcie symulacji:
  * Terminal (fragment):
  ```
  Marcin Kowalczyk 49 pid 84151 vip=0
  aktywnych petentow - 1
  pid 84151 otrzymal - jestes przetworzony
  Marcin Wiśniewski 48 pid 84152 vip=0
  aktywnych petentow - 1
  pid 84152 otrzymal - jestes przetworzony
  Katarzyna Wiśniewski 70 pid 84153 vip=0
  aktywnych petentow - 1
  pid 84153 otrzymal - jestes przetworzony
  Marek Kamiński 44 pid 84154 vip=0
  aktywnych petentow - 0
  pid 84154 otrzymal - jestes przetworzony
  Marcin Nowak 19 pid 84155 vip=0
  aktywnych petentow - 1
  pid 84155 otrzymal - jestes przetworzony
  Ewa Szymański 16 pid 84156 vip=0
  aktywnych petentow - 1
  pid 84156 otrzymal - jestes przetworzony
  Jan Kowalczyk 46 pid 84158 vip=0
  aktywnych petentow - 0
  ^Z
  [1]+  Stopped                 ./main
  ```
  Output ps(fragment):
  ```
  6053 pts/1    00:00:00 bash
  82958 pts/1    00:00:00 main
  82959 pts/1    00:00:00 urzednik
  82960 pts/1    00:00:00 urzednik
  82961 pts/1    00:00:00 urzednik
  82962 pts/1    00:00:00 urzednik
  82963 pts/1    00:00:00 urzednik
  82964 pts/1    00:00:00 urzednik
  82965 pts/1    00:00:00 rejestr
  82966 pts/1    00:00:00 generator_peten
  82967 pts/1    00:00:00 dyrektor
  83063 pts/1    00:00:00 petent
  83064 pts/1    00:00:00 petent
  83065 pts/1    00:00:00 petent
  83066 pts/1    00:00:00 petent
  83067 pts/1    00:00:00 petent
  83068 pts/1    00:00:00 petent
  83069 pts/1    00:00:00 petent
  ...
  kilkadziesiąt procesów petent + kilka procesów rejestr
  ...
  ```
  * Output ipcs:
  ```
  ------ Message Queues --------
  key        msqid      owner      perms      used-bytes   messages    
  0x1102195c 43         e          666        0            0           
  0x0f02195c 44         e          666        0            0           
  0x1302195c 45         e          666        0            0           
  0x1202195c 46         e          666        0            0           
  0x1002195c 47         e          666        0            0           
  0x1502195c 48         e          666        0            0           

  ------ Shared Memory Segments --------
  key        shmid      owner      perms      bytes      nattch     status      
  0x0102195c 37         e          666        4          1                       
  0x0202195c 38         e          666        20         1                       
  0x1502195c 39         e          666        8          1                       

  ------ Semaphore Arrays --------
  key        semid      owner      perms      nsems     
  0x0102195c 27         e          666        7         
  0x1502195c 28         e          666        1 
  ```
Po wykonaniu fg program wraca do pracy.