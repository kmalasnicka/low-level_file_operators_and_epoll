#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void create_file(){
    int fd = open("a.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if(fd == -1) ERR("open");
    char *data = "abcdefghijklmnopqrstuvwxyz\n";
    if(write(fd, data, strlen(data)) < 0) ERR("write");
    close(fd);
}

int main(void){
    create_file(); //creating file
    
    int fd = open("a.txt", O_RDWR);
    if(fd == -1) ERR("open");

    struct stat s;
    if(fstat(fd, &s) == -1) ERR("fstat");

    ssize_t length = s.st_size;
    //mapping file into memory 
    char *buff = mmap(NULL, length, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
    if(buff == MAP_FAILED) ERR("mmap");

    printf("a.txt[3] = %c (ASCII: %d)\n", buff[3], buff[3]);
    buff[3] = 'Z'; //we are changing one character 
    msync(buff, length, MS_SYNC); //synchronizing change

    munmap(buff, length);
    close(fd);
    return 0;
}