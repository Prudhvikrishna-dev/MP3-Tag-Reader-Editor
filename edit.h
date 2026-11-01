
#ifndef EDIT_H
#define EDIT_H

// Header files required for editing operations
#include "common.h"
#include "types.h"

// Function to edit the selected ID3 tag in the MP3 file
void edit_tags(char *filename, char *option, char *new_value,char *argv[]);

#endif
