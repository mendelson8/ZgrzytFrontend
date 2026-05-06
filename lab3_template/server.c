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