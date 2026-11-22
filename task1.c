#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <poll.h>


#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define MAX_TOWERS 5

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
    struct pollfd fds[MAX_TOWERS];
    char *paths[MAX_TOWERS];
    int tower_number = argc - 2;
    for(int i = 2; i < argc; i++){
        struct stat st;
        if(stat(argv[i], &st) == -1) {
            if(errno == ENOENT){ //plik nie isnieje
                fprintf(stderr, "there is no granny in %s\n", argv[i]); //nie istnieje plik tower1
                exit(EXIT_FAILURE);
            }
            ERR("stat");
        }
        if(!S_ISFIFO(st.st_mode)){ //nie jest to fifo
            fprintf(stderr, "there is no granny in %s\n", argv[i]); //nie istnieje plik tower1
            exit(EXIT_FAILURE);
        }
        paths[i-2] = argv[i];
        int fd = open(paths[i - 2], O_RDONLY | O_NONBLOCK); //dzieki O_NONBLOCK nie bedziemy czekac na nadawce podczas otwierania pierwszego fifo
        if(fd == -1) ERR("open");
        fds[i-2].fd = fd;
        fds[i-2].events = POLLIN;
        fds[i-2].revents = 0;
    }

    int active = tower_number;
    char message[MAX_TOWERS][129];
    int msg_len[MAX_TOWERS] = {0};
    int too_long[MAX_TOWERS] = {0};
    char buf[256];
    int line_number = 0;

    while(active > 0){
        int ret = poll(fds, tower_number, 3000); //czekamy 3 sekundy 
        if (ret < 0) ERR("poll");
        if(ret == 0){ //no data arrived
            printf("Grannies are pouring more tea…\n");
            continue;
        }
        for(int i = 0; i < tower_number; i++){
            if(fds[i].fd == -1) continue;
            if(fds[i].revents & (POLLIN | POLLHUP)){
                //dane z roznych fifo moga przychodzic naprzemiennie wiec musimy miec dla kazdego fifo message, msg_len i too_long
                ssize_t n;
                while((n = read(fds[i].fd, buf, sizeof(buf))) > 0){ //odczytujemy dane z fifo
                    for(int j = 0; j < n; j++){
                        if(buf[j] == '\n'){ //kompletna wiadomosc
                            message[i][msg_len[i]] = '\0'; //dodajemy \0
                            if(too_long[i]){
                                fprintf(stderr, "Granny got carried away: %s\n", message[i]);
                                msg_len[i] = 0;
                                too_long[i] = 0;
                                continue;
                            }

                            char *colon = strchr(message[i], ':'); //dwukropek
                            if(colon == NULL || colon == message[i]){
                                fprintf(stderr, "Granny got carried away: %s\n", message[i]);
                                msg_len[i] = 0;
                                continue;
                            }
                            line_number = 0;
                            int valid = 1;
                            for (char *p = message[i]; p < colon; p++) {
                                if (!isdigit(*p)) {
                                    valid = 0;
                                    break;
                                }
                            }

                            if (colon - message[i] == 1) { // jedna cyfra
                                line_number = message[i][0] - '0';
                            }
                            else if (colon - message[i] == 2) { // dwie cyfry
                                line_number = (message[i][0] - '0') * 10 + (message[i][1] - '0');
                            }
                            else {
                                valid = 0;
                            }
                            
                            if (line_number > 31) valid = 0;
                            if (!valid) {
                                fprintf(stderr, "Granny got carried away: %s\n", message[i]);
                                msg_len[i] = 0;
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
                            msg_len[i] = 0;
                            line_number = 0;
                        } else {
                            if(msg_len[i] < 127){
                                message[i][msg_len[i]] = buf[j]; //przechodzimy po wszystkich otrzymanych bajtach
                                msg_len[i]++;
                            } else {
                                too_long[i] = 1;
                            }
                        }
                    }
                }
                if(n == 0){ //end of file
                    printf("Granny from %s finished dictating\n", paths[i]);
                    if(close(fds[i].fd) == -1) ERR("close"); //zamykamy fifo
                    fds[i].fd = -1; //usuwamy z monitored poll
                    active--;
                }
                if(n == -1) {
                    if(errno != EAGAIN && errno != EWOULDBLOCK){ //ignorujemy te bledy bo one mowia ze chwilowo nie ma danych
                        ERR("read");
                    }
                }
            }
        }
    }
        if(close(fd_recipe) == -1){
            ERR("close");
        }
        return EXIT_SUCCESS;
    }
