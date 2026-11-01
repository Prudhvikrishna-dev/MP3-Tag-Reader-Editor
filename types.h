#ifndef TYPES_H
#define TYPES_H

// Enum to indicate the status of an operation
typedef enum
{
    e_success,
    e_failure
} Status;

// Enum to identify the operation requested by the user
typedef enum
{
    e_view,
    e_edit,
    e_help,
    e_unsupported
} OperationType;

// Structure to store the metadata fields of ID3v1 and ID3v2 tags
typedef struct
{
    char *title;
    char *artist;
    char *album;
    char *year;
    char *comment;
    char *genre1;
    unsigned char genre;
} ID3vTag;

// Declares the global tag structure defined in main.c
extern ID3vTag tag;

#endif