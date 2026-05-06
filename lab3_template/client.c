#include "common.h"

void usage(char *name) { fprintf(stderr, "USAGE: %s domain port\n", name); }

int main(int argc, char **argv)
{
    int fd;
    if (argc != 3) { // Zmien na odpowiednia liczbe argumentow, jesli zadanie tego wymaga
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    // 1. Nawiaznie polaczenia (funkcja blokuje az sie polaczy)
    fd = connect_tcp_socket(argv[1], argv[2]);

    // =========================================================================
    // --- LOGIKA ZADANIA: PRZYGOTOWANIE I WYSYLANIE DANYCH ---
    // char buffer[128] = "moja wiadomosc";
    // bulk_write(fd, buffer, strlen(buffer));
    // (pamietaj o htonl/htons dla typow binarnych int!)
    // =========================================================================


    // =========================================================================
    // --- LOGIKA ZADANIA: ODBIOR ODPOWIEDZI ---
    // char response[128];
    // if (bulk_read(fd, response, oczekiwany_rozmiar) < 0) ERR("read");
    // printf("Odpowiedz: %s\n", response);
    // =========================================================================


    // Poprawne zamkniecie gniazda klienta
    if (TEMP_FAILURE_RETRY(close(fd)) < 0) ERR("close");

    return EXIT_SUCCESS;
}


#include "common.h"
#include <time.h>
#include <sys/epoll.h> // NIEZBĘDNE: Biblioteka do obsługi mechanizmu epoll

// ============================================================================
// === FRAGMENT: BEZPIECZNA OBSŁUGA SYGNAŁÓW (np. Ctrl+C) ===
// ============================================================================
// volatile sig_atomic_t gwarantuje, że zmiana tej zmiennej nie zostanie
// przerwana w połowie przez system. Idealne do flag wyjścia z pętli.
volatile sig_atomic_t work = 1;

void usage(char *name) { fprintf(stderr, "USAGE: %s domain port\n", name); }

// Funkcja przechwytująca sygnał (np. SIGINT z Ctrl+C).
// WAŻNE: Zawsze musi przyjmować jeden argument (int sig), nawet jeśli go nie używa!
void stop_work(int sig) {
    work = 0; // Nie używaj tu printf ani exit(), po prostu zmień flagę
}

int main(int argc, char **argv) {
    // Standardowe sprawdzenie argumentów wejściowych klienta
    if (argc != 3) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    // 1. Nawiązanie połączenia z serwerem (funkcja z common.h)
    int fd = connect_tcp_socket(argv[1], argv[2]);

    // 2. Podpięcie naszej funkcji stop_work pod skrót Ctrl+C (SIGINT)
    sethandler(stop_work, SIGINT);

    // srand odpalamy TYLKO RAZ na cały program. Jeśli zadanie wymaga losowania, zostaw to.
    srand(time(NULL));

    // Twoja lokalna tablica miast (specyficzne dla tego zadania)
    char cities[21];
    for (int i = 1; i <= 20; i++) {
        cities[i] = 'x';
    }

    // ============================================================================
    // === FRAGMENT: INICJALIZACJA I KONFIGURACJA EPOLL (Kopiuj w całości!) ===
    // ============================================================================
    int epoll_fd = epoll_create1(0); // Utworzenie "stróża"
    if (epoll_fd < 0) ERR("epoll_create1");

    // events[X] -> X to maksymalna liczba rzeczy, które chcemy nasłuchiwać jednocześnie
    // Tutaj: 2 rzeczy (Klawiatura + Gniazdo sieciowe)
    struct epoll_event event, events[2];

    // --- REJESTRACJA KLAWIATURY (STDIN_FILENO) ---
    event.events = EPOLLIN; // Interesują nas przychodzące dane (INput)
    event.data.fd = STDIN_FILENO; // Klawiatura
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, STDIN_FILENO, &event) == -1) ERR("epoll stdin");

    // --- REJESTRACJA GNIAZDA SERWERA (fd) ---
    event.events = EPOLLIN; // Interesują nas dane przychodzące z sieci
    event.data.fd = fd; // Zmienna z deskryptorem naszego połączonego serwera
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) ERR("epoll fd");

    // ============================================================================
    // === FRAGMENT: ZABEZPIECZENIE SYGNAŁÓW (Maska dla epoll_pwait) ===
    // ============================================================================
    // Ten kod blokuje sygnał SIGINT (Ctrl+C) w całym programie po to, aby epoll
    // mógł go odblokować tylko w momencie, gdy program "śpi". Zapobiega to błędom.
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT); // Dodajemy Ctrl+C do maski
    sigprocmask(SIG_BLOCK, &mask, &oldmask); // Nakładamy blokadę, starą maskę zapisujemy w oldmask


    // ============================================================================
    // === FRAGMENT: GŁÓWNA PĘTLA ZDARZEŃ EPOLL ===
    // ============================================================================
    while (work) {
        // epoll_pwait USYPIA program dopóki nie nadejdą dane.
        // 2 -> liczba nasłuchiwanych deskryptorów.
        // -1 -> timeout (czekaj w nieskończoność).
        // &oldmask -> na ten ułamek sekundy odblokuj Ctrl+C.
        int nfds = epoll_pwait(epoll_fd, events, 2, -1, &oldmask);

        if (nfds < 0) {
            if (errno == EINTR) continue; // Pwait zostało przerwane przez Ctrl+C, wracamy do "while(work)"
            ERR("epoll_pwait"); // Inny, krytyczny błąd
        }

        // Przechodzimy przez wszystkie zdarzenia, które nas obudziły (zazwyczaj nfds = 1)
        for (int i = 0; i < nfds; i++) {

            // --------------------------------------------------------------------
            // ZDARZENIE A: Użytkownik nacisnął Enter na klawiaturze
            // --------------------------------------------------------------------
            if (events[i].data.fd == STDIN_FILENO) {
                char input[128]; // Lokalny bufor na wpisywany tekst

                // Używamy fgets, bo jesteśmy pewni, że dane już tam są (epoll nas o tym poinformował)
                if (fgets(input, sizeof(input), stdin) == NULL) {
                    work = 0; // Użytkownik wcisnął Ctrl+D (EOF), kończymy
                    break;
                }

                // --- PARSOWANIE KOMEND Z ZADANIA (DOSTOSUJ DO SWOICH POTRZEB) ---
                if (input[0] == 'e') {
                    work = 0; // e - exit
                    break;
                }
                else if (input[0] == 'm') {
                    // Jak wyciągnąć ciąg znaków z wiadomości?
                    // input = "m ABC\n", chcemy przesłać samo "ABC\n" do serwera
                    char msg[5];
                    // snprintf to najbezpieczniejszy sposób. %.3s ucina tekst do 3 znaków z wejścia
                    snprintf(msg, sizeof(msg), "%.3s\n", &input[2]);
                    bulk_write(fd, msg, strlen(msg)); // Wysłanie do serwera
                }
                else if (input[0] == 't') {
                    // Jak wyciągnąć liczbę ze stringa? Zawsze używaj strtol zamiast atoi!
                    // &input[2] oznacza "zacznij czytać od 3-go znaku", baza to 10 (dziesiętny)
                    int number = strtol(&input[2], NULL, 10);

                    if (number < 1 || number > 20) {
                        printf("Bledny numer miasta\n");
                        continue; // Błędne dane, wracamy do nasłuchiwania
                    }

                    char side = (rand() % 2 == 0) ? 'p' : 'g';
                    cities[number] = side;

                    // Budowanie komunikatu formatowanego liczbą (np. g05\n)
                    // %c to pojedynczy znak, %02d to liczba dopełniona zerem do dwóch cyfr (np 5 -> 05)
                    char msg[5];
                    snprintf(msg, sizeof(msg), "%c%02d\n", side, number);
                    bulk_write(fd, msg, strlen(msg));
                }
                else if (input[0] == 'o') {
                    // Logika lokalna - wypisywanie na ekran
                    for (int c = 1; c <= 20; c++) {
                        printf("%d: %c \n", c, cities[c]);
                    }
                }
            }

            // --------------------------------------------------------------------
            // ZDARZENIE B: Przyszły dane z sieci od Serwera
            // --------------------------------------------------------------------
            else if (events[i].data.fd == fd) {
                char response[5];

                // WAŻNE: Używamy standardowego read(), a NIE bulk_read().
                // Dlaczego? bulk_read by nas zablokował, jeśli serwer wysłałby np. tylko 2 bajty,
                // a my kazalibyśmy mu czekać na 4. Zwykły read() czyta tyle, ile przyszło.
                ssize_t size = TEMP_FAILURE_RETRY(read(fd, response, 4));

                if (size <= 0) {
                    // size == 0 oznacza poprawne rozłączenie, size < 0 oznacza błąd sieciowy
                    printf("Serwer rozłączył się.\n");
                    work = 0;
                    break;
                }

                // ZAWSZE zamykaj odebrane dane znakówkiem '\0', żeby móc używać np. printf("%s")
                response[size] = '\0';

                // Parsowanie odpowiedzi serwera (zakładamy format YXX\n)
                if (size >= 3) {
                    char side = response[0];
                    int number = strtol(&response[1], NULL, 10);

                    // Aktualizacja logiki aplikacji na podstawie tego, co przyszło z sieci
                    if (number >= 1 && number <= 20) {
                        cities[number] = side;
                        printf("\n[Z serwera]: Miasto %d przejęte przez: %c\n", number, side);
                    }
                }
            }
        }
    }

    // ============================================================================
    // === FRAGMENT: CZYSZCZENIE ZASOBÓW PO ZAKOŃCZENIU (WYMAGANE NA LABACH) ===
    // ============================================================================
    // W to miejsce program wchodzi tylko, gdy work == 0 (np. Ctrl+C lub rozłączenie serwera)

    printf("\n--- Koniec działania. Stan końcowy miast ---\n");
    for (int i = 1; i <= 20; i++) {
        printf("%d: %c \n", i, cities[i]);
    }

    // Dobrą praktyką jest dokładne posprzątanie deskryptorów i sygnałów
    if (TEMP_FAILURE_RETRY(close(fd)) < 0) ERR("close fd");
    if (TEMP_FAILURE_RETRY(close(epoll_fd)) < 0) ERR("close epoll");
    sigprocmask(SIG_UNBLOCK, &mask, NULL); // Zdejmujemy naszą blokadę Ctrl+C ze środowiska

    return EXIT_SUCCESS; // Zwracamy kod 0 do systemu
}