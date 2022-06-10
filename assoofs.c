#include <linux/module.h>       /* Needed by all modules */
#include <linux/kernel.h>       /* Needed for KERN_INFO  */
#include <linux/init.h>         /* Needed for the macros */
#include <linux/fs.h>           /* libfs stuff           */
#include <linux/buffer_head.h>  /* buffer_head           */
#include <linux/slab.h>         /* kmem_cache            */
#include "assoofs.h"

/* 
 *  PROTOTIPOS (No sé si hace falta declararlas aquí o no)
 */
/* 2.3.2. Inicializar y registrar el nuevo sistema de ficheros en el kernel */
// extern int register_filesystem(struct file_system_type *);
// extern int unregister_filesystem(struct file_system_type *);
/* 2.3.3. Función que permita montar dispositivos con el nuevo sistema de ficheros */ 
// extern struct dentry *mount_bdev(struct file_system_type *fs_type, int flags, const char *dev_name, void *data, int (*fill_super)(struct super_block *, void *, int));
/* 2.3.4. Función para inicializar el superbloque */
//int assoofs_fill_super(struct super_block *sb, void *data, int silent);

//static inline void d_add(struct dentry *entry, struct inode *inode);


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
    .create = assoofs_create,
    .lookup = assoofs_lookup,
    .mkdir = assoofs_mkdir,
};

struct dentry *assoofs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags) {
    printk(KERN_INFO "Lookup request\n");
    return NULL;
}


static int assoofs_create(struct user_namespace *mnt_userns, struct inode *dir, struct dentry *dentry, umode_t mode, bool excl) {
    printk(KERN_INFO "New file request\n");
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

    bh = sb_bread(sb, ASSOOFS_INODESTORE_BLOCK_NUMBER);
    inode_info = (struct assoofs_inode_info *) bh -> b_data; //Puntero al principio del almacen de inodos

    int i;

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
    struct inode *root_inode;
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
static struct dentry *assoofs_mount(struct file_system_type *fs_type, int flags, const char *dev_name, void *data) {
    struct dentry *ret;
    printk(KERN_INFO "assoofs_mount request\n");
    ret = mount_bdev(fs_type, flags, dev_name, data, assoofs_fill_super);
    // Control de errores a partir del valor de ret. En este caso se puede utilizar la macro IS_ERR: if (IS_ERR(ret)) ...
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

static int __init assoofs_init(void) {
    int ret;
    printk(KERN_INFO "assoofs_init request\n");
    ret = register_filesystem(&assoofs_type);
    // Control de errores a partir del valor de ret
    return ret;
}

static void __exit assoofs_exit(void) {
    int ret;
    printk(KERN_INFO "assoofs_exit request\n");
    ret = unregister_filesystem(&assoofs_type);
    // Control de errores a partir del valor de ret
}

module_init(assoofs_init);
module_exit(assoofs_exit);
