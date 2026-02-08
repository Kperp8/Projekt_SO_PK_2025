# Jak uruchomić

Potrzebne:
* Kompilator gcc C11+
* Make

Należy skompilować program używając polecenia make i uruchomić plik binarny make, znajdując się w folderze nadrzędnym.

Program działa tylko na Linuxie. Testowane na WSL Ubuntu 24.04.3 LTS oraz Ubuntu 25.10

# Opis projektu

## Wstęp
Projekt jest ***symulacją*** działania budynku urzędu miasta Kraków, gdzie urzędnicy oraz petenci są reprezentowani przez **procesy programu**. Głównym celem programu jest przedstawienie wiedzy o funkcjonowaniu systemu operacyjnego komputera, dokładniej funkcjonowania programów. 

## Rodzaje procesów

### `Dyrektor`
Proces nadrzędny, który:
* Definiuje wartości `Tp`, `Tk`, `N`, `K`;
* Nadzoruje pracę pozostałych procesów;
* Wysyła `sygnały 1 i 2`;

### `Rejestracja`
Proces podrzędny, który:
* Wydaje procesom `Petent` **bilet** do jednego z urzędników;
* Ilość aktywnych procesów `Rejestracja` zmienia się w zależności od ilości procesów `Petent`;

### `Urzędnik`
Proces podrzędny, który:
* Przyjmuje i **obsługuje** ilość procesów `Petent` zależną od **rodzaju** procesu `Urzędnik`;
* Obsługuje sygnały wysyłane przez proces `Dyrektor`;

### `Petent`
Proces podrzędny, który:
* Otrzymuje **bilet** od procesu `Rejestracja`;
* *Próbuje* zostać obsłużony przez odpowiedni proces `Urzędnik`;
* Obsługuje sygnały wysyłane przez proces `Dyrektor`;

### `Generator petentów`
Proces podrzędny, który:
* Stara się generować procesy `Petent` do osiągnięcia limitu `N`;

## Omówienie działania symulacji
W budynku urzędu miasta znajdują się następujące wydziały/urzędy:
* Urząd Stanu Cywilnego - `SC`
* Wydział Ewidencji Pojazdów i Kierowców - `KM`
* Wydział Mieszkalnictwa - `ML`
* Wydział Podatków i Opłat - `PD`
* Wydział Spraw Administracyjnych - `SA`

W **Wydziale Spraw Administracyjnych** pracuje **dwóch** urzędników, w pozostałych wydziałach po
**jednym** urzędniku. Każdy z urzędników w danym dniu może przyjąć określoną **liczbę** osób: 
* urzędnicy `SA` każdy **X1** osób;
* urzędnik `SC` **X2** osób;
* urzędnik `KM` **X3** osób;
* urzędnik `ML` **X4** osób;
* urzędnik `PD` **X5** osób;

Urzędnicy `SA` przyjmują ok. **60%** wszystkich petentów, pozostali urzędnicy każdy po ok. **10%** wszystkich petentów (wartość *losowa*). Do obu urzędników `SA` jest jedna wspólna kolejka petentów, do pozostałych urzędników kolejki są indywidualne.
Urzędnicy `SA` część petentów (ok. **40%** z **X1**) kierują do innych urzędników (`SC`, `KM`, `ML`, `PD`) – ***bilet*** do danego urzędnika jest pobierany przez urzędnika `SA` i ***wręczany*** danemu petentowi, jeżeli dany urzędnik (`SC`, `KM`, `ML`, `PD`) **nie ma już wolnych terminów.**

**Dane petenta** (`id` – skierowanie do **urzędnik** - wystawił) powinny zostać zapisane w **raporcie dziennym.** Urzędnicy (`SC`, `KM`, `ML`, `PD`) dodatkowo mogą skierować petenta do **kasy** (ok. **10%**) celem uiszczenia *opłaty*, po dokonaniu której **petent** wraca do danego **urzędnika** i wchodzi do gabinetu bez kolejki (po uprzednim wyjściu **petenta**). Jeśli w chwili zamknięcia urzędu w kolejce do **urzędnika** czekali **petenci** to te osoby zostaną przyjęte w tym dniu ale nie mogą zostać skierowane do innych **urzędników** lub do **kasy**.


Zasady działania **urzędu** ustalone przez `Dyrektora` są następujące:
* **urząd** jest czynny w godzinach od `Tp` do `Tk`;
* W budynku w danej chwili może się znajdować co najwyżej `N` **petentów** (pozostali, jeżeli są czekają przed wejściem);
* Dzieci w wieku **poniżej 18 lat** do urzędu przychodzą pod opieką osoby **dorosłej**;
* Każdy **petent** przed wizytą u **urzędnika** musi pobrać **bilet** z ***systemu kolejkowego***;
* W urzędzie są **3 automaty biletowe**, zawsze działa **min. 1** stanowisko;
* Jeżeli w kolejce po **bilet** stoi więcej niż `K` petentów (`K` >= `N/3`) uruchamia się **drugi automat biletowy**. Drugi automat zamyka się jeżeli liczba petentów w kolejce jest mniejsza niż `N/3`.
Jeżeli w kolejkach po bilet stoi więcej niż `2K` **petentów** (`K` >= `N/3`) uruchamia się **trzeci automat biletowy**. Trzeci automat zamyka się jeżeli liczba petentów w kolejkach jest mniejsza niż `2/3*N`;
* Osoby uprawnione `VIP` (np. honorowy dawca krwi) do gabinetu lekarskiego **wchodzą bez kolejki**;
* Jeżeli zostaną wyczerpane **limity przyjęć** do danego **urzędnika**, **petenci** ci nie są przyjmowani (nie mogą pobrać **biletu** po wyczerpaniu limitu);
* Jeżeli zostaną wyczerpane **limity przyjęć** w danym dniu do wszystkich **urzędników**, **petenci nie są wpuszczani** do budynku;
* Jeśli w chwili zamknięcia urzędu w kolejce do **urzędnika** czekali **petenci** to te osoby **nie zostaną przyjęte** w tym dniu przez **urzędnika**. Dane tych **petentów** (`id` - sprawa do …. – `nr biletu`) powinny zostać zapisane w **raporcie dziennym** (procesy symulujące nieprzyjęte osoby działają jeszcze **2 minuty** po zakończeniu symulacji wypisując komunikat: **„PID - jestem sfrustrowany”**);

Na polecenie `Dyrektora` (`sygnał 1`) dany **urzędnik** obsługuje bieżącego **petenta** i kończy pracę przed **zamknięciem urzędu**. Dane **petentów** (`id` - skierowanie do **urzędnik** - wystawił), którzy nie zostali przyjęci powinny zostać zapisane w **raporcie dziennym**.
Na polecenie `Dyrektora` (`sygnał 2`) wszyscy **petenci** natychmiast opuszczają budynek.

# Raport z testów

## Test 1: Mała skala bez ingerencji

### Specyfikacja

* `tab_X`: po 20 dla każdego limitu urzędnika → **łącznie 100 petentów**
* `N = 9`
* `K = 3`

### Wyniki

**Output `summary.sh` po zakończeniu programu:**

```text
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

**Wnioski:**

* Utworzono 102 petentów – dopuszczone w specyfikacji.
* Przetworzono 100, brak porażek.
* Brak pozostawionych procesów i struktur System V.
* Test powtórzony dla obu sygnałów – identyczne wyniki.

---

## Test 2: Średnia skala bez ingerencji

### Specyfikacja

* `tab_X = {1000, 500, 600, 400, 2000}` → **łącznie 4500**
* `N = 60`
* `K = 20`

### Wyniki

**Output `summary.sh`:**

```text
petentow stworzonych: 4506
petentow przetworzonych: 4500
petentow porazek: 0
uruchomione procesy(powinien być tylko bash i ps):
    PID TTY          TIME CMD
   4294 pts/1    00:00:00 bash
  85857 pts/1    00:00:00 ps
pozostałe struktury V(powinny być puste):

------ Message Queues --------

------ Shared Memory Segments --------

------ Semaphore Arrays --------
```

**Wnioski:**

* Poprawne działanie, brak wycieków zasobów.
* Test powtórzony dla obu sygnałów – identyczne wyniki.

---

## Test 3: Średnia skala z `Ctrl+Z`

### Specyfikacja

Jak w teście 2.

### Wyniki

#### 1. Przed startem symulacji

* **Terminal:**

```text
./main
^Z
[1]+  Stopped                 ./main
```

* **`ps`:**

```text
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

* **`ipcs`:**
```text
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

* **Po `fg`:** program wraca do pracy.

#### 2. W trakcie symulacji

* **Terminal (fragment):**

```text
Marcin Kowalczyk 49 pid 84151 vip=0
...
^Z
[1]+  Stopped                 ./main
```

* **`ps`:** dziesiątki procesów `petent`, kilka `rejestr`, pełen zestaw procesów roboczych.

* **`ipcs`:**
```text
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

**Wnioski:**

* Program poprawnie obsługuje zatrzymanie (`SIGTSTP`) i wznowienie (`fg`).

---

## Test 4: Weryfikacja liczby petentów

### Specyfikacja

* `N = 5`
* `K = 2`

### Wyniki

* Semafory gwarantują, że logicznie pracuje jedynie N petentów, ale zazwyczaj uruchomionych jest więcej (w trakcie wyłączania itp).

---

## Test 5: Weryfikacja dodatkowych rejestrów

### Specyfikacja

* `N = 10`
* `K = 4`

### Wyniki

**Logi `rejestr_1`:**

```text
<18:09:42> rejestr uruchomiony pid=11003
<18:09:42> rejestr uruchamia handle_petent_klon
<18:09:42> rejestr uruchamia glowna petle
<18:09:42> wartosc CLOSE=0
<18:09:42> rejestr odbiera wiadomosc
<18:09:42> wartosc CLOSE=1
<18:09:42> rejestr czysci kolejke i sie zamyka
<18:09:42> rejestr uruchamia cleanup_klon
<18:09:42> rejestr uruchomiony pid=11028
<18:09:42> rejestr uruchamia handle_petent_klon
<18:09:42> rejestr uruchamia glowna petle
<18:09:42> wartosc CLOSE=0
<18:09:42> rejestr odbiera wiadomosc
<18:09:42> wartosc CLOSE=1
<18:09:42> rejestr czysci kolejke i sie zamyka
<18:09:42> rejestr uruchamia cleanup_klon
```

**Logi `rejestr_2`:**

```text
<18:09:42> rejestr uruchomiony pid=11004
<18:09:42> rejestr uruchamia handle_petent_klon
<18:09:42> rejestr uruchamia glowna petle
<18:09:42> wartosc CLOSE=1
<18:09:42> rejestr czysci kolejke i sie zamyka
<18:09:42> rejestr uruchamia cleanup_klon
```

**Wnioski:**

* Dodatkowe rejestry funkcjonują poprawnie.

---

## Test 6: Weryfikacja petentów VIP

### Specyfikacja

* Generator generuje wyłącznie VIP.

### Wyniki

**`summary.sh`:**

```text
petentow stworzonych: 2303
petentow przetworzonych: 2300
petentow porazek: 0
uruchomione procesy(powinien być tylko bash i ps):
    PID TTY          TIME CMD
   5722 pts/1    00:00:00 bash
  16432 pts/1    00:00:00 ps
pozostałe struktury V(powinny być puste):

------ Message Queues --------
key        msqid      owner      perms      used-bytes   messages    

------ Shared Memory Segments --------
key        shmid      owner      perms      bytes      nattch     status      

------ Semaphore Arrays --------
key        semid      owner      perms      nsems
```

**Logi `petent` (fragment):**

```text
<18:14:48> 15130 odbiera wiadomosc
<18:14:48> petent Tomasz Lewandowski wiek 43 pid 15131 r_pid 13066
<18:14:48> 15130 idzie do kasy
<18:14:48> 15131 uruchamia recieve_rejestr
<18:14:48> 15131 wysyla wiadomosc do rejestr
<18:14:48> 15131 blokuje semafor 0 rejestr
<18:14:48> 15131 oddaje semafor 0 rejestr
<18:14:48> 15131 odbiera wiadomosc
<18:14:48> 15131 blokuje semafor 0 rejestr
<18:14:48> 15131 oddaje semafor 0 rejestr
<18:14:48> 15131 uruchamia handle_urzednik pid=13064
<18:14:48> 15131 wysyla wiadomosc do urzednik
<18:14:48> 15131 odbiera wiadomosc
<18:14:48> 15131 otrzymal wiadomosc jestes przetworzony
```

**Wnioski:**

* Obsługa VIP działa poprawnie.

---

## Test 7: Weryfikacja petentów nieletnich

### Specyfikacja

* Generator generuje wyłącznie nieletnich.

### Wyniki

**`summary.sh`:**

```text
petentow stworzonych: 2304
petentow przetworzonych: 2300
petentow porazek: 0
uruchomione procesy(powinien być tylko bash i ps):
    PID TTY          TIME CMD
   5722 pts/1    00:00:00 bash
  27638 pts/1    00:00:00 ps
pozostałe struktury V(powinny być puste):

------ Message Queues --------
key        msqid      owner      perms      used-bytes   messages    

------ Shared Memory Segments --------
key        shmid      owner      perms      bytes      nattch     status      

------ Semaphore Arrays --------
key        semid      owner      perms      nsems
```

**Logi `petent` (fragment):**

```text
<18:23:50> petent Katarzyna Kowalski wiek 8 pid 25563 r_pid 22359 vip 0
<18:23:50> 25563 nieletni obslugiwany
<18:23:50> 25561 blokuje semafor 0 rejestr
<18:23:50> 25561 oddaje semafor 0 rejestr
<18:23:50> 25561 uruchamia handle_urzednik pid=22357
<18:23:50> 25561 wysyla wiadomosc do urzednik
<18:23:50> 25561 odbiera wiadomosc
<18:23:50> 25562 uruchamia recieve_rejestr
<18:23:50> 25562 wysyla wiadomosc do rejestr
<18:23:50> 25561 idzie do urzednika pid=22353
<18:23:50> 25561 uruchamia handle_urzednik pid=22353
<18:23:50> 25562 blokuje semafor 0 rejestr
<18:23:50> 25561 wysyla wiadomosc do urzednik
<18:23:50> 25563 uruchamia recieve_rejestr
<18:23:50> 25562 oddaje semafor 0 rejestr
<18:23:50> 25562 odbiera wiadomosc
<18:23:50> 25561 odbiera wiadomosc
<18:23:50> 25563 wysyla wiadomosc do rejestr
<18:23:50> 25563 blokuje semafor 0 rejestr
<18:23:50> 25562 blokuje semafor 0 rejestr
<18:23:50> 25563 oddaje semafor 0 rejestr
<18:23:50> 25561 otrzymal wiadomosc jestes przetworzony
```

**Wnioski:**

* Obsługa nieletnich działa poprawnie.

---

## Test 8: Obsługa SIGINT

### Wyniki

#### Przed startem symulacji

```text
Wszystkie procesy uruchomione
program uruchomi sie o 18:59:26, zakonczy sie o 19:00:06, wysle sygnal=1
^C
```

**`summary.sh`:**
```text
petentow stworzonych: 0
petentow przetworzonych: 0
petentow porazek: 0
uruchomione procesy(powinien być tylko bash i ps):
    PID TTY          TIME CMD
   5853 pts/1    00:00:00 bash
   6867 pts/1    00:00:00 ps
pozostałe struktury V(powinny być puste):

------ Message Queues --------
key        msqid      owner      perms      used-bytes   messages    

------ Shared Memory Segments --------
key        shmid      owner      perms      bytes      nattch     status      

------ Semaphore Arrays --------
key        semid      owner      perms      nsems
```

#### W trakcie symulacji

```text
Marcin Wójcik 27 pid 7615 vip=0
aktywnych petentow - 1
pid 7615 otrzymal - jestes przetworzony
Katarzyna Szymański 53 pid 7616 vip=0
aktywnych petentow - 0
pid 7616 otrzymal - jestes przetworzony
Tomasz Szymański 43 pid 7617 vip=0
aktywnych petentow - 0
pid 7617 otrzymal - jestes przetworzony
Tomasz Kamiński 62 pid 7618 vip=0
aktywnych petentow - 0
^C
```

**`summary.sh`:**

```text
petentow stworzonych: 394
petentow przetworzonych: 383
petentow porazek: 0
uruchomione procesy(powinien być tylko bash i ps):
    PID TTY          TIME CMD
   5853 pts/1    00:00:00 bash
   7649 pts/1    00:00:00 ps
pozostałe struktury V(powinny być puste):

------ Message Queues --------
key        msqid      owner      perms      used-bytes   messages    

------ Shared Memory Segments --------
key        shmid      owner      perms      bytes      nattch     status      

------ Semaphore Arrays --------
key        semid      owner      perms      nsems 
```

**Wnioski:**

* Program poprawnie sprząta zasoby po SIGINT.

---

## Test 9: Obsługa SIGUSR1 i SIGUSR2

### SIGUSR1

```text
petentow stworzonych: 1029
petentow przetworzonych: 1025
petentow porazek: 0
```

**Wniosek:**

* Petenci obsłużeni przed zakończeniem pracy.

### SIGUSR2

```text
petentow stworzonych: 855
petentow przetworzonych: 835
```

**Terminal:**

```text
PID ... - JESTEM SFRUSTROWANY
```

**Wniosek:**

* Zgodnie ze specyfikacją część petentów pozostaje nieobsłużona.

---

## Test 10: Duża skala – test stabilności

### Specyfikacja

* `tab_X`: po 5000 → **łącznie 25000**
* `N = 100`
* `K = 40`

### Wyniki

```text
petentow stworzonych: 855
petentow przetworzonych: 835
petentow porazek: 0
uruchomione procesy(powinien być tylko bash i ps):
    PID TTY          TIME CMD
   5853 pts/1    00:00:00 bash
  10723 pts/1    00:00:00 petent
  10724 pts/1    00:00:00 petent
  10853 pts/1    00:00:00 petent
  10875 pts/1    00:00:00 petent
  11004 pts/1    00:00:00 petent
  11014 pts/1    00:00:00 petent
  11026 pts/1    00:00:00 petent
  11036 pts/1    00:00:00 petent
  11038 pts/1    00:00:00 petent
  11064 pts/1    00:00:00 ps
pozostałe struktury V(powinny być puste):

------ Message Queues --------
key        msqid      owner      perms      used-bytes   messages    

------ Shared Memory Segments --------
key        shmid      owner      perms      bytes      nattch     status      

------ Semaphore Arrays --------
key        semid      owner      perms      nsems
```

**Wnioski:**

* Program stabilny przy bardzo dużym obciążeniu.

---