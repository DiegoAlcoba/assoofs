#include <linux/module.h>       /* Needed by all modules */
#include <linux/kernel.h>       /* Needed for KERN_INFO  */
#include <linux/init.h>         /* Needed for the macros */
#include <linux/fs.h>           /* libfs stuff           */
#include <linux/buffer_head.h>  /* buffer_head           */
#include <linux/slab.h>         /* kmem_cache            */
#include "assoofs.h"

MODULE_LICENSE("GPL");

/* 
 *  PROTOTIPOS
 */
struct assoofs_inode_info *assoofs_get_inode_info(struct super_block *sb, uint64_t inode_no);
static struct inode *assoofs_get_inode(struct super_block *sb, int ino);

/*
 *  Operaciones sobre ficheros
 */
ssize_t assoofs_read(struct file * filp, char __user * buf, size_t len, loff_t * ppos);
ssize_t assoofs_write(struct file * filp, const char __user * buf, size_t len, loff_t * ppos);
const struct file_operations assoofs_file_operations = {
    .read = assoofs_read,
    .write = assoofs_write,
};

ssize_t assoofs_read(struct file * filp, char __user * buf, size_t len, loff_t * ppos) {
    printk(KERN_INFO "Read request\n");
    return 0;
}

ssize_t assoofs_write(struct file * filp, const char __user * buf, size_t len, loff_t * ppos) {
    printk(KERN_INFO "Write request\n");
    return 0;
}

/*
 *  Operaciones sobre directorios
 */
static int assoofs_iterate(struct file *filp, struct dir_context *ctx);
const struct file_operations assoofs_dir_operations = {
    .owner = THIS_MODULE,
    .iterate = assoofs_iterate,
};

static int assoofs_iterate(struct file *filp, struct dir_context *ctx) {
    printk(KERN_INFO "Iterate request\n");
    return 0;
}

/*
 *  Operaciones sobre inodos
 */
static int assoofs_create(struct user_namespace *mnt_userns, struct inode *dir, struct dentry *dentry, umode_t mode, bool excl);
struct dentry *assoofs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags);
static int assoofs_mkdir(struct user_namespace *mnt_userns, struct inode *dir , struct dentry *dentry, umode_t mode);
static struct inode_operations assoofs_inode_ops = {
    .create = assoofs_create, //crear inodos
    .lookup = assoofs_lookup, //recorre el arbol de inodos y dado un nombre de fichero obtener su ID(inodo)
    .mkdir = assoofs_mkdir, //crear directorios
};

struct dentry *assoofs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags) {
    //1
    struct assoofs_inode_info *parent_info = parent_inode -> i_private;
    struct super_block *sb = parent_inode -> i_sb;
    struct buffer_head *bh;
    //2
    struct assoofs_dir_record_entry *record;
    struct inode *inode;
    
    int i;
        
    printk(KERN_INFO "Lookup request\n");

    /* 1. Acceder al bloque de disco con el contenido del directorio apuntado por parent_inode */
    bh = sb_bread(sb, parent_info -> data_block_number);
    /* 2. Recorrer el contenido del directorio buscando la entrada cuyo nombre se corresponda con el que buscamos. Si se localiza la entrada, entonces tenemos que construir el inodo correspondiente */
    record = (struct assoofs_dir_record_entry *) bh -> b_data;

    for (i = 0; i < parent_info -> dir_children_count; i++) {
        if (!strcmp(record -> filename, child_dentry -> d_name.name)) {
            inode = assoofs_get_inode(sb, record -> inode_no); //Función que obtiene la informaciónde un inodo a partir de su número de inodo
            inode_init_owner(sb -> s_user_ns, inode, parent_inode, ((struct assoofs_inode_info *) inode -> i_private) -> mode);
            d_add(child_dentry, inode);

            return NULL;
        }
        record++;
    }

    printk(KERN_ERR "No inode found for the filename [%s]\n", child_dentry -> d_name.name);

    return NULL;
}


static int assoofs_create(struct user_namespace *mnt_userns, struct inode *dir, struct dentry *dentry, umode_t mode, bool excl) {
    //1
    struct inode *inode;
    struct super_block *sb;
    uint64_t count;
    struct assoofs_inode_info *inode_info;
    //2
    struct asoofs_inode_info *parent_inode_info;
    struct assoofs_dir_record_entry *dir_contents;
    struct buffer_head *bh;

    printk(KERN_INFO "New file request\n");

    /* 1. Crear el nuevo inodo */
    sb = dir -> i_sb; //Obtengo un puntero al superbloque desde dir
    count = ((struct assoofs_super_block_info *) sb -> s_fs_info) -> inodes_count; //obtengo el nº de inodos en la información persistente del superbloque
    
    inode = new_inode(sb);
    inode -> i_sb = sb;
    inode -> i_atime = inode -> i_mtime = inode -> i_ctime = current_time(inode);
    inode -> i_op = &assoofs_inode_ops;
    inode -> i_ino = count + 1; //asigno nº al nuevo inodo a partir de count

    if (inode -> i_ino > ASSOOFS_MAX_FILESYSTEM_OBJECTS_SUPPORTED) {
        printk(KERN_ERR "Assoofs %d max file system objects suported\n", ASSOOFS_MAX_FILESYSTEM_OBJECTS_SUPPORTED);

        return -1;
    }
    
    //Guardar en el campo i_private la info persistente del mismo. Es un nuevo inodo y hay que crearlo desde cero
    inode_info = kmalloc(sizeof(struct assoofs_inode_info), GFP_KERNEL);
    inode_info -> inode_no = inode -> i_ino;
    inode_info -> mode = mode; //el segundo mode llega como argumento
    inode_info -> file_size = 0;
    inode -> i_private = inode_info;

    //Para las operaciones sobre ficheros
    inode -> i_fop = &assoofs_file_operations;

    //Asignamos propietario y permisos, y guardamos el nuevo inodo en el árbol de directorios
    inode_init_owner(sb -> s_user_ns, inode, dir, mode);
    d_add(dentry, inode);

    //Asignación de bloque al nuevo inodo, por lo que habrá que consultar el mapa de bits del superbloque.
    assoofs_sb_get_a_freeblock(sb, &inode_info -> data_block_number);

    //Guardar la información persistente del nuevo inodo en disco
    assoofs_add_inode_info(sb, inode_info);

    /* 
     * 2. Modificar el contenido del directorio padre, añadiento una nueva entrada para el nuevo archivo. 
     * El nombre se sacará del tercer parámetro
     */ 
    parent_inode_info = dir -> i_private;
    bh = sb_bread(sb, parent_inode_info -> data_block_number);

    dir_contents = (struct assoofs_dir_record_entry *) bh -> b_data;
    dir_contents += parent_inode_info -> dir_children_count;
    dir_contents -> inode_no = inode_info -> inode_no; //inode_info es la información persistente del inodo creado en el paso 2.

    strcpy(dis_contents -> filename, dentru -> d_name.name);
    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);

    /* 
     * 3. Actualizar la información persistente del inodo padre indicando que ahora tiene un archivo más.
     * Para actualizar la información persistente de un inodo es necesario recorrer el almacen y localizar dicho nodo.  
     */
    parent_inode_info -> dir_children_count++;
    assoofs_save_inode_info(sb, parent_inode_info);

    return 0;
}

static int assoofs_mkdir(struct user_namespace *mnt_userns, struct inode *dir , struct dentry *dentry, umode_t mode) {
    printk(KERN_INFO "New directory request\n");
    return 0;
}


/*
 *  FUNCIONES AUXILIARES
 */
struct assoofs_inode_info *assoofs_get_inode_info(struct super_block *sb, uint64_t inode_no) {
    struct assoofs_inode_info *inode_info = NULL;
    struct buffer_head *bh;
    struct assoofs_super_block_info *afs_sb = sb -> s_fs_info;
    struct assoofs_inode_info *buffer = NULL;
    int i;

    bh = sb_bread(sb, ASSOOFS_INODESTORE_BLOCK_NUMBER);
    inode_info = (struct assoofs_inode_info *) bh -> b_data; //Puntero al principio del almacen de inodos

    for (i = 0; i < afs_sb -> inodes_count; i++) { 
        if (inode_info -> inode_no == inode_no) {
            buffer = kmalloc(sizeof(struct assoofs_inode_info), GFP_KERNEL); //reserva memori para otra variable de tipo assoofs_inode_info
            memcpy(buffer, inode_info, sizeof(*buffer)); //variable buffer, dirección destino, nº bytes a copiar
            break;
        }
        inode_info++;
    }

    brelse(bh);

    return buffer;
}

static struct inode *assoofs_get_inode(struct super_block *sb, int ino) {
    /* 1. Obtener la información persistente del inodo ino. Ver la función auxiliar assoofs_get_inode_info descrita anteriormente */
    struct inode *inode;
    struct assoofs_inode_info *inode_info; 
    inode_info = assoofs_get_inode_info(sb, ino);

    /* 2. Crear una nueva variable de tipo struct inode e inicializarla con la función new_inode (antes creada) Asignar valores a los campos i_ino, i_sb, i_op, i_fop, i_atime, i_mtime, i_ctime e i_private del nuevo inodo */
    inode = new_inode(sb);
    inode -> i_ino = ino; //nº del inodo
    inode -> i_sb = sb; //puntero al superbloque
    inode -> i_op = &assoofs_inode_ops; //dirección de una variable de tipo struct inode_operations previamente declarada
    //2.1
    if (S_ISDIR(inode_info -> mode)){ //S_ISDIR: macro
        inode -> i_fop = &assoofs_dir_operations;
    } else if (S_ISREG(inode_info -> mode)) {
        inode -> i_fop = &assoofs_file_operations;
    } else {
        printk(KERN_ERR "Unknown inode type. Neither a directory nor a file");
    }
    //2.2
    inode -> i_atime = inode -> i_mtime = inode -> i_ctime = current_time(inode); //fechas
    //2.3 
    inode -> i_private = inode_info;

    /* Por último, devolvemos el inodo inode recién creado */
    return inode;
    
}


/*
 *  Operaciones sobre el superbloque
 */
static const struct super_operations assoofs_sops = {
    .drop_inode = generic_delete_inode,
};

/*********************************************************************************************************************************************************************************/

/*
 *  Inicialización del superbloque
 */
int assoofs_fill_super(struct super_block *sb, void *data, int silent) {   
    //struct inode *root_inode; //crear un inodo (inodo en memoria(struct inode))
    struct buffer_head *bh;
    struct assoofs_super_block_info *assoofs_sb;
    struct inode *root_inode;
    
    printk(KERN_INFO "assoofs_fill_super request\n");

    // 1.- Leer la información persistente del superbloque del dispositivo de bloques  
    bh = sb_bread(sb, ASSOOFS_SUPERBLOCK_BLOCK_NUMBER); // sb lo recibe assoosfs_fill_super como argumento -> puntero, nº bloques que quiero leer: 0 (assoofs.h)
    assoofs_sb = (struct assoofs_super_block_info *)bh -> b_data;

    printk(KERN_INFO "The magic number obtained in disk is %llu\n", assoofs_sb -> magic);
    
    // 2.- Comprobar los parámetros del superbloque
    if (unlikely(assoofs_sb -> magic != ASSOOFS_MAGIC)) { //Este if nunca se cumple, es por eficiencia

        printk(KERN_ERR "The filesystem that you try to mount is not of type assoofs. Magic number missmatch.");
        brelse(bh);

        return -EPERM; //-1 si no devuelve el número correcto
    }

    if (unlikely(assoofs_sb -> block_size != ASSOOFS_DEFAULT_BLOCK_SIZE)) {

        printk(KERN_ERR "ASSOOFS seem to be formatted using a wrong block size");
        brelse(bh);

        return -EPERM;
    }

    printk(KERN_INFO "ASSOOFS filesystem of version %llu formatted with block size of %llu detected in the device.\n", assoofs_sb -> version, assoofs_sb -> block_size);

    // 3.- Escribir la información persistente leída del dispositivo de bloques en el superbloque sb, incluído el campo s_op con las operaciones que soporta.
    sb -> s_magic = ASSOOFS_MAGIC;
    sb -> s_maxbytes = ASSOOFS_DEFAULT_BLOCK_SIZE;
    sb -> s_op = &assoofs_sops; //Dirección de un var que contiene las operaciones que se pueden realizar con el superbloque
    sb -> s_fs_info = assoofs_sb; // fs.h = libreria generica -> sistema de ficheros basados en inodos

    // 4.- Crear el inodo raíz y asignarle operaciones sobre inodos (i_op) y sobre directorios (i_fop)

    root_inode = new_inode(sb); //Creación
    inode_init_owner(sb -> s_user_ns, root_inode, NULL, S_IFDIR); //S_IFDIR para directorios, S_IFREG para ficheros ----- El primer argumento puede ser directamente "root_inode"?

    root_inode -> i_ino = ASSOOFS_ROOTDIR_INODE_NUMBER; //numero de inodo
    root_inode -> i_sb = sb; //puntero al superbloque asociado a este inodo
    root_inode -> i_op = &assoofs_inode_ops; //operaciones en inodo, la variable &assoofs... contiene las funciones para trabajar con inodos
    root_inode -> i_fop = &assoofs_dir_operations; //operaciones para directorios = funciones
    root_inode -> i_atime = root_inode -> i_mtime = root_inode -> i_ctime = current_time(root_inode); //fechas (actual, acceso, modificación)
    root_inode -> i_private = assoofs_get_inode_info(sb, ASSOOFS_ROOTDIR_INODE_NUMBER); // (superbloque, nº inodo)
  
    sb -> s_root = d_make_root(root_inode); //campo s_root (del superbloque) va a contener la dirección de memoria que devuelve la función d_make_root cuando se le pasa el root_inode
    //d_add(dentry, inode);

    if (!sb -> s_root) { //Comprueba si ha habido algún error en s_root
        brelse(bh);
        
        return -1;
    }

    brelse(bh);

    return 0;
}

/**************************************************************************************************************************************************************************************/

/*
 *  Montaje de dispositivos assoofs 
 */

/* APARTADO 2.3.3 */
static struct dentry *assoofs_mount(struct file_system_type *fs_type, int flags, const char *dev_name, void *data) {
    struct dentry *ret = mount_bdev(fs_type, flags, dev_name, data, assoofs_fill_super);
    
    printk(KERN_INFO "assoofs_mount request\n");  
    // Control de errores a partir del valor de ret. En este caso se puede utilizar la macro IS_ERR: if (IS_ERR(ret)) ...
    if (IS_ERR(ret)) {
        printk(KERN_INFO "mount ERROR\n");

        return NULL;
    } else {
        printk(KERN_INFO "mount SUCCESFUL\n");
    }

    return ret;
}

/*
 *  assoofs file system type
 */

static struct file_system_type assoofs_type = {
    .owner   = THIS_MODULE,
    .name    = "assoofs",
    .mount   = assoofs_mount,
    .kill_sb = kill_litter_super,
};

/* APARTADO 2.3.2 */
static int __init assoofs_init(void) {
    int ret = register_filesystem(&assoofs_type);
    printk(KERN_INFO "assoofs_init request\n");
    
    if (ret != 0) {
        printk(KERN_INFO "assoofs register failed\n");
        return -1;
    }

    printk(KERN_INFO "assoofs succesfully registered\n");
    // Control de errores a partir del valor de ret
    return 0;
}

static void __exit assoofs_exit(void) {
    int ret = unregister_filesystem(&assoofs_type);
    printk(KERN_INFO "assoofs_exit request\n");
    
    if (ret != 0) {
        printk(KERN_INFO "assoofs unregistered failed\n");
    } else {
        printk(KERN_INFO "assoofs succesfully registered\n");
    }
    // Control de errores a partir del valor de ret
}

module_init(assoofs_init);
module_exit(assoofs_exit);