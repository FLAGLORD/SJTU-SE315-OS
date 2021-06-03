#include "tmpfs.h"

#include <defs.h>
#include <syscall.h>
#include <string.h>
#include <cpio.h>
#include <launcher.h>

static struct inode *tmpfs_root;

/*
 * Helper functions to calucate hash value of string
 */
static inline u64 hash_chars(const char *str, ssize_t len)
{
	u64 seed = 131;		/* 31 131 1313 13131 131313 etc.. */
	u64 hash = 0;
	int i;

	if (len < 0) {
		while (*str) {
			hash = (hash * seed) + *str;
			str++;
		}
	} else {
		for (i = 0; i < len; ++i)
			hash = (hash * seed) + str[i];
	}

	return hash;
}

/* BKDR hash */
static inline u64 hash_string(struct string *s)
{
	return (s->hash = hash_chars(s->str, s->len));
}

static inline int init_string(struct string *s, const char *name, size_t len)
{
	int i;

	s->str = malloc(len + 1);
	if (!s->str)
		return -ENOMEM;
	s->len = len;

	for (i = 0; i < len; ++i)
		s->str[i] = name[i];
	s->str[len] = '\0';

	hash_string(s);
	return 0;
}

/*
 *  Helper functions to create instances of key structures
 */
static inline struct inode *new_inode(void)
{
	struct inode *inode = malloc(sizeof(*inode));

	if (!inode)
		return ERR_PTR(-ENOMEM);

	inode->type = 0;
	inode->size = 0;

	return inode;
}

static struct inode *new_dir(void)
{
	struct inode *inode;

	inode = new_inode();
	if (IS_ERR(inode))
		return inode;
	inode->type = FS_DIR;
	init_htable(&inode->dentries, 1024);

	return inode;
}

static struct inode *new_reg(void)
{
	struct inode *inode;

	inode = new_inode();
	if (IS_ERR(inode))
		return inode;
	inode->type = FS_REG;
	init_radix_w_deleter(&inode->data, free);

	return inode;
}

static struct dentry *new_dent(struct inode *inode, const char *name,
			       size_t len)
{
	struct dentry *dent;
	int err;

	dent = malloc(sizeof(*dent));
	if (!dent)
		return ERR_PTR(-ENOMEM);
	err = init_string(&dent->name, name, len);
	if (err) {
		free(dent);
		return ERR_PTR(err);
	}
	dent->inode = inode;

	return dent;
}

// this function create a file (directory if `mkdir` == true, otherwise regular
// file) and its size is `len`. You should create an inode and corresponding 
// dentry, then add dentey to `dir`'s htable by `htable_add`.
// Assume that no separator ('/') in `name`.
static int tfs_mknod(struct inode *dir, const char *name, size_t len, int mkdir)
{
	struct inode *inode;
	struct dentry *dent;

	BUG_ON(!name);

	if (len == 0) {
		WARN("mknod with len of 0");
		return -ENOENT;
	}
	// TODO: write your code here
	// 判断创建的inode类型
	inode = mkdir? new_dir(): new_reg();
	if(IS_ERR(inode)){
		return -ENOMEM;
	}

	// 创建相应的dentry
	dent = new_dent(inode, name, len);
	if(IS_ERR(dent)){
		free(inode);
		return -ENOMEM;
	}
	init_hlist_node(&dent->node);
	// 添加到目录中去
	htable_add(&(dir->dentries), dent->name.hash, &dent->node);
	return 0;
}

int tfs_mkdir(struct inode *dir, const char *name, size_t len)
{
	return tfs_mknod(dir, name, len, 1 /* mkdir */ );
}

int tfs_creat(struct inode *dir, const char *name, size_t len)
{
	return tfs_mknod(dir, name, len, 0 /* mkdir */ );
}

// look up a file called `name` under the inode `dir` 
// and return the dentry of this file
static struct dentry *tfs_lookup(struct inode *dir, const char *name,
				 size_t len)
{
	u64 hash = hash_chars(name, len);
	struct dentry *dent;
	struct hlist_head *head;

	head = htable_get_bucket(&dir->dentries, (u32) hash);

	for_each_in_hlist(dent, node, head) {
		if (dent->name.len == len && 0 == strcmp(dent->name.str, name))
			return dent;
	}
	return NULL;
}

// Walk the file system structure to locate a file with the pathname stored in `*name`
// and saves parent dir to `*dirat` and the filename to `*name`.
// If `mkdir_p` is true, you need to create intermediate directories when it missing.
// If the pathname `*name` starts with '/', then lookup starts from `tmpfs_root`, 
// else from `*dirat`.
// Note that when `*name` ends with '/', the inode of last component will be
// saved in `*dirat` regardless of its type (e.g., even when it's FS_REG) and
// `*name` will point to '\0'
int tfs_namex(struct inode **dirat, const char **name, int mkdir_p)
{
	BUG_ON(dirat == NULL);
	BUG_ON(name == NULL);
	BUG_ON(*name == NULL);

	char buff[MAX_FILENAME_LEN + 1];
	int i;
	struct dentry *dent;
	int err;

	if (**name == '/') {
		*dirat = tmpfs_root;
		// make sure `name` starts with actual name
		while (**name && **name == '/')
			++(*name);
	} else {
		BUG_ON(*dirat == NULL);
		BUG_ON((*dirat)->type != FS_DIR);
	}
	// LAB 5 CODE BEGIN
	for(i = 0; i < MAX_FILENAME_LEN; i++){
		// 第一段名字已经获取完毕或者路径到达结尾
		if((*name)[i] == '/' || !(*name)[i]){
			buff[i] = '\0';
			break;
		}
		buff[i] = (*name)[i];
	}

	// get dentry
	dent = tfs_lookup(*dirat, buff, i);
	if(!(*name)[i]){ //(*name)[i] == '\0', 已经到达路径名字末尾
		// 检查文件是否存在
		return dent ? 0 : -ENOENT;
	}

	// 不存在文件，分两种情况考虑
	if(!dent){
		// mkdir_p = true
		if(mkdir_p){
			if((err = tfs_mkdir(*dirat,buff,i)) < 0){
				return err;
			}
			dent = tfs_lookup(*dirat, buff, i);
		}else{
			return -ENOENT;
		}
	}

	(*name) += i;
	if(**name){
		*dirat = dent->inode;
		// 需要自己忽略开头的'/‘
		while((**name) == '/'){
			(*name)++;
		}
		if(**name){
			// 递归继续查询
			return tfs_namex(dirat, name, mkdir_p);
		}
		// else name ends up with '/'
	}
	// LAB 5 CODE END
	// make sure a child name exists
	if (!**name)
		return -EINVAL;
	return 0;
}

int tfs_remove(struct inode *dir, const char *name, size_t len)
{
	u64 hash = hash_chars(name, len);
	struct dentry *dent, *target = NULL;
	struct hlist_head *head;

	BUG_ON(!name);

	if (len == 0) {
		WARN("mknod with len of 0");
		return -ENOENT;
	}

	head = htable_get_bucket(&dir->dentries, (u32) hash);

	for_each_in_hlist(dent, node, head) {
		if (dent->name.len == len && 0 == strcmp(dent->name.str, name)) {
			target = dent;
			break;
		}
	}

	if (!target)
		return -ENOENT;

	BUG_ON(!target->inode);

	// remove only when file is closed by all processes
	if (target->inode->type == FS_REG) {
		// free radix tree
		radix_free(&target->inode->data);
		// free inode
		free(target->inode);
		// remove dentry from parent
		htable_del(&target->node);
		// free dentry
		free(target);
	} else if (target->inode->type == FS_DIR) {
		if (!htable_empty(&target->inode->dentries))
			return -ENOTEMPTY;

		// free htable
		htable_free(&target->inode->dentries);
		// free inode
		free(target->inode);
		// remove dentry from parent
		htable_del(&target->node);
		// free dentry
		free(target);
	} else {
		BUG("inode type that shall not exist");
	}

	return 0;
}

int init_tmpfs(void)
{
	tmpfs_root = new_dir();

	return 0;
}

// write memory into `inode` at `offset` from `buf` for length is `size`
// it may resize the file
// `radix_get`, `radix_add` are used in this function
// You can use memory functions defined in libc
ssize_t tfs_file_write(struct inode * inode, off_t offset, const char *data,
		       size_t size)
{
	BUG_ON(inode->type != FS_REG);
	BUG_ON(offset > inode->size);

	u64 page_no, page_off;
	u64 cur_off = offset;
	size_t to_write;
	void *page;

	// TODO: write your code here
	u64 origin_page_num = ROUND_UP(inode->size, PAGE_SIZE)/PAGE_SIZE;
	size_t page_to_write;// to_write size in the current page
	for(to_write = size; to_write > 0;){
		page_no = ROUND_DOWN(cur_off,PAGE_SIZE)/PAGE_SIZE;
		// cur_off超过目前可用页
		// 按需分配，没必要一次性将中间空缺的部分全部分配
		if(origin_page_num == 0 || page_no >= origin_page_num){
			page = malloc(PAGE_SIZE);
			if(!page){
				goto on_fail;
			}

			if(radix_add(&inode->data, page_no * PAGE_SIZE, page) < 0){ //radix tree key is offset(integer)
				goto on_fail;
			}
		}
		page = radix_get(&inode->data, page_no * PAGE_SIZE);
		if(!page){
			goto on_fail;
		}
		page_off = cur_off % PAGE_SIZE;
		page_to_write = MIN(PAGE_SIZE - page_off, to_write);
		// write data
		memcpy((char*)page + page_off, data, page_to_write);
		to_write -= page_to_write;
		cur_off += page_to_write;
		data += page_to_write;
	}
	// update inode size
	if(inode->size < cur_off){
		inode->size = cur_off;
	}
on_fail:
	return cur_off - offset;
}

// read memory from `inode` at `offset` in to `buf` for length is `size`, do not
// exceed the file size
// `radix_get` is used in this function
// You can use memory functions defined in libc
ssize_t tfs_file_read(struct inode * inode, off_t offset, char *buff,
		      size_t size)
{
	BUG_ON(inode->type != FS_REG);
	BUG_ON(offset > inode->size);

	u64 page_no, page_off;
	u64 cur_off = offset;
	size_t to_read;
	void *page;
	
	// LAB 5 CODE BEGIN HERE
	to_read = MIN(inode->size - offset, size);
	size_t page_to_read;
	while (to_read)
	{
		page_no = ROUND_DOWN(cur_off, PAGE_SIZE)/PAGE_SIZE;
		page = radix_get(&inode->data, page_no*PAGE_SIZE);
		if(!page){
			goto on_fail;
		}

		page_off = cur_off % PAGE_SIZE;
		page_to_read = MIN(PAGE_SIZE - page_off, to_read);
		// read data
		memcpy(buff,(char *)page+page_off, page_to_read);
		//update
		to_read -= page_to_read;
		cur_off += page_to_read;
		buff += page_to_read;
	}
on_fail:
	// LAB 5 CODE END
	return cur_off - offset;
}

// load the cpio archive into tmpfs with the begin address as `start` in memory
// You need to create directories and files if necessary. You also need to write
// the data into the tmpfs.
int tfs_load_image(const char *start)
{
	struct cpio_file *f;
	struct inode *dirat;
	struct dentry *dent;
	const char *leaf;
	size_t len;
	int err;
	ssize_t write_count;

	BUG_ON(start == NULL);

	cpio_init_g_files();
	cpio_extract(start, "/");

	// size_t file_size;
	// struct inode *reg_file;
	int f_type;
	for (f = g_files.head.next; f; f = f->next) {
		// TODO: Lab5: your code is here
		// dirat = tmpfs_root;
		// leaf = f->name;
		// file_size = f->header.c_filesize;

		// if(!tfs_namex(&dirat, &leaf, 1)){
		// 	dent = tfs_lookup(dirat, leaf, strlen(leaf));
		// 	if(dent == NULL){
		// 		if(file_size != 0){
		// 			// reg file
		// 			tfs_creat(dirat,leaf,file_size);
		// 			if(dent = tfs_lookup(dirat, leaf, strlen(leaf))){
		// 				reg_file = dent->inode;
		// 			}else{
		// 				return -1;
		// 			}
		// 		}else{ 
		// 			// dir
		// 			tfs_mkdir(dirat, leaf, strlen(leaf));
		// 			continue;
		// 		}
		// 	}else{
		// 		reg_file = dent->inode;
		// 	}

		// 	write_count = tfs_file_write(reg_file, 0, f->data, file_size);
		// }
		dirat = tmpfs_root;
		leaf = f->name; //path name?
		printf("origin leaf name: %s\n", leaf);
		err = tfs_namex(&dirat, &leaf, 0);
		if(err < 0 && err != -ENOENT){
			return err;
		}
		f_type = f->header.c_mode & CPIO_FT_MASK;
		printf("leaf name: %s f_type: %d\n", leaf, f_type);
		// dent = tfs_lookup(dirat, leaf, strlen(leaf));
		// if(!dent){
		if(err == -ENOENT){
			switch (f_type){
			case CPIO_REG:
				err = tfs_creat(dirat, leaf, strlen(leaf));
				break;
			case CPIO_DIR:
				err = tfs_mkdir(dirat, leaf, strlen(leaf));
				break;
			default:
				printf("Unknown cpio file type\n");
				err = -ESUPPORT;
				break;
			}
			if(err < 0){
				return err;
			}
		}
		dent = tfs_lookup(dirat, leaf, strlen(leaf));
		if(f_type == CPIO_REG){
			err = tfs_file_write(dent->inode, 0, f->data, f->header.c_filesize);
			BUG_ON(err != f->header.c_filesize);
			if(err < 0){
				return err;
			}
		}

	}

	return 0;
}

static int dirent_filler(void **dirpp, void *end, char *name, off_t off,
			 unsigned char type, ino_t ino)
{
	struct dirent *dirp = *(struct dirent **)dirpp;
	void *p = dirp;
	unsigned short len = strlen(name) + 1 +
	    sizeof(dirp->d_ino) +
	    sizeof(dirp->d_off) + sizeof(dirp->d_reclen) + sizeof(dirp->d_type);
	p += len;
	if (p > end)
		return -EAGAIN;
	dirp->d_ino = ino;
	dirp->d_off = off;
	dirp->d_reclen = len;
	dirp->d_type = type;
	strcpy(dirp->d_name, name);
	*dirpp = p;
	return len;
}

int tfs_scan(struct inode *dir, unsigned int start, void *buf, void *end)
{
	s64 cnt = 0;
	int b;
	int ret;
	ino_t ino;
	void *p = buf;
	unsigned char type;
	struct dentry *iter;

	for_each_in_htable(iter, b, node, &dir->dentries) {
		if (cnt >= start) {
			type = iter->inode->type;
			ino = iter->inode->size;
			ret = dirent_filler(&p, end, iter->name.str,
					    cnt, type, ino);
			if (ret <= 0) {
				return cnt - start;
			}
		}
		cnt++;
	}
	return cnt - start;

}

/* path[0] must be '/' */
struct inode *tfs_open_path(const char *path)
{
	struct inode *dirat = NULL;
	const char *leaf = path;
	struct dentry *dent;
	int err;

	if (*path == '/' && !*(path + 1))
		return tmpfs_root;

	err = tfs_namex(&dirat, &leaf, 0);
	if (err)
		return NULL;

	dent = tfs_lookup(dirat, leaf, strlen(leaf));
	return dent ? dent->inode : NULL;
}
