enum {
	ERR_NONE = 0,
	ERR_READ,
	ERR_ALLOC,
	ERR_CODE,
	ERR_CONSTANT,
	ERR_DESCRIPTOR,
	ERR_EOF,
	ERR_INDEX,
	ERR_KIND,
	ERR_MAGIC,
	ERR_TAG,
};

void file_free(ClassFile *class);
int file_read(FILE *fp, ClassFile *class);
