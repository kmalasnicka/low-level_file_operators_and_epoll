#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
 
void usage(int argc, char **argv){
    fprintf(stderr, "usage: %s recipe_path tower1 ... tower5", argv[0]);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv){
    if(argc < 3 || argc > 7) usage(argc, argv);
    char *recipe_path;
    char *tower1;
    int fd_recipe, fd_tower;

    recipe_path = argv[1];
    fd_recipe = open(recipe_path, O_RDWR); //otwieramy plik recipe 
    if(fd_recipe == -1){ //jak sie nie udalo sprawdzamy czy moze nie istnieje
        if(errno == ENOENT){//file doesnt exist
            fd_recipe = open(recipe_path, O_CREAT | O_RDWR, 0644); //tworzymy plik
            if(fd_recipe == -1) ERR("open");
            //jedna linia przepisu ktora ma 63 spacje i 1 znak \n
            char data[64];
            for(int j = 0; j < 63; j++){
                data[j] = ' ';
            }
            data[63] = '\n';
            //zapisujemy ta sama linie 32 razy do pliku
            for(int i = 0; i < 32; i++){
                if(write(fd_recipe, data, 64) != 64) ERR("write");
            }
        } else{ 
            ERR("open");
        }
    } else { //istnieje i udalo sie otworzyc
        off_t position = lseek(fd_recipe, 0, SEEK_END); //sprawdzamy rozmiar
        if(position == -1) ERR("lseek");
        if(position != 2048){ //rozmiar inny niz 2048 bajtow
            fprintf(stderr, "This book is not a recipe");
            exit(EXIT_FAILURE);
        }
    }
    //sprawdzamy czy tower1 jest fifo
    tower1 = argv[2];

    struct stat st;
    if(stat(tower1, &st) == -1) {
        if(errno == ENOENT){ //plik nie isnieje
            fprintf(stderr, "there is no granny in %s\n", tower1); //nie istnieje plik tower1
            exit(EXIT_FAILURE);
        }
        ERR("stat");
    }
    if(!S_ISFIFO(st.st_mode)){ //nie jest to fifo
        fprintf(stderr, "there is no granny in %s\n", tower1); //nie istnieje plik tower1
        exit(EXIT_FAILURE);
    }
    //jesli plik istnieje i jest to fifo
    fd_tower = open(tower1, O_RDONLY); //otwieramy
    if(fd_tower == -1){
        ERR("open");
    } else {
        char buf[256];
        ssize_t n;
        //czytamy do eof
        while((n = read(fd_tower, buf, sizeof(buf))) > 0){
            ssize_t written = 0;
            while(written < n){
                //wypisujemy to do stdout
                ssize_t w = write(STDOUT_FILENO, buf + written, n - written);
                if(w == -1) ERR("write");
                if(w == 0) {
                    fprintf(stderr, "write returned 0\n");
                    exit(EXIT_FAILURE);
                }
                written+=w;
            }
        }
        if(n == -1) ERR("read");
    }
    if(close(fd_recipe) == -1){
        ERR("close");
    }
    if(close(fd_tower) == -1){
        ERR("close");
    }
    return EXIT_SUCCESS;
}