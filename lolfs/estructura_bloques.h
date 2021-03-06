#ifndef ESTRUCTURA_BLOQUES_H
#define ESTRUCTURA_BLOQUES_H

#include <sys/types.h>

#define BLOCK_SIZE 4096
#define MAGIC 0x101


#define CANT_PADDING ((BLOCK_SIZE - 28) / 4)
struct Super_Block
{
    int magic_number;
    int total_bloques;
    int size_bloques;
    int sizebloques_mapabits;
    int primerbloque_mapabits;
    int bloques_libres;
    int bloque_root;
    int padding[CANT_PADDING];
};

#define MAX_NAME 58
struct Directory_Entry
{
    char            nombre [MAX_NAME];
    unsigned short  tipo_bloque;
    unsigned int    apuntador;
};

struct inodo
{
    unsigned short  tipo_bloque;        //2
    unsigned short  uid;                //2
    unsigned short  gid;                //2
    unsigned short  mode;               //2
    unsigned int    facceso;            //4
    unsigned int    fcreacion;          //4
    unsigned int    fmodificacion;      //4
};

#define LIBRE               0
#define ARCHIVO             1
#define DIRECTORIO          2
#define CANT_DIR_ENTRIES    ((BLOCK_SIZE - 64) / 64)

struct Directory
{
    struct inodo    info;               //20
    unsigned int    cantidad_elementos; //4
    char            creador[40];        //40
    struct Directory_Entry directory_Entries[CANT_DIR_ENTRIES];
};

//#define CANT_BLOQUES_DIRECTOS (((BLOCK_SIZE - 64) / sizeof(unsigned int))  - 3)
#define CANT_BLOQUES_DIRECTOS (((BLOCK_SIZE - 68) / sizeof(unsigned int))  - 3)

struct file_control_block
{
    struct inodo    info;               //20
    off_t    lenght;                  //8     //tamaño en bytes
    //unsigned int    lenght;             //4     //tamaño en bytes
    char            creador[40];        //40
    unsigned int    bloques_directos[CANT_BLOQUES_DIRECTOS];
    unsigned int    bloque_una_indireccion;
    unsigned int    bloque_dos_indireccion;
    unsigned int    bloque_tres_indireccion;
};

#define CANT_BLOQUES (BLOCK_SIZE / 4)
struct indirect_block
{
    unsigned int bloques[CANT_BLOQUES];
};


#endif // ESTRUCTURA_BLOQUES_H
