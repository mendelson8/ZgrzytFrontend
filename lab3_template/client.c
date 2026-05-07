
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
#include <sys/epoll.h> // NIEZBEDNE: Biblioteka do obslugi mechanizmu epoll

// ============================================================================
// === FRAGMENT: BEZPIECZNA OBSŁUGA SYGNAŁÓW (np. Ctrl+C) ===
// ============================================================================
// volatile sig_atomic_t gwarantuje, ze zmiana tej zmiennej nie zostanie
// przerwana w polowie przez system. Idealne do flag wyjscia z petli.
volatile sig_atomic_t work = 1;

void usage(char *name) { fprintf(stderr, "USAGE: %s domain port\n", name); }

// Funkcja przechwytujaca sygnal (np. SIGINT z Ctrl+C).
// WAZNE: Zawsze musi przyjmowac jeden argument (int sig), nawet jesli go nie uzywasz!
void stop_work(int sig) {
    work = 0; // Nie uzywaj tu printf ani exit(), po prostu zmien flage
}

int main(int argc, char **argv) {
    // Standardowe sprawdzenie argumentow wejsciowych klienta
    if (argc != 3) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    // 1. Nawiazanie polaczenia z serwerem (funkcja z common.h)
    int fd = connect_tcp_socket(argv[1], argv[2]);

    // 2. Podpiecie naszej funkcji stop_work pod skrot Ctrl+C (SIGINT)
    sethandler(stop_work, SIGINT);

    // srand odpalamy TYLKO RAZ na caly program. Jesli zadanie wymaga losowania, zostaw to.
    srand(time(NULL));

    // Twoja lokalna tablica miast (specyficzne dla tego zadania)
    char cities[21];
    for (int i = 1; i <= 20; i++) {
        cities[i] = 'x';
    }

    // ============================================================================
    // === FRAGMENT: INICJALIZACJA I KONFIGURACJA EPOLL (Kopiuj w calosci!) ===
    // ============================================================================
    int epoll_fd = epoll_create1(0); // Utworzenie "stroza"
    if (epoll_fd < 0) ERR("epoll_create1");

    // events[X] -> X to maksymalna liczba rzeczy, ktore chcemy nasluchiwac jednoczesnie
    // Tutaj: 2 rzeczy (Klawiatura + Gniazdo sieciowe)
    struct epoll_event event, events[2];

    // --- REJESTRACJA KLAWIATURY (STDIN_FILENO) ---
    event.events = EPOLLIN; // Interesuja nas przychodzace dane (INput)
    event.data.fd = STDIN_FILENO; // Klawiatura
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, STDIN_FILENO, &event) == -1) ERR("epoll stdin");

    // --- REJESTRACJA GNIAZDA SERWERA (fd) ---
    event.events = EPOLLIN; // Interesuja nas dane przychodzace z sieci
    event.data.fd = fd; // Zmienna z deskryptorem naszego polaczonego serwera
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) ERR("epoll fd");

    // ============================================================================
    // === FRAGMENT: ZABEZPIECZENIE SYGNAŁÓW (Maska dla epoll_pwait) ===
    // ============================================================================
    // Ten kod blokuje sygnal SIGINT (Ctrl+C) w calym programie po to, aby epoll
    // mogl go odblokowac tylko w momencie, gdy program "spi". Zapobiega to bledom.
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT); // Dodajemy Ctrl+C do maski
    sigprocmask(SIG_BLOCK, &mask, &oldmask); // Nakladamy blokade, stara maske zapisujemy w oldmask


    // ============================================================================
    // === FRAGMENT: GŁÓWNA PĘTLA ZDARZEŃ EPOLL ===
    // ============================================================================
    while (work) {
        // epoll_pwait USYPIA program dopoki nie nadejda dane.
        // 2 -> liczba nasluchiwanych deskryptorow.
        // -1 -> timeout (czekaj w nieskonczonosc).
        // &oldmask -> na ten ulamek sekundy odblokuj Ctrl+C.
        int nfds = epoll_pwait(epoll_fd, events, 2, -1, &oldmask);

        if (nfds < 0) {
            if (errno == EINTR) continue; // Pwait zostal przerwany przez Ctrl+C, wracamy do "while(work)"
            ERR("epoll_pwait"); // Inny, krytyczny blad
        }

        // Przechodzimy przez wszystkie zdarzenia, ktore nas obudzily (zazwyczaj nfds = 1)
        for (int i = 0; i < nfds; i++) {

            // --------------------------------------------------------------------
            // ZDARZENIE A: Uzytkownik nacisnal Enter na klawiaturze
            // --------------------------------------------------------------------
            if (events[i].data.fd == STDIN_FILENO) {
                char input[128]; // Lokalny bufor na wpisywany tekst

                // Uzywamy fgets, bo jestesmy pewni, ze dane juz tam sa (epoll nas o tym poinformowal)
                if (fgets(input, sizeof(input), stdin) == NULL) {
                    work = 0; // Uzytkownik wcisnal Ctrl+D (EOF), konczymy
                    break;
                }

                // --- PARSOWANIE KOMEND Z ZADANIA (DOSTOSUJ DO SWOICH POTRZEB) ---
                if (input[0] == 'e') {
                    work = 0; // e - exit
                    break;
                }
                else if (input[0] == 'm') {
                    // Jak wyciagnac ciag znakow z wiadomosci?
                    // input = "m ABC\n", chcemy przeslac samo "ABC\n" do serwera
                    char msg[5];
                    // snprintf to najbezpieczniejszy sposob. %.3s ucina tekst do 3 znakow z wejscia
                    snprintf(msg, sizeof(msg), "%.3s\n", &input[2]);
                    bulk_write(fd, msg, strlen(msg)); // Wyslanie do serwera
                }
                else if (input[0] == 't') {
                    // Jak wyciagnac liczbe ze stringa? Zawsze uzywaj strtol zamiast atoi!
                    // &input[2] oznacza "zacznij czytac od 3-go znaku", baza to 10 (dziesietny)
                    int number = strtol(&input[2], NULL, 10);

                    if (number < 1 || number > 20) {
                        printf("Bledny numer miasta\n");
                        continue; // Bledne dane, wracamy do nasluchiwania
                    }

                    char side = (rand() % 2 == 0) ? 'p' : 'g';
                    cities[number] = side;

                    // Budowanie komunikatu formatowanego liczba (np. g05\n)
                    // %c to pojedynczy znak, %02d to liczba dopelniona zerem do dwoch cyfr (np 5 -> 05)
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
            // ZDARZENIE B: Przysly dane z sieci od Serwera
            // --------------------------------------------------------------------
            else if (events[i].data.fd == fd) {
                char response[5];

                // WAZNE: Uzywamy standardowego read(), a NIE bulk_read().
                // Dlaczego? bulk_read by nas zablokowal, jesli serwer wyslalby np. tylko 2 bajty,
                // a my kazalibysmy mu czekac na 4. Zwykly read() czyta tyle, ile przyslo.
                ssize_t size = TEMP_FAILURE_RETRY(read(fd, response, 4));

                if (size <= 0) {
                    // size == 0 oznacza poprawne rozlaczenie, size < 0 oznacza blad sieciowy
                    printf("Serwer rozlaczyl sie.\n");
                    work = 0;
                    break;
                }

                // ZAWSZE zamykaj odebrane dane znakowkiem '\0', zeby moc uzywac np. printf("%s")
                response[size] = '\0';

                // Parsowanie odpowiedzi serwera (zakladamy format YXX\n)
                if (size >= 3) {
                    char side = response[0];
                    int number = strtol(&response[1], NULL, 10);

                    // Aktualizacja logiki aplikacji na podstawie tego, co przyslo z sieci
                    if (number >= 1 && number <= 20) {
                        cities[number] = side;
                        printf("\n[Z serwera]: Miasto %d przejete przez: %c\n", number, side);
                    }
                }
            }
        }
    }

    // ============================================================================
    // === FRAGMENT: CZYSZCZENIE ZASOBÓW PO ZAKONCZENIU (WYMAGANE NA LABACH) ===
    // ============================================================================
    // W to miejsce program wchodzi tylko, gdy work == 0 (np. Ctrl+C lub rozlaczenie serwera)

    printf("\n--- Koniec dzialania. Stan koncowy miast ---\n");
    for (int i = 1; i <= 20; i++) {
        printf("%d: %c \n", i, cities[i]);
    }

    // Dobra praktyka jest dokladne posprzatanie deskryptorow i sygnalow
    if (TEMP_FAILURE_RETRY(close(fd)) < 0) ERR("close fd");
    if (TEMP_FAILURE_RETRY(close(epoll_fd)) < 0) ERR("close epoll");
    sigprocmask(SIG_UNBLOCK, &mask, NULL); // Zdejmujemy nasza blokade Ctrl+C ze srodowiska

    return EXIT_SUCCESS; // Zwracamy kod 0 do systemu
}