#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define BUFF_SIZE 128
#define RESP_SIZE 1024

int main() {
    char ip[] = "0.0.0.0";
    int port = 7999, tn = 8;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == 1) {
        perror("can't sock");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in serv_addr = {
            .sin_family = AF_INET,
            .sin_port = htons(port),
            .sin_addr.s_addr = inet_addr(ip)
    };

    if (connect(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) == -1) {
        perror("can't connect");
        exit(EXIT_FAILURE);
    }

    char msg_to[BUFF_SIZE] = "v.mp4", buff_resp[RESP_SIZE] = "";
    if (send(sock, msg_to, BUFF_SIZE, 0) == -1) {
        perror("can't send");
        exit(EXIT_FAILURE);
    }

    FILE *f= fopen("./t.mp4", "wb");
    if (f == NULL) {
        perror("fopen error: ");
        close(sock);
        return -1;
    }

    unsigned long long total_read = 0, total_write = 0;
    int byte_read = 0, byte_write = 0;
    while ((byte_read = read(sock, buff_resp, RESP_SIZE)) > 0) {
        printf("read %d bytes\n", byte_read);
        total_read += byte_read;
        byte_write = fwrite(buff_resp, sizeof(char), byte_read, f);
        if(byte_write <= 0){
            perror("write error");
            return -1;
        }
        printf("write %d bytes\n", byte_write);
        total_write += byte_write;
    }
    printf("total raed %llu bytes\n", total_read);
    printf("total writen %llu bytes\n", total_write);
    fclose(f);
    close(sock);
    return 0;
}