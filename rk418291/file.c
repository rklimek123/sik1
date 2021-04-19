#include "file.h"

int is_file(const char* filepath) {
    struct stat file_stat;
    stat(filepath, &file_stat);
    if (!S_ISREG(file_stat.st_mode)) {
        return FILE_NOT_FOUND;
    }
    return FILE_OK;
}

static bool verify_file_contained_in_root(const char* filename) {
    int64_t depth = 0;  // On .. depth decrements.
                        // On . or blank it remains constant.
                        // On everything else it increments.
                        // When it is less than 0, that means we are outside root.
    
    const char* current_dir = filename + 1;
    while (current_dir != NULL && *current_dir != '\0') {
        if (*current_dir == '.') {

            if (*(current_dir + 1) == '.' && *(current_dir + 2) == '/') {
                // ..
                if (--depth < 0) return false;
            }
            else if (*(current_dir + 1) != '/') {
                // ^.[a-zA-Z0-9\-\.]+, other than ..
                ++depth;
            }
        }
        else if (*current_dir != '/') {
            ++depth;
        }
        current_dir = strchr(current_dir, '/');
        if (current_dir != NULL) ++current_dir;
    }

    return true;
}

int take_file(const char* filesystem, char* filename, FILE** out_fptr) {
    // Verify if filename doesn't try to get outside the root directory
    if (!verify_file_contained_in_root(filename))
        return FILE_REACHOUT;
    
    size_t filesystem_len = strlen(filesystem);
    size_t filename_len = strlen(filename);
    size_t concat_len = filesystem_len + filename_len;

    char* concat = malloc(concat_len + 1);
    if (!concat) return FILE_INTERNAL_ERR;

    if (strcpy(concat, filesystem) == NULL) {
        free(concat);
        return FILE_INTERNAL_ERR;
    }
    if (strcpy(concat + filesystem_len, filename) == NULL) {
        free(concat);
        return FILE_INTERNAL_ERR;
    }

    if (is_file(concat) == FILE_NOT_FOUND) {
        free(concat);
        return FILE_NOT_FOUND;
    }
    
    *out_fptr = fopen(concat, "r");
    free(concat);

    if (!*out_fptr)
        return FILE_INTERNAL_ERR;
    return FILE_OK;
}

int take_filesize(FILE* fptr, size_t* out_filesize) {
    if (fseek (fptr, 0, SEEK_END) != 0)
        return FILE_INTERNAL_ERR;
    long fsize = ftell(fptr);
    if (fsize < 0)
        return FILE_INTERNAL_ERR;
    *out_filesize = fsize;

    if (fseek(fptr, 0, SEEK_SET) != 0)
        return FILE_INTERNAL_ERR;
    
    return FILE_OK;
}

int take_filecontent_chunk(FILE* fptr, size_t chunk_size, char** out_content, size_t* out_hasread) {
    *out_hasread = fread(*out_content, 1, chunk_size, fptr);
    if (*out_hasread != chunk_size) {
        if (feof(fptr) != 0)
            return FILE_EOF;
        else
            return FILE_INTERNAL_ERR;
    }
    
    return FILE_OK;
}
