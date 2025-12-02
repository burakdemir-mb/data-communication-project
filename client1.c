#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <unistd.h>
#endif

#define MAXLINE 4096

static void to_hex_str(const uint8_t *buf, size_t len, char *out) {
    static const char *hex = "0123456789ABCDEF";
    for (size_t i=0;i<len;i++) {
        out[2*i] = hex[(buf[i]>>4)&0xF];
        out[2*i+1] = hex[buf[i]&0xF];
    }
    out[2*len] = '\0';
}

static void u16_to_hex(uint16_t val, char *out) {
    sprintf(out, "%04X", val & 0xFFFF);
}

void gen_parity(const uint8_t *data, size_t len, char *out) {
    uint8_t *par = malloc(len);
    for (size_t i=0;i<len;i++) {
        uint8_t b = data[i];
        int ones = __builtin_popcount(b);
        par[i] = (ones % 2 == 0) ? 0x00 : 0x01;
    }
    to_hex_str(par, len, out);
    free(par);
}

void gen_2dpar(const uint8_t *data, size_t len, char *out) {
    size_t rows = (len + 7) / 8;
    uint8_t *outbytes = malloc((rows + 1));
    for (size_t r=0;r<rows;r++) {
        uint8_t parity = 0;
        for (size_t c=0;c<8;c++) {
            size_t idx = r*8 + c;
            if (idx >= len) break;
            parity ^= data[idx];
        }
        outbytes[r] = parity;
    }
    uint8_t overall = 0;
    for (size_t r=0;r<rows;r++) overall ^= outbytes[r];
    outbytes[rows] = overall;
    to_hex_str(outbytes, rows+1, out);
    free(outbytes);
}

uint16_t crc16_ccitt(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i=0;i<len;i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j=0;j<8;j++) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else crc <<= 1;
        }
    }
    return crc & 0xFFFF;
}

void gen_crc16(const uint8_t *data, size_t len, char *out) {
    uint16_t r = crc16_ccitt(data, len);
    u16_to_hex(r, out);
}

uint8_t hamming74_encode_nibble(uint8_t d) {
    uint8_t d0 = (d >> 0) & 1;
    uint8_t d1 = (d >> 1) & 1;
    uint8_t d2 = (d >> 2) & 1;
    uint8_t d3 = (d >> 3) & 1;
    uint8_t p1 = d0 ^ d1 ^ d3;
    uint8_t p2 = d0 ^ d2 ^ d3;
    uint8_t p4 = d1 ^ d2 ^ d3;
    uint8_t encoded = (p1<<0) | (p2<<1) | (d0<<2) | (p4<<3) | (d1<<4) | (d2<<5) | (d3<<6);
    return encoded;
}

void gen_hamming(const uint8_t *data, size_t len, char *out) {
    size_t nibbles = len * 2;
    uint8_t *buf = malloc(nibbles);
    for (size_t i=0;i<len;i++) {
        uint8_t b = data[i];
        uint8_t high = (b >> 4) & 0xF;
        uint8_t low = b & 0xF;
        buf[2*i] = hamming74_encode_nibble(high);
        buf[2*i+1] = hamming74_encode_nibble(low);
    }
    to_hex_str(buf, nibbles, out);
    free(buf);
}

uint16_t internet_checksum(const uint8_t *data, size_t len) {
    uint32_t sum = 0;
    size_t i = 0;
    while (len > 1) {
        uint16_t w = (data[i]<<8) | data[i+1];
        sum += w;
        if (sum & 0x10000) sum = (sum & 0xFFFF) + 1;
        i += 2;
        len -= 2;
    }
    if (len == 1) {
        uint16_t w = (data[i]<<8);
        sum += w;
        if (sum & 0x10000) sum = (sum & 0xFFFF) + 1;
    }
    return (uint16_t)(~sum & 0xFFFF);
}

void gen_checksum(const uint8_t *data, size_t len, char *out) {
    uint16_t c = internet_checksum(data, len);
    u16_to_hex(c, out);
}
 
int main(int argc, char **argv) {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }
#endif

    if (argc < 3) {
        printf("Usage: %s <server_ip> <port>\n", argv[0]);
        return 1;
    }
    char *server_ip = argv[1];
    int port = atoi(argv[2]);

    char line[MAXLINE];
    printf("Enter text (single line): ");
    if (!fgets(line, sizeof(line), stdin)) return 1;
    size_t len = strlen(line);
    if (len>0 && line[len-1]=='\n') { line[len-1] = '\0'; len--; }

    printf("Choose method: 1=PARITY 2=2DPAR 3=CRC16 4=HAMMING 5=CHECKSUM\n");
    int choice = 0;
    if (scanf("%d", &choice) != 1) return 1;
    while (getchar() != '\n');

    char method_str[32];
    char control_hex[MAXLINE];
    memset(control_hex,0,sizeof(control_hex));
    const uint8_t *data = (const uint8_t*)line;
    size_t data_len = strlen(line);

    switch(choice) {
        case 1: strcpy(method_str, "PARITY"); gen_parity(data, data_len, control_hex); break;
        case 2: strcpy(method_str, "2DPAR"); gen_2dpar(data, data_len, control_hex); break;
        case 3: strcpy(method_str, "CRC16"); gen_crc16(data, data_len, control_hex); break;
        case 4: strcpy(method_str, "HAMMING"); gen_hamming(data, data_len, control_hex); break;
        case 5: strcpy(method_str, "CHECKSUM"); gen_checksum(data, data_len, control_hex); break;
        default: printf("Invalid choice\n"); return 1;
    }

    char packet[MAXLINE*2];
    snprintf(packet, sizeof(packet), "%s|%s|%s", line, method_str, control_hex);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }
    struct sockaddr_in serv;
    serv.sin_family = AF_INET;
    serv.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &serv.sin_addr) <= 0) {
        perror("inet_pton");
#ifdef _WIN32
        closesocket(sock);
        WSACleanup();
#else
        close(sock);
#endif
        return 1;
    }
    if (connect(sock, (struct sockaddr*)&serv, sizeof(serv)) < 0) {
        perror("connect");
#ifdef _WIN32
        closesocket(sock);
        WSACleanup();
#else
        close(sock);
#endif
        return 1;
    }
    size_t pklen = strlen(packet);
#ifdef _WIN32
    send(sock, packet, pklen, 0);
    printf("Sent packet (%zu bytes): %s\n", pklen, packet);
    closesocket(sock);
    WSACleanup();
#else
    if (write(sock, packet, pklen) != pklen) perror("write");
    printf("Sent packet (%zu bytes): %s\n", pklen, packet);
    close(sock);
#endif
    return 0;
}
 