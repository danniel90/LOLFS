#ifndef ESTRUCTURA_BLOQUES_H
#define ESTRUCTURA_BLOQUES_H

#define SIZE_BLOCK 4096
#define MAGIC 0xD23

/* Estructura para el super bloque
   Basada en el examen de SO2*/


#define CANT_PADDING (SIZE_BLOCK - 28) / 4
struct super_bloque
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
struct entrada_directorio
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
#define CANT_DIR_ENTRIES    (SIZE_BLOCK - 64) / 64

struct directorio
{
    struct inodo    info;               //20
    unsigned int    cantidad_elementos; //4
    char            creador[40];        //40
    struct entrada_directorio directory_Entries[CANT_DIR_ENTRIES];
};

#define CANT_BLOQUES_DATA   (SIZE_BLOCK - 64) / sizeof(unsigned int)

struct file_control_block
{
    struct inodo    info;               //20
    unsigned int    lenght;             //4     //tamaño en bytes
    char            creador[40];        //40
    unsigned int    bloques[CANT_BLOQUES_DATA];
};


#endif // ESTRUCTURA_BLOQUES_H
