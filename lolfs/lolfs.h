#ifndef LOLFS_H
#define LOLFS_H

#define FUSE_USE_VERSION 27

#include <fuse.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <pwd.h>
#include <sys/types.h> //para obtener uid y username

#include <time.h>

#include "estructura_bloques.h"

struct lolfs_path
{
    char path[16][44];//**path;
    int index;
};

char buffer[BLOCK_SIZE];
struct super_bloque superBlock;
struct lolfs_path lolfs;

#endif // LOLFS_H
