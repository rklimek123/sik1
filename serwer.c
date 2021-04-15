#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "http.h"

#define BUFFER_SIZE 4096
#define DEFAULT_HTTP_PORT 8080

/////  ERR  /////
void syserr() {
    exit(EXIT_FAILURE);
}
/////////////////

int main (int argc, char *argv[]) {
    if (argc < 3 || argc > 4) {
        fprintf(stderr, "Usage: %s server's_filesystem_root corelated_servers [port_number]\n", argv[0]);
        syserr();
    }

    const char* filesystem = argv[1];
    DIR* root = opendir(filesystem); // Only used to confirm the existence of the target directory.
    if (!root)
        // Cannot open the directory
        syserr();
    closedir(root);

    FILE* corelated_servers = fopen(argv[2], "r");
    if (!corelated_servers)
        // Cannot open the corelated servers file
        syserr();

    short port;
    if (argc == 4)
        port = (short)atoi(argv[3]);
    else
        port = DEFAULT_HTTP_PORT;

    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1)
        // Socket creation unsuccessful
        syserr();

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(port);
    
    if (bind(sock, (struct sockaddr *)&server, sizeof(server)) == -1)
        // Socket binding unsuccessful
        syserr();

    if (listen(sock, 5) == -1)
        // Socket opening to listening unsuccessful
        syserr();

    for (;;) {
        int rcv = accept(sock, (struct sockaddr *)NULL, NULL);
        if (rcv == -1)
            // Setting a connection failed
            syserr();
        
        // Handling connection
        size_t buffer_size = BUFFER_SIZE;
        size_t remaining_buffer_size = buffer_size;
        char* buffer = malloc(buffer_size + 1);
        memset(buffer, 0, buffer_size + 1);
        void* read_loc = buffer;
        size_t read_len = buffer_size; // how many chars to fill the buffer

        // Buffer starts with a fragment of the starting line with length ≥ 0.  
        for (;;) {
            int ret;
            
            starting_t starting_line;
            char* new_begin;
            // We are trying to read the whole starting-line
            while (true)
            {
                ret = read(rcv, read_loc, remaining_buffer_size);
                if (ret == -1) {
                    // CO TUTAJ MUSZĘ ROBIĆ? chyba to co po send not implemented, ale z send internal server error
                }

                // Searching for the end of the first line -> CRLF
                char* content = strchr(read_loc, 13); // Search for CR
                read_loc += ret;
                remaining_buffer_size -= ret;
                if (content != NULL) {
                    *content = '\0';
                    new_begin = content + 2;
                    break;
                }

                if (remaining_buffer_size == 0) {
                    
                }

            }

            ret = parse_starting_line(buffer, &starting_line);
            if (ret == -1) {
                send_bad_request(rcv);
                break;
            }

            if (ret == 0) {
                send_not_implemented(rcv);
                następnie dotyczaj aż do nowej wiadomosci
            }
            else {
                new_begin
            }
        }

        close(rcv);
    }

    close(sock);
    fclose(corelated_servers);
}