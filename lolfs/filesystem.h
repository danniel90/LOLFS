#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#define FUSE_USE_VERSION 27

#include <fuse.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "estructura_bloques.h"

char *buffer;//[SIZE_BLOCK];
struct super_bloque superBlock;

#endif // FILESYSTEM_H
