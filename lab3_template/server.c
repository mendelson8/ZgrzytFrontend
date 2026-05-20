MNIEJ WIECEJ PIERWSZY ETAP
#include "common.h"

void usage(char *name) {
    fprintf(stderr, "USAGE: %s port\n", name);
}

int main(int argc, char **argv) {
    // 1. Sprawdzenie argumentów (uruchamiamy przez: ./server 8888)
    if (argc != 2) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    // 2. OTWIERANIE "DRZWI" SERWERA
    // bind_tcp_socket(port, rozmiar_kolejki)
    // Funkcja z Twojej biblioteki przygotowuje gniazdo i ustawia je w tryb nasłuchiwania.
    // Argument 1 oznacza, że w kolejce może czekać max 1 klient (na tym etapie więcej nie potrzebujemy).
    int listen_socket = bind_tcp_socket(atoi(argv[1]), 1);
    printf("Serwer uruchomiony. Czekam na pierwszego klienta na porcie %s...\n", argv[1]);

    // 3. AKCEPTACJA KLIENTA (Program zatrzymuje się tutaj i czeka!)
    // add_new_client to wrapper na systemową funkcję accept().
    // Gdy klient użyje connect(), serwer go wpuszcza i tworzy NOWE gniazdo (client_socket)
    // służące tylko do rozmowy z tym konkretnym klientem.
    int client_socket = add_new_client(listen_socket);
    if (client_socket < 0) ERR("add_new_client");

    printf("Klient pomyślnie podłączony!\n");

    // 4. ODBIÓR DANYCH (Dokładnie 4 bajty)
    char buffer[5]; // 4 bajty na dane + 1 bajt na znak końca stringa '\0'

    // Używamy bulk_read, ponieważ daje nam gwarancję, że program poczeka,
    // aż odczyta dokładnie tyle bajtów, o ile go poprosiliśmy (tutaj 4).
    ssize_t size = bulk_read(client_socket, buffer, 4);

    if (size < 0) {
        ERR("read błąd");
    } else if (size == 0) {
        printf("Klient rozłączył się przed wysłaniem czegokolwiek.\n");
    } else {
        // Zabezpieczamy bufor zerem, żeby printf wiedział, gdzie kończy się tekst
        buffer[size] = '\0';
        printf("Otrzymano od klienta: %s\n", buffer);
    }

    // 5. GRZECZNE SPRZĄTANIE I WYJŚCIE
    // Zamykamy rurę do klienta
    if (TEMP_FAILURE_RETRY(close(client_socket)) < 0) ERR("close client");
    // Zamykamy główne drzwi serwera
    if (TEMP_FAILURE_RETRY(close(listen_socket)) < 0) ERR("close server");

    printf("Zamykam serwer. Koniec Etapu 1.\n");
    return EXIT_SUCCESS;
}


MNIEJ WIECEJ DRUGI ETAP


#include "common.h"

#define BACKLOG 5
#define MAX_EVENTS 16
#define BUF_SIZE 256 // Dostosuj rozmiar bufora do zadania

volatile sig_atomic_t do_work = 1;

void sigint_handler(int sig) { do_work = 0; }
void usage(char *name) { fprintf(stderr, "USAGE: %s port\n", name); }

// =========================================================================
// --- LOGIKA ZADANIA: ZMIENNE GLOBALNE ---
// Dodaj tutaj struktury, tablice, liczniki (np. int active_clients = 0; itp.)
// =========================================================================


void doServer(int tcp_listen_socket)
{
    int epoll_descriptor;
    if ((epoll_descriptor = epoll_create1(0)) < 0) ERR("epoll_create:");

    struct epoll_event event, events[MAX_EVENTS];
    event.events = EPOLLIN;
    event.data.fd = tcp_listen_socket;

    if (epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, tcp_listen_socket, &event) == -1) {
        perror("epoll_ctl: listen_sock");
        exit(EXIT_FAILURE);
    }

    int nfds;
    sigset_t mask, empty_mask;
    sigemptyset(&mask);
    sigemptyset(&empty_mask); // Wymuszenie czystej maski chroni przed zablokowaniem sygnalow
    sigaddset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, NULL);

    while (do_work)
    {
        // epoll usypia serwer i czeka z pusta maska (przyjmie SIGINT)
        if ((nfds = epoll_pwait(epoll_descriptor, events, MAX_EVENTS, -1, &empty_mask)) > 0)
        {
            for (int n = 0; n < nfds; n++)
            {
                if (events[n].data.fd == tcp_listen_socket)
                {
                    // --- 1. NOWE POŁĄCZENIE ---
                    int client_socket = add_new_client(tcp_listen_socket);
                    if (client_socket < 0) continue;

                    // =========================================================
                    // --- LOGIKA ZADANIA: ODRZUCANIE NADMIARU KLIENTÓW ---
                    // Np. if(active_clients >= MAX_CLIENTS) { close(client_socket); continue; }
                    // =========================================================

                    event.events = EPOLLIN;
                    event.data.fd = client_socket;
                    if (epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, client_socket, &event) == -1)
                        ERR("epoll_ctl: add client");
                }
                else
                {
                    // --- 2. ODBIÓR DANYCH OD KLIENTA ---
                    int client_fd = events[n].data.fd;
                    char buffer[BUF_SIZE];
                    ssize_t size;

                    // Zwykle read, zdejmujemy z niego blokady zeby poprawnie obsluzylo rozlaczenie
                    if ((size = TEMP_FAILURE_RETRY(read(client_fd, buffer, sizeof(buffer) - 1))) < 0)
                    {
                        if (errno != ECONNRESET) ERR("read");
                        size = 0; // Traktuj jako rozlaczenie, jesli padl reset polaczenia
                    }

                    if (size == 0)
                    {
                        // KLIENT ZERWAŁ POŁĄCZENIE
                        epoll_ctl(epoll_descriptor, EPOLL_CTL_DEL, client_fd, NULL);
                        TEMP_FAILURE_RETRY(close(client_fd));

                        // =========================================================
                        // --- LOGIKA ZADANIA: AKTUALIZACJA STANU PO ROZŁĄCZENIU ---
                        // Np. active_clients--;
                        // =========================================================
                    }
                    else
                    {
                        // KLIENT PRZYSŁAŁ DANE
                        buffer[size] = '\0'; // Zabezpiecz stringa (dla wygody operacji tekstowych)

                        // =========================================================
                        // --- LOGIKA ZADANIA: PRZETWARZANIE DANYCH I ODPOWIEDŹ ---
                        // 1. Zrob obliczenia na podstawie 'buffer'
                        // 2. Wyslij wynik: bulk_write(client_fd, ...);
                        // 3. Jesli to klient jednorazowy (jak w kalkulatorze):
                        //    epoll_ctl(epoll_descriptor, EPOLL_CTL_DEL, client_fd, NULL);
                        //    TEMP_FAILURE_RETRY(close(client_fd));
                        // =========================================================
                    }
                }
            }
        }
        else
        {
            if (errno == EINTR) continue; // Przechwycono sygnal, wroc na poczatek petli
            ERR("epoll_pwait");
        }
    }

    // Sprzatanie po wyjsciu z petli
    if (TEMP_FAILURE_RETRY(close(epoll_descriptor)) < 0) ERR("close");
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    // Zabezpieczenie przed przerwaniem dzialania przy pisaniu do rozlaczonego gniazda
    if (sethandler(SIG_IGN, SIGPIPE)) ERR("Setting SIGPIPE:");
    if (sethandler(sigint_handler, SIGINT)) ERR("Setting SIGINT:");

    int tcp_listen_socket = bind_tcp_socket(atoi(argv[1]), BACKLOG);

    // Gniazdo musi byc nieblokujace (bardzo wazne dla epoll!)
    int new_flags = fcntl(tcp_listen_socket, F_GETFL) | O_NONBLOCK;
    fcntl(tcp_listen_socket, F_SETFL, new_flags);

    // Glowne wejscie w petle zdarzen
    doServer(tcp_listen_socket);

    // =========================================================================
    // --- LOGIKA ZADANIA: WYPISANIE WYNIKOW PO C-c (SIGINT) ---
    // =========================================================================

    // Poprawne zamkniecie gniazda
    if (TEMP_FAILURE_RETRY(close(tcp_listen_socket)) < 0) ERR("close");
    fprintf(stderr, "Server terminated gracefully.\n");
    return EXIT_SUCCESS;
}

// docker run -it --name so_lab --rm -v "$(pwd)":/workspace -w /workspace ubuntu:latest bash
// apt-get update && apt-get install -y build-essential netcat-openbsd psmisc
// docker exec -it so_lab bash


#include "common.h"
#include <ctype.h>     // Do funkcji isdigit()
#include <sys/epoll.h>

#define MAX_CLIENTS 4  // Limit klientow z Etapu 2
#define MAX_EVENTS 10  // Ile zdarzen epoll moze przetworzyc na raz
#define MAX_CITIES 20  // Liczba miast z zadania

// ============================================================================
// === ZMIENNE GLOBALNE I SYGNAŁY (Stan serwera) ===
// ============================================================================
volatile sig_atomic_t do_work = 1;

void sigint_handler(int sig) {
    do_work = 0; // Flaga lagodnego wyjscia po Ctrl+C
}

void usage(char *name) {
    fprintf(stderr, "USAGE: %s port\n", name);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    // --- ETAP 3: INICJALIZACJA STANU GRY ---
    // Na samym poczatku wszystkie miasta (1-20) naleza do Grekow ('g')
    char cities[MAX_CITIES + 1];
    for (int i = 1; i <= MAX_CITIES; i++) {
        cities[i] = 'g';
    }

    // --- TABLICA KLIENTÓW ---
    // Musimy wiedziec, kto jest polazczony, zeby robic Broadcast (rozsylanie)
    // Wypelniamy tablice wartoscia -1, co oznacza "puste miejsce"
    int active_clients[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        active_clients[i] = -1;
    }

    // ============================================================================
    // === BEZPIECZEŃSTWO SIECIOWE: IGNOROWANIE SIGPIPE (Wymog Etapu 4) ===
    // ============================================================================
    // Jesli serwer sprobuje wyslac dane (write) do klienta, ktory nagle odlaczyl
    // kabel, system wysle serwerowi sygnal SIGPIPE, ktory "zabija" program.
    // Ignorujac ten sygnal, funkcja write() po prostu zwroci -1 i ustawi blad EPIPE,
    // co pozwala nam lagodnie usunac klienta zamiast psuc caly serwer!
    if (sethandler(SIG_IGN, SIGPIPE)) ERR("Setting SIGPIPE:");
    if (sethandler(sigint_handler, SIGINT)) ERR("Setting SIGINT:");

    // ============================================================================
    // === INICJALIZACJA GNIAZDA SERWERA ===
    // ============================================================================
    // 1. Tworzymy i bindujemy gniazdo na podanym porcie
    int listen_socket = bind_tcp_socket(atoi(argv[1]), MAX_CLIENTS);

    // 2. [DOBRA PRAKTYKA] Gniazdo nasluchujace ustawiamy jako nieblokujace (O_NONBLOCK)
    int new_flags = fcntl(listen_socket, F_GETFL) | O_NONBLOCK;
    fcntl(listen_socket, F_SETFL, new_flags);

    // ============================================================================
    // === KONFIGURACJA EPOLL ===
    // ============================================================================
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) ERR("epoll_create1");

    struct epoll_event event, events[MAX_EVENTS];

    // Dodajemy nasze glowne gniazdo nasluchujace do epolla
    event.events = EPOLLIN;
    event.data.fd = listen_socket;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_socket, &event) == -1) ERR("epoll_ctl: listen_sock");

    // Zabezpieczenie Ctrl+C dla epoll_pwait
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    printf("Serwer uruchomiony. Nasluchiuje na porcie %s...\n", argv[1]);

    // ============================================================================
    // === GŁÓWNA PĘTLA SERWERA ===
    // ============================================================================
    while (do_work) {
        int nfds = epoll_pwait(epoll_fd, events, MAX_EVENTS, -1, &oldmask);

        if (nfds < 0) {
            if (errno == EINTR) continue; // Wcisnieto Ctrl+C
            ERR("epoll_pwait");
        }

        for (int i = 0; i < nfds; i++) {
            int current_fd = events[i].data.fd;

            // ********************************************************************
            // *** ZDARZENIE 1: NOWE POŁĄCZENIE (Ktos puka do serwera) ***
            // ********************************************************************
            if (current_fd == listen_socket) {
                int client_fd = add_new_client(listen_socket);
                if (client_fd < 0) continue;

                // Szukamy wolnego miejsca w tablicy klientow
                int free_slot = -1;
                for (int c = 0; c < MAX_CLIENTS; c++) {
                    if (active_clients[c] == -1) {
                        free_slot = c;
                        break;
                    }
                }

                if (free_slot != -1) {
                    // JEST MIEJSCE: Dodajemy klienta
                    active_clients[free_slot] = client_fd;
                    event.events = EPOLLIN;
                    event.data.fd = client_fd;
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event);
                    printf("[Serwer]: Nowy klient polazczony (slot %d)\n", free_slot);
                } else {
                    // BRAK MIEJSCA (Zgodnie z Etapem 2: Odrzucamy nadmiar)
                    printf("[Serwer]: Odrzucono klienta - brak wolnych miejsc.\n");
                    TEMP_FAILURE_RETRY(close(client_fd));
                }
            }
            // ********************************************************************
            // *** ZDARZENIE 2: DANE OD PODŁĄCZONEGO KLIENTA ***
            // ********************************************************************
            else {
                char buffer[16];
                // Uzywamy read, zeby obsluzyc poprawne rozlaczenie
                ssize_t size = TEMP_FAILURE_RETRY(read(current_fd, buffer, sizeof(buffer) - 1));

                // --- ETAP 4: OBSŁUGA ROZŁĄCZENIA / BŁĘDU ---
                if (size <= 0) {
                    printf("[Serwer]: Klient rozlaczyl sie (fd: %d)\n", current_fd);

                    // Usuwamy klienta z Epolla
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, current_fd, NULL);
                    TEMP_FAILURE_RETRY(close(current_fd));

                    // Zwalniamy miejsce w tablicy klientow, zeby ktos inny mogl wejsc!
                    for (int c = 0; c < MAX_CLIENTS; c++) {
                        if (active_clients[c] == current_fd) {
                            active_clients[c] = -1;
                            break;
                        }
                    }
                    continue; // Koniec obslugi tego zdarzenia
                }

                buffer[size] = '\0';
                printf("[Odebrano]: %s", buffer); // Wyswietlanie zgodnie z Etapem 1 i 2

                // --- ETAP 4: WALIDACJA WIADOMOŚCI ---
                // Format: "gXX\n" lub "pXX\n" (Dokladnie 4 znaki)
                int is_valid = 0;
                int city_num = 0;
                char new_owner = buffer[0];

                if (size == 4 && (new_owner == 'g' || new_owner == 'p') && buffer[3] == '\n') {
                    if (isdigit(buffer[1]) && isdigit(buffer[2])) {
                        // Reczne zamienienie dwoch znakow na liczbe (np. '0' i '5' -> 5)
                        city_num = (buffer[1] - '0') * 10 + (buffer[2] - '0');
                        if (city_num >= 1 && city_num <= MAX_CITIES) {
                            is_valid = 1; // Wiadomosc jest w 100% poprawna
                        }
                    }
                }

                if (!is_valid) {
                    // ETAP 4: Nieprawidlowy format = KICK klienta
                    printf("[Serwer]: Bledny format wiadomosci. Rozlaczam klienta %d.\n", current_fd);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, current_fd, NULL);
                    TEMP_FAILURE_RETRY(close(current_fd));
                    for (int c = 0; c < MAX_CLIENTS; c++) {
                        if (active_clients[c] == current_fd) active_clients[c] = -1;
                    }
                    continue;
                }

                // --- ETAP 3: AKTUALIZACJA I BROADCAST (Rozsylanie) ---
                // "Jesli wlasciciel miasta sie zmienil, naleza zaktualizowac tablice
                // i rozeslac do wszystkich INNYCH polazczonych klientow"
                if (cities[city_num] != new_owner) {
                    cities[city_num] = new_owner; // Aktualizacja w bazie serwera
                    printf(" -> Miasto %d przejete przez: %c\n", city_num, new_owner);

                    // Rozsylamy do innych
                    for (int c = 0; c < MAX_CLIENTS; c++) {
                        int target_fd = active_clients[c];

                        // Ignorujemy puste sloty oraz klienta, KTÓRY TO WYSŁAŁ (current_fd)
                        if (target_fd != -1 && target_fd != current_fd) {

                            // Wyslanie wiadomosci (4 bajty).
                            // Jesli klient nagle odlaczyl kabel (EPIPE), bulk_write zwroci blad < 0.
                            if (bulk_write(target_fd, buffer, 4) < 0) {
                                printf("[Serwer]: Blad EPIPE przy wysylaniu. Usuwam klienta %d\n", target_fd);
                                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, target_fd, NULL);
                                TEMP_FAILURE_RETRY(close(target_fd));
                                active_clients[c] = -1; // Zwolnienie miejsca
                            }
                        }
                    }
                }
            }
        }
    }

    // ============================================================================
    // === ZAKONCZENIE I CZYSZCZENIE ZASOBÓW (Zgodnie z Etapem 3) ===
    // ============================================================================
    printf("\n--- Podsumowanie stanu miast po Ctrl+C ---\n");
    for (int i = 1; i <= MAX_CITIES; i++) {
        printf("Miasto %02d: %c\n", i, cities[i]);
    }

    // Zamkniecie gniazd wszystkich pozostalych klientow
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (active_clients[i] != -1) {
            TEMP_FAILURE_RETRY(close(active_clients[i]));
        }
    }

    // Zamkniecie wlasnych deskryptorow
    TEMP_FAILURE_RETRY(close(listen_socket));
    TEMP_FAILURE_RETRY(close(epoll_fd));
    sigprocmask(SIG_UNBLOCK, &mask, NULL);

    printf("Serwer zakonczyl prace poprawnie.\n");
    return EXIT_SUCCESS;
}////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>

#include "l8_common.h"

#define BACKLOG 3
#define MAXBUF 576
#define STACK_SIZE 16
#define DIVISION_NAMES_SIZE 128
#define MAP_SIZE 100

int make_socket(int domain, int type)
{
    int sock;
    sock = socket(domain, type, 0);
    if (sock < 0)
        ERR("socket");
    return sock;
}

int bind_inet_socket(uint16_t port, int type)
{
    struct sockaddr_in addr;
    int socketfd, t = 1;
    socketfd = make_socket(PF_INET, type);
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(t)))
        ERR("setsockopt");
    if (bind(socketfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        ERR("bind");
    if (SOCK_STREAM == type)
        if (listen(socketfd, BACKLOG) < 0)
            ERR("listen");
    return socketfd;
}

void usage(char *name) { fprintf(stderr, "USAGE: %s port\n", name); }

// --- STRUKTURY DANYCH ---

// Pojedynczy raport odkładany na stos
typedef struct {
    char text[256];
    struct sockaddr_in sender_addr;
} Raport;

// Centralna struktura sztabu
typedef struct {
    // Współdzielone gniazdo UDP (potrzebne Napoleonowi do wysyłania)
    int sockfd;

    // Etap 2: Stos raportów
    Raport messages[STACK_SIZE];
    int top;
    pthread_mutex_t m;
    pthread_cond_t c;

    // Etap 3 & 4: Rejestr oddziałów (nazwy, przynależność, adresy)
    char division_names[DIVISION_NAMES_SIZE][128];
    int division_affiliation[DIVISION_NAMES_SIZE]; // 1 - sojusznik, 0 - wróg
    struct sockaddr_in division_addrs[DIVISION_NAMES_SIZE]; // Adresy nadawców
    int num_divisions;
    pthread_mutex_t names_mutex;

    // Etap 3: Mapa sztabowa
    int map[MAP_SIZE][MAP_SIZE];
    pthread_mutex_t map_row_mutexes[MAP_SIZE];
} Sztab;

// --- WĄTKI ---

void* doAdiutant(void* arg) {
    Sztab* p = (Sztab*)arg;
    Raport roboczy_raport;

    while (1) {
        pthread_mutex_lock(&p->m);
        while (p->top == 0) {
            if (pthread_cond_wait(&p->c, &p->m) != 0)
                ERR("pthread_cond_wait");
        }

        p->top--;
        roboczy_raport = p->messages[p->top]; // Kopiujemy cały raport (tekst + adres)
        pthread_mutex_unlock(&p->m);

        usleep(10 * 1000); // 10ms pracy

        int x, y, czy_nasz;
        char wiadomosc[128] = {0};

        if (sscanf(roboczy_raport.text, "%d %d %d %127[^\n\r]", &x, &y, &czy_nasz, wiadomosc) == 4) {

            if (x < 0 || x >= MAP_SIZE || y < 0 || y >= MAP_SIZE) continue;

            // Rejestracja oddziału
            int division_id = -1;
            pthread_mutex_lock(&p->names_mutex);

            for (int i = 0; i < p->num_divisions; i++) {
                if (strcmp(p->division_names[i], wiadomosc) == 0) {
                    division_id = i;
                    break;
                }
            }

            if (division_id == -1 && p->num_divisions < DIVISION_NAMES_SIZE) {
                division_id = p->num_divisions;
                strncpy(p->division_names[division_id], wiadomosc, 127);
                p->num_divisions++;
            }

            // Etap 4: Aktualizacja danych o oddziale (adres i flaga sojusznika)
            if (division_id != -1) {
                p->division_affiliation[division_id] = czy_nasz;
                p->division_addrs[division_id] = roboczy_raport.sender_addr;
            }
            pthread_mutex_unlock(&p->names_mutex);

            if (division_id == -1) continue; // Brak miejsca w tablicy

            // Aktualizacja mapy
            for (int i = 0; i < MAP_SIZE; i++) {
                pthread_mutex_lock(&p->map_row_mutexes[i]);
                for (int j = 0; j < MAP_SIZE; j++) {
                    if (p->map[i][j] == division_id) p->map[i][j] = -1;
                }
                pthread_mutex_unlock(&p->map_row_mutexes[i]);
            }

            pthread_mutex_lock(&p->map_row_mutexes[y]);
            p->map[y][x] = division_id;
            pthread_mutex_unlock(&p->map_row_mutexes[y]);

        }
    }
    return NULL;
}

void* doNapoleon(void* arg) {
    Sztab* p = (Sztab*)arg;
    unsigned int seed = time(NULL); // Seed dla rand_r
    char buf[256];

    while (1) {
        usleep(30 * 1000); // Cesarz radzi co 30ms

        // 1. Wypisywanie stanu mapy
        // Ze względu na to, że wypisywanie macierzy 100x100 co 30ms zaspamuje konsolę,
        // wypisujemy tylko zajęte koordynaty (jako podgląd mapy).
        printf("\n[NAPOLEON] Stan Mapy:\n");
        int active_units = 0;
        for (int i = 0; i < MAP_SIZE; i++) {
            pthread_mutex_lock(&p->map_row_mutexes[i]);
            for (int j = 0; j < MAP_SIZE; j++) {
                if (p->map[i][j] != -1) {
                    int d_id = p->map[i][j];
                    printf(" -> [%02d:%02d] %s\n", j, i, p->division_names[d_id]);
                    active_units++;
                }
            }
            pthread_mutex_unlock(&p->map_row_mutexes[i]);
        }
        if (active_units == 0) printf(" -> Mapa pusta.\n");

        // 2. Szukanie sojuszników
        pthread_mutex_lock(&p->names_mutex);
        int allied_indices[DIVISION_NAMES_SIZE];
        int allied_count = 0;

        for (int i = 0; i < p->num_divisions; i++) {
            if (p->division_affiliation[i] == 1) {
                allied_indices[allied_count++] = i;
            }
        }

        // 3. Wysłanie rozkazu
        if (allied_count > 0) {
            int random_idx = rand_r(&seed) % allied_count;
            int target_id = allied_indices[random_idx];

            struct sockaddr_in target_addr = p->division_addrs[target_id];
            char target_name[128];
            strcpy(target_name, p->division_names[target_id]);

            pthread_mutex_unlock(&p->names_mutex); // Zwalniamy przed wysłaniem!

            int order_x = rand_r(&seed) % MAP_SIZE;
            int order_y = rand_r(&seed) % MAP_SIZE;

            // Format rozkazu: <X> <Y> <P> <nazwa oddziału>
            int msg_len = snprintf(buf, sizeof(buf), "%d %d 1 %s\n", order_x, order_y, target_name);

            if (TEMP_FAILURE_RETRY(sendto(p->sockfd, buf, msg_len, 0, (struct sockaddr*)&target_addr, sizeof(target_addr))) < 0) {
                ERR("sendto");
            }

            printf("[NAPOLEON] Wysłano rozkaz do %s: udaj się na %d:%d\n", target_name, order_x, order_y);

        } else {
            pthread_mutex_unlock(&p->names_mutex);
        }
    }
    return NULL;
}

void doServer(int fd, Sztab* p) {
    char buf[MAXBUF + 1];
    struct sockaddr_in addr;
    socklen_t size;

    while (1) {
        size = sizeof(addr);
        int receivedBytes = TEMP_FAILURE_RETRY(recvfrom(fd, buf, MAXBUF, 0, (struct sockaddr *)&addr, &size));
        if (receivedBytes < 0) ERR("recvfrom");

        buf[receivedBytes] = '\0';

        pthread_mutex_lock(&p->m);
        if (p->top < STACK_SIZE) {
            strncpy(p->messages[p->top].text, buf, 256);
            p->messages[p->top].sender_addr = addr; // Zapisz adres nadawcy
            p->top++;
            pthread_cond_signal(&p->c);
        } else {
            fprintf(stderr, "Stos pelny! Odrzucono raport.\n");
        }
        pthread_mutex_unlock(&p->m);
    }
}

int main(int argc, char **argv)
{
    int fd;
    if (argc != 2)
    {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    if (sethandler(SIG_IGN, SIGPIPE)) ERR("Seting SIGPIPE:");

    fd = bind_inet_socket(atoi(argv[1]), SOCK_DGRAM);

    Sztab sztab;
    memset(&sztab, 0, sizeof(Sztab)); // Zerowanie pamięci
    sztab.sockfd = fd; // Przypisanie gniazda dla Napoleona

    for (int i = 0; i < MAP_SIZE; i++) {
        for (int j = 0; j < MAP_SIZE; j++) sztab.map[i][j] = -1;
    }

    if (pthread_mutex_init(&sztab.m, NULL) != 0) ERR("pthread_mutex_init");
    if (pthread_cond_init(&sztab.c, NULL) != 0) ERR("pthread_cond_init");
    if (pthread_mutex_init(&sztab.names_mutex, NULL) != 0) ERR("pthread_mutex_init");
    for (int i = 0; i < MAP_SIZE; i++) {
        if (pthread_mutex_init(&sztab.map_row_mutexes[i], NULL) != 0) ERR("pthread_mutex_init");
    }

    pthread_t adiutanci[4];
    for (int i = 0; i < 4; i++) {
        if (pthread_create(&adiutanci[i], NULL, doAdiutant, &sztab) != 0) ERR("pthread_create");
    }

    // Odpalenie wątku Napoleona
    pthread_t napoleon_thread;
    if (pthread_create(&napoleon_thread, NULL, doNapoleon, &sztab) != 0) ERR("pthread_create");

    doServer(fd, &sztab);

    if (TEMP_FAILURE_RETRY(close(fd)) < 0) ERR("close");

    // Sprzątanie
    pthread_mutex_destroy(&sztab.m);
    pthread_cond_destroy(&sztab.c);
    pthread_mutex_destroy(&sztab.names_mutex);
    for (int i = 0; i < MAP_SIZE; i++) pthread_mutex_destroy(&sztab.map_row_mutexes[i]);

    return EXIT_SUCCESS;
}