/*ESTRUCTURAS Y CONSTANTES*/
#define ASSOOFS_MAGIC 0x20200406 //Identifica el dispositivo
#define ASSOOFS_DEFAULT_BLOCK_SIZE 4096 //Tamaño de bloque por defecto
#define ASSOOFS_FILENAME_MAXLEN 255 //Longitud maxima ficheros
#define ASSOOFS_LAST_RESERVED_BLOCK ASSOOFS_ROOTDIR_BLOCK_NUMBER //Ultimo bloque reservado
#define ASSOOFS_LAST_RESERVED_INODE ASSOOFS_ROOTDIR_INODE_NUMBER //Ultimo inodo reservado
const int ASSOOFS_SUPERBLOCK_BLOCK_NUMBER = 0; //superbloque = bloque 0
const int ASSOOFS_INODESTORE_BLOCK_NUMBER = 1; //almacen de inodos
const int ASSOOFS_ROOTDIR_BLOCK_NUMBER = 2; //nº bloque del directorio raiz(2)
const int ASSOOFS_ROOTDIR_INODE_NUMBER = 1; //inodo 1
const int ASSOOFS_MAX_FILESYSTEM_OBJECTS_SUPPORTED = 64; //constante que define en nº maximo de ficheros que soporta

struct assoofs_super_block_info { //struct que define la información que permite definir el superbloque (los parametros generales del sistema de ficheros)
  	uint64_t version; //version = entero +(*u*int) de 64 bitsj
    uint64_t magic;
    uint64_t block_size;    
    uint64_t inodes_count;
    uint64_t free_blocks; //mapa de bits que indica los bloques disponibles
    char padding[4056]; //relleno = array de caracteres
};


struct assoofs_dir_record_entry { //directorios
    char filename[ASSOOFS_FILENAME_MAXLEN]; //nombre directorio
    uint64_t inode_no; //nº inodo asociado
};

struct assoofs_inode_info { //info sobre mis nodos 
    mode_t mode; //modo permisos
    uint64_t inode_no; 
    uint64_t data_block_number; //nº bloque donde se encuentra
    union { 
        uint64_t file_size;
        uint64_t dir_children_count;
    };
};
