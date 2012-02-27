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
    unsigned int inicio_bitmap = superBlock.primerbloque_mapabits;
    unsigned int size_bitmap = superBlock.sizebloques_mapabits;


    unsigned int i;
    for(i = inicio_bitmap; i < (inicio_bitmap + size_bitmap); i++){
        read_block(i,buffer);

        unsigned int j;
        for(j = 0; j < SIZE_BLOCK; j++){
            if (buffer[j] != 0){

                int x;
                for(x = 0; x < 7; x++){
                    if (buffer[j] & (1 << x)){
                        buffer[j] &= ~(1 << x);
                        write_block(i, buffer);

                        return (i-inicio_bitmap) * (8 + size_bitmap) + (j * 8) + x;//return  (i * size_bloques * 8) + (j * 8) + x;
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


 void setPath(const char *path)
 {
    printf("setPath dbg: entra\n");
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
    printf("setPath dbg: sale\n");
}

 int searchFile(struct directorio *dir, char *file_name)
 {
     int identry;
     for (identry = 0; identry < CANT_DIR_ENTRIES; identry++){
         struct entrada_directorio dirEntry = dir->directory_Entries[identry];

         if (strcmp(file_name, dirEntry.nombre) == 0)
             return dirEntry.apuntador;
     }

     return -ENOENT;
 }

 int getBlock(const char *path)
 {
     printf("getBlock dbg: entra\n");
     setPath(path);

     int bloque = superBlock.bloque_root;

     if (lolfs.index == 0){
         printf("getBlock dbg: sale - bloque = %d\n", bloque);
         return bloque;
     }

     read_block(bloque, &buffer);

     int x;
     for (x = 0; x < lolfs.index - 1; x++) {
         bloque = searchFile((void *)buffer, lolfs.path[x]);

         if (bloque == -ENOENT)
             return -ENOENT;

         read_block(bloque, &buffer);
     }

     printf("getBlock dbg: sale - bloque = %d\n", bloque);
     return bloque;
 }

char *getFilename()
{
    printf("getFilename dbg: saliendo\n");
    return lolfs.path[lolfs.index - 1];
}

char *getDirectory(const char *path)
{
    printf("getDirectory dbg: entrando\n");
    char *temp = malloc(strlen(path) + 1);
    strcpy(temp, path);

    int x;
    for (x = strlen(path) - 2; x >= 0; x--){
        if (path[x] == '/'){
            temp = (char *) malloc(x + 1);
            memcpy(temp, path, x + 1);
            break;
        }
    }

    printf("getDirectory dbg: saliendo\n");
    return temp;
    /*
    printf("getDirectory dbg: entrando\n");
    char dirname[2];

    int x;
    for (x = 0; x < lolfs.index - 1; x++){
        strncat(dirname, lolfs.path[x], sizeof(lolfs.path[x]));
    }

    printf("getDirectory dbg: saliendo\n");
    return dirname;*/
}

int setDirStat(struct stat* sb, struct directorio *entry)
{
    /*sb->st_dev      = 0;
    sb->st_ino      = 0;
    sb->st_rdev     = 0;
    sb->st_blocks   = 0;*/
    sb->st_nlink    = 2;
    sb->st_uid      = entry->uid;
    sb->st_gid      = entry->gid;
    sb->st_size     = 0;                        //calculado dinamicamente
    sb->st_mtime    = entry->fmodificacion;     //TODO: copiar fechas correctas de entry
    sb->st_atime    = entry->fmodificacion;
    sb->st_ctime    = entry->fcreacion;
    sb->st_mode     = entry->mode;// | S_IFDIR;
    return 0;
}


//static const char *hello_str = "Hello World!\n";
//static const char *hello_path = "/hello";

/*----------------------------------------------------------FUSE--------------------------------------------------------*/

/* init - called once at startup
 * This might be a good place to read in the super-block and set up
 * any global variables you need. You don't need to worry about the
 * argument or the return value.
 */
void *ondisk_init(struct fuse_conn_info *conn)
{
    read_block(0, &buffer);
    memcpy(&superBlock, buffer, sizeof(struct super_bloque));

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
    struct directorio dir;
    memset(&dir, 0, sizeof(struct directorio));

    int i = getBlock(path);

    if (i < 0)
        return i;

    printf("dbg: int i = %d\n", i);
    read_block(i, &dir);

    setDirStat(stbuf, &dir); //TODO: set for files!!!

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
    struct directorio dir;

    int bloque = getBlock(path);

    if (bloque <= 0) {
        printf("readdir DUMP: %s error al obtener entrada!!\n", path);
        return bloque;
    }

    printf("readdir dbg: bloque = %d\n", bloque);

    read_block(bloque, &dir);

    if (dir.tipo_bloque != DIRECTORIO) {
        printf("readdir DUMP: %s no es directorio!! es %d\n", path, dir.tipo_bloque);
        return -ENOTDIR;
    }

    if ((dir.mode & S_IRUSR) == 0) {
        printf("readdir DUMP: no tiene permisos para leer %s!!\n", path);
        return -EACCES;
    }

    int x;
    for (x = 0; x < CANT_DIR_ENTRIES; x++) {
        if (dir.directory_Entries[x].tipo_bloque == LIBRE)
            continue;
        else {
            read_block(x,buffer);
            setDirStat(&sb, (void *)buffer);
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
    return -EOPNOTSUPP;
}

/* mkdir - create a directory with the given mode.
 * Errors - path resolution, EACCES, EEXIST
 * Conditions for EACCES and EEXIST are the same as for create.
 */
static int ondisk_mkdir(const char *path, mode_t mode)
{
    int bloque = getBlock(path);

    if (bloque < 0) {
        printf("mkdir DUMP: %s\n", path);
        return bloque;
    }

    char *filename = getFilename();
    char *directoryname = getDirectory(path);

    printf("mkdir dbg: PATH = %s\n Parent Dir = %s\n BloqNum = %d\n File = %s\n", path, directoryname, bloque, filename);

    //variable declaration/init
    struct directorio dir;
    read_block(bloque, &dir);

    //dir permissions & file name validation
    if ((dir.mode & S_IWUSR) == 0){
        printf("mkdir DUMP: Usuario no tiene write permissions en %s!!\n", directoryname);
        return -EACCES;
    }

    if (searchFile(&dir, filename) != -ENOENT){
        printf("mkdir DUMP: %s ya existe en %s!!\n", filename, directoryname);
        return -EEXIST;
    }

    //crear entrada nueva
    if (dir.cantidad_elementos == 63){
        printf("mkdir DUMP: Directorio %s lleno!!\n", directoryname);
        return -ENOTEMPTY;
    }

    //new directory creation/init
    unsigned int new_bloque = alocarBloque();
    printf("mkdir dbg: new_bloque = %d\n", new_bloque);

    //directory entry creation
    struct entrada_directorio new_dirEntry;

    memcpy(new_dirEntry.nombre, filename, strlen(filename) + 1);
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

    time_t tiempo = time(NULL);

    struct passwd *pws;
    pws = getpwuid(dir.uid);

    new_dir->tipo_bloque = DIRECTORIO;
    memcpy(new_dir->creador, pws->pw_name, strlen(pws->pw_name) + 1);
    new_dir->uid = dir.uid;
    new_dir->gid = dir.gid;
    new_dir->mode = dir.mode;//S_IFDIR | mode;
    printf("mkdir dbg: new_bloque mode = %d\n", new_dir->mode);
    new_dir->fcreacion = tiempo;
    new_dir->fmodificacion = tiempo;
    new_dir->cantidad_elementos = 0;

    for (x = 0; x < CANT_DIR_ENTRIES; x++)
        new_dir->directory_Entries[x].tipo_bloque = LIBRE; //verificar que estan LIBRE los dirEntries

    //writing
    write_block(bloque, &dir);

    //writing directory
    write_block(new_bloque, new_dir);

    return 0;
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
                case LIBRE:         return 0;//nada mas que hacer
                case DIRECTORIO:    return -EISDIR;//Es directrori! Deberia ser con rmdir!!
                case ARCHIVO:       dir.directory_Entries[x].tipo_bloque = LIBRE;
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

    //release file block
    liberarBloque(ptrBlock);

    return 0;
}

/* rmdir - remove a directory
 *  Errors - path resolution, EACCES, ENOENT, ENOTDIR, ENOTEMPTY
 * Requires user *write* permission to the containing
 *  directory. (permissions on the directory itself are ignored)
 */
static int ondisk_rmdir(const char *path)
{   
    //TODO: no estoy seguro si es necesario hacerlo recursivo..??
    printf("rmdir dbg: path = %s\n", path);
    int bloque = getBlock(path);

    if (bloque < 0)
        return bloque;

    printf("rmdir dbg: bloque = %s\n", bloque);

    char *filename = getFilename();
    char *directoryname = getDirectory(path);

    printf("rmdir dbg: filename = %s | directory = %s \n", filename, directoryname);

    //variable declaration/init
    struct directorio dir;
    read_block(bloque, &dir);

    //dir permissions & file name validation
    if ((dir.mode & S_IWUSR) == 0) {
        printf("rmdir DUMP: Usuario no tiene write permissions en %s!!\n", directoryname);
        return -EACCES;
    }

    int dirEntryIdx =  searchFile(&dir, filename);
    if (dirEntryIdx == -ENOENT) {
        printf("rmdir DUMP: %s no existe en %s!!\n", filename, directoryname);
        return -ENOENT;
    }

    //delete directory, modify element_count & delete dirEntry
    dir.directory_Entries[dirEntryIdx].tipo_bloque = LIBRE;
    dir.cantidad_elementos--;

    //write container directory
    printf("rmdir dbg: excribiendo %s\n", directoryname);
    write_block(bloque, &dir);

    //release Directory block
    printf("rmdir dbg: liberando bloques de %s\n", filename);
    liberarBloque(bloque);

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
