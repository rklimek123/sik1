#include "file.h"

static bool verify_file_contained_in_root(const char* filename) {
    int64_t depth = 0;  // On .. depth decrements.
                        // On . or blank it remains constant.
                        // On everything else it increments.
                        // When it is less than 0, that means we are outside root.
    
    const char* current_dir = filename + 1;
    while (*current_dir != '\0') {

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
    }

    return true;
}

int take_file(const char* filesystem, char* filename, FILE** out_fptr) {
    // Verify if filename doesn't try to get outside the root directory
    if (!verify_file_contained_in_root(filename))
        return FILE_NOT_FOUND;
    
    char* concat = malloc(strlen(filesystem) + strlen(filename) + 1);
    concat[strlen(filesystem) + strlen(filename)] = '\0';
    *out_fptr = fopen(concat, "r");
    free(concat);

    if (!*out_fptr)
        return FILE_NOT_FOUND;
    return FILE_OK;
}

int take_filesize(FILE* fptr, size_t* out_filesize) {
    if (fseek (fptr, 0, SEEK_END) != 0)
        return FILE_INTERNAL_ERR;

    if ((*out_filesize = ftell(fptr)) < 0)
        return FILE_INTERNAL_ERR;

    if (fseek(fptr, 0, SEEK_SET) != 0)
        return FILE_INTERNAL_ERR;
    
    return FILE_OK;
}

int take_filecontent(FILE* fptr, size_t filesize, char** out_content) {
    *out_content = malloc(filesize);
    if (*out_content == NULL)
        return FILE_INTERNAL_ERR;
  
    if (fread(*out_content, 1, filesize, fptr) != filesize)
        return FILE_INTERNAL_ERR;
    
    return FILE_OK;
}
