#include "lolfs.h"


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
    //printf("setPath dbg: entra\n");
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
    //printf("setPath dbg: sale\n");
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
     //printf("getBlock dbg: entra\n");
     setPath(path);

     int bloque = superBlock.bloque_root;

     if (lolfs.index <= 0){
         //printf("getBlock dbg: sale - bloque = %d\n", bloque);
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

     //printf("getBlock dbg: sale - bloque = %d\n", bloque);
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

char *getdirectorio(const char *path)
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

int setDirStat(struct stat* sb, struct directorio *entry)
{
    /*sb->st_dev      = 0;
    sb->st_ino      = 0;
    sb->st_rdev     = 0;
    sb->st_blocks   = 0;*/
    sb->st_nlink    = 2;
    sb->st_uid      = entry->uid;
    sb->st_gid      = entry->gid;
    sb->st_size     = SIZE_BLOCK;
    sb->st_mtime    = entry->fmodificacion;
    sb->st_atime    = entry->facceso;
    sb->st_ctime    = entry->fcreacion;
    sb->st_mode     = entry->mode;
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
 * locate a file or directorio corresponding to a specified path.
 *
 * ENOENT - a component of the path is not present.
 * ENOTDIR - an intermediate component of the path (e.g. 'b' in
 *           /a/b/c) is not a directorio
 * EACCES  - the user lacks *execute* permission (yes, execute) for an
 *           intermediate directorio in the path.
 *
 * In our case we assume a single user, and so it is sufficient to
 * use this test:    if ((mode & S_IXUSR) == 0)
 *                        return -EACCES;
 *
 * See 'man path_resolution' for more information.
 */

/* getattr - get file or directorio attributes. For a description of
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

    int bloque = getBlock(path);

    if (bloque <= 0) {
        printf("\t getattr DUMP: bloque = %d | path = %s\n", bloque, path);
        return -ENOENT;
    }

    printf("\t getattr dbg: bloque = %d | path = %s\n", bloque, path);
    read_block(bloque, &dir);

    setDirStat(stbuf, &dir); //TODO: set for files!!!

    return 0;

}

/* readdir - get directorio contents
 * for each entry in the directorio, invoke:
 *     filler(buf, <name>, <statbuf>, 0)
 * where <statbuf> is a struct stat, just like in getattr.
 * Errors - path resolution, EACCES, ENOTDIR, ENOENT
 *
 * EACCES is returned if the user lacks *read* permission to the
 * directorio - i.e.:     if ((mode & S_IRUSR) == 0)
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
        printf("\t readdir DUMP: %s error al obtener entrada!!\n", path);
        return bloque;
    }

    printf("\t readdir dbg: bloque = %d\n", bloque);

    read_block(bloque, &dir);

    if (dir.tipo_bloque != DIRECTORIO) {
        printf("\t readdir DUMP: %s no es directorio!! es %d\n", path, dir.tipo_bloque);
        return -ENOTDIR;
    }

    if ((dir.mode & S_IRUSR) == 0) {
        printf("\t readdir DUMP: no tiene permisos para leer %s!!\n", path);
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
 * the enclosing directorio:
 *       if ((mode & S_IWUSR) == 0)
 *           return -EACCES;
 * If a file or directorio of this name already exists, return -EEXIST.
 */
static int ondisk_create(const char *path, mode_t mode,
                         struct fuse_file_info *fi)
{
    return -EOPNOTSUPP;
}

/* mkdir - create a directorio with the given mode.
 * Errors - path resolution, EACCES, EEXIST
 * Conditions for EACCES and EEXIST are the same as for create.
 */
static int ondisk_mkdir(const char *path, mode_t mode)
{
    char *directoryname = getdirectorio(path);
    //int bloque = getBlock(path);
    int bloque = getBlock(directoryname);

    if (bloque <= 0) {
        printf("\t mkdir DUMP: bloque = %d | directoryname = %s\n", bloque, directoryname);
        return -ENOTDIR;
    }

    char *filename = getFilename(path);
    //char *directoryname = getdirectorio(path);

    printf("\t mkdir dbg: PATH = %s | Parent Dir = %s | BloqNum = %d | File = %s\n", path, directoryname, bloque, filename);

    //variable declaration/init
    struct directorio dir;
    read_block(bloque, &dir);

    //dir permissions & file name validation
    if ((dir.mode & S_IWUSR) == 0){
        printf("\t mkdir DUMP: Usuario no tiene write permissions en %s!!\n", directoryname);
        return -EACCES;
    }

    if (searchFile(&dir, filename) != -ENOENT){
        printf("\t mkdir DUMP: %s ya existe en %s!!\n", filename, directoryname);
        return -EEXIST;
    }

    //crear entrada nueva
    if (dir.cantidad_elementos == 63){
        printf("\t mkdir DUMP: directorio %s lleno!!\n", directoryname);
        return -ENOTEMPTY;
    }

    //new directorio creation/init
    unsigned int new_bloque = alocarBloque();
    printf("\t mkdir dbg: new_bloque = %d\n", new_bloque);

    //directorio entry creation
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

    //directorio creation
    struct directorio *new_dir = (struct directorio *) calloc(CANT_DIR_ENTRIES, sizeof(struct entrada_directorio));

    time_t tiempo = time(NULL);

    struct passwd *pws;
    pws = getpwuid(dir.uid);

    new_dir->tipo_bloque = DIRECTORIO;
    memcpy(new_dir->creador, pws->pw_name, strlen(pws->pw_name) + 1);
    new_dir->uid = dir.uid;
    new_dir->gid = dir.gid;
    new_dir->mode = S_IFDIR | mode;
    printf("\t mkdir dbg: new_bloque mode = %d\n", new_dir->mode);
    new_dir->fcreacion = tiempo;
    new_dir->facceso = tiempo;
    new_dir->fmodificacion = tiempo;
    new_dir->cantidad_elementos = 0;

    for (x = 0; x < CANT_DIR_ENTRIES; x++)
        new_dir->directory_Entries[x].tipo_bloque = LIBRE; //verificar que estan LIBRE los dirEntries

    //writing
    write_block(bloque, &dir);

    //writing directorio
    write_block(new_bloque, new_dir);

    printf("\t mkdir dbg: escribiendo super bloque \n");
    superBlock.bloques_libres--;
    write_block(0, &superBlock);

    return 0;
}

/* unlink - delete a file
 *  Errors - path resolution, EACCES, ENOENT, EISDIR
 * Requires user *write* permission to the containing
 *  directorio. (permissions on the file itself are ignored)
 */
static int ondisk_unlink(const char *path)
{
    printf("\t unlink dbg: path = %s\n", path);
    //TODO: hacerlo recursivo
    printf("\t unlink dbg: path = %s\n", path);

    char *filename = getFilename(path);
    char *directoryname = getdirectorio(path);
    int bloque = getBlock(path);

    if (bloque <= 0) {
        printf("\t unlink DUMP: bloque = %d | path = %s\n", bloque, path);
        return -ENOTDIR;
    }

    printf("\t unlink dbg: filename = %s | directorio = %s \n", filename, directoryname);

    struct directorio dir;
    read_block(bloque, &dir);

    if ((dir.mode & S_IWUSR) == 0) {
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

    int x ;
    for (x = 0; x < CANT_BLOQUES_DATA; x++) {
        liberarBloque(fcb.bloques[x]);
        superBlock.bloques_libres++;
    }
    liberarBloque(bloque_liberar);

    printf("\t unlink dbg: excribiendo %s\n", directoryname);
    write_block(bloque, &dir);

    printf("\t unlink dbg: escribiendo super bloque \n");
    superBlock.bloques_libres++;
    write_block(0, &superBlock);

    return 0;
}

/* rmdir - remove a directorio
 *  Errors - path resolution, EACCES, ENOENT, ENOTDIR, ENOTEMPTY
 * Requires user *write* permission to the containing
 *  directorio. (permissions on the directorio itself are ignored)
 */
static int ondisk_rmdir(const char *path)
{   
    //TODO: hacerlo recursivo
    printf("\t rmdir dbg: path = %s\n", path);

    char *filename = getFilename(path);
    char *directoryname = getdirectorio(path);
    int bloque = getBlock(directoryname);

    if (bloque <= 0) {
        printf("\t rmdir DUMP: bloque = %d | directoryname = %s\n", bloque, directoryname);
        return -ENOTDIR;
    }

    printf("\t rmdir dbg: filename = %s | directorio = %s \n", filename, directoryname);

    struct directorio dir;
    read_block(bloque, &dir);

    if ((dir.mode & S_IWUSR) == 0) {
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

    printf("\t rmdir dbg: liberando bloque de %s\n", filename);
    liberarBloque(bloque_liberar);

    printf("\t rmdir dbg: escribiendo super bloque \n");
    superBlock.bloques_libres++;
    write_block(0, &superBlock);

    return 0;
}

/* rename - rename a file or directorio
 * Errors - path resolution, ENOENT, EACCES, EINVAL, EEXIST
 *
 * ENOENT - source does not exist
 * EEXIST - destination already exists
 * EACCES - no write permission to directorio. Permissions
 * EINVAL - source and destination are not in the same directorio
 *
 * Note that this is a simplified version of the UNIX rename
 * functionality - see 'man 2 rename' for full semantics. In
 * particular, the full version can move across directories, replace a
 * destination file, and replace an empty directorio with a full one.
 */
static int ondisk_rename(const char *src_path, const char *dst_path)
{
    char *filename_src = getFilename(src_path);
    char *directoryname_src = getdirectorio(src_path);

    char *filename_dst = getFilename(dst_path);
    char *directoryname_dst = getdirectorio(dst_path);

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

    printf("\t rename dbg: SRC_PATH = %s | DST_PATH = %s | SRC_Parent Dir = %s | DST_Parent Dir = %s | BloqNum = %d | SRC_File = %s | DST_File = %s\n",
                           src_path, dst_path, directoryname_src, directoryname_dst, bloque, filename_src, filename_dst);

    //variable declaration/init
    struct directorio dir;
    read_block(bloque, &dir);

    //dir permissions & file name validation
    if ((dir.mode & S_IWUSR) == 0){
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
 * Note that no write permissions to the file/directorio or containing
 * are needed - if you can resolve the path, then you can make the
 * change. (normally EACCES is returned if the invoking user does not
 * own the file)
 */
static int ondisk_chmod(const char *path, mode_t mode)
{
    printf("\t chmod dbg: path = %s\n", path);

    char *filename = getFilename(path);
    char *directoryname = getdirectorio(path);
    int bloque = getBlock(directoryname);

    if (bloque <= 0) {
        printf("\t chmod DUMP: bloque = %d | directoryname = %s\n", bloque, directoryname);
        return -ENOTDIR;
    }

    printf("\t chmod dbg: filename = %s | directorio = %s \n", filename, directoryname);

    struct directorio dir;
    read_block(bloque, &dir);

    int bloque_modificar = searchFile(&dir, filename);
    if (bloque_modificar == -ENOENT) {
        printf("\t rmdir DUMP: %s no existe en %s!!\n", filename, directoryname);
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
        ((struct directorio *) &buffer)->mode = mode;
        ((struct directorio *) &buffer)->fcreacion = tiempo;
    } else {
        ((struct file_control_block *) &buffer)->mode = mode;
        ((struct file_control_block *) &buffer)->fcreacion = tiempo;
    }

    write_block(bloque_modificar, &buffer);

    return 0;
}

int ondisk_utime(const char *path, struct utimbuf *ut)
{
    printf("\t utime dbg: path = %s\n", path);

    char *filename = getFilename(path);
    char *directoryname = getdirectorio(path);
    int bloque = getBlock(directoryname);

    if (bloque <= 0) {
        printf("\t utime DUMP: bloque = %d | directoryname = %s\n", bloque, directoryname);
        return -ENOTDIR;
    }

    printf("\t utime dbg: filename = %s | directorio = %s \n", filename, directoryname);

    struct directorio dir;
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
        ((struct directorio *) &buffer)->facceso = ut->actime;
        ((struct directorio *) &buffer)->fmodificacion = ut->modtime;
        ((struct directorio *) &buffer)->fcreacion = tiempo;
    } else {
        ((struct file_control_block *) &buffer)->facceso = ut->actime;
        ((struct file_control_block *) &buffer)->fmodificacion = ut->modtime;
        ((struct file_control_block *) &buffer)->fcreacion = tiempo;
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
     *   f_blocks = total image - (superBlock + FAT)
     *   f_bfree = f_blocks - blocks used
     *   f_bavail = f_bfree
     *   f_namelen = <whatever your max namelength is>
     *
     * it's OK to calculate this dynamically on the rare occasions
     * when this function is called.
     */
    st->f_bsize = SIZE_BLOCK;
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
