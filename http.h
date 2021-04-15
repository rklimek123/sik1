#ifndef HTTP_H
#define HTTP_H

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "string_set.h"

///// Methods /////
#define M_OTHER 0
#define M_GET   1
#define M_HEAD  2

///// Header fields /////
#define H_OTHER          0
#define H_CONNECTION     1
#define H_CONTENT_TYPE   2
#define H_CONTENT_LENGTH 3
#define H_SERVER         4

///// Status codes /////
#define C_OK              200
#define C_FOUND           302
#define C_BAD_REQUEST     400
#define C_NOT_FOUND       404
#define C_INTERNAL_ERROR  500
#define C_NOT_IMPLEMENTED 501

///// Types /////

// Starting line of HTTP request
typedef struct http_starting {
    uint8_t method; // GET / HEAD / OTHER
    char*   target; // target file requested in the request
} starting_t;

// HTTP headers tracked by this server
typedef struct http_headers {
    bool con_close;
    char* content_type;
    size_t content_len;
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

/* Parsing functions.
 * Return 0 when successfully parsed raw string to the corresponding struct.
 * Return -1 on bad request.
 * Return 1 otherwise (for example when parse_starting_line gets a method other than GET/HEAD).
 */
int parse_starting_line(const char* raw, starting_t* out);
int parse_header(const char* raw, headers_t* out);

#endif /* HTTP_H */
