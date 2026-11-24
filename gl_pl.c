#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <poll.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define BATCH_BYTES 64
#define MAX_FIFOS 5

void usage(int argc, char **argv){
    fprintf(stderr, "usage: %s smbox_file [fifo...]", argv[0]);
    exit(EXIT_FAILURE);
}

//plik smbox: dlugosc nagowka, dlugosc tresci, naglowek, wiadomosc 

void print_headers(unsigned char *data, size_t *offsets, int message_count, int selected_message){
    for(int i = 0; i < message_count; i++){ //iternujemy sie przez wszystkie wiadomosci
        size_t pos = offsets[i]; //pozycja wiadomosci
        unsigned char header_size = data[pos]; //dlugosc naglowka 
        //tworzymy trzy fragmenty do wypisania
        //strzalka
        struct iovec iov[3];
        iov[0].iov_base = "-> ";
        if(i == selected_message){
            iov[0].iov_len = 3;
        }
        else{
            iov[0].iov_len = 0;
        }

        //nagowek
        iov[1].iov_base = data + pos + 2;
        iov[1].iov_len = header_size - 1;
        //nowa linia
        iov[2].iov_base = "\n";
        iov[2].iov_len = 1;
        
        //wypisujemy dlugosc naglowka + iov[0].iov_len(0 lub 3 jak strzaka)
        if(writev(STDOUT_FILENO, iov, 3) != iov[0].iov_len + header_size) ERR("writev");
    }       
}
//wypisujemy selected message 
void print_message(unsigned char *data, size_t *offsets, int selected_message){
    size_t pos = offsets[selected_message];
    unsigned char header_size = data[pos];
    unsigned char message_size = data[pos + 1];

    struct iovec iov[4];
    iov[0].iov_base = data + pos + 2; //naglowek
    iov[0].iov_len = header_size - 1;

    iov[1].iov_base = "\n";
    iov[1].iov_len = 1;

    iov[2].iov_base = data + pos + 2 + header_size; //wiadomosc
    iov[2].iov_len = message_size - 1;

    iov[3].iov_base = "\n";
    iov[3].iov_len = 1;

    if(writev(STDOUT_FILENO, iov, 4) != header_size + message_size) ERR("writev");
}

int main(int argc, char **argv){
    if(argc < 2 || argc > MAX_FIFOS + 2){
        usage(argc, argv);
    }

    int fd_count = argc - 1;
    struct pollfd fds[fd_count];

    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[0].revents = 0;

    for(int i = 2; i < argc; i++){
        fds[i - 1].fd = open(argv[i], O_RDONLY | O_NONBLOCK);
        if(fds[i - 1].fd == -1) ERR("open fifo");
        fds[i - 1].events = POLLIN;
        fds[i - 1].revents = 0;
    }
    //otwieramy plik 
    int fd_smbox = open(argv[1], O_RDONLY);
    if(fd_smbox == -1) ERR("open");

    struct stat st;
    if(fstat(fd_smbox, &st) == -1) ERR("fstat");
    size_t size = st.st_size;
    
    if(size == 0){
        fprintf(stderr, "Empty file\n");
        exit(EXIT_FAILURE);
    }
    //mapujemy plik do pamieci, dzieki temu nie musimy uzywac read tylko czytamy z data 
    unsigned char *data = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd_smbox, 0);
    if(data == MAP_FAILED) ERR("mmap");

    //mamy czytac z stdin w sposob nieblokujacy, ustawiamy to!
    int flags = fcntl(STDIN_FILENO, F_GETFL);
    if(flags == -1) ERR("fcntl");
    if(fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) == -1) ERR("fcntl");

    char commands[BATCH_BYTES]; //tablica 64 bajtow 
    int running = 1; //kontroluje dzialanie programu
 
    size_t offset = 0; //aktualna pozycja bajtowa w pliku
    int message_count = 0; // liczba znalezionych wiadomości
    int selected_message = 0; //numer aktualnie wybranej wiadomosci
    while(offset < size){
        if(size - offset < 2){ //czy istnieją oba bajty określające długości
            fprintf(stderr, "Invalid file\n");
            exit(EXIT_FAILURE);
        }
        unsigned char header_size = data[offset];
        unsigned char message_size = data[offset + 1];
        size_t block_size = 2 + header_size +  message_size;
        //czy długości są dozwolone, czy cały blok mieści się w pliku
        if(header_size < 2 || message_size < 2 || block_size > size - offset){
            fprintf(stderr, "Invalid file\n");
            exit(EXIT_FAILURE);
        }
        //czy nagłówek i treść kończą się '\0'
        if(data[offset + 1 + header_size] != '\0' || data[offset + block_size - 1] != '\0'){
            fprintf(stderr, "Invalid file\n");
            exit(EXIT_FAILURE);
        }
        //sprawdzamy czy '\0' nie wystepuje wczesniej
        if(memchr(data + offset + 2, '\0', header_size - 1) != NULL || memchr(data + offset + 2 + header_size, '\0', message_size - 1) != NULL){
            fprintf(stderr, "Invalid file\n");
            exit(EXIT_FAILURE);
        }
        //przechodzimy do nasteonej wiadomosci
        message_count++;
        offset+= block_size;
    }

    //tablica do przechowywania pozycji kazdej wiadomosci w pliku
    size_t *offsets = malloc(message_count * sizeof(size_t));
    if(offsets == NULL) ERR("malloc"); 

    offset = 0; //poczatek pliku
    for(int i = 0; i < message_count; i++){ //przechodzimy przez wszystkie wiadomosci i zapisujemy offsety 
        offsets[i] = offset;
        unsigned char header_size = data[offset];
        unsigned char message_size = data[offset + 1];
        offset += 2 + header_size + message_size;
    }

    while(running){
        if(fd_count == 1 && fds[0].fd == -1) break;
        int ready = poll(fds, fd_count, -1);
        if(ready == -1 && errno == EINTR) continue;
        if(ready == -1) ERR("poll");

        for(int j = 0; j < fd_count && running; j++){
            if(fds[j].revents & (POLLERR | POLLNVAL)){
                fprintf(stderr, "Invalid input descriptor\n");
                exit(EXIT_FAILURE);
            }
            if(fds[j].revents & POLLIN){
                ssize_t n = read(fds[j].fd, commands, BATCH_BYTES);
                if(n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) continue;
                if(n == -1 && errno == EINTR) continue; //sygnal
                if(n == -1) ERR("read"); //blad
                if(n == 0){ //eof
                    if(j == 0){ //stdin
                        fds[j].fd = -1;//juz nie odczytujemy
                    }
                    continue;
                }

                for(ssize_t i = 0; i < n; i++){ //iterujemy sie po kazdym znaku z stdin
                    switch(commands[i]){
                        case 'w':
                            if(selected_message > 0){
                                selected_message--;
                            }
                            print_headers(data, offsets, message_count, selected_message);
                            break;
                        case 's':
                            if(selected_message < message_count - 1){
                                selected_message++;
                            }
                            print_headers(data, offsets, message_count, selected_message);
                            break;
                        case 'm': //aktualna wiadomosc
                            print_message(data, offsets, selected_message);
                            break;
                        case 'h': //wszystkie naglowki
                            print_headers(data, offsets, message_count, selected_message);
                            break;
                        case 'd':
                            printf("Dumping the file...\n"); //printf korzysta z buforowania, dane nie zawsze są od razu wysyłane na stdout
                            fflush(stdout); //wymusza wypisanie danych oczekujących w buforze stdout

                            size_t written = 0;
                            while(written < size){
                                ssize_t n = write(STDOUT_FILENO, data + written, size - written);
                                if(n == -1 && errno == EINTR) continue;
                                if(n <= 0) ERR("write");
                                written+=n;
                            }
                            break;
                        case 'q':
                            running = 0;
                            break;
                        case '\n': 
                            break;
                        default: 
                            printf("Invalid input\n");
                            fflush(stdout);
                            break;
                    }
                    if(!running) break;
                }
            } else if(fds[j].revents & POLLHUP){ //proces zapisujacy do fifo zamknal sie
                if(j > 0){ //sprawdzamy czy deskryptor to fifo
                    if(close(fds[j].fd) == -1) ERR("close fifo"); //zamykamy stary deskryptor
                    fds[j].fd = open(argv[j + 1], O_RDONLY | O_NONBLOCK); //ponownie otwieramy to samo fifo i nie czekamy na podłączenie procesu zapisującego(O_NONBLOCK)
                    if(fds[j].fd == -1)  ERR("open fifo");
                    fds[j].revents = 0; //usuwamy info o poprzednim zdarzeniu POLLHUP
                }
                else{ //to jest stdin
                    fds[j].fd = -1; //ignorujemy w poll
                }
            }
        }

    }

    for(int j = 1; j < fd_count; j++){ //zamykamy fifo
        if(close(fds[j].fd) == -1) ERR("close fifo");
    }
    free(offsets);
    if(munmap(data, size) == -1) ERR("munmap");
    if(close(fd_smbox)== -1) ERR("close");
    exit(EXIT_SUCCESS);
}

//readv- odczytanie dlugosci 

// unsigned char header_size;
// unsigned char message_size;
// struct iovec iov[2];
// iov[0].iov_base = &header_size;
// iov[0].iov_len = 1;
// iov[1].iov_base = &message_size;
// iov[1].iov_len = 1;
// ssize_t n = readv(fd, iov, 2);

//poprawnosc odczytu:
// if(n == -1) ERR("readv");
// if(n == 0){
//     // koniec pliku
// }
// if(n != 2){
//     fprintf(stderr, "Invalid file\n");
//     exit(EXIT_FAILURE);
// }

//odczytanie naglowka i tresci
// char header[255];
// char message[255];

// iov[0].iov_base = header;
// iov[0].iov_len = header_size;
// iov[1].iov_base = message;
// iov[1].iov_len = message_size;
// n = readv(fd, iov, 2);
// if(n == -1) ERR("readv");
// if(n != header_size + message_size){
//     fprintf(stderr, "Invalid file\n");
//     exit(EXIT_FAILURE);
// }


//np ksiazki:
// #include <stdint.h>
// #include <sys/uio.h>

// uint32_t id;
// char title[64];
// char author[32];
// uint32_t pages;
// struct iovec iov[4];
// iov[0].iov_base = &id;
// iov[0].iov_len = sizeof(id);
// iov[1].iov_base = title;
// iov[1].iov_len = sizeof(title);
// iov[2].iov_base = author;
// iov[2].iov_len = sizeof(author);
// iov[3].iov_base = &pages;
// iov[3].iov_len = sizeof(pages);
// ssize_t n = readv(fd, iov, 4);

//poczatek ostatniego recordu lseek(fd, -104, SEEK_END);
//akturalna pozycja o 104 w przod lseek(fd, 104, SEEK_CUR);
//wracamy na poczatek pliku lseek(fd, 104, SEEK_CUR);
//lseek(fd, selected_message * 104, SEEK_SET); poczatek wybranego recordu

//synchronizacja zmian: msync(data, size, MS_SYNC);

//zmiana flag:
//int flags = fcntl(STDIN_FILENO, F_GETFL);
// if(flags == -1) ERR("fcntl");
// if(fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) == -1) ERR("fcntl");

//poll #include <poll.h>
// struct pollfd fds[3];
// fds[0].fd = STDIN_FILENO;
// fds[0].events = POLLIN;
// fds[1].fd = fd1;
// fds[1].events = POLLIN;
// fds[2].fd = fd2;
// fds[2].events = POLLIN;
// int ready = poll(fds, 3, -1);

//blocking - domyslny, read czeka na dane
//nonblocking - jak nie ma danych to read zwraca -1 i errno na EAGAIN lub EWOULDBLOCK

//poll
// while(running){

//     int ready = poll(fds, fd_count, -1);

//     if(ready == -1 && errno == EINTR)
//         continue;

//     if(ready == -1)
//         ERR("poll");

//     for(int j = 0; j < fd_count && running; j++){

//         if(fds[j].revents & POLLIN){

//             ssize_t n = read(fds[j].fd, buffer, 64);

//             if(n == -1 && errno == EINTR)
//                 continue;

//             if(n == -1 && (errno == EAGAIN ||
//                            errno == EWOULDBLOCK))
//                 continue;

//             if(n == -1)
//                 ERR("read");

//             // obsługa EOF i odczytanych danych
//         }
//         else if(fds[j].revents & POLLHUP){

//             // obsługa rozłączenia
//         }
//     }
// }