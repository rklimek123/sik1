#include "http.h"
// debug
#include <stdio.h>

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
    if (regcomp(&starting_line, "^[^\\s]+ \\/[^\\s]* HTTP\\/1\\.1$", REG_NOSUB) == -1) {
        printf("DEBUG: 1st\n");
        return -1;
    }
    if (regcomp(&verify_target_file, "^\\/[a-zA-Z0-9\\.\\-\\/]*$", REG_NOSUB) == -1) {
        printf("DEBUG: 4th\n");
        return -1;
    }

    if (regcomp(&header, "^[^\\s:]+:[ ]*[^\\s]+[ ]*$", REG_NOSUB) == -1)
        return -1;
    if (regcomp(&connection, "^Connection:[ ]*[^\\s]+[ ]*$", REG_NOSUB || REG_ICASE) == -1)
        return -1;
    if (regcomp(&connection_close, "^[^\\s:]+:[ ]*close[ ]*$", REG_NOSUB) == -1)
        return -1;
    if (regcomp(&content_type, "^Content-Type:[ ]*[^\\s]+[ ]*$", REG_NOSUB || REG_ICASE) == -1)
        return -1;
    if (regcomp(&content_length, "^Content-Length:[ ]*[^\\s]+[ ]*$", REG_NOSUB || REG_ICASE) == -1)
        return -1;
    if (regcomp(&server, "^Server:[ ]*[^\\s]+[ ]*$", REG_NOSUB || REG_ICASE) == -1)
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
        printf("DEBUG: need to compile regexes\n");
        regex_compiled = true;
        if (compile_regexes() != 0) return PARSE_INTERNAL_ERR;
        printf("DEBUG: compiled regexes\n");
    }

    size_t len = strlen(raw);
    // Get the starting line
    char* headers = strchr(raw, '\r');
    if (!headers) {
        //printf("DEBUG: headers\n");
        return PARSE_INTERNAL_ERR;  // At least one '\r' should exist.
                                    // A proper HTTP label ends with CRLF'\0' (before the body).
    }
        
    *headers = '\0';
    headers += 2; // header points to the first line of headers or to '\0'

    ret = parse_starting_line(raw, &(out->starting));
    if (ret == PARSE_BAD_REQ || ret == PARSE_INTERNAL_ERR)
        return ret;
    printf("DEBUG: successfully parsed starting line\n");
    
    ret = parse_headers(raw, &(out->headers));
    if (ret == PARSE_BAD_REQ || ret == PARSE_INTERNAL_ERR)
        return ret;
    printf("DEBUG: successfully parsed headers\n");
    return PARSE_SUCCESS;
}

static int parse_starting_line(char* raw, starting_t* out) {
    int ret;
    ret = regexec(&starting_line, raw, 0, NULL, 0);
    if (ret == REG_NOMATCH)
        return PARSE_BAD_REQ;
    //printf("DEBUG: found starting line\n");

    // Method
    char* found = strstr(raw, "GET");
    if (found != raw) {
        found = strstr(raw, "HEAD");
        if (found != raw) {
            //printf("DEBUG: method OTHER\n");
            out->method = M_OTHER;
            return PARSE_SUCCESS;
        }
        //printf("DEBUG: method HEAD\n");
        out->method = M_HEAD;
    }
    else {
        //printf("DEBUG: method GET\n");
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
    //printf("DEBUG: target_file: %s\n", target_file);

    ret = regexec(&verify_target_file, target_file, 0, NULL, 0);
    if (ret == REG_NOMATCH) {
        //printf("DEBUG: Illegal characters in filename\n");
        out->target_type = F_INCORRECT;
    }
    else {
        //printf("DEBUG: Filename legal\n");
        out->target_type = F_OK;
    }

    return PARSE_SUCCESS;
}

static int parse_headers(char* raw, headers_t* out) {
    printf("DEBUG: headers: %s\n", raw);
    
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
        out->con_close == (ret == REG_NOMATCH);
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
