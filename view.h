#ifndef VIEW_H
#define VIEW_H

// Header files required for viewing operations
#include "common.h"
#include "types.h"

// Function to read ID3v2 tag information
Status read_id3v2(FILE *fp);

// Function to display ID3v1 tag information
Status display_id3v1(FILE *fp);

// Function to view the tags in an MP3 file
Status view_tags(char *argv[],int is_edit);

// Function to open an MP3 file
Status open_file(char *filename, FILE **fp);

// Function to copy and decode frame text
Status copy_frame_text(char **destination, unsigned char *data, unsigned int frame_size, int is_comment);

// Function to read an ID3v1 field
Status read_id3v1_field(char **destination, FILE *fp, unsigned int size);

#endif