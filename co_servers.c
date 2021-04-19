#include "co_servers.h"

int search_corelated_servers(const char* lookfilepath, const char* lookfor, char** out_address) {
    FILE* lookfile = fopen(lookfilepath, "r");
    if (!lookfile)
        return COS_INTERNAL_ERR;
    
    // lookfor + tab + XXX.XXX.XXX.XXX + tab + 65535 + '\n'
    size_t buffer_size = strlen(lookfor) + 1 + 15 + 1 + 5 + 1;
    if (buffer_size > __INT32_MAX__) {
        fclose(lookfile);
        return COS_INTERNAL_ERR;
    }

    char* buffer = malloc(buffer_size + 1);
    if (!buffer) {
        fclose(lookfile);
        return COS_INTERNAL_ERR;
    }

    size_t lookfor_len = strlen(lookfor);
    char look_for[lookfor_len + 2];
    if (strcpy(look_for, lookfor) == NULL) {
        fclose(lookfile);
        free(buffer);
        return COS_INTERNAL_ERR;
    }
    look_for[lookfor_len] = '\t';
    look_for[lookfor_len + 1] = '\0';

    while (fgets(buffer, buffer_size + 1, lookfile) != NULL) {
        char* ret = strstr(buffer, look_for);
        
        if (buffer == ret) {
            fclose(lookfile);

            char* ip = strchr(buffer, '\t');
            if (!ip) {
                free(buffer);
                return COS_INTERNAL_ERR;
            }
            ++ip;

            char* ip_end = strchr(ip, '\t');
            if (!ip_end) {
                free(buffer);
                return COS_INTERNAL_ERR;
            }
            *ip_end = ':';

            ip_end = strchr(ip_end, '\n');
            if (!ip_end) {
                free(buffer);
                return COS_INTERNAL_ERR;
            }
            *ip_end = '\0';

            size_t out_size = ip_end - ip;
            *out_address = malloc(out_size + 1);
            if (!*out_address) {
                free(buffer);
                return COS_INTERNAL_ERR;
            }

            if (strcpy(*out_address, ip) == NULL) {
                free(*out_address);
                free(buffer);
                return COS_INTERNAL_ERR;
            }
            free(buffer);
            return COS_FOUND;
        }
    }

    fclose(lookfile);
    free(buffer);
    return COS_NOT_FOUND;
}
