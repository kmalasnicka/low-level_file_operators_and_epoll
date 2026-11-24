#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/uio.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define BATCH_BYTES 64

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
    if(argc < 2){
        usage(argc, argv);
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
        //odczytujemy koemndy
        ssize_t n = read(STDIN_FILENO, commands, BATCH_BYTES);
        //brak danych:
        if(n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            usleep(100000);
            continue;
        }
        if(n == -1 && errno == EINTR) continue; //sygnal
        if(n == -1) ERR("read"); //blad
        if(n == 0) break; //eof
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
    }
    free(offsets);
    if(munmap(data, size) == -1) ERR("munmap");
    if(close(fd_smbox)== -1) ERR("close");
    exit(EXIT_SUCCESS);
}