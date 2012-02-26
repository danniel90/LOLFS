#define _XOPEN_SOURCE 500  //ESTO ES NECESARIO PARA EL PWRITE Y PREAD
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>

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

#include <getopt.h> /*Este include es para parsiar argumentos de la consola */

unsigned long parsiarint(char *s)
{
    unsigned long valor = strtol(s, &s, 0); /*convierte la parte inicial de la cadena nptr en un valor entero largo de acuerdo con la base dada */

    if (toupper(*s) == 'G'){
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
        total_bloques = size * 1024 * 1024 * 1024;
        tmp = total_bloques/SIZE_BLOCK;
        tmp = tmp / 8;
        total_bloques = tmp / SIZE_BLOCK;
        r = total_bloques;
        return r;
    }
    else if (tipo == 'M'){
        total_bloques = size * 1024 * 1024;
        tmp = total_bloques / SIZE_BLOCK;
        tmp = tmp / 8;
        total_bloques = tmp / SIZE_BLOCK;
        r = total_bloques;
        return r;
    }
    else if(tipo =='K')
    {
        total_bloques = size * 1024;
        tmp = total_bloques / SIZE_BLOCK;
        tmp = tmp / 8;
        total_bloques = tmp / SIZE_BLOCK;
        r = total_bloques;
        return r;

    }

    return -1;
}

int main(int argc, char **argv)
{
    char *path;
    int i;
    int c;
    int fd, numero_bloques;
    struct stat sb;
    unsigned long size_disco = 0;
    void *buffer = calloc(SIZE_BLOCK, 1);

    static struct option opciones[] =
    {
        {"crear", required_argument, NULL, 'c'},
        {0, 0, 0, 0}
    };/*El Struct getgetopt.h*/

    while((c = getopt_long_only(argc, argv, "", opciones, NULL)) !=-1){
        switch(c)
        {
            case 'c':   size_disco=parsiarint(optarg);
                        break;
            default:    printf("Erro opcion erronea");
        }
    }

    printf("Size_Disco es de: %d", size_disco);
    if(size_disco == -1){
        perror("Tamano no valido para formatiar el disco duro, tiene que ser un numero divisible entre 128\n");
        exit(1);
    }

    if(optind >=argc)
    {
        mensaje();
    }
    path=argv[optind];

    /* Ver que tan grande es la imagen o crear una nueva */
    if(size_disco==0)
    {
        if((fd=open(path,O_WRONLY,0777)<0 ||fstat(fd,&sb)< 0))/* O_WRONLY viene de fcntl.h  y el fstat() de stat.h*/
        {
            perror("Error al a-------brir el archivo\n"),
            exit(1);
        }
        numero_bloques=sb.st_size/SIZE_BLOCK;  /* Size of file, in bytes.*/
    }

    else
    {
        numero_bloques=size_disco/SIZE_BLOCK;
        if((fd=open(path,O_WRONLY|O_CREAT|O_TRUNC))<0)
        {
              perror("Error al crear el archivo"),exit(1);
        }
        for(i=0;i<numero_bloques;i++)
        {
              if(write(fd,buffer,SIZE_BLOCK)<0)
              {
                    perror("Error al escribir al archivo"),exit(1);
               }
        }
               lseek(fd,0,SEEK_SET);
     }
    printf("Entro\n");
     /* Escribir superbloque */

    struct super_bloque *sp=buffer;
    sp->magic_number=MAGIC;
    sp->size_bloques=SIZE_BLOCK;
    sp->total_bloques=numero_bloques;
    sp->primerbloque_mapabits=1;

    int size_bloquesmapa=cantidad_bloques_bits(size_disco,descripcion);
    sp->sizebloques_mapabits=size_bloquesmapa;
    sp->bloque_root=size_bloquesmapa+1;
    sp->bloques_libres=numero_bloques-size_bloquesmapa-2;

    escribir_bloque(fd,0,buffer);
    printf("SE ESCRIBIO EL SUPER BLOQUE\n");
    printf("A ESCRIBIR EL MAPA DE BITS\n");


    /*MAPA DE BITS
     Escribir los Bloques que son necesarios para el mapa de bits          */

    unsigned char arr[SIZE_BLOCK];
    void *bitsmapa=malloc(SIZE_BLOCK);

    int j,k,cont=0;
    int escribirmapabits=1+1+size_bloquesmapa;//Cantidad de bits que voy a escribir en el mapa de bits
    for(i=1;i<i+size_bloquesmapa;i++)
    {
        for(j=0;j<SIZE_BLOCK;j++)
        {
            arr[j]=255;
        }

        for(j=0;j<SIZE_BLOCK;j++)
        {
            if(arr[j]!=0)
            {
                for(k=0;k<7;k++)
                {
                    if(cont<escribirmapabits)
                    {
                        if(arr[j]&(1<<k))
                        {
                            arr[j]=arr[j]&~(1<<k);
                            cont++;
                        }
                    }
                    else
                        break;
                }
            }

        }
        bitsmapa=&arr;
        escribir_bloque(fd,i,bitsmapa);
    }



    const int cantidad_estructdirectorios=SIZE_BLOCK/sizeof(struct directorio);
    struct directorio *folder=buffer;


    printf("SE ESCRIBIO EL MAPA DE BITS\n");
    printf("A ESCRIBIR EL DIRECTORIO ROOT\n");

    folder->tipo_bloque=1;
    memcpy(folder->creador,"Diego",10);
    folder->uid=1000;
    folder->gid=1;
    folder->mode=2;
    folder->fcreacion=10;
    folder->fmodificacion=11;
    folder->cantidad_elementos=0;


    /*for(i=0;i<cantidad_estructdirectorios;i++)
        folder[i].valid=0;*/

    escribir_bloque(fd,1+size_bloquesmapa,buffer);
    printf("SE ESCRIBIO EL BLOQUE DE DIRECROIOS\n");

    close(fd);

    return 0;



}


