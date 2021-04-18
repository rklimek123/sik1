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
///// BUFFER /////
// 0 - success, -1 - internal server error
int adjust_buffer_state(char** buffer, size_t* buffer_size, size_t* remaining_buffer_size, char** read_loc, char* request_end) {
    char* new_request = request_end + 2;
    size_t new_request_size = *buffer_size - *remaining_buffer_size;

    size_t new_buffer_size = BUFFER_SIZE;
    while (new_request_size >= new_buffer_size) {
        new_buffer_size *= 2;
    }

    char* new_buffer = malloc(new_buffer_size + 1);
    if (!new_buffer) {
        return -1;
    }
    
    if (strcpy(new_buffer, new_request) == NULL) {
        free(new_buffer);
        return -1;
    }
    
    memset(new_buffer + new_request_size, 0, new_buffer_size - new_request_size + 1);
    
    free(*buffer);
    *buffer = new_buffer;
    *buffer_size = new_buffer_size;
    *remaining_buffer_size = new_buffer_size - new_request_size;
    *read_loc = *buffer + new_request_size;

    return 0;
}

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
        syserr();
    }
        
    closedir(root);

    FILE* corelated_servers = fopen(argv[2], "r");
    if (!corelated_servers)
        // Cannot open the corelated servers file
        syserr();
    fclose(corelated_servers);

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
                    send_internal_server_error(rcv);
                    break;
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
                        send_internal_server_error(rcv);
                        break;
                    }

                    strcpy(buf, buffer);
                    memset(buf + remaining_buffer_size, 0, remaining_buffer_size + 1);
                    read_loc = buf + remaining_buffer_size;
                    free(buffer);
                    buffer = buf;
                }
            }

            // Parsing the request
            *(request_end + 2) = '\0';
            
            ret = parse_http_request(buffer, &http_request);
            if (ret == PARSE_BAD_REQ) {
                //send_bad_request(rcv);
                break;
            }
            if (ret == PARSE_INTERNAL_ERR) {
                send_internal_server_error(rcv);
                break;
            }

            if (http_request.starting.method == M_OTHER) {
                //send_not_implemented(rcv);
                // todo
                if (http_request.headers.con_close) break;
                if (adjust_buffer_state(&buffer, &buffer_size, &remaining_buffer_size, &read_loc, request_end) != 0) {
                    send_internal_server_error(rcv);
                    break;
                }
                continue;
            }

            if (http_request.starting.target_type == F_INCORRECT) {
                // send_not_found(rcv);
                // todo
                if (http_request.headers.con_close) break;
                if (adjust_buffer_state(&buffer, &buffer_size, &remaining_buffer_size, &read_loc, request_end) != 0) {
                    send_internal_server_error(rcv);
                    break;
                }
                continue;
            }

            // We know, that the method requested is either GET or HEAD.
            // Both need to verify file access.
            FILE* fptr;
            ret = take_file(filesystem, http_request.starting.target, &fptr);
            
            if (ret != FILE_OK) {
                fclose(fptr);
                if (ret == FILE_REACHOUT) {
                    //send_not_found(rcv);
                    // todo
                    if (http_request.headers.con_close) break;
                    if (adjust_buffer_state(&buffer, &buffer_size, &remaining_buffer_size, &read_loc, request_end) != 0) {
                        send_internal_server_error(rcv);
                        break;
                    }
                    continue;
                }
                else if (ret == FILE_NOT_FOUND) {
                    // see in correlated servers
                    // send_not_found(rcv);
                    // TODO
                    if (http_request.headers.con_close) break;
                    continue;
                }
                else if (ret == FILE_INTERNAL_ERR) {
                    send_internal_server_error(rcv);
                    break;
                }
            }

            size_t filesize;
            ret = take_filesize(fptr, &filesize);
            if (ret == FILE_INTERNAL_ERR) {
                send_internal_server_error(rcv);
                break;
            }

            char* filecontent;
            if (http_request.starting.method == M_GET) {
                ret = take_filecontent(fptr, filesize, &filecontent);
                fclose(fptr);
                if (ret == FILE_INTERNAL_ERR) {
                    send_internal_server_error(rcv);
                    break;
                }

                printf("DEBUG: get ok\n\tfilename: %s\n\tfilesize: %lu\n\tfilecontent: %s\n",
                    http_request.starting.target, filesize, filecontent);
                // send_get_success(rcv);
                // todo
                free(filecontent);
            }
            else { /* if (http_request.starting.method == M_HEAD) { */
                fclose(fptr);
                printf("DEBUG: HEAD ok\n\tfilename: %s\n\tfilesize: %lu\n\tfilecontent: %s\n",
                    http_request.starting.target, filesize, filecontent);
                // send_head_success(rcv);
            }

            if (http_request.headers.con_close) break;

            // Preparing buffer for another message
            if (adjust_buffer_state(&buffer, &buffer_size, &remaining_buffer_size, &read_loc, request_end) != 0) {
                send_internal_server_error(rcv);
                break;
            }
        }

        free(buffer);
        close(rcv);
    }

    close(sock);
    parse_http_clean();
}