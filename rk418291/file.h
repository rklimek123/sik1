#ifndef FILE_H
#define FILE_H

#include <sys/stat.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Return codes //
#define FILE_INTERNAL_ERR -1
#define FILE_OK            0
#define FILE_NOT_FOUND     1
#define FILE_REACHOUT      2
#define FILE_EOF           3

// Checks whether a file in filepath exists and is a file.
int is_file(const char* filepath);

// Opens a file named filename into out_fptr ion a readonly mode,
// treating the directory supplied in filesystem as root.
int take_file(const char* filesystem, char* filename, FILE** out_fptr);

// Writes size of file fptr to out_filesize.
// fptr should be open in read mode.
int take_filesize(FILE* fptr, size_t* out_filesize);

// Writes contents of file fptr to out_content.
// fptr should be open in read mode.
// *out_content should have enough space to contain chunk_size bytes.
int take_filecontent_chunk(FILE* fptr, size_t chunk_size, char** out_content, size_t* out_hasread);

#endif /* FILE_H */