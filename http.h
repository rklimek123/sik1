#ifndef HTTP_H
#define HTTP_H

#include <regex.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

///// Methods /////
#define M_OTHER 0
#define M_GET   1
#define M_HEAD  2

///// Header fields /////
#define H_CONNECTION     0
#define H_CONTENT_TYPE   1
#define H_CONTENT_LENGTH 2
#define H_SERVER         3

///// Target files /////
#define F_OK         0   // Filename falls under the regex [a-zA-Z0-9\.-/]*
#define F_INCORRECT  1   // Contrary to F_OK

///// Types /////

// Starting line of HTTP request
typedef struct http_starting {
    uint8_t method;      // GET / HEAD / OTHER
    char*   target;      // target file requested in the request
    uint8_t target_type; // listed in "Target files" 
} starting_t;

// HTTP headers tracked by this server
typedef struct http_headers {
    bool con_close;
    char* content_type;
    size_t content_len;
    char* server;

    bool checked_header[4]; // Marks header fields which had been read.
                            // According to "Header fields".
} headers_t;

// HTTP message body
typedef char* body_t;

// Formatted HTTP request
typedef struct http_request {
    starting_t starting;
    headers_t  headers;
    body_t     body;
} request_t;

///// Functions /////

// Parse returns
#define PARSE_INTERNAL_ERR -2
#define PARSE_BAD_REQ      -1
#define PARSE_SUCCESS       0

// raw - a string which will be interpreted as a http-request and parsed into out.
// Returns on of the values listed in "Parse returns".
// Changes raw internally, then restores it almost to the previous form.
// The only exception from this which may occur,
// is that there will be a '\0' char after the target file's name.
//
// raw needs to be terminated with a '\0' instead of a CRLF before the body.
int parse_http_request(char* raw, request_t* out);

// Frees the library data, eg. regexes
void parse_http_clean();

///// Response codes /////
#define C_OK              200
#define C_FOUND           302
#define C_BAD_REQUEST     400
#define C_NOT_FOUND       404
#define C_INTERNAL_ERROR  500
#define C_NOT_IMPLEMENTED 501

#define STR_OK              "OK"
#define STR_FOUND           "Found"
#define STR_BAD_REQUEST     ("Bad Request")
#define STR_NOT_FOUND       ("Not Found")
#define STR_INTERNAL_ERROR  ("Internal Server Error")
#define STR_NOT_IMPLEMENTED ("Not Implemented")

// Send returns
#define SEND_ERROR -1
#define SEND_OK     0
int send_bad_request(int target);
int send_internal_server_error(int target);
int send_not_implemented(int target);

#endif /* HTTP_H */
