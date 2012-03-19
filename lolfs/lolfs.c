#include "lolfs.h"

#include <stdbool.h>

/* write_block, read_block:
 *  these are the functions you will use to access the disk
 *  image - *do not* access the disk image file directly.
 *
 *  These functions read or write a single 1024-byte block at a time,
 *  and take a logical block address. (i.e. block 0 refers to bytes
 *  0-1023, block 1 is bytes 1024-2047, etc.)
 *
 *  Make sure you pass a valid block address (i.e. less than the
 *  'fs_size' field in the superBlock)
 */
extern int write_block(int lba, void *buf);
extern int read_block(int lba, void *buf);


/*--------------------------------------------------------Bitmap----------------------------------------------------------*/

unsigned int alocarBloque()
{
    unsigned int inicio_bitmap = superBlock.primerbloque_mapabits;
    unsigned int size_bitmap = superBlock.sizebloques_mapabits;


    unsigned int i;
    for(i = inicio_bitmap; i < (inicio_bitmap + size_bitmap); i++) {
        read_block(i,buffer);

        unsigned int j;
        for(j = 0; j < BLOCK_SIZE; j++) {
            if (buffer[j] != 0) {

                unsigned int x;
                for(x = 0; x < 8; x++) {
                    if (buffer[j] & (1 << x)) {
                        buffer[j] &= ~(1 << x);
                        write_block(i, buffer);                        
                        unsigned int res = (i - inicio_bitmap) * (8 + size_bitmap) + (j * 8) + x;
                        printf("\t\t\t alocarBloque dbg: (%u - %u) * (8 + %u) + (%u * 8) + %u =  %u!!\n",
                               i,inicio_bitmap, size_bitmap, j, x, res);
                        return res;//return  (i * size_bloques * 8) + (j * 8) + x;
                    }
                }
            }
        }
    }
    return 0; // bloque 0 es utilizado por el Super_Block pro ende es imposible utilizarlo y es considerado cmomo error
}

void liberarBloque(unsigned int bloque)
{
    unsigned int size_bloques   = superBlock.sizebloques_mapabits;
    unsigned int size_bitmap    = superBlock.sizebloques_mapabits;

    unsigned int queBloque  = (bloque / (8 * size_bitmap));
    unsigned int tmp        = (queBloque % (8 * size_bloques));
    unsigned int queByte    = tmp / 8;
    unsigned int queBit     = tmp % 8;

    read_block(queBloque, buffer);
    buffer[queByte] |= (1 << queBit);
    write_block(queBloque, buffer);
}


/*--------------------------------------------------------Filesystem----------------------------------------------------------*/

void initFileRanges()
{
    BYTES_BLOQUES_DIRECTOS_FCB     = CANT_BLOQUES_DIRECTOS * BLOCK_SIZE;
    BYTES_BLOQUES_DIRECTOS         = CANT_BLOQUES * BLOCK_SIZE;
    BYTES_BLOQUE_UNA_INDIRECCION   = BYTES_BLOQUES_DIRECTOS;
    BYTES_BLOQUE_DOS_INDIRECCION   = BYTES_BLOQUE_UNA_INDIRECCION * CANT_BLOQUES;
    BYTES_BLOQUE_TRES_INDIRECCION  = BYTES_BLOQUE_DOS_INDIRECCION * CANT_BLOQUES;

    MAX_RANGE_FCB_DIRECT_BLOCKS     = BYTES_BLOQUES_DIRECTOS_FCB;
    MAX_RANGE_1LVL_INDIRECT_BLOCK   = BYTES_BLOQUE_UNA_INDIRECCION + MAX_RANGE_FCB_DIRECT_BLOCKS;
    MAX_RANGE_2LVL_INDIRECT_BLOCK   = BYTES_BLOQUE_DOS_INDIRECCION + MAX_RANGE_1LVL_INDIRECT_BLOCK;
    MAX_RANGE_3LVL_INDIRECT_BLOCK   = BYTES_BLOQUE_TRES_INDIRECCION + MAX_RANGE_2LVL_INDIRECT_BLOCK;


    printf("BLOCK_SIZE = \t\t\t%d\n", BLOCK_SIZE);
    printf("CANT_BLOQUES = \t\t\t%d\n", CANT_BLOQUES);
    printf("CANT_BLOQUES_DIRECTOS = \t%d\n\n", CANT_BLOQUES_DIRECTOS);

    printf("BYTES_BLOQUES_DIRECTOS_FCB = \t%lld\n", BYTES_BLOQUES_DIRECTOS_FCB);
    printf("BYTES_BLOQUES_DIRECTOS = \t%lld\n", BYTES_BLOQUES_DIRECTOS);
    printf("BYTES_BLOQUE_UNA_INDIRECCION = \t%lld\n", BYTES_BLOQUE_UNA_INDIRECCION);
    printf("BYTES_BLOQUE_DOS_INDIRECCION = \t%lld\n", BYTES_BLOQUE_DOS_INDIRECCION / 1024 / 1024 / 1024);
    printf("BYTES_BLOQUE_TRES_INDIRECCION = %lld\n\n", BYTES_BLOQUE_TRES_INDIRECCION / 1024 / 1024 / 1024);

    printf("MAX_RANGE_FCB_DIRECT_BLOCKS = \t%lld\n", MAX_RANGE_FCB_DIRECT_BLOCKS);
    printf("MAX_RANGE_1LVL_INDIRECT_BLOCK = %lld\n", MAX_RANGE_1LVL_INDIRECT_BLOCK);
    printf("MAX_RANGE_2LVL_INDIRECT_BLOCK = %lld\n", MAX_RANGE_2LVL_INDIRECT_BLOCK / 1024 / 1024 / 1024);
    printf("MAX_RANGE_3LVL_INDIRECT_BLOCK = %lld\n", MAX_RANGE_3LVL_INDIRECT_BLOCK / 1024 / 1024 / 1024);
}

 void setPath(const char *path)
 {
    lolfs.index = 0;
    char copia[strlen(path) + 1];
    strcpy(copia, path);

    char *dir_name = strtok(copia, "/");

    while (dir_name != NULL){
        if (strcmp(dir_name, "..") == 0) {
            lolfs.index--;
        } else {
            if (strcmp(dir_name, ".") != 0) {
                strcpy(lolfs.path[lolfs.index], dir_name);
                lolfs.index++;
            }
        }
        dir_name = strtok(NULL, "/");
    }
}

 int searchFile(struct Directory *dir, char *file_name)
 {
     int identry;
     for (identry = 0; identry < CANT_DIR_ENTRIES; identry++) {
         if (dir->directory_Entries[identry].tipo_bloque == LIBRE)
             continue;

         struct Directory_Entry dirEntry = dir->directory_Entries[identry];

         if (strcmp(file_name, dirEntry.nombre) == 0)
             return dirEntry.apuntador;
     }

     return -ENOENT;
 }

 int getBlock(const char *path)
 {
     setPath(path);

     int bloque = superBlock.bloque_root;

     if (lolfs.index <= 0){
         return bloque;
     }

     read_block(bloque, &buffer);

     int x;
     for (x = 0; x < lolfs.index; x++) {
         bloque = searchFile((void *)buffer, lolfs.path[x]);

         if (bloque == -ENOENT)
             return -ENOENT;

         read_block(bloque, &buffer);
     }

     return bloque;
 }

char *getFilename(const char *path)
{
    if (strcmp(path, "/") == 0)
        return "";

    char *temp = NULL;

    int x;
    for (x = strlen(path) - 1; x >= 0; x--) {
        if (path[x] == '/') {
            if (x == (strlen(path) - 1))
                continue;

            temp = (char *) malloc(strlen(path) - x - 1);
            memcpy(temp, path + x + 1, strlen(path) - x);

            if (temp[strlen(temp) - 1] == '/')
                temp[strlen(temp) - 1] = '\0';

            break;
        }
    }
    return temp;
}

char *getDirectory(const char *path)
{
    if (strcmp(path, "/") == 0)
            return "/";

    char *temp = malloc(strlen(path) + 1);
    strcpy(temp, path);

    int x;
    for (x = strlen(path) - 1; x >= 0; x--){
        if (path[x] == '/') {
            if (x == (strlen(path) - 1))
                continue;

            temp = (char *) malloc(x + 1);
            memcpy(temp, path, x + 1);
            temp[x + 1] = '\0';
            break;
        }
    }

    return temp;
}

int setStat(struct stat* sb, void *entry)
{
    sb->st_blocks   = ((struct inodo *)entry)->tipo_bloque == DIRECTORIO ? BLOCK_SIZE / 512 : ((unsigned)((struct file_control_block *) entry)->lenght) / 512;
    sb->st_nlink    = ((struct inodo *)entry)->tipo_bloque == DIRECTORIO ? 2 : 1;
    sb->st_size     = ((struct inodo *)entry)->tipo_bloque == DIRECTORIO ? BLOCK_SIZE : ((struct file_control_block *) entry)->lenght;
    sb->st_uid      = ((struct inodo *)entry)->uid;
    sb->st_gid      = ((struct inodo *)entry)->gid;
    sb->st_mode     = ((struct inodo *)entry)->mode;
    sb->st_ctime    = ((struct inodo *)entry)->fcreacion;
    sb->st_mtime    = ((struct inodo *)entry)->fmodificacion;
    sb->st_atime    = ((struct inodo *)entry)->facceso;

    return 0;
}

/*----------------------------------------------------------FUSE--------------------------------------------------------*/

/* init - called once at startup
 * This might be a good place to read in the super-block and set up
 * any global variables you need. You don't need to worry about the
 * argument or the return value.
 */
void *ondisk_init(struct fuse_conn_info *conn)
{
    initFileRanges();
    read_block(0, &buffer);
    memcpy(&superBlock, &buffer, sizeof(struct Super_Block));

    return 0;
}

/* Note on path translation errors:
 * In addition to the method-specific errors listed below, almost
 * every method can return one of the following errors if it fails to
 * locate a file or directory corresponding to a specified path.
 *
 * ENOENT - a component of the path is not present.
 * ENOTDIR - an intermediate component of the path (e.g. 'b' in
 *           /a/b/c) is not a directory
 * EACCES  - the user lacks *execute* permission (yes, execute) for an
 *           intermediate directory in the path.
 *
 * In our case we assume a single user, and so it is sufficient to
 * use this test:    if ((mode & S_IXUSR) == 0)
 *                        return -EACCES;
 *
 * See 'man path_resolution' for more information.
 */

/* getattr - get file or directory attributes. For a description of
 *  the fields in 'struct stat', see 'man lstat'.
 * Note - fields not provided in CS7600fs are:
 *    st_nlink - always set to 1
 *    st_atime, st_ctime - set to same value as st_mtime
 * errors - path translation, ENOENT
 */
static int ondisk_getattr(const char *path, struct stat *stbuf)
{
    struct Directory dir;
    memset(&dir, 0, sizeof(struct Directory));

    int bloque = getBlock(path);

    if (bloque <= 0) {
        printf("\t getattr DUMP: bloque = %d | path = %s\n", bloque, path);
        return -ENOENT;
    }

    printf("\t getattr dbg: bloque = %d | path = %s\n", bloque, path);
    read_block(bloque, &dir);

    setStat(stbuf, &dir);

    return 0;
}

/* readdir - get directory contents
 * for each entry in the directory, invoke:
 *     filler(buf, <name>, <statbuf>, 0)
 * where <statbuf> is a struct stat, just like in getattr.
 * Errors - path resolution, EACCES, ENOTDIR, ENOENT
 *
 * EACCES is returned if the user lacks *read* permission to the
 * directory - i.e.:     if ((mode & S_IRUSR) == 0)
 *                           return -EACCES;
 */
static int ondisk_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                          off_t offset, struct fuse_file_info *fi)
{
    char *name;
    struct stat sb;
    memset(&sb, 0, sizeof(struct stat));
    struct Directory dir;

    int bloque = getBlock(path);

    if (bloque <= 0) {
        printf("\t readdir DUMP: %s error al obtener entrada!!\n", path);
        return bloque;
    }

    printf("\t readdir dbg: bloque = %d\n", bloque);

    read_block(bloque, &dir);

    if (dir.info.tipo_bloque != DIRECTORIO) {
        printf("\t readdir DUMP: %s no es directorio!! es %d\n", path, dir.info.tipo_bloque);
        return -ENOTDIR;
    }

    if ((dir.info.mode & S_IRUSR) == 0) {
        printf("\t readdir DUMP: no tiene permisos para leer %s!!\n", path);
        return -EACCES;
    }

    int x;
    for (x = 0; x < CANT_DIR_ENTRIES; x++) {
        if (dir.directory_Entries[x].tipo_bloque == LIBRE)
            continue;
        else {
            read_block(x, &buffer);
            setStat(&sb, (void *)buffer);
            name = dir.directory_Entries[x].nombre;
            filler(buf, name, &sb, 0);
        }
    }
    return 0;
}

/* create - create a new file
 *   object permissions:    (mode & 01777)
 * Errors - path resolution, EACCES, EEXIST
 *
 * EACCES is returned if the user does not have write permission to
 * the enclosing directory:
 *       if ((mode & S_IWUSR) == 0)
 *           return -EACCES;
 * If a file or directory of this name already exists, return -EEXIST.
 */
static int ondisk_create(const char *path, mode_t mode,
                         struct fuse_file_info *fi)
{
    char *directoryname = getDirectory(path);
    int bloque = getBlock(directoryname);

    if (bloque <= 0) {
        printf("\t create DUMP: bloque = %d | directoryname = %s\n", bloque, directoryname);
        return -ENOTDIR;
    }

    char *filename = getFilename(path);

    printf("\t create dbg: PATH = %s | Parent Dir = %s | BloqNum = %d | File = %s\n", path, directoryname, bloque, filename);

    //variable declaration/init
    struct Directory dir;
    read_block(bloque, &dir);

    //dir permissions & file name validation
    if ((dir.info.mode & S_IWUSR) == 0) {
        printf("\t create DUMP: Usuario no tiene write permissions en %s!!\n", directoryname);
        return -EACCES;
    }

    if (searchFile(&dir, filename) != -ENOENT) {
        printf("\t create DUMP: %s ya existe en %s!!\n", filename, directoryname);
        return -EEXIST;
    }

    //crear entrada nueva
    if (dir.cantidad_elementos == 63) {
        printf("\t create DUMP: directorio %s lleno!!\n", directoryname);
        return -ENOTEMPTY;
    }

    //new directory creation/init
    unsigned int new_bloque = alocarBloque();
    printf("\t create dbg: new_bloque = %d\n", new_bloque);

    //directory entry creation
    struct Directory_Entry new_dirEntry;

    memcpy(new_dirEntry.nombre, filename, strlen(filename) + 1);
    new_dirEntry.apuntador = new_bloque;
    new_dirEntry.tipo_bloque = ARCHIVO;

    dir.cantidad_elementos++;

    int x;
    for (x = 0; x < CANT_DIR_ENTRIES; x++) {
        if (dir.directory_Entries[x].tipo_bloque == LIBRE){
            dir.directory_Entries[x] = new_dirEntry;
            break;
        }
    }

    //file creation
    struct file_control_block file;
    memset(&file, 0, BLOCK_SIZE);

    time_t tiempo = time(NULL);
    struct passwd *pws = getpwuid(dir.info.uid);

    file.info.tipo_bloque   = ARCHIVO;
    memcpy(file.creador, pws->pw_name, strlen(pws->pw_name) + 1);
    file.info.uid           = dir.info.uid;
    file.info.gid           = dir.info.gid;
    file.info.mode          = S_IFREG | mode;
    file.info.fcreacion     = tiempo;
    file.info.facceso       = tiempo;
    file.info.fmodificacion = tiempo;
    file.lenght             = 0;

    //writing
    write_block(bloque, &dir);

    //writing directory
    write_block(new_bloque, &file);

    printf("\t create dbg: escribiendo super bloque \n");
    superBlock.bloques_libres--;
    write_block(0, &superBlock);

    return 0;
}

/* mkdir - create a directory with the given mode.
 * Errors - path resolution, EACCES, EEXIST
 * Conditions for EACCES and EEXIST are the same as for create.
 */
static int ondisk_mkdir(const char *path, mode_t mode)
{
    char *directoryname = getDirectory(path);
    int bloque = getBlock(directoryname);

    if (bloque <= 0) {
        printf("\t mkdir DUMP: bloque = %d | directoryname = %s\n", bloque, directoryname);
        return -ENOTDIR;
    }

    char *filename = getFilename(path);

    printf("\t mkdir dbg: PATH = %s | Parent Dir = %s | BloqNum = %d | File = %s\n", path, directoryname, bloque, filename);

    //variable declaration/init
    struct Directory dir;
    read_block(bloque, &dir);

    //dir permissions & file name validation
    if ((dir.info.mode & S_IWUSR) == 0) {
        printf("\t mkdir DUMP: Usuario no tiene write permissions en %s!!\n", directoryname);
        return -EACCES;
    }

    if (searchFile(&dir, filename) != -ENOENT) {
        printf("\t mkdir DUMP: %s ya existe en %s!!\n", filename, directoryname);
        return -EEXIST;
    }

    //crear entrada nueva
    if (dir.cantidad_elementos == 63) {
        printf("\t mkdir DUMP: directorio %s lleno!!\n", directoryname);
        return -ENOTEMPTY;
    }

    //new directory creation/init
    unsigned int new_bloque = alocarBloque();
    printf("\t mkdir dbg: new_bloque = %d\n", new_bloque);

    //directory entry creation
    struct Directory_Entry new_dirEntry;

    memcpy(new_dirEntry.nombre, filename, strlen(filename) + 1);
    new_dirEntry.apuntador = new_bloque;
    new_dirEntry.tipo_bloque = DIRECTORIO;

    dir.cantidad_elementos++;

    int x;
    for (x = 0; x < CANT_DIR_ENTRIES; x++) {
        if (dir.directory_Entries[x].tipo_bloque == LIBRE){
            dir.directory_Entries[x] = new_dirEntry;
            break;
        }
    }

    //directory creation
    struct Directory new_dir;// = (struct Directory *) calloc(CANT_DIR_ENTRIES, sizeof(struct Directory_Entry));
    memset(&new_dir, 0, BLOCK_SIZE);

    time_t tiempo = time(NULL);
    struct passwd *pws = getpwuid(dir.info.uid);

    new_dir.info.tipo_bloque   = DIRECTORIO;
    memcpy(new_dir.creador, pws->pw_name, strlen(pws->pw_name) + 1);
    new_dir.info.uid           = dir.info.uid;
    new_dir.info.gid           = dir.info.gid;
    new_dir.info.mode          = S_IFDIR | mode;
    new_dir.info.fcreacion     = tiempo;
    new_dir.info.facceso       = tiempo;
    new_dir.info.fmodificacion = tiempo;
    new_dir.cantidad_elementos = 0;

    //for (x = 0; x < CANT_DIR_ENTRIES; x++)
        //new_dir->directory_Entries[x].tipo_bloque = LIBRE; //verificar que estan LIBRE los dirEntries

    //writing
    write_block(bloque, &dir);

    //writing directory
    write_block(new_bloque, &new_dir);

    printf("\t mkdir dbg: escribiendo super bloque \n");
    superBlock.bloques_libres--;
    write_block(0, &superBlock);

    return 0;
}


void free_indirect_block(unsigned int bloque)
{
    struct indirect_block iblock;
    memset(&iblock, 0, BLOCK_SIZE);

    read_block(bloque, &iblock);

    int x;
    for (x = 0; x < CANT_BLOQUES; x++) {
        if (iblock.bloques[x] == 0)
            break;
        liberarBloque(iblock.bloques[x]);
        superBlock.bloques_libres++;
        iblock.bloques[x] = 0;
    }
}

void free_double_indirect_block(unsigned int bloque)
{
    struct indirect_block iblock;
    memset(&iblock, 0, BLOCK_SIZE);

    read_block(bloque, &iblock);

    int x;
    for (x = 0; x < CANT_BLOQUES; x++) {
        if (iblock.bloques[x] == 0)
            break;
        free_indirect_block(iblock.bloques[x]);
        iblock.bloques[x] = 0;
    }
}

void free_triple_indirect_block(unsigned int bloque)
{
    struct indirect_block iblock;
    memset(&iblock, 0, BLOCK_SIZE);

    read_block(bloque, &iblock);

    int x;
    for (x = 0; x < CANT_BLOQUES; x++) {
        if (iblock.bloques[x] == 0)
            break;
        free_double_indirect_block(iblock.bloques[x]);
        iblock.bloques[x] = 0;
    }
}


/* unlink - delete a file
 *  Errors - path resolution, EACCES, ENOENT, EISDIR
 * Requires user *write* permission to the containing
 *  directory. (permissions on the file itself are ignored)
 */
static int ondisk_unlink(const char *path)
{
    //TODO: hacerlo recursivo para cada bloque de indireccion de datos
    printf("\t unlink dbg: path = %s\n", path);

    char *filename = getFilename(path);
    char *directoryname = getDirectory(path);
    int bloque = getBlock(directoryname);

    if (bloque <= 0) {
        printf("\t unlink DUMP: bloque = %d | directoryname = %s\n", bloque, directoryname);
        return -ENOTDIR;
    }

    printf("\t unlink dbg: filename = %s | directorio = %s \n", filename, directoryname);

    struct Directory dir;
    read_block(bloque, &dir);

    if ((dir.info.mode & S_IWUSR) == 0) {
        printf("\t unlink DUMP: Usuario no tiene write permissions en %s!!\n", directoryname);
        return -EACCES;
    }

    int bloque_liberar = searchFile(&dir, filename);
    if (bloque_liberar == -ENOENT) {
        printf("\t unlink DUMP: %s no existe en %s!!\n", filename, directoryname);
        return -ENOENT;
    }

    int identry;
    for (identry = 0; identry < CANT_DIR_ENTRIES; identry++){
        if (strcmp(filename, dir.directory_Entries[identry].nombre) == 0) {
            dir.directory_Entries[identry].tipo_bloque = LIBRE;
            break;
        }
    }
    dir.cantidad_elementos--;   


    printf("\t unlink dbg: liberando bloques de %s\n", filename);
    struct file_control_block fcb;
    read_block(bloque_liberar, &fcb);

                                                                          //TODO: terminar :P
    int x;
    for (x = 0; x < CANT_BLOQUES_DIRECTOS; x++) {
        if (fcb.bloques_directos[x] == 0)
            break;
        liberarBloque(fcb.bloques_directos[x]);
        superBlock.bloques_libres++;
    }

    free_indirect_block(fcb.bloque_una_indireccion);
    free_double_indirect_block(fcb.bloque_dos_indireccion);
    free_triple_indirect_block(fcb.bloque_tres_indireccion);

    liberarBloque(bloque_liberar);
    superBlock.bloques_libres++;

    printf("\t unlink dbg: excribiendo %s\n", directoryname);
    write_block(bloque, &dir);

    printf("\t unlink dbg: escribiendo super bloque \n");

    write_block(0, &superBlock);

    return 0;
}

int recursive_rmdir(int bloque, char *filename)
{
    read_block(bloque, &buffer);

    if (((struct inodo*)&buffer)->tipo_bloque == DIRECTORIO) {
        int x;
        for (x = 0; x < CANT_DIR_ENTRIES; x++) {
            if (((struct Directory *) &buffer)->directory_Entries[x].tipo_bloque != LIBRE){
                int ret = recursive_rmdir(((struct Directory *) &buffer)->directory_Entries[x].apuntador,
                                          (((struct Directory *) &buffer)->directory_Entries[x].nombre));

                if (ret < 0)
                    return ret;
            }
        }

        printf("\t rmdir dbg: liberando bloque #%d de %s\n", bloque, filename);
        liberarBloque(bloque);
        superBlock.bloques_libres++;
    } else {
        printf("\t rmdir dbg: liberando bloque de %s\n", filename);
        liberarBloque(bloque);
    }

    return 0;
}

/* rmdir - remove a directory
 *  Errors - path resolution, EACCES, ENOENT, ENOTDIR, ENOTEMPTY
 * Requires user *write* permission to the containing
 *  directory. (permissions on the directory itself are ignored)
 */
static int ondisk_rmdir(const char *path)
{
    printf("\t rmdir dbg: path = %s\n", path);

    char *filename = getFilename(path);
    char *directoryname = getDirectory(path);
    int bloque = getBlock(directoryname);

    if (bloque <= 0) {
        printf("\t rmdir DUMP: bloque = %d | directoryname = %s\n", bloque, directoryname);
        return -ENOTDIR;
    }

    printf("\t rmdir dbg: filename = %s | directorio = %s \n", filename, directoryname);

    struct Directory dir;
    read_block(bloque, &dir);

    if ((dir.info.mode & S_IWUSR) == 0) {
        printf("\t rmdir DUMP: Usuario no tiene write permissions en %s!!\n", directoryname);
        return -EACCES;
    }

    int bloque_liberar = searchFile(&dir, filename);
    if (bloque_liberar == -ENOENT) {
        printf("\t rmdir DUMP: %s no existe en %s!!\n", filename, directoryname);
        return -ENOENT;
    }

    int identry;
    for (identry = 0; identry < CANT_DIR_ENTRIES; identry++){
        if (strcmp(filename, dir.directory_Entries[identry].nombre) == 0) {
            dir.directory_Entries[identry].tipo_bloque = LIBRE;
            break;
        }
    }
    dir.cantidad_elementos--;

    printf("\t rmdir dbg: excribiendo %s\n", directoryname);
    write_block(bloque, &dir);

    int rec = recursive_rmdir(bloque_liberar, filename);

    if (rec < 0)
        return rec;

    printf("\t rmdir dbg: escribiendo super bloque \n");
    write_block(0, &superBlock);

    return 0;
}

/* rename - rename a file or directory
 * Errors - path resolution, ENOENT, EACCES, EINVAL, EEXIST
 *
 * ENOENT - source does not exist
 * EEXIST - destination already exists
 * EACCES - no write permission to directory. Permissions
 * EINVAL - source and destination are not in the same directory
 *
 * Note that this is a simplified version of the UNIX rename
 * functionality - see 'man 2 rename' for full semantics. In
 * particular, the full version can move across directories, replace a
 * destination file, and replace an empty directory with a full one.
 */
static int ondisk_rename(const char *src_path, const char *dst_path)
{
    char *filename_src = getFilename(src_path);
    char *directoryname_src = getDirectory(src_path);

    char *filename_dst = getFilename(dst_path);
    char *directoryname_dst = getDirectory(dst_path);

    int bloque = getBlock(directoryname_src);
    int bloque2 = getBlock(directoryname_dst);

    if (bloque <= 0) {
        printf("\t rename DUMP: bloque = %d | SRC_PATH = %s \n", bloque, src_path);
        return bloque;
    }

    if (bloque != bloque2) {
        printf("\t rename DUMP: %s y %s no estan en el mismo directorio!!\n", src_path, dst_path);
        return -EINVAL;
    }

    printf("\t rename dbg: SRC_PATH = %s | DST_PATH = %s \n\t\t SRC_Parent Dir = %s | DST_Parent Dir = %s | BloqNum = %d \n\t\t SRC_File = %s | DST_File = %s\n",
                           src_path, dst_path, directoryname_src, directoryname_dst, bloque, filename_src, filename_dst);

    //variable declaration/init
    struct Directory dir;
    read_block(bloque, &dir);

    //dir permissions & file name validation
    if ((dir.info.mode & S_IWUSR) == 0){
        printf("\t rename DUMP: Usuario no tiene write permissions en %s!!\n", directoryname_src);
        return -EACCES;
    }

    if (searchFile(&dir, filename_dst) != -ENOENT){
        printf("\t rename DUMP: %s ya existe en %s!!\n", filename_dst, directoryname_src);
        return -EEXIST;
    }

    int identry;
    for (identry = 0; identry < CANT_DIR_ENTRIES; identry++){
        if (strcmp(filename_src, dir.directory_Entries[identry].nombre) == 0) {
            strcpy(dir.directory_Entries[identry].nombre, filename_dst);
            break;
        }
    }


    printf("\t rename dbg: writing %s block\n", directoryname_src);
    write_block(bloque, &dir);

    printf("\t rename dbg: %s renamed in %s\n", filename_dst, directoryname_src);
    return 0;
}

/* chmod - change file permissions
 * utime - change access and modification times
 *         (for definition of 'struct utimebuf', see 'man utime')
 *
 * Errors - path resolution, ENOENT.
 *
 * Note that no write permissions to the file/directory or containing
 * are needed - if you can resolve the path, then you can make the
 * change. (normally EACCES is returned if the invoking user does not
 * own the file)
 */
static int ondisk_chmod(const char *path, mode_t mode)
{
    printf("\t chmod dbg: path = %s\n", path);

    char *filename = getFilename(path);
    char *directoryname = getDirectory(path);
    int bloque = getBlock(directoryname);

    if (bloque <= 0) {
        printf("\t chmod DUMP: bloque = %d | directoryname = %s\n", bloque, directoryname);
        return -ENOTDIR;
    }

    printf("\t chmod dbg: filename = %s | directorio = %s \n", filename, directoryname);

    struct Directory dir;
    read_block(bloque, &dir);

    int bloque_modificar = searchFile(&dir, filename);
    if (bloque_modificar == -ENOENT) {
        printf("\t chmod DUMP: %s no existe en %s!!\n", filename, directoryname);
        return -ENOENT;
    }

    int identry, tipo_bloque = 0;
    for (identry = 0; identry < CANT_DIR_ENTRIES; identry++){
        if (strcmp(filename, dir.directory_Entries[identry].nombre) == 0) {
            tipo_bloque = dir.directory_Entries[identry].tipo_bloque;
            break;
        }
    }

    read_block(bloque_modificar, &buffer);

    time_t tiempo = time(NULL);
    if (tipo_bloque == DIRECTORIO) {
        ((struct Directory *) &buffer)->info.mode = mode;
        ((struct Directory *) &buffer)->info.fcreacion = tiempo;
    } else {
        ((struct file_control_block *) &buffer)->info.mode = mode;
        ((struct file_control_block *) &buffer)->info.fcreacion = tiempo;
    }

    write_block(bloque_modificar, &buffer);

    return 0;
}

int ondisk_utime(const char *path, struct utimbuf *ut)
{
    printf("\t utime dbg: path = %s\n", path);

    char *filename = getFilename(path);
    char *directoryname = getDirectory(path);
    int bloque = getBlock(directoryname);

    if (bloque <= 0) {
        printf("\t utime DUMP: bloque = %d | directoryname = %s\n", bloque, directoryname);
        return -ENOTDIR;
    }

    printf("\t utime dbg: filename = %s | directorio = %s \n", filename, directoryname);

    struct Directory dir;
    read_block(bloque, &dir);

    int bloque_modificar = searchFile(&dir, filename);
    if (bloque_modificar == -ENOENT) {
        printf("\t utime DUMP: %s no existe en %s!!\n", filename, directoryname);
        return -ENOENT;
    }

    int identry, tipo_bloque = 0;
    for (identry = 0; identry < CANT_DIR_ENTRIES; identry++){
        if (strcmp(filename, dir.directory_Entries[identry].nombre) == 0) {
            tipo_bloque = dir.directory_Entries[identry].tipo_bloque;
            break;
        }
    }

    read_block(bloque_modificar, &buffer);

    time_t tiempo = time(NULL);
    if (tipo_bloque == DIRECTORIO) {
        ((struct Directory *) &buffer)->info.facceso = ut->actime;
        ((struct Directory *) &buffer)->info.fmodificacion = ut->modtime;
        ((struct Directory *) &buffer)->info.fcreacion = tiempo;
    } else {
        ((struct file_control_block *) &buffer)->info.facceso = ut->actime;
        ((struct file_control_block *) &buffer)->info.fmodificacion = ut->modtime;
        ((struct file_control_block *) &buffer)->info.fcreacion = tiempo;
    }

    write_block(bloque_modificar, &buffer);

    return 0;
}

/* truncate - truncate file to exactly 'len' bytes
 * Errors - path resolution, EACCES, ENOENT, EISDIR
 * EACCES - requires write permission to the file itself.
 */
static int ondisk_truncate(const char *path, off_t len)
{
    /* you can cheat by only implementing this for the case of len==0,
     * and an error otherwise.
     */
    if (len != 0)
        return -EINVAL;		/* invalid argument */

    printf("\t truncate dbg: path = %s\n", path);

    char *filename = getFilename(path);
    char *directoryname = getDirectory(path);
    int bloque = getBlock(directoryname);

    if (bloque <= 0) {
        printf("\t truncate DUMP: bloque = %d | directoryname = %s\n", bloque, directoryname);
        return -ENOENT;
    }

    printf("\t truncate dbg: filename = %s | directorio = %s \n", filename, directoryname);

    struct Directory dir;
    read_block(bloque, &dir);

    int bloque_modificar = searchFile(&dir, filename);
    if (bloque_modificar == -ENOENT) {
        printf("\t truncate DUMP: %s no existe en %s!!\n", filename, directoryname);
        return -ENOENT;
    }

    int identry, tipo_bloque = 0;
    for (identry = 0; identry < CANT_DIR_ENTRIES; identry++) {
        if (strcmp(filename, dir.directory_Entries[identry].nombre) == 0) {
            tipo_bloque = dir.directory_Entries[identry].tipo_bloque;
            break;
        }
    }

    if (tipo_bloque != ARCHIVO) {
        printf("\t truncate DUMP: %s no es archivo!!\n", filename);
        return -EISDIR;
    }

    struct file_control_block file;
    read_block(bloque_modificar, &file);

    //logica
    //bloques directos
    printf("\t truncate dbg: liberando bloques directos\n");
    int x;
    for (x = 0; x < CANT_BLOQUES_DIRECTOS; x++){
        if (file.bloques_directos[x] == 0)
            break;

        liberarBloque(file.bloques_directos[x]);
        superBlock.bloques_libres++;
        file.bloques_directos[x] = 0;
    }

    //bloques indirectos 1er nivel
    printf("\t truncate dbg: liberando bloques de 1 nivel de indireccion\n");
    free_indirect_block(file.bloque_una_indireccion);
    file.bloque_una_indireccion = 0;

    //bloques indirectos 2do nivel
    printf("\t truncate dbg: liberando bloques de 2 niveles de indireccion\n");
    free_double_indirect_block(file.bloque_dos_indireccion);
    file.bloque_dos_indireccion = 0;

    //bloques indirectos 3er nivel
    printf("\t truncate dbg: liberando bloques de 3 niveles de indireccion\n");
    free_triple_indirect_block(file.bloque_tres_indireccion);
    file.bloque_tres_indireccion = 0;

    file.lenght = 0;

    write_block(bloque_modificar, &file);

    write_block(0, &superBlock);

    return 0;
}

int getTipoDirEntry(struct Directory *dir, char *filename) {
    int identry, tipo_bloque = 0;
    for (identry = 0; identry < CANT_DIR_ENTRIES; identry++) {
        if (strcmp(filename, dir->directory_Entries[identry].nombre) == 0) {
            tipo_bloque = dir->directory_Entries[identry].tipo_bloque;
            break;
        }
    }

    return tipo_bloque;
}


int read_DirectBlocks(struct file_control_block *file, off_t offset,
                     char *buff, size_t *size)
{
    int readBytes = 0;
    int bloque_pivote = offset / BLOCK_SIZE;
    off_t offset_interno = offset % BLOCK_SIZE;

    printf("\t read_DirectBlocks dbg: bloque_pivote = %d | offset_interno = %lld\n", bloque_pivote, offset_interno);

    while ((*size > 0) && (bloque_pivote < CANT_BLOQUES_DIRECTOS)) {
        printf("\t read_DirectBlocks dbg: size = %d | pivote = %d | bloque[pivote] = %d | readBytes = %d\n", *size, bloque_pivote,file->bloques_directos[bloque_pivote], readBytes);
        if (file->bloques_directos[bloque_pivote] == 0)
            return readBytes;

        read_block(file->bloques_directos[bloque_pivote++], &buffer);
        //printf("\t read_DirectBlocks dbg: buffer = %s\n", buffer);

        if (*size >= BLOCK_SIZE) {
            memcpy(buff + readBytes, buffer, BLOCK_SIZE);
            readBytes += BLOCK_SIZE;
            *size -= BLOCK_SIZE;
        } else {
            memcpy(buff + readBytes, buffer, *size);
            readBytes += *size;
            *size = 0;
        }
        printf("\t read_DirectBlocks dbg: al fin de while size = %d\n", *size);
    }

    return readBytes;
}

int read_single_IndirectBlock(unsigned int bloque, off_t offset,
                     char *buff, size_t *size, int buff_offset)
{
    struct indirect_block indirectBlock;
    memset(&indirectBlock, 0, BLOCK_SIZE);

    read_block(bloque, &indirectBlock);

    int readBytes = 0;
    int bloque_pivote = offset / BLOCK_SIZE;
    off_t offset_interno = offset % BLOCK_SIZE;

    printf("\t read_single_IndirectBlock dbg: bloque_pivote = %d | offset = %d | offset_interno = %lld\n", bloque_pivote, offset, offset_interno);

    while (*size > 0 && (bloque_pivote < CANT_BLOQUES)) {
        printf("\t read_single_IndirectBlock dbg: size = %d | pivote = %d | bloque[pivote] = %d | readBytes = %d\n", *size, bloque_pivote,indirectBlock.bloques[bloque_pivote], readBytes);
        if (indirectBlock.bloques[bloque_pivote] == 0)
            return readBytes;

        read_block(indirectBlock.bloques[bloque_pivote++], &buffer);

        if (*size >= BLOCK_SIZE) {
            memcpy(buff + buff_offset + readBytes, buffer, BLOCK_SIZE);
            readBytes += BLOCK_SIZE;
            *size -= BLOCK_SIZE;
        } else {
            memcpy(buff + buff_offset + readBytes, buffer, *size);
            readBytes += *size;
            *size = 0;
        }
        printf("\t read_single_IndirectBlock dbg: al fin de while size = %d\n", *size);
    }

    return readBytes;
}

int read_double_IndirectBlock(unsigned int bloque, off_t offset,
                            char *buff, size_t *size)
{
    struct indirect_block indirectBlock;
    memset(&indirectBlock, 0, BLOCK_SIZE);

    read_block(bloque, &indirectBlock);

    int readBytes = 0;
    int bloque_pivote = offset / BLOCK_SIZE;
    off_t offset_interno = offset % (BLOCK_SIZE*CANT_BLOQUES);

    printf("\t read_double_IndirectBlock dbg: bloque_pivote = %d || offset_interno = %lld\n", bloque_pivote, offset_interno);

    while (*size >= 0 && (bloque_pivote < CANT_BLOQUES)) {
        if (indirectBlock.bloques[bloque_pivote] == 0)
            return readBytes;
        readBytes += read_single_IndirectBlock(indirectBlock.bloques[bloque_pivote++], offset_interno, buff, size, readBytes);
    }

    return readBytes;
}

int read_triple_IndirectBlock(unsigned int bloque, off_t offset,
                            char *buff, size_t *size)
{
    struct indirect_block indirectBlock;
    memset(&indirectBlock, 0, BLOCK_SIZE);

    read_block(bloque, &indirectBlock);

    int readBytes = 0;
    int bloque_pivote = offset / BLOCK_SIZE;
    off_t offset_interno = offset % (BLOCK_SIZE * CANT_BLOQUES * CANT_BLOQUES);

    printf("\t read_double_IndirectBlock dbg: bloque_pivote = %d || offset_interno = %lld\n", bloque_pivote, offset_interno);

    while (*size >= 0 && (bloque < CANT_BLOQUES)) {
        if (indirectBlock.bloques == 0)
            return readBytes;

        readBytes += read_double_IndirectBlock(indirectBlock.bloques[bloque_pivote++], offset_interno, buff, size);
    }

    return readBytes;
}

/* read - read data from an open file.
 * should return exactly the number of bytes requested, except:
 *   - if offset >= size, return 0
 *   - on error, return < 0
 * Errors - path resolution, ENOENT, EISDIR, EACCES
 * EACCES - requires read permission to the file itself
 */
static int ondisk_read(const char *path, char *buff, size_t size,
                       off_t offset, struct fuse_file_info *fi)
{
    printf("\n\n");
    printf("\t read dbg: offset =  %lld | size = %d\n", offset, size);

    char *filename = getFilename(path);
    char *directoryname = getDirectory(path);
    int bloque = getBlock(directoryname);

    if (bloque <= 0) {
        printf("\t read DUMP: bloque = %d | directoryname = %s\n", bloque, directoryname);
        return -ENOENT;
    }

    printf("\t read dbg: filename = %s | directorio = %s \n", filename, directoryname);

    struct Directory dir;
    read_block(bloque, &dir);

    int bloque_modificar = searchFile(&dir, filename);
    if (bloque_modificar == -ENOENT) {
        printf("\t read DUMP: %s no existe en %s!!\n", filename, directoryname);
        return -ENOENT;
    }

    if (getTipoDirEntry(&dir, filename) != ARCHIVO) {
        printf("\t read DUMP: %s no es archivo!!\n", filename);
        return -EISDIR;
    }

    struct file_control_block file;
    read_block(bloque_modificar, &file);

    if ((file.info.mode & S_IRUSR) == 0) {
        printf("\t read DUMP: Usuario no tiene permisos de lectura en %s!!\n", filename);
        return -EACCES;
    }

    int readBytes = 0;

    if (offset < MAX_RANGE_FCB_DIRECT_BLOCKS) {
        printf("\t read dbg: offset dentro de rango de Bloques Directos!!\n");

        readBytes += read_DirectBlocks(&file, offset, buff, &size);
        printf("\t read dbg: readBytes = %d | size = %d\n", readBytes, size);

        if (size > 0) {
            readBytes += read_single_IndirectBlock(file.bloque_una_indireccion, 0, buff, &size, readBytes);
            printf("\t read dbg: readBytes = %d | size = %d\n", readBytes, size);

            if (size > 0) {
                readBytes += read_double_IndirectBlock(file.bloque_dos_indireccion, 0, buff, &size);
                printf("\t read dbg: readBytes = %d | size = %d\n", readBytes, size);

                if (size >0) {
                    readBytes += read_triple_IndirectBlock(file.bloque_tres_indireccion, 0, buff, &size);
                    printf("\t read dbg: readBytes = %d | size = %d\n", readBytes, size);
                }
            }
        }
    } else if (offset < MAX_RANGE_1LVL_INDIRECT_BLOCK) {
        printf("\t read dbg: offset dentro de rango de Bloque Indirecto!!\n");
        offset -= MAX_RANGE_FCB_DIRECT_BLOCKS;

        readBytes += read_single_IndirectBlock(file.bloque_una_indireccion, offset, buff, &size, readBytes);

        if (size > 0) {
            readBytes += read_double_IndirectBlock(file.bloque_dos_indireccion, 0, buff, &size);
            printf("\t read dbg: readBytes = %d | size = %d\n", readBytes, size);

            if (size > 0) {
                readBytes += read_triple_IndirectBlock(file.bloque_tres_indireccion, 0, buff, &size);
                printf("\t read dbg: readBytes = %d | size = %d\n", readBytes, size);
            }
        }
    } else if (offset < MAX_RANGE_2LVL_INDIRECT_BLOCK) {
        printf("\t read dbg: offset dentro de rango de Bloque Doble Indirecto!!\n");
        offset -= MAX_RANGE_1LVL_INDIRECT_BLOCK;

        readBytes += read_double_IndirectBlock(file.bloque_tres_indireccion, offset, buff, &size);

        if (size > 0) {
            readBytes += read_triple_IndirectBlock(file.bloque_tres_indireccion, 0, buff, &size);
            printf("\t read dbg: readBytes = %d | size = %d\n", readBytes, size);
        }

    } else if (offset < MAX_RANGE_3LVL_INDIRECT_BLOCK) {
        printf("\t read dbg: offset dentro de rango de Bloque Triple Indirecto!!\n");
        offset -= MAX_RANGE_2LVL_INDIRECT_BLOCK;

        readBytes += read_triple_IndirectBlock(file.bloque_tres_indireccion, offset, buff, &size);
        printf("\t read dbg: readBytes = %d | size = %d\n", readBytes, size);
    }

    return readBytes;
}


int write_DirectBlocks(struct file_control_block *file, off_t offset,
                       const char *buff, size_t *size)
{    
    int writtenBytes = 0;
    int bloque_pivote = offset / BLOCK_SIZE;
    off_t offset_interno = offset % BLOCK_SIZE;

    printf("\t write_DirectBlocks dbg: bloque_pivote = %d || offset_interno = %lld\n", bloque_pivote, offset_interno);

    while (*size > 0 && (bloque_pivote < CANT_BLOQUES_DIRECTOS)) {
        printf("\t write_DirectBlocks dbg: size = %d | pivote = %d | bloque[pivote] = %d | readBytes = %d\n", *size, bloque_pivote,file->bloques_directos[bloque_pivote], writtenBytes);
        if (file->bloques_directos[bloque_pivote] == 0) {
            file->bloques_directos[bloque_pivote] = alocarBloque();
            memset(&buffer, 0, BLOCK_SIZE);
            printf("\t write_DirectBlocks dbg: alocarBloque() = %d\n", file->bloques_directos[bloque_pivote]);
        } else
            read_block(file->bloques_directos[bloque_pivote], &buffer);

        if (*size >= BLOCK_SIZE) {
            memcpy(buffer, buff + writtenBytes, BLOCK_SIZE);
            writtenBytes += BLOCK_SIZE;
            *size -= BLOCK_SIZE;

        } else {
            memcpy(buffer, buff + writtenBytes, *size);
            writtenBytes += *size;
            *size = 0;
        }
        printf("\t write_DirectBlocks dbg: al fin de while size = %d\n", *size);
        write_block(file->bloques_directos[bloque_pivote++], &buffer);
    }

    file->lenght += writtenBytes;

    return writtenBytes;
}

int write_single_IndirectBlock(unsigned int bloque, off_t offset,
                               const char *buff, size_t *size)
{
    struct indirect_block ind_block;
    memset(&ind_block, 0, BLOCK_SIZE);

    read_block(bloque, &ind_block);

    int writtenBytes = 0;
    int bloque_pivote = offset / BLOCK_SIZE;
    off_t offset_interno = offset % BLOCK_SIZE;

    printf("\t write_single_IndirectBlock dbg: bloque_pivote = %d | offset = %lld | offset_interno = %lld\n", bloque_pivote, offset, offset_interno);

    bool extended = false;

    while (*size > 0 && (bloque_pivote < CANT_BLOQUES)) {
        printf("\t write_single_IndirectBlock dbg: size = %d | pivote = %d | bloque[pivote] = %d | readBytes = %d\n", *size, bloque_pivote, ind_block.bloques[bloque_pivote], writtenBytes);
        if (ind_block.bloques[bloque_pivote] == 0) {
            ind_block.bloques[bloque_pivote] = alocarBloque();
            memset(&buffer, 0, BLOCK_SIZE);

            printf("\t write_single_IndirectBlock dbg: alocarBloque() = %d\n", ind_block.bloques[bloque_pivote]);

            if (!extended)
                extended = true;
        } else
            read_block(ind_block.bloques[bloque_pivote], &buffer);

        if (*size >= BLOCK_SIZE) {
            memcpy(buffer, buff + writtenBytes, BLOCK_SIZE);
            writtenBytes += BLOCK_SIZE;
            *size -= BLOCK_SIZE;
        } else {
            memcpy(buffer, buff + writtenBytes, *size);
            writtenBytes += *size;
            *size = 0;
        }
        printf("\t write_single_IndirectBlock dbg: al fin de while size = %d\n", *size);
        write_block(ind_block.bloques[bloque_pivote++], &buffer);
    }    

    if (extended)
        write_block(bloque, &ind_block);


    return writtenBytes;
}

int write_double_IndirectBlock(unsigned int bloque, off_t offset,
                               const char *buff, size_t *size)
{
    struct indirect_block double_ind_block;
    memset(&double_ind_block, 0, BLOCK_SIZE);

    read_block(bloque, &double_ind_block);

    int writtenBytes = 0;
    int bloque_pivote = offset / BLOCK_SIZE;
    off_t offset_interno = offset % BLOCK_SIZE;

    printf("\t write_double_IndirectBlock dbg: bloque_pivote = %d || offset_interno = %lld\n", bloque_pivote, offset_interno);

    bool extended = false;

    while (*size > 0 && (bloque_pivote < CANT_BLOQUES)) {
        if (double_ind_block.bloques[bloque_pivote] == 0) {
            double_ind_block.bloques[bloque_pivote] = alocarBloque();

            if (!extended)
                extended = true;
        }

        writtenBytes += write_single_IndirectBlock(double_ind_block.bloques[bloque_pivote++], offset_interno, buff, size);
    }

    if (extended)
        write_block(bloque, &double_ind_block);

    return writtenBytes;
}

int write_triple_IndirectBlock(unsigned int bloque, off_t offset,
                               const char *buff, size_t *size)
{
    struct indirect_block triple_ind_block;
    memset(&triple_ind_block, 0, BLOCK_SIZE);

    read_block(bloque, &triple_ind_block);

    int writtenBytes = 0;
    int bloque_pivote = offset / BLOCK_SIZE;
    off_t offset_interno = offset % BLOCK_SIZE;

    printf("\t write_triple_IndirectBlock dbg: bloque_pivote = %d || offset_interno = %lld\n", bloque_pivote, offset_interno);

    bool extended = false;

    while (*size > 0 && (bloque_pivote < CANT_BLOQUES)) {
        if (triple_ind_block.bloques[bloque_pivote] == 0) {
            triple_ind_block.bloques[bloque_pivote] = alocarBloque();

            if (!extended)
                extended = true;
        }

        writtenBytes += write_double_IndirectBlock(triple_ind_block.bloques[bloque_pivote++], offset_interno, buff, size);
    }

    if (extended)
        write_block(bloque, &triple_ind_block);

    return writtenBytes;
}

/* write - write data to a file
 * It should return exactly the number of bytes requested, except on
 * error.
 * Errors - path resolution, ENOENT, EISDIR, EACCES
 * EACCES - requires write permission to the file itself
 */
static int ondisk_write(const char *path, const char *buff, size_t size,
                     off_t offset, struct fuse_file_info *fi)
{
    printf("\n\n");
    printf("\t write dbg: offset =  %lld | size = %d\n", offset, size);

    char *filename = getFilename(path);
    char *directoryname = getDirectory(path);
    int bloque = getBlock(directoryname);

    if (bloque <= 0) {
        printf("\t write DUMP: bloque = %d | directoryname = %s\n", bloque, directoryname);
        return -ENOENT;
    }

    printf("\t write dbg: filename = %s | directorio = %s | bloque = %d\n", filename, directoryname, bloque);

    struct Directory dir;
    read_block(bloque, &dir);

    int bloque_modificar = searchFile(&dir, filename);
    if (bloque_modificar == -ENOENT) {
        printf("\t write DUMP: %s no existe en %s!!\n", filename, directoryname);
        return -ENOENT;
    }

    if (getTipoDirEntry(&dir, filename) != ARCHIVO) {
        printf("\t write DUMP: %s no es archivo!!\n", filename);
        return -EISDIR;
    }

    struct file_control_block file;
    read_block(bloque_modificar, &file);

    if ((file.info.mode & S_IRUSR) == 0) {
        printf("\t write DUMP: Usuario no tiene permisos de lectura en %s!!\n", filename);
        return -EACCES;
    }

    int writtenBytes = 0;

    if (offset < MAX_RANGE_FCB_DIRECT_BLOCKS) {
        printf("\t write dbg: offset dentro de rango de Bloques Directos!!\n");

        writtenBytes += write_DirectBlocks(&file, offset, buff, &size);
        printf("\t write dbg: bloques escritos %d!!\n", writtenBytes);        

        /*if (size > 0) {
            writtenBytes += write_single_IndirectBlock(file.bloque_una_indireccion, 0, buff, &size);
            printf("\t write dbg: bloques escritos %d!!\n", writtenBytes);

            if (size > 0) {
                writtenBytes += write_double_IndirectBlock(file.bloque_dos_indireccion, 0, buff, &size);
                printf("\t write dbg: bloques escritos %d!!\n", writtenBytes);

                if (size > 0) {
                    writtenBytes += write_triple_IndirectBlock(file.bloque_tres_indireccion, 0, buff, &size);
                    printf("\t write dbg: bloques escritos %d!!\n", writtenBytes);
                }
            }
        }*/
    } else if (offset < MAX_RANGE_1LVL_INDIRECT_BLOCK) {
        printf("\t write dbg: offset dentro de rango de Bloque Indirecto!!\n");
        printf("\t write dbg: file.bloque_una_indireccion = %d\n", file.bloque_una_indireccion);

        if (file.bloque_una_indireccion == 0) {
            file.bloque_una_indireccion = alocarBloque();
            printf("\t write dbg: alocarBloque() = %d\n", file.bloque_una_indireccion);
        }

        offset -= MAX_RANGE_FCB_DIRECT_BLOCKS;

        writtenBytes += write_single_IndirectBlock(file.bloque_una_indireccion, offset, buff, &size);
        file.lenght += writtenBytes;

        /*if (size > 0) {
            writtenBytes += write_double_IndirectBlock(file.bloque_dos_indireccion, 0, buff, &size);

            if (size > 0)
                writtenBytes += write_triple_IndirectBlock(file.bloque_tres_indireccion, 0, buff, &size);
        }*/
    } else if (offset < MAX_RANGE_2LVL_INDIRECT_BLOCK) {
        printf("\t write dbg: offset dentro de rango de Bloque Doble Indirecto!!\n");
        printf("\t write dbg: file.bloque_dos_indireccion = %d\n", file.bloque_dos_indireccion);

        if (file.bloque_dos_indireccion == 0) {
            file.bloque_dos_indireccion = alocarBloque();
            printf("\t write dbg: alocarBloque() = %d\n", file.bloque_dos_indireccion);
        }

        offset -= MAX_RANGE_1LVL_INDIRECT_BLOCK;

        writtenBytes += write_double_IndirectBlock(file.bloque_dos_indireccion, offset, buff, &size);

        /*if (size > 0)
            writtenBytes += write_triple_IndirectBlock(file.bloque_dos_indireccion, 0, buff, &size);*/

    } else if (offset < MAX_RANGE_3LVL_INDIRECT_BLOCK) {
        printf("\t write dbg: offset dentro de rango de Bloque Triple Indirecto!!\n");        
        printf("\t write dbg: file.bloque_dotres_indireccion = %d\n", file.bloque_tres_indireccion);

        if (file.bloque_tres_indireccion == 0) {
            file.bloque_tres_indireccion = alocarBloque();
            printf("\t write dbg: alocarBloque() = %d\n", file.bloque_tres_indireccion);
        }

        offset -= MAX_RANGE_2LVL_INDIRECT_BLOCK;

        writtenBytes += write_triple_IndirectBlock(file.bloque_tres_indireccion, offset, buff, &size);
    }

    write_block(bloque_modificar, &file);

    return writtenBytes;
}

/* statfs - get file system statistics
 * see 'man 2 statfs' for description of 'struct statvfs'.
 * Errors - none. Needs to work.
 */
static int ondisk_statfs(const char *path, struct statvfs *st)
{
    /* needs to return the following fields (set others to zero):
     *   f_bsize = BLOCK_SIZE
     *   f_blocks = total image - (superBlock + FAT)
     *   f_bfree = f_blocks - blocks used
     *   f_bavail = f_bfree
     *   f_namelen = <whatever your max namelength is>
     *
     * it's OK to calculate this dynamically on the rare occasions
     * when this function is called.
     */
    st->f_bsize = BLOCK_SIZE;
    st->f_blocks = superBlock.total_bloques - (superBlock.sizebloques_mapabits - superBlock.primerbloque_mapabits) - 1;
    st->f_bfree = superBlock.bloques_libres;
    st->f_bavail = st->f_bfree;
    st->f_namemax = MAX_NAME;

    return 0;
}

/* operations vector. Please don't rename it, as the skeleton code in
 * misc.c assumes it is named 'hw4_ops'.
 */
struct fuse_operations hw4_ops = {
    .init = ondisk_init,
    .getattr = ondisk_getattr,
    .readdir = ondisk_readdir,
    .create = ondisk_create,
    .mkdir = ondisk_mkdir,
    .unlink = ondisk_unlink,
    .rmdir = ondisk_rmdir,
    .rename = ondisk_rename,
    .chmod = ondisk_chmod,
    .utime = ondisk_utime,
    .truncate = ondisk_truncate,
    .read = ondisk_read,
    .write = ondisk_write,
    .statfs = ondisk_statfs,
};

int need_img = 1;
