#ifndef COMMON_H
#define COMMON_H

// Standard header files required for the project
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "types.h"

// ID3 tag identifiers and ID3v1 tag size
#define ID3V1_TAG "TAG"
#define ID3V2_TAG "ID3"
#define ID3V1_TAG_SIZE 128

// Function to display the help menu
void display_help();

#endif