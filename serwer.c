#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "file.h"
#include "http.h"

#define BUFFER_SIZE 4096
#define DEFAULT_HTTP_PORT 8080

/////  ERR  /////
void syserr() {
    exit(EXIT_FAILURE);
}
/////////////////

int main (int argc, char *argv[]) {
    const char headers_end[] = {13, 10, 13, 10, 0};

    if (argc < 3 || argc > 4) {
        fprintf(stderr, "Usage: %s server's_filesystem_root corelated_servers [port_number]\n", argv[0]);
        syserr();
    }

    const char* filesystem = argv[1];
    DIR* root = opendir(filesystem); // Only used to confirm the existence of the target directory.
    if (!root) {
        // Cannot open the directory
        printf("DEBUG, cannot open directory\n");
        syserr();
    }
        
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
    
    if (bind(sock, (struct sockaddr *)&server, sizeof(server)) == -1) {
        // Socket binding unsuccessful
        printf("DEBUG - unsuccesfull binding to port %d\n", port);
        syserr();
    }

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
        char* read_loc = buffer;
        char* request_end;

        // Buffer starts with a fragment of the request, which already has been read (with length ≥ 0).  
        for (;;) {
            int ret;
            
            request_t http_request;
            // We are trying to read the whole request (preferably without a potential body)
            while (true)
            {
                ret = read(rcv, read_loc, remaining_buffer_size);
                if (ret == -1) {
                    exit(1); //todo
                    // CO TUTAJ MUSZĘ ROBIĆ? chyba to co po send not implemented, ale z send internal server error
                }

                // Searching for the end of headers -> CR LF CR LF
                request_end = strstr(buffer, headers_end);
                read_loc += ret;
                remaining_buffer_size -= ret;
                if (request_end != NULL)
                    break;

                if (remaining_buffer_size == 0) {
                    remaining_buffer_size = buffer_size;
                    buffer_size *= 2;
                    char* buf = malloc(buffer_size + 1);

                    if (!buf) {
                        //send_internal_error(rcv);
                        // todo
                        //następnie dotyczaj aż do nowej wiadomosci
                        exit(1);
                    }

                    memset(buf + remaining_buffer_size, 0, remaining_buffer_size + 1);
                    
                    strcpy(buf, buffer);
                    read_loc = buf + remaining_buffer_size;
                    free(buffer);
                    buffer = buf;
                }
            }

            // Parsing the request
            *(request_end + 2) = '\0';
            
            ret = parse_http_request(buffer, &http_request);
            if (ret == PARSE_BAD_REQ) {
                printf("DEBUG parsed bad req\n");
                //send_bad_request(rcv);
                break;
            }
            if (ret == PARSE_INTERNAL_ERR) {
                printf("DEBUG parsed internal\n");
                //send_internal_server_error(rcv);
                break;
            }

            if (http_request.starting.method == M_OTHER) {
                printf("DEBUG parsed other method\n");
                //send_not_implemented(rcv);
                // todo
                continue;
            }

            if (http_request.starting.target_type == F_INCORRECT) {
                printf("DEBUG parsed incorrect chars in filename\n");
                // send_not_found(rcv);
                // todo
                continue;
            }

            // We know, that the method requested is either GET or HEAD.
            // Both need to verify file access.
            printf("DEBUG: taking the file %s\n", http_request.starting.target);
            FILE* fptr;
            ret = take_file(filesystem, http_request.starting.target, &fptr);
            printf("DEBUG: took the file %s\n", http_request.starting.target);
            
            if (ret == FILE_NOT_FOUND) {
                //send_not_found(rcv); // tutaj też próba wyszukania w serwerach skorelowanych
                // todo
                continue;
            }
            
            if (ret == FILE_INTERNAL_ERR) {
                //send_internal_server_error(rcv);
                break;
            }
            printf("DEBUG: took the file\n");

            size_t filesize;
            ret = take_filesize(fptr, &filesize);
            if (ret == FILE_INTERNAL_ERR) {
                //send_internal_server_error(rcv);
                break;
            }

            char* filecontent;
            if (http_request.starting.method == M_GET) {
                ret = take_filecontent(fptr, filesize, &filecontent);
                if (ret == FILE_INTERNAL_ERR) {
                    //send_internal_server_error(rcv);
                    break;
                }

                printf("DEBUG: get ok\n\tfilename: %s\n\tfilesize: %lu\n\tfilecontent: %s\n",
                    http_request.starting.target, filesize, filecontent);
                // send_get_success(rcv);
            }
            else { /* if (http_request.starting.method == M_HEAD) { */
                printf("DEBUG: HEAD ok\n\tfilename: %s\n\tfilesize: %lu\n\tfilecontent: %s\n",
                    http_request.starting.target, filesize, filecontent);
                // send_head_success(rcv);
            }

            // Preparing buffer for another communicate
            //char* new_request = request_end + 2;


            exit(1);
        }

        close(rcv);
    }

    close(sock);
    fclose(corelated_servers);
}