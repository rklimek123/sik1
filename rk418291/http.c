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
    if (regcomp(&verify_target_file, "^\\/[a-zA-Z0-9\\.\\/|-]*$", flags) == -1)
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
    out->con_close = false;

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
    if (write(target, message, msg_size) != (ssize_t)msg_size)
        return SEND_ERROR;
    return SEND_OK;
}

int send_success(int target, request_t* response) {
    static const char* base_msg = "HTTP/1.1 200 OK\r\n";
    static size_t base_msg_size = 17;

    /// Content-Type header ///
    // strlen("Content-Type:") = 13, strlen("\r\n") = 2
    size_t content_type_size = 13 + strlen(response->headers.content_type) + 2;
    char* content_type = malloc(content_type_size + 1);
    if (!content_type) {
        return SEND_ERROR;
    }

    if (strcpy(content_type, "Content-Type:") == NULL) {
        free(content_type);
        return SEND_ERROR;
    }
    if (strcpy(content_type + 13, response->headers.content_type) == NULL) {
        free(content_type);
        return SEND_ERROR;
    }
    content_type[content_type_size - 2] = '\r';
    content_type[content_type_size - 1] = '\n';

    /// Content-Length header with additional CRLF ///
    char* content_len = malloc(21);  // maximum size_t is less than 2*10^20
    if (!content_len) {
        free(content_type);
        return SEND_ERROR;
    }
    int content_len_size = snprintf(content_len, 20, "%lu", response->headers.content_len);
    if (content_len_size < 0) {
        free(content_len);
        free(content_type);
        return SEND_ERROR;
    }

    // strlen("Content-Length:") = 15, strlen("\r\n\r\n") = 4
    size_t content_lenh_size = 15 + content_len_size + 4;
    char* content_lenh = malloc(content_lenh_size + 1);
    if (!content_len) {
        free(content_len);
        free(content_type);
        return SEND_ERROR;
    }
    if (strcpy(content_lenh, "Content-Length:") == NULL) {
        free(content_lenh);
        free(content_len);
        free(content_type);
        return SEND_ERROR;
    }
    if (strcpy(content_lenh + 15, content_len) == NULL) {
        free(content_lenh);
        free(content_len);
        free(content_type);
        return SEND_ERROR;
    } free(content_len);
    content_lenh[content_lenh_size - 4] = '\r';
    content_lenh[content_lenh_size - 3] = '\n';
    content_lenh[content_lenh_size - 2] = '\r';
    content_lenh[content_lenh_size - 1] = '\n';

    /// Concatenate all parts ///
    size_t result_size = base_msg_size + content_type_size + content_lenh_size;
    char* result = malloc(result_size + 1);
    if (!result) {
        free(content_lenh);
        free(content_type);
        return SEND_ERROR;
    }
    if (strcpy(result, base_msg) == NULL) {
        free(result);
        free(content_lenh);
        free(content_type);
        return SEND_ERROR;
    }
    if (strcpy(result + base_msg_size, content_type) == NULL) {
        free(result);
        free(content_lenh);
        free(content_type);
        return SEND_ERROR;
    } free(content_type);
    if (strcpy(result + base_msg_size + content_type_size, content_lenh) == NULL) {
        free(result);
        free(content_lenh);
        free(content_type);
        return SEND_ERROR;
    } free(content_lenh);

    int ret = send_msg(target, result, result_size);
    free(result);
    return ret;
}

int send_body_chunk(int target, const char* chunk, size_t chunk_size) {
    return send_msg(target, chunk, chunk_size);
}

int send_found(int target, const char* filename, const char* address) {
    static const char* err_msg = "HTTP/1.1 302 Found\r\n";
    static size_t err_msg_size = 20;

    // Address
    size_t address_size = strlen(address);

    // File
    size_t filename_size = strlen(filename);

    /// Concatenation ///
    // strlen("Location: http://") = 17, strlen("\r\n\r\n") = 4
    size_t result_size = err_msg_size + 17 + address_size + filename_size + 4;
    char* result = malloc(result_size + 1);
    if (!result) {
        return SEND_ERROR;
    }
    result[result_size] = '\0';
    if (strcpy(result, err_msg) == NULL) {
        free(result);
        return SEND_ERROR;
    }
    if (strcpy(result + err_msg_size, "Location: http://") == NULL) {
        free(result);
        return SEND_ERROR;
    }
    if (strcpy(result + err_msg_size + 17, address) == NULL) {
        free(result);
        return SEND_ERROR;
    }
    if (strcpy(result + err_msg_size + 17 + address_size, filename) == NULL) {
        free(result);
        return SEND_ERROR;
    }
    result[result_size - 4] = '\r';
    result[result_size - 3] = '\n';
    result[result_size - 2] = '\r';
    result[result_size - 1] = '\n';

    int ret = send_msg(target, result, result_size);
    free(result);
    return ret;
}

int send_bad_request(int target) {
    static const char* err_msg = "HTTP/1.1 400 Bad Request\r\nConnection:close\r\n\r\n";
    static size_t err_msg_size = 46;
    return send_msg(target, err_msg, err_msg_size);
}

int send_not_found(int target) {
    static const char* err_msg = "HTTP/1.1 404 Not Found\r\n\r\n";
    static size_t err_msg_size = 26;
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