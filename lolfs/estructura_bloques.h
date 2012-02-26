#ifndef ESTRUCTURA_BLOQUES_H
#define ESTRUCTURA_BLOQUES_H

#define SIZE_BLOCK 4096
#define MAGIC 0xD23

/* Estructura para el super bloque
   Basada en el examen de SO2*/

struct super_bloque
{
    int magic_number;
    int total_bloques;
    int size_bloques;
    int sizebloques_mapabits;
    int primerbloque_mapabits;
    int bloques_libres;
    int bloque_root;
};

struct entrada_directorio
{
    char            nombre [58];
    unsigned short  tipo_bloque;
    unsigned int    apuntador;
};


#define LIBRE               0
#define ARCHIVO             0100000
#define DIRECTORIO          0040000
#define CANT_DIR_ENTRIES    (SIZE_BLOCK - 64) / 64

struct directorio
{
    unsigned short  tipo_bloque;         //2
    unsigned short  uid;                //2
    unsigned short  gid;                //2
    unsigned short  mode;               //2
    unsigned int    fcreacion;          //4
    unsigned int    fmodificacion;      //4
    unsigned int    cantidad_elementos; //4
    char            creador[44];        //44
    struct entrada_directorio directory_Entries[CANT_DIR_ENTRIES];
};

#define CANT_BLOQUES    (SIZE_BLOCK - 64)/sizeof(unsigned int)

struct file_control_block
{
    unsigned short  tipo_bloque;         //2
    unsigned short  uid;                //2
    unsigned short  gid;                //2
    unsigned short  mode;               //2
    unsigned int    fcreacion;          //4
    unsigned int    fmodificacion;      //4
    unsigned int    lenght;             //4     //tamaño en bytes
    char            creador[44];        //44
    unsigned int    bloques[CANT_BLOQUES];
};


#endif // ESTRUCTURA_BLOQUES_H
