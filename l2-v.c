#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <sys/uio.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define BOOK_SIZE 4
#define TITLE_SIZE 64

typedef struct {
    unsigned int size;
    char title[65];
}book;

void usage(int argc, char **argv){
    fprintf(stderr, "usage: %s database_path output_file", argv[0]);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv){
    if(argc != 3) usage(argc, argv);
    char *database = argv[1];
    char *output_file = argv[2];

    int fd1, fd2;
    fd1 = open(database, O_RDONLY);
    if(fd1 == -1) ERR("open");

    struct stat st;
    if(stat(database, &st) == -1) ERR("stat");
    off_t file_size = st.st_size;
    if(file_size % 68 != 0){
        fprintf(stderr, "Invalid database size\n");
        exit(EXIT_FAILURE);
    }
    int record_number = file_size / 68;

    struct iovec* buffers = malloc(record_number * sizeof(struct iovec)); //instrukcje dla readv gdzie umiescic dane
    book* books = malloc(record_number * sizeof(book)); //pr`zechowujemy ksiazki
    if(buffers == NULL || books == NULL) ERR("malloc");

    //zapisujemy to przed readv bo musimy powiedziec funckji gdzie ma zapisac odczytane dane i ile bajtow ma umiescic w kazdym buforze
    for(int i = 0; i < record_number; i++){
        buffers[i].iov_base = &books[i]; //zapisz odczytane dane pod adresem books[i]
        buffers[i].iov_len = 68; //do tej książki odczytaj 68 bajtów
        books[i].title[64] = 0; //ostatni znak tablicy title to '\0', bo tytul zajmjuje 64 bajty ale w strukturze mamy char title[65];
    }
    if(readv(fd1, buffers, record_number) < file_size) ERR("readv");
    if(close(fd1) == -1) ERR("close"); 

    for(int i = 0; i < record_number; i++){
        printf("size: %u, title: %s\n", books[i].size, books[i].title);
        books[i].size++;
    }

    fd2 = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if(fd2 == -1) ERR("open");

    if(writev(fd2, buffers, record_number) < file_size) ERR("writev");
    if(close(fd2) == -1) ERR("close"); 

    free(buffers);
    free(books);
    return EXIT_SUCCESS;
}