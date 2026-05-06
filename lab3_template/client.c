#include "common.h"

void usage(char *name) { fprintf(stderr, "USAGE: %s domain port\n", name); }

int main(int argc, char **argv)
{
    int fd;
    if (argc != 3) { // Zmień na odpowiednią liczbę argumentów, jeśli zadanie tego wymaga
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    // 1. Nawiązanie połączenia (funkcja blokuje aż się połączy)
    fd = connect_tcp_socket(argv[1], argv[2]);

    // =========================================================================
    // --- LOGIKA ZADANIA: PRZYGOTOWANIE I WYSŁANIE DANYCH ---
    // char buffer[128] = "moja wiadomosc";
    // bulk_write(fd, buffer, strlen(buffer));
    // (pamiętaj o htonl/htons dla typów binarnych int!)
    // =========================================================================


    // =========================================================================
    // --- LOGIKA ZADANIA: ODBIÓR ODPOWIEDZI ---
    // char response[128];
    // if (bulk_read(fd, response, oczekiwany_rozmiar) < 0) ERR("read");
    // printf("Odpowiedź: %s\n", response);
    // =========================================================================


    // Poprawne zamknięcie gniazda klienta
    if (TEMP_FAILURE_RETRY(close(fd)) < 0) ERR("close");

    return EXIT_SUCCESS;
}