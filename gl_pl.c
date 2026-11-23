#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/mman.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define BATCH_BYTES 64

void usage(int argc, char **argv){
    fprintf(stderr, "usage: %s smbox_file [fifo...]", argv[0]);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv){
    if(argc < 2){
        usage(argc, argv);
    }
    //otwieramy plik 
    int fd_smbox = open(argv[1], O_RDONLY);
    if(fd_smbox == -1) ERR("open");

    //mamy czytac z stdin w sposob nieblokujacy, ustawiamy to!
    int flags = fcntl(STDIN_FILENO, F_GETFL);
    if(flags == -1) ERR("fcntl");
    if(fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) == -1) ERR("fcntl");

    char commands[BATCH_BYTES]; //tablica 64 bajtow 
    int running = 1; //kontroluje dzialanie programu
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
                    printf("Selected message: 0\n");
                    break;
                case 's':
                    printf("Selected message: 0\n");
                    break;
                case 'm':
                    printf("Printing message 0\n");
                    break;
                case 'h':
                    printf("Printing headers...\n");
                    break;
                case 'd':
                    //sprawdzamy rozmiar pliku
                    printf("Dumping the file...\n"); //printf korzysta z buforowania, dane nie zawsze są od razu wysyłane na stdout
                    fflush(stdout); //wymusza wypisanie danych oczekujących w buforze stdout

                    struct stat st;
                    if(fstat(fd_smbox, &st) == -1) ERR("fstat");
                    size_t size = st.st_size;
                    if(size == 0) break;

                    char *data = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd_smbox, 0);
                    if(data == MAP_FAILED) ERR("mmap");

                    size_t written = 0;
                    while(written < size){
                        ssize_t n = write(STDOUT_FILENO, data + written, size - written);
                        if(n == -1 || n == 0) ERR("write");
                        written+=n;
                    }
                    if(munmap(data, size) == -1) ERR("munmap");
                    break;
                case 'q':
                    running = 0;
                    break;
                case '\n': 
                    break;
                default: 
                    printf("Invalid input\n");
                    break;
            }
            if(!running) break;
        }
    }
    if(close(fd_smbox)== -1) ERR("close");
    exit(EXIT_SUCCESS);
}