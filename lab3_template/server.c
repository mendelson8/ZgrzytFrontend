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
    sigemptyset(&empty_mask); // Wymuszenie czystej maski chroni przed zablokowaniem sygnałów
    sigaddset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, NULL);

    while (do_work)
    {
        // epoll usypia serwer i czeka z pustą maską (przyjmie SIGINT)
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

                    // Zwykłe read, zdejmujemy z niego blokady żeby poprawnie obsłużyło rozłączenie
                    if ((size = TEMP_FAILURE_RETRY(read(client_fd, buffer, sizeof(buffer) - 1))) < 0)
                    {
                        if (errno != ECONNRESET) ERR("read");
                        size = 0; // Traktuj jako rozłączenie, jeśli padł reset połączenia
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
                        // 1. Zrób obliczenia na podstawie 'buffer'
                        // 2. Wyślij wynik: bulk_write(client_fd, ...);
                        // 3. Jeśli to klient jednorazowy (jak w kalkulatorze):
                        //    epoll_ctl(epoll_descriptor, EPOLL_CTL_DEL, client_fd, NULL);
                        //    TEMP_FAILURE_RETRY(close(client_fd));
                        // =========================================================
                    }
                }
            }
        }
        else
        {
            if (errno == EINTR) continue; // Przechwycono sygnał, wróć na początek pętli
            ERR("epoll_pwait");
        }
    }

    // Sprzątanie po wyjściu z pętli
    if (TEMP_FAILURE_RETRY(close(epoll_descriptor)) < 0) ERR("close");
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    // Zabezpieczenie przed przerwaniem działania przy pisaniu do rozłączonego gniazda
    if (sethandler(SIG_IGN, SIGPIPE)) ERR("Setting SIGPIPE:");
    if (sethandler(sigint_handler, SIGINT)) ERR("Setting SIGINT:");

    int tcp_listen_socket = bind_tcp_socket(atoi(argv[1]), BACKLOG);

    // Gniazdo musi być nieblokujące (bardzo ważne dla epoll!)
    int new_flags = fcntl(tcp_listen_socket, F_GETFL) | O_NONBLOCK;
    fcntl(tcp_listen_socket, F_SETFL, new_flags);

    // Główne wejście w pętlę zdarzeń
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

#define MAX_CLIENTS 4  // Limit klientów z Etapu 2
#define MAX_EVENTS 10  // Ile zdarzeń epoll może przetworzyć na raz
#define MAX_CITIES 20  // Liczba miast z zadania

// ============================================================================
// === ZMIENNE GLOBALNE I SYGNAŁY (Stan serwera) ===
// ============================================================================
volatile sig_atomic_t do_work = 1;

void sigint_handler(int sig) {
    do_work = 0; // Flaga łagodnego wyjścia po Ctrl+C
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
    // Na samym początku wszystkie miasta (1-20) należą do Greków ('g')
    char cities[MAX_CITIES + 1];
    for (int i = 1; i <= MAX_CITIES; i++) {
        cities[i] = 'g';
    }

    // --- TABLICA KLIENTÓW ---
    // Musimy wiedzieć, kto jest podłączony, żeby robić Broadcast (rozsyłanie)
    // Wypełniamy tablicę wartością -1, co oznacza "puste miejsce"
    int active_clients[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        active_clients[i] = -1;
    }

    // ============================================================================
    // === BEZPIECZEŃSTWO SIECIOWE: IGNOROWANIE SIGPIPE (Wymóg Etapu 4) ===
    // ============================================================================
    // Jeśli serwer spróbuje wysłać dane (write) do klienta, który nagle odłączył
    // kabel, system wyśle serwerowi sygnał SIGPIPE, który "zabija" program.
    // Ignorując ten sygnał, funkcja write() po prostu zwróci -1 i ustawi błąd EPIPE,
    // co pozwala nam łagodnie usunąć klienta zamiast psuć cały serwer!
    if (sethandler(SIG_IGN, SIGPIPE)) ERR("Setting SIGPIPE:");
    if (sethandler(sigint_handler, SIGINT)) ERR("Setting SIGINT:");

    // ============================================================================
    // === INICJALIZACJA GNIAZDA SERWERA ===
    // ============================================================================
    // 1. Tworzymy i bindujemy gniazdo na podanym porcie
    int listen_socket = bind_tcp_socket(atoi(argv[1]), MAX_CLIENTS);

    // 2. [DOBRA PRAKTYKA] Gniazdo nasłuchujące ustawiamy jako nieblokujące (O_NONBLOCK)
    int new_flags = fcntl(listen_socket, F_GETFL) | O_NONBLOCK;
    fcntl(listen_socket, F_SETFL, new_flags);

    // ============================================================================
    // === KONFIGURACJA EPOLL ===
    // ============================================================================
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) ERR("epoll_create1");

    struct epoll_event event, events[MAX_EVENTS];

    // Dodajemy nasze główne gniazdo nasłuchujące do epolla
    event.events = EPOLLIN;
    event.data.fd = listen_socket;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_socket, &event) == -1) ERR("epoll_ctl: listen_sock");

    // Zabezpieczenie Ctrl+C dla epoll_pwait
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    printf("Serwer uruchomiony. Nasłuchuję na porcie %s...\n", argv[1]);

    // ============================================================================
    // === GŁÓWNA PĘTLA SERWERA ===
    // ============================================================================
    while (do_work) {
        int nfds = epoll_pwait(epoll_fd, events, MAX_EVENTS, -1, &oldmask);

        if (nfds < 0) {
            if (errno == EINTR) continue; // Wciśnięto Ctrl+C
            ERR("epoll_pwait");
        }

        for (int i = 0; i < nfds; i++) {
            int current_fd = events[i].data.fd;

            // ********************************************************************
            // *** ZDARZENIE 1: NOWE POŁĄCZENIE (Ktoś puka do serwera) ***
            // ********************************************************************
            if (current_fd == listen_socket) {
                int client_fd = add_new_client(listen_socket);
                if (client_fd < 0) continue;

                // Szukamy wolnego miejsca w tablicy klientów
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
                    printf("[Serwer]: Nowy klient podłączony (slot %d)\n", free_slot);
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
                // Używamy read, żeby obsłużyć poprawne rozłączenie
                ssize_t size = TEMP_FAILURE_RETRY(read(current_fd, buffer, sizeof(buffer) - 1));

                // --- ETAP 4: OBSŁUGA ROZŁĄCZENIA / BŁĘDU ---
                if (size <= 0) {
                    printf("[Serwer]: Klient rozłączył się (fd: %d)\n", current_fd);

                    // Usuwamy klienta z Epolla
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, current_fd, NULL);
                    TEMP_FAILURE_RETRY(close(current_fd));

                    // Zwalniamy miejsce w tablicy klientów, żeby ktoś inny mógł wejść!
                    for (int c = 0; c < MAX_CLIENTS; c++) {
                        if (active_clients[c] == current_fd) {
                            active_clients[c] = -1;
                            break;
                        }
                    }
                    continue; // Koniec obsługi tego zdarzenia
                }

                buffer[size] = '\0';
                printf("[Odebrano]: %s", buffer); // Wyświetlanie zgodnie z Etapem 1 i 2

                // --- ETAP 4: WALIDACJA WIADOMOŚCI ---
                // Format: "gXX\n" lub "pXX\n" (Dokładnie 4 znaki)
                int is_valid = 0;
                int city_num = 0;
                char new_owner = buffer[0];

                if (size == 4 && (new_owner == 'g' || new_owner == 'p') && buffer[3] == '\n') {
                    if (isdigit(buffer[1]) && isdigit(buffer[2])) {
                        // Ręczne zamienienie dwóch znaków na liczbę (np. '0' i '5' -> 5)
                        city_num = (buffer[1] - '0') * 10 + (buffer[2] - '0');
                        if (city_num >= 1 && city_num <= MAX_CITIES) {
                            is_valid = 1; // Wiadomość jest w 100% poprawna
                        }
                    }
                }

                if (!is_valid) {
                    // ETAP 4: Nieprawidłowy format = KICK klienta
                    printf("[Serwer]: Błędny format wiadomości. Rozłączam klienta %d.\n", current_fd);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, current_fd, NULL);
                    TEMP_FAILURE_RETRY(close(current_fd));
                    for (int c = 0; c < MAX_CLIENTS; c++) {
                        if (active_clients[c] == current_fd) active_clients[c] = -1;
                    }
                    continue;
                }

                // --- ETAP 3: AKTUALIZACJA I BROADCAST (Rozsyłanie) ---
                // "Jeśli właściciel miasta się zmienił, należy zaktualizować tablicę
                // i rozesłać do wszystkich INNYCH podłączonych klientów"
                if (cities[city_num] != new_owner) {
                    cities[city_num] = new_owner; // Aktualizacja w bazie serwera
                    printf(" -> Miasto %d przejęte przez: %c\n", city_num, new_owner);

                    // Rozsyłamy do innych
                    for (int c = 0; c < MAX_CLIENTS; c++) {
                        int target_fd = active_clients[c];

                        // Ignorujemy puste sloty oraz klienta, KTÓRY TO WYSŁAŁ (current_fd)
                        if (target_fd != -1 && target_fd != current_fd) {

                            // Wysłanie wiadomości (4 bajty).
                            // Jeśli klient nagle odłączył kabel (EPIPE), bulk_write zwróci błąd < 0.
                            if (bulk_write(target_fd, buffer, 4) < 0) {
                                printf("[Serwer]: Błąd EPIPE przy wysyłaniu. Usuwam klienta %d\n", target_fd);
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
    // === ZAKOŃCZENIE I CZYSZCZENIE ZASOBÓW (Zgodnie z Etapem 3) ===
    // ============================================================================
    printf("\n--- Podsumowanie stanu miast po Ctrl+C ---\n");
    for (int i = 1; i <= MAX_CITIES; i++) {
        printf("Miasto %02d: %c\n", i, cities[i]);
    }

    // Zamknięcie gniazd wszystkich pozostałych klientów
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (active_clients[i] != -1) {
            TEMP_FAILURE_RETRY(close(active_clients[i]));
        }
    }

    // Zamknięcie własnych deskryptorów
    TEMP_FAILURE_RETRY(close(listen_socket));
    TEMP_FAILURE_RETRY(close(epoll_fd));
    sigprocmask(SIG_UNBLOCK, &mask, NULL);

    printf("Serwer zakończył pracę poprawnie.\n");
    return EXIT_SUCCESS;
}