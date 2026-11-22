#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>

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
    if(fd_recipe == -1){ 
        if(errno == ENOENT){//file doesnt exist
            fd_recipe = open(recipe_path, O_CREAT | O_RDWR, 0644); //tworzymy plik
            if(fd_recipe == -1) ERR("open");
            //jedna linia przepisu ktora ma 63 spacje i 1 znak \n
            char data[64];
            for(int j = 0; j < 63; j++){
                data[j] = ' ';
            }
            data[63] = '\n';
            //zapisujemy ta sama linie 32 razy kazda ma 64 bajty
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
    }
    char buf[256]; // dane z read
    char message[129]; //tutaj zapisujemy wiadomosc: 127 wiadomosc bez \n, 128 z \n, 129 jeszcze z \0
    int msg_len = 0; //dlugosc wiadomosci, gdyby byla w chunkach to dopisujemy do offsetu
    int line_number = 0; //gdzie zapisujemy w pliku recipe
    ssize_t n = 0;
    int too_long = 0;
    while((n = read(fd_tower, buf, sizeof(buf))) > 0){ //odczytujemy dane z fifo
        for(int i = 0; i < n; i++){
            if(buf[i] == '\n'){ //kompletna wiadomosc
                message[msg_len] = '\0'; //dodajemy \0
                if(too_long){
                    fprintf(stderr, "Granny got carried away: %s\n", message);
                    msg_len = 0;
                    too_long = 0;
                    continue;
                }

                char *colon = strchr(message, ':'); //dwukropek
                if(colon == NULL || colon == message){
                    fprintf(stderr, "Granny got carried away: %s\n", message);
                    msg_len = 0;
                    continue;
                }
                line_number = 0;
                int valid = 1;
                for (char *p = message; p < colon; p++) {
                    if (!isdigit(*p)) {
                        valid = 0;
                        break;
                    }
                }

                if (colon - message == 1) { // jedna cyfra
                    line_number = message[0] - '0';
                }
                else if (colon - message == 2) { // dwie cyfry
                    line_number = (message[0] - '0') * 10 + (message[1] - '0');
                }
                else {
                    valid = 0;
                }
                
                if (line_number > 31) valid = 0;
                if (!valid) {
                    fprintf(stderr, "Granny got carried away: %s\n", message);
                    msg_len = 0;
                    continue;
                }
                char *content = colon + 1; //tekst po dwukroipku 
                size_t content_len = strlen(content);
                if(content_len > 63) content_len = 63; //wiadomosc ma ax 63 znakow
                //zapisujemy do odpowiedniej lini
                if(lseek(fd_recipe, line_number * 64, SEEK_SET) == -1) ERR("lseek");
                //przygotowujemy linie
                char line[64];
                memset(line, ' ', 63);
                line[63] = '\n';
                memcpy(line, content, content_len);
                //zapisujemy do pliku i wypisujemy ze sie udalo
                if(write(fd_recipe, line, sizeof(line)) < 64) ERR("write");
                printf("Step %02d saved\n", line_number); //%02d zapewnia dwucyfrowy zapis numeru, np. 03, 07, 15
                msg_len = 0;
                line_number = 0;
            } else {
                if(msg_len < 127){
                    message[msg_len] = buf[i]; //przechodzimy po wszystkich otrzymanych bajtach
                    msg_len++;
                } else {
                    too_long = 1;
                }
            }
        }
    }
        if(n == -1) ERR("read");
        if(close(fd_recipe) == -1){
            ERR("close");
        }
        if(close(fd_tower) == -1){
            ERR("close");
        }
        return EXIT_SUCCESS;
    }
