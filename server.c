#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#include <arpa/inet.h>
#endif

#define MAXLINE 8192
 
void bit_flip_random(char *data) {
    size_t len = strlen(data);
    if (len==0) return;
    size_t byte_idx = rand() % len;
    int bit = rand() % 8;
    data[byte_idx] ^= (1 << bit);
}

void char_substitution(char *data) {
    size_t len = strlen(data);
    if (len==0) return;
    size_t idx = rand() % len;
    char newc = (char)(32 + (rand() % 95));
    data[idx] = newc;
}

void char_deletion(char *data) {
    size_t len = strlen(data);
    if (len==0) return;
    size_t idx = rand() % len;
    memmove(&data[idx], &data[idx+1], len - idx);
}

void char_insertion(char *data) {
    size_t len = strlen(data);
    size_t idx = (len==0) ? 0 : (rand() % (len+1));
    if (len+1 >= MAXLINE-1) return;
    memmove(&data[idx+1], &data[idx], len - idx + 1);
    data[idx] = (char)(32 + (rand() % 95));
}

void char_swap_adjacent(char *data) {
    size_t len = strlen(data);
    if (len < 2) return;
    size_t idx = rand() % (len-1);
    char t = data[idx]; data[idx] = data[idx+1]; data[idx+1] = t;
}

void multiple_bit_flips(char *data) {
    int flips = 1 + rand()%4;
    size_t len = strlen(data);
    if (len==0) return;
    for (int i=0;i<flips;i++) {
        size_t byte_idx = rand() % len;
        int bit = rand() % 8;
        data[byte_idx] ^= (1 << bit);
    }
}

void burst_error(char *data) {
    size_t len = strlen(data);
    if (len==0) return;
    int burst_len = 3 + rand()%6;
    size_t start = rand() % len;
    for (int i=0;i<burst_len;i++) {
        size_t idx = (start + i) % len;
        data[idx] = (char)(32 + (rand() % 95));
    }
}

void corrupt_data(char *data) {
    int methods = 1 + rand()%3;
    for (int i=0;i<methods;i++) {
        int m = rand() % 7;
        switch(m) {
            case 0: bit_flip_random(data); break;
            case 1: char_substitution(data); break;
            case 2: char_deletion(data); break;
            case 3: char_insertion(data); break;
            case 4: char_swap_adjacent(data); break;
            case 5: multiple_bit_flips(data); break;
            case 6: burst_error(data); break;
        }
    }
}
 
int main(int argc, char **argv) {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }
#endif

    if (argc < 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return 1;
    }
    int port = atoi(argv[1]);
    srand(time(NULL));

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) { perror("socket"); return 1; }
    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
    struct sockaddr_in serv;
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = INADDR_ANY;
    serv.sin_port = htons(port);
    if (bind(listenfd, (struct sockaddr*)&serv, sizeof(serv)) < 0) { perror("bind"); return 1; }
    listen(listenfd, 5);
    printf("Server listening on port %d\n", port);

    printf("Waiting for client1 (sender) to connect...\n");
    struct sockaddr_in cli;
    socklen_t clilen = sizeof(cli);
    int conn1 = accept(listenfd, (struct sockaddr*)&cli, &clilen);
    if (conn1 < 0) { perror("accept"); return 1; }
    printf("client1 connected\n");

    char buffer[MAXLINE];
#ifdef _WIN32
    int n = recv(conn1, buffer, sizeof(buffer)-1, 0);
#else
    ssize_t n = read(conn1, buffer, sizeof(buffer)-1);
#endif
    if (n <= 0) { perror("read"); 
#ifdef _WIN32
        closesocket(conn1);
        closesocket(listenfd);
        WSACleanup();
#else
        close(conn1);
        close(listenfd);
#endif
        return 1; 
    }
    buffer[n] = '\0';
    printf("Received packet from client1: %s\n", buffer);

    char data[MAXLINE], method[128], control[MAXLINE];
    data[0]=method[0]=control[0]=0;
    char *p1 = strchr(buffer, '|');
    if (!p1) {
        printf("Bad packet format\n");
#ifdef _WIN32
        closesocket(conn1);
        closesocket(listenfd);
        WSACleanup();
#else
        close(conn1);
        close(listenfd);
#endif
        return 1;
    }
    size_t dlen = p1 - buffer;
    strncpy(data, buffer, dlen);
    data[dlen] = '\0';
    char *p2 = strchr(p1+1, '|');
    if (!p2) {
        printf("Bad packet format\n");
#ifdef _WIN32
        closesocket(conn1);
        closesocket(listenfd);
        WSACleanup();
#else
        close(conn1);
        close(listenfd);
#endif
        return 1;
    }
    size_t mlen = p2 - (p1+1);
    strncpy(method, p1+1, mlen);
    method[mlen] = '\0';
    strcpy(control, p2+1);

    printf("Original Data: %s\nMethod: %s\nControl: %s\n", data, method, control);

    char corrupted[MAXLINE];
    strncpy(corrupted, data, sizeof(corrupted)-1);
    corrupted[sizeof(corrupted)-1] = 0;
    corrupt_data(corrupted);
    printf("Corrupted Data: %s\n", corrupted);

    char outpkt[MAXLINE*2];
    snprintf(outpkt, sizeof(outpkt), "%s|%s|%s", corrupted, method, control);

    printf("Waiting for client2 (receiver) to connect...\n");
    int conn2 = accept(listenfd, (struct sockaddr*)&cli, &clilen);
    if (conn2 < 0) { perror("accept"); 
#ifdef _WIN32
        closesocket(conn1);
        closesocket(listenfd);
        WSACleanup();
#else
        close(conn1);
        close(listenfd);
#endif
        return 1; 
    }
    printf("client2 connected\n");

    size_t outlen = strlen(outpkt);
#ifdef _WIN32
    send(conn2, outpkt, outlen, 0);
    printf("Forwarded corrupted packet to client2: %s\n", outpkt);
    closesocket(conn1);
    closesocket(conn2);
    closesocket(listenfd);
    WSACleanup();
#else
    if (write(conn2, outpkt, outlen) != outlen) perror("write");
    printf("Forwarded corrupted packet to client2: %s\n", outpkt);
    close(conn1);
    close(conn2);
    close(listenfd);
#endif
    return 0;
}
 