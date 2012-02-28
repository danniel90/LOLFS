#define _XOPEN_SOURCE 500  //ESTO ES NECESARIO PARA EL PWRITE Y PREAD
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>

#include <pwd.h>
#include <time.h>

#include "estructura_bloques.h"
char descripcion;

int escribir_bloque(int fd, int lba, void *buffer)
{
    return pwrite(fd, buffer, SIZE_BLOCK, lba*SIZE_BLOCK); //funcion para escribir son para linux
}

int leer_bloque(int fd, int lba, void *buffer)
{
    return pread(fd, buffer, SIZE_BLOCK, lba*SIZE_BLOCK);// funcion para leer son para linux
}

#include <getopt.h>

unsigned long getSize(char *s)
{
    unsigned long valor = strtol(s, &s, 0); /*convierte la parte inicial de la cadena nptr en un valor entero largo de acuerdo con la base dada */

    if (toupper(*s) == 'G') {
        valor *= (1024 * 1024 * 1024);
        return valor;
    }

    if (toupper(*s) == 'M'){
        valor *= (1024 * 1024);

        if (valor % 128 == 0){
            descripcion = 'M';
            return valor;
        }
    }

    if (toupper(*s) == 'K'){
        valor *= 1024;

        if (valor % 128 == 0){
            descripcion = 'K';
            return valor;
        }
    }

    return -1;
}

void mensaje()
{
    printf("uso: mkfs [--crear <tamano (g|m|k)>] <image-file>\n");
    exit(1);
}

 int cantidad_bloques_bits(unsigned long size, char tipo)
{
    unsigned long total_bloques;
    unsigned long tmp;
    int r;

    if (tipo == 'G'){
        //total_bloques = size * 1024 * 1024 * 1024;
        tmp = size / SIZE_BLOCK;
        tmp = tmp / 8;
        total_bloques = tmp / SIZE_BLOCK;
        r = total_bloques;
        return r;
    } else if (tipo == 'M'){
        //total_bloques = size * 1024 * 1024;
        tmp = size / SIZE_BLOCK;
        tmp = tmp / 8;
        total_bloques = tmp / SIZE_BLOCK;
        r = total_bloques;
        return r;
    } else if(tipo =='K'){
        total_bloques = size * 1024;
        tmp = total_bloques / SIZE_BLOCK;
        tmp = tmp / 8;
        total_bloques = tmp / SIZE_BLOCK;
        r = total_bloques;
        return r;
    }

    return -1;
}

 unsigned int alocarBloque(int fd, struct super_bloque *superBlock)
 {
     unsigned int size_bloques = superBlock->size_bloques;
     unsigned int inicio_bitmap = superBlock->primerbloque_mapabits;
     printf("primer blq = %d\n", inicio_bitmap);
     unsigned int size_bitmap = superBlock->sizebloques_mapabits;

     unsigned int i;
     unsigned char *buffer = (unsigned char*)calloc(size_bloques, 1);

     for(i = inicio_bitmap; i < (inicio_bitmap + size_bitmap); i++){
         leer_bloque(fd, i, buffer);

         unsigned int j;
         for(j = 0; j < SIZE_BLOCK; j++){
             if (buffer[j] != 0){
                 int x;
                 for(x = 0; x < 7; x++){
                     if (buffer[j] & (1 << x)){
                         buffer[j] &= ~(1 << x);
                         printf("bit #%d\n", x);
                         escribir_bloque(fd, i, buffer);

                         return (i-inicio_bitmap) * (8 + size_bitmap) + (j * 8) + x;
                     }
                 }
             }
         }
     }

     return 0; // bloque 0 es utilizado por el super_bloque pro ende es imposible utilizarlo y es considerado cmomo error
 }

int main(int argc, char **argv)
{
    static struct option opciones[] =
    {
        {"crear", required_argument, NULL, 'c'},
        {0, 0, 0, 0}
    };/*Struct getgetopt.h*/

    int c;
    unsigned long size_disco = 0;

    while ((c = getopt_long_only(argc, argv, "", opciones, NULL)) != -1) {
        switch(c) {
            case 'c':   size_disco = getSize(optarg);
                        break;
            default:    printf("Opcion erronea!!");
        }
    }

    printf("Size del disco es de: %lu\n", size_disco);
    if (size_disco == -1) {
        perror("Tamano invalido para formatear, numero debe ser multiplo de 128\n");
        exit(1);
    }

    if (optind >= argc) {
        mensaje();
    }
    char *path = argv[optind];

    int i, fd, numero_bloques;
    struct stat sb;
    void *buffer = calloc(SIZE_BLOCK, 1);

    if (size_disco == 0) {
        if ((fd = open(path, O_WRONLY, 0777) < 0) || (fstat(fd, &sb) <  0)) {
            perror("Error al abrir el archivo!!\n"),
            exit(1);
        }
        numero_bloques = sb.st_size / SIZE_BLOCK;
    } else {
        numero_bloques = size_disco / SIZE_BLOCK;

        if ((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC)) < 0) {
              perror("Error al crear el archivo");
              exit(1);
        }

        for (i = 0; i < numero_bloques; i++) {
            if (write(fd, buffer, SIZE_BLOCK) < 0) {
                perror("Error al escribir al archivo!!\n");
                exit(1);
            }
        }
        lseek(fd, 0, SEEK_SET);
     }

    /* Escribir superbloque */

    struct super_bloque *sp = buffer;
    sp->magic_number = MAGIC;
    sp->size_bloques = SIZE_BLOCK;
    sp->total_bloques = numero_bloques;
    sp->primerbloque_mapabits = 1;

    int size_bloquesmapa = cantidad_bloques_bits(size_disco, descripcion);
    sp->sizebloques_mapabits = size_bloquesmapa;
    sp->bloque_root = size_bloquesmapa + 1;
    sp->bloques_libres = numero_bloques - size_bloquesmapa - 2;

    escribir_bloque(fd, 0, buffer);

    /*MAPA DE BITS
     Escribir los Bloques que son necesarios para el mapa de bits          */

    unsigned char arr[SIZE_BLOCK];
    void *bitsmapa = malloc(SIZE_BLOCK);

    int j, k, cont = 0;
    int escribirmapabits = 1 + 1 + size_bloquesmapa;//Cantidad de bits que voy a escribir en el mapa de bits
    for (j = 0; j < SIZE_BLOCK; j++)
        arr[j] = 255;

    for (i = 1; i < i + size_bloquesmapa; i++) {        
        bool flag = false;
        for (j = 0;j < SIZE_BLOCK; j++) {
            if (arr[j] != 0) {
                for (k = 0;k < 7; k++) {
                    if (cont < escribirmapabits) {
                        if(arr[j] & (1 << k)) {
                            arr[j]=arr[j]&~(1<<k);
                            cont++;
                        }
                    } else {
                        bitsmapa = &arr;
                        escribir_bloque(fd,i,bitsmapa);

                        flag = true;
                    }
                    if (flag)
                        break;
                }
            }
            if (flag)
                break;
        }
        if (flag)
            break;
    }



    struct directorio *folder = buffer;


    uid_t uid;
    if ((uid = getuid()) == -1)
        perror("getuid() error.");

    gid_t gid;
    if ((gid = getgid()) == -1)
         perror("getgid() error.");

    time_t tiempo = time(NULL);

    struct passwd *pws;
    pws = getpwuid(uid);

    folder->tipo_bloque = DIRECTORIO;
    memcpy(folder->creador, pws->pw_name, strlen(pws->pw_name) + 1); printf("creador = %s\n", pws->pw_name);
    folder->uid = uid; printf("uid = %d\n", uid);
    folder->gid = gid; printf("gid = %d\n", gid);
    folder->mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO; printf("Permisos para / = %d\n", S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO);
    folder->fcreacion = tiempo; printf("fcreacion = %s\n", ctime(&tiempo));
    folder->fmodificacion = tiempo;
    folder->cantidad_elementos = 0;

    for (k = 0; k < CANT_DIR_ENTRIES; k++)
        folder->directory_Entries[k].tipo_bloque = LIBRE; //verificar que estan LIBRE los dirEntries

    escribir_bloque(fd, 1 + size_bloquesmapa, folder);

    close(fd);
    return 0;
}
