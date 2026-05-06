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