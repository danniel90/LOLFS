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

off_t BYTES_BLOQUES_DIRECTOS_FCB;
off_t BYTES_BLOQUES_DIRECTOS;
off_t BYTES_BLOQUE_UNA_INDIRECCION;
off_t BYTES_BLOQUE_DOS_INDIRECCION;
off_t BYTES_BLOQUE_TRES_INDIRECCION;

off_t MAX_RANGE_FCB_DIRECT_BLOCKS;
off_t MAX_RANGE_1LVL_INDIRECT_BLOCK;
off_t MAX_RANGE_2LVL_INDIRECT_BLOCK;
off_t MAX_RANGE_3LVL_INDIRECT_BLOCK;

#endif // LOLFS_H
