#include "filesystem.h"


/* write_block, read_block:
 *  these are the functions you will use to access the disk
 *  image - *do not* access the disk image file directly.
 *
 *  These functions read or write a single 1024-byte block at a time,
 *  and take a logical block address. (i.e. block 0 refers to bytes
 *  0-1023, block 1 is bytes 1024-2047, etc.)
 *
 *  Make sure you pass a valid block address (i.e. less than the
 *  'fs_size' field in the superblock)
 */
extern int write_block(int lba, void *buf);
extern int read_block(int lba, void *buf);


/*--------------------------------------------------------Bitmap----------------------------------------------------------*/

unsigned int alocarBloque()
{
    unsigned int size_bloques = superBlock.size_bloques;
    unsigned int inicio_bitmap = superBlock.primerbloque_mapabits;
    unsigned int size_bitmap = superBlock.sizebloques_mapabits;

    unsigned int i;
    for(i = inicio_bitmap; i < (inicio_bitmap + size_bitmap); i++){
        read_block(i,buffer);

        unsigned int j;
        for(j = 0; i < SIZE_BLOCK; j++){
            if (buffer[j] != 0){

                unsigned x;
                for(x = 0; x < 7; x++){
                    if (buffer[j] & (1 << x)){
                        buffer[j] &= ~(1 << x);
                        write_block(i, buffer);

                        return  (i * size_bloques * 8) + (j * 8) + x;
                    }
                }
            }
        }
    }
    return 0; // bloque 0 es utilizado por el super_bloque pro ende es imposible utilizarlo y es considerado cmomo error
}

void liberarBloque(unsigned int bloque)
{
    unsigned int size_bloques = superBlock.sizebloques_mapabits;
    unsigned int size_bitmap = superBlock.sizebloques_mapabits;

    unsigned int queBloque = (bloque / (8 * size_bitmap));
    unsigned int tmp = (queBloque % (8 * size_bloques));
    unsigned int queByte = tmp / 8;
    unsigned int queBit = tmp % 8;

    read_block(queBloque, buffer);
    buffer[queByte] |= (1 << queBit);
    write_block(queBloque, buffer);
}


/*--------------------------------------------------------Filesystem----------------------------------------------------------*/

int searchFile(struct directorio *dir, char *file_name){
    //struct directorio dir;
    //read_block(num_bloque, &dir);

    int identry;
    for (identry = 0; identry < CANT_DIR_ENTRIES; identry++){
        struct entrada_directorio dirEntry = dir.directory_Entries[identry];

        if (strcmp(file_name, dirEntry.nombre) == 0)
            return dirEntry.apuntador;
    }

    return -ENOENT;
}

int getBlock(const char *path){
    char path2[sizeof(path)];
    strcpy(path2,path);

    int bloque = superBlock.bloque_root;

    char *subPath = strtok(path2, "/");
    while (subPath != NULL){
        struct directorio dir;      //para que strtok no genere SIGSEGV!!! WTF!
        read_block(bloque, &dir);

        bloque = searchFile(&dir, subPath);

        if (bloque == -ENOENT)
            return -ENOENT;

        subPath = strtok(NULL, "/");
    }

    return bloque;
}


char *getFilename(const char *path){
    char *temp = NULL;

    int x;
    for (x = strlen(path) - 2; x >= 0; x--){
        if (path[x] == '/'){
            temp = path + x + 1;
            break;
        }
    }

    return temp;
}

char *getDirectory(const char *path){
    char *temp = NULL;

    int x;
    for (x = strlen(path) - 2; x >= 0; x--){
        if (path[x] == '/'){
            temp = (char *) malloc(x + 1);
            memcpy(temp, path, x + 1);
            break;
        }
    }

    return temp;
}

int setStat(struct stat* sb, struct directorio *entry)
{
    sb->st_dev=101;
    sb->st_ino=0;
    sb->st_rdev=0;
    sb->st_blocks=0;
    sb->st_nlink=1;
    sb->st_uid = entry->uid;
    sb->st_gid = entry->gid;
    sb->st_size = 0;                                                           //TODO
    sb->st_mtime=entry->mtime;
    sb->st_atime = entry->mtime;
    sb->st_ctime = entry->mtime;
    sb->st_mode = entry->mode | (entry->isDir ? S_IFDIR : S_IFREG);
    return 0;
}


/*----------------------------------------------------------FUSE--------------------------------------------------------*/

/* init - called once at startup
 * This might be a good place to read in the super-block and set up
 * any global variables you need. You don't need to worry about the
 * argument or the return value.
 */
void* ondisk_init(struct fuse_conn_info *conn)
{
    read_block(0, &buffer);
    memcpy(&superBlock, buffer, sizeof(super_bloque));
    return NULL;
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
static int ondisk_getattr(const char *path, struct stat *sb)
{
    struct directorio dir;
    int i = getBlock(path);
    read_block(i, &dir);

    setStat(sb, &dir);

    return i;
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

    struct directorio dir;
    unsigned int bloque = getBlock(path);

    if (bloque >= 0){
        read_block(bloque, &dir);
        setStat(&sb, &dir);
    } else
        return -ENOTDIR;

    if (dir.tipo_bloque != DIRECTORIO)
        return -ENOTDIR;

    int x;
    for (x = 0; x < CANT_DIR_ENTRIES; x++){
        if (dir.directory_Entries[x].tipo_bloque == LIBRE)
            continue;
        else {
            read_block(x,buffer);
            setStat(&sb, &buffer);
            name = dir.directory_Entries[x].nombre;
            filler(buf, name, &sb, 0);
        }
    }
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
    return -EOPNOTSUPP;
}

/* mkdir - create a directory with the given mode.
 * Errors - path resolution, EACCES, EEXIST
 * Conditions for EACCES and EEXIST are the same as for create.
 */
static int ondisk_mkdir(const char *path, mode_t mode)
{
    char *filename = getFilename(path);
    char *directoryname = getDirectory(path);

    int bloque = getBlock(directoryname);

    if (bloque == -ENOENT)
        return -ENOENT;

    //variable declaration/init
    struct directorio dir;
    read_block(bloque, &dir);

    //dir permissions & file name validation
    if ((dir.mode & S_IWUSR) == 0)
        return -EACCES;

    if (searchFile(&dir, filename) != -ENOENT)
        return -EEXIST;

    //crear entrada nueva
    if (dir.cantidad_elementos == 63)
        return -ENOTEMPTY;//ENOTEMPTY ("directory not empty = FULL") si no hay espacio para mas dirEntries

    //new directory creation/init
    unsigned int new_bloque = alocarBloque();

    //directory entry creation
    struct entrada_directorio new_dirEntry;

    new_dirEntry.nombre = filename;
    new_dirEntry.apuntador = new_bloque;
    new_dirEntry.tipo_bloque = DIRECTORIO;

    dir.cantidad_elementos++;

    int x;
    for (x = 0; x < CANT_DIR_ENTRIES; x++){
        if (dir.directory_Entries[x].tipo_bloque == LIBRE){
            dir.directory_Entries[x] = new_dirEntry;
            break;
        }
    }

    //directory creation
    struct directorio *new_dir = (struct directorio *) calloc(CANT_DIR_ENTRIES, sizeof(struct entrada_directorio));

    new_dir->tipo_bloque = DIRECTORIO;
    new_dir->uid = 0;
    new_dir->gid = 0;
    new_dir->mode = dir.mode;
    new_dir->fcreacion = 0;      //TODO get dates
    new_dir->fmodificacion = 0;
    new_dir->cantidad_elementos = 0;
    memcpy(creador, "YO");      //TODO get username

    for (x = 0; x < CANT_DIR_ENTRIES; x++){
        new_dir->directory_Entries[x].tipo_bloque = LIBRE; //verificar que estan LIBRE los dirEntries
    }   

    //writing
    write_block(bloque, &dir);

    //writing directory
    write_block(new_bloque, new_dir);

    return NULL;
}

/* unlink - delete a file
 *  Errors - path resolution, EACCES, ENOENT, EISDIR
 * Requires user *write* permission to the containing
 *  directory. (permissions on the file itself are ignored)
 */
static int ondisk_unlink(const char *path)
{
    char *filename = getFilename(path);
    char *directory = getDirectory(path);

    int bloque = getBlock(directory);

    if (bloque == -ENOENT)
        return -ENOENT;

    //variable declaration/init
    struct directorio dir;
    read_block(bloque, &dir);

    //dir permissions & file name validation
    if ((dir.mode & S_IWUSR) == 0)
        return -EACCES;

    if (searchFile(&dir, filename) == -ENOENT)
        return -ENOENT;

    //delete file or directory, modify element_count & delete dirEntry
    int x, ptrBlock = 0;
    for (x = 0; x < CANT_DIR_ENTRIES; x++){
        if (strcmp(dir.directory_Entries[x].nombre, filename) == 0){

            switch (dir.directory_Entries[x].tipo_bloque){
                case LIBRE:         return NULL;//nada mas que hacer
                case DIRECTORIO:    return -EISDIR;//Es directrori! Deberia ser con rmdir!!
                case ARCHIVO:       dir.directory_Entries[x].tipo_bloque == LIBRE;
                                    ptrBlock = dir.directory_Entries[x].apuntador;
                                    break;
                default:            return -EOPNOTSUPP;
            }
            break;
        }
    }

    dir.cantidad_elementos--;

    //liberar fcb de archivo
    struct file_control_block fcb;
    read_block(ptrBlock, &fcb);
    fcb.tipo_bloque = LIBRE;
    fcb.lenght = 0;
    write_block(bloque, &fcb);

    //write container directory
    write_block(bloque, &dir);

    return NULL;
}

/* rmdir - remove a directory
 *  Errors - path resolution, EACCES, ENOENT, ENOTDIR, ENOTEMPTY
 * Requires user *write* permission to the containing
 *  directory. (permissions on the directory itself are ignored)
 */
static int ondisk_rmdir(const char *path)
{   
    //TODO: no estoy seguro si es necesario hacerlo recursivo..??
    char *filename = getFilename(path);
    char *directory = getDirectory(path);

    int bloque = getBlock(directory);

    if (bloque == -ENOENT)
        return -ENOENT;

    //variable declaration/init
    struct directorio dir;
    read_block(bloque, &dir);

    //dir permissions & file name validation
    if ((dir.mode & S_IWUSR) == 0)
        return -EACCES;

    if (searchFile(&dir, filename) == -ENOENT)
        return -ENOENT;

    //delete directory, modify element_count & delete dirEntry
    int x;
    for (x = 0; x < CANT_DIR_ENTRIES; x++){
        if (strcmp(dir.directory_Entries[x].nombre, filename) == 0){
            dir.directory_Entries[x].tipo_bloque == LIBRE;
            break;
        }
    }

    dir.cantidad_elementos--;

    //write container directory
    write_block(bloque, &dir);

    return NULL;
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
    return -EOPNOTSUPP;
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
    return -EOPNOTSUPP;
}
int ondisk_utime(const char *path, struct utimbuf *ut)
{
    return -EOPNOTSUPP;
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

    return -EOPNOTSUPP;
}

/* read - read data from an open file.
 * should return exactly the number of bytes requested, except:
 *   - if offset >= len, return 0
 *   - on error, return <0
 * Errors - path resolution, ENOENT, EISDIR, EACCES
 * EACCES - requires read permission to the file itself
 */
static int ondisk_read(const char *path, char *buf, size_t len, off_t offset,
                    struct fuse_file_info *fi)
{
    return -EOPNOTSUPP;
}

/* write - write data to a file
 * It should return exactly the number of bytes requested, except on
 * error.
 * Errors - path resolution, ENOENT, EISDIR, EACCES
 * EACCES - requires write permission to the file itself
 */
static int ondisk_write(const char *path, const char *buf, size_t len,
                     off_t offset, struct fuse_file_info *fi)
{
    return -EOPNOTSUPP;
}

/* statfs - get file system statistics
 * see 'man 2 statfs' for description of 'struct statvfs'.
 * Errors - none. Needs to work.
 */
static int ondisk_statfs(const char *path, struct statvfs *st)
{
    /* needs to return the following fields (set others to zero):
     *   f_bsize = BLOCK_SIZE
     *   f_blocks = total image - (superblock + FAT)
     *   f_bfree = f_blocks - blocks used
     *   f_bavail = f_bfree
     *   f_namelen = <whatever your max namelength is>
     *
     * it's OK to calculate this dynamically on the rare occasions
     * when this function is called.
     */
    return -EOPNOTSUPP;
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
