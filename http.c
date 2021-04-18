#include "http.h"

///// Parsing /////
static regex_t starting_line;
static regex_t verify_target_file;

static regex_t header;
static regex_t connection;
static regex_t connection_close;
static regex_t content_type;
static regex_t content_length;
static regex_t server;

// Returns 0 on a success
static int compile_regexes() {
    int flags = REG_EXTENDED | REG_NOSUB;
    int flags_icase = flags | REG_ICASE;

    if (regcomp(&starting_line, "^[^ \t\n\r\f\v]+ \\/[^ \t\n\r\f\v]* HTTP\\/1\\.1$", flags) == -1)
        return -1;
    if (regcomp(&verify_target_file, "^\\/[a-zA-Z0-9\\.\\-\\/]*$", flags) == -1)
        return -1;

    if (regcomp(&header, "^[^ \t\n\r\f\v:]+:[ ]*.+[ ]*$", flags) == -1)
        return -1;
    if (regcomp(&connection, "^Connection:", flags_icase) == -1)
        return -1;
    if (regcomp(&connection_close, "^[^ \t\n\r\f\v]+:[ ]*close[ ]*$", flags) == -1)
        return -1;
    if (regcomp(&content_type, "^Content-Type:", flags_icase) == -1)
        return -1;
    if (regcomp(&content_length, "^Content-Length:", flags_icase) == -1)
        return -1;
    if (regcomp(&server, "^Server:", flags_icase) == -1)
        return -1;
    
    return 0;
}

static int parse_starting_line(char* raw, starting_t* out);
static int parse_headers(char* raw, headers_t* out);
static int parse_header(char* raw, headers_t* out);

int parse_http_request(char* raw, request_t* out) {
    static bool regex_compiled = false;
    int ret;

    if (!regex_compiled) {
        regex_compiled = true;
        if (compile_regexes() != 0) return PARSE_INTERNAL_ERR;
    }

    size_t len = strlen(raw);
    // Get the starting line
    char* headers = strchr(raw, '\r');
    if (!headers) {
        return PARSE_INTERNAL_ERR;  // At least one '\r' should exist.
                                    // A proper HTTP label ends with CRLF'\0' (before the body).
    }
        
    *headers = '\0';
    headers += 2; // header points to the first line of headers or to '\0'

    ret = parse_starting_line(raw, &(out->starting));
    if (ret == PARSE_BAD_REQ || ret == PARSE_INTERNAL_ERR)
        return ret;
    
    ret = parse_headers(headers, &(out->headers));
    if (ret == PARSE_BAD_REQ || ret == PARSE_INTERNAL_ERR)
        return ret;
    return PARSE_SUCCESS;
}

static int parse_starting_line(char* raw, starting_t* out) {
    int ret;

    ret = regexec(&starting_line, raw, 0, NULL, 0);
    if (ret == REG_NOMATCH)
        return PARSE_BAD_REQ;

    // Method
    char* found = strstr(raw, "GET");
    if (found != raw) {
        found = strstr(raw, "HEAD");
        if (found != raw) {
            out->method = M_OTHER;
            return PARSE_SUCCESS;
        }
        out->method = M_HEAD;
    }
    else {
        out->method = M_GET;
    }

    // Target file
    char* target_file = strchr(raw, ' ');
    if (!target_file)
        return PARSE_INTERNAL_ERR;
    ++target_file;
    
    char* target_file_end = strchr(target_file, ' ');
    if (!target_file_end)
        return PARSE_INTERNAL_ERR;
    *target_file_end = '\0';

    out->target = target_file;

    ret = regexec(&verify_target_file, target_file, 0, NULL, 0);
    if (ret == REG_NOMATCH)
        out->target_type = F_INCORRECT;
    else
        out->target_type = F_OK;

    return PARSE_SUCCESS;
}

static int parse_headers(char* raw, headers_t* out) {
    out->checked_header[H_CONNECTION] = false;
    out->checked_header[H_CONTENT_LENGTH] = false;
    out->checked_header[H_CONTENT_TYPE] = false;
    out->checked_header[H_SERVER] = false;

    while (*raw != '\0') {
        char* next_header = strchr(raw, '\r');
        if (!next_header)
            return PARSE_INTERNAL_ERR;
        
        *next_header = '\0';
        next_header += 2;

        int ret = parse_header(raw, out);
        if (ret == PARSE_BAD_REQ || ret == PARSE_INTERNAL_ERR)
            return ret;
        
        raw = next_header;
    }

    return PARSE_SUCCESS;
}

static int parse_header(char* raw, headers_t* out) {
    int ret = regexec(&header, raw, 0, NULL, 0);
    if (ret == REG_NOMATCH)
        return PARSE_BAD_REQ;
    
    ret = regexec(&connection, raw, 0, NULL, 0);
    if (ret == 0) {
        if (out->checked_header[H_CONNECTION])
            return PARSE_BAD_REQ; // Double header
        out->checked_header[H_CONNECTION] = true;

        ret = regexec(&connection_close, raw, 0, NULL, 0);
        out->con_close = (ret == 0);
        return PARSE_SUCCESS;
    }

    ret = regexec(&content_type, raw, 0, NULL, 0);
    if (ret == 0) {
        // Content-Type tells something about the body.
        // Impossible in a request.
        return PARSE_BAD_REQ;
    }

    ret = regexec(&content_length, raw, 0, NULL, 0);
    if (ret == 0) {
        // Content-Length tells something about the body.
        // Impossible in a request.
        return PARSE_BAD_REQ;
    }

    ret = regexec(&server, raw, 0, NULL, 0);
    if (ret == 0) {
        if (out->checked_header[H_SERVER])
            return PARSE_BAD_REQ; // Double header
        out->checked_header[H_SERVER] = true;
        return PARSE_SUCCESS;
    }

    // Otherwise the header is ignored.
    return PARSE_SUCCESS;
}

void parse_http_clean() {
    regfree(&starting_line);
    regfree(&verify_target_file);

    regfree(&header);
    regfree(&connection);
    regfree(&connection_close);
    regfree(&content_type);
    regfree(&content_length);
    regfree(&server);
}

///// Sending /////
static int send_msg(int target, const char* message, size_t msg_size) {
    if (write(target, message, msg_size) != msg_size)
        return SEND_ERROR;
    return SEND_OK;
}

//debug
#include <stdio.h>

int send_bad_request(int target) {
    static const char* err_msg = "HTTP/1.1 400 Bad Request\r\nConnection:close\r\n\r\n";
    static size_t err_msg_size = 46;
    return send_msg(target, err_msg, err_msg_size);
}

int send_internal_server_error(int target) {
    static const char* err_msg = "HTTP/1.1 500 Internal Server Error\r\nConnection:close\r\n\r\n";
    static size_t err_msg_size = 56;
    return send_msg(target, err_msg, err_msg_size);
}

int send_not_implemented(int target) {
    static const char* err_msg = "HTTP/1.1 501 Not Implemented\r\n\r\n";
    static size_t err_msg_size = 32;
    return send_msg(target, err_msg, err_msg_size);
}