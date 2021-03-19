#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif
#include "util.h"
#include "class.h"
#include "file.h"
#include "frame.h"
#include "native.h"

/* path separator */
#ifdef _WIN32
#define PATHSEP ';'
#else
#define PATHSEP ':'
#endif

void methodcall(ClassFile *class, Method *method);

static char **classpath = NULL;         /* NULL-terminated array of path strings */
static ClassFile *classes = NULL;       /* list of loaded classes */

/* show usage */
static void
usage(void)
{
	(void)fprintf(stderr, "usage: java [-cp classpath] class\n");
	exit(EXIT_FAILURE);
}

/* check if a class with the given name is loaded */
static ClassFile *
getclass(char *classname)
{
	ClassFile *class;

	for (class = classes; class; class = class->next)
		if (strcmp(classname, class_getclassname(class, class->this_class)) == 0)
			return class;
	return NULL;
}

/* break cpath into paths and set classpath global variable */
void
setclasspath(char *cpath)
{
	char *s;
	size_t i, n;

	for (n = 1, s = cpath; *s; s++) {
		if (*s == PATHSEP) {
			*s = '\0';
			n++;
		}
	}
	classpath = ecalloc(n + 1, sizeof *classpath);
	classpath[0] = cpath;
	classpath[n] = NULL;
	for (i = 1; i < n; i++) {
		while (*cpath++)
			;
		classpath[i] = cpath;
	}
}

/* free all the classes in the list of loaded classes */
void
classfree(void)
{
	ClassFile *tmp;

	while (classes) {
		tmp = classes;
		classes = classes->next;
		file_free(tmp);
		free(tmp);
	}
}

/* recursivelly load class and its superclasses from file matching class name */
ClassFile *
classload(char *classname)
{
	ClassFile *class, *tmp;
	FILE *fp = NULL;
	size_t len, i;
	char *s;
	int n;

	if ((class = getclass(classname)) != NULL)
		return class;
	len = strlen(classname);
	s = emalloc(len + 7);           /* 7 == strlen(".class") + 1 */
	memcpy(s, classname, len);
	memcpy(s + len, ".class", 6);
	s[len + 6] = '\0';
	for (i = 0; classpath[i] != NULL; i++) {
#ifdef _WIN32
		n = _chdir(classpath[i]);
#else
		n = chdir(classpath[i]);
#endif
		if (n == 0) {
			if ((fp = fopen(s, "rb")) != NULL) {
				break;
			}
		}
	}
	free(s);
	if (fp == NULL)
		errx(EXIT_FAILURE, "could not find class %s", classname);
	class = emalloc(sizeof *class);
	if (file_read(fp, class) != 0) {
		fclose(fp);
		free(class);
		errx(EXIT_FAILURE, "could not load class %s", classname);
	}
	fclose(fp);
	if (strcmp(class_getclassname(class, class->this_class), classname) != 0) {
		free(class);
		errx(EXIT_FAILURE, "could not find class %s", classname);
	}
	class->next = classes;
	class->super = NULL;
	classes = class;
	if (!class_istopclass(class)) {
		class->super = classload(class_getclassname(class, class->super_class));
		for (tmp = class->super; tmp; tmp = tmp->super) {
			if (strcmp(class_getclassname(class, class->this_class),
			           class_getclassname(tmp, tmp->this_class)) == 0) {
				errx(EXIT_FAILURE, "class circularity error");
			}
		}
	}
	return class;
}

/* initialize class */
void
classinit(ClassFile *class)
{
	Method *method;

	if (class->init)
		return;
	class->init = 1;
	if (class->super)
		classinit(class->super);
	if ((method = class_getmethod(class, "<clinit>", "()V")) != NULL)
		methodcall(class, method);
}

/* resolve field reference */
Value
resolvefield(ClassFile *class, CONSTANT_Fieldref_info *fieldref)
{
	Value value;
	enum JavaClass jclass;
	char *classname, *name, *type;

	value.v = NULL;
	classname = class_getclassname(class, fieldref->class_index);
	class_getnameandtype(class, fieldref->name_and_type_index, &name, &type);
	if ((jclass = native_javaclass(classname)) != 0) {
		value.v = native_javaobj(jclass, name, type);
	} else {
		value.v = class_getfield(class, name, type);
		// TODO search in superinterfaces
		if (value.v == NULL && class->super != NULL) {
			value = resolvefield(class->super, fieldref);
		}
	}
	if (value.v == NULL)
		errx(EXIT_FAILURE, "could not resolve field");
	return value;
}

/* resolve method reference */
void
resolvemethod(Frame *frame, ClassFile *class, CONSTANT_Methodref_info *methodref)
{
	enum JavaClass jclass;
	char *classname, *name, *type;

	classname = class_getclassname(class, methodref->class_index);
	class_getnameandtype(class, methodref->name_and_type_index, &name, &type);
	if ((jclass = native_javaclass(classname)) != 0) {
		native_javamethod(frame, jclass, name, type);
	} else {
		// TODO
	}
}

/* getstatic: get static field from class */
int
opgetstatic(Frame *frame)
{
	CONSTANT_Fieldref_info *fieldref;
	Value value;
	U2 i;

	i = frame->code->code[frame->pc++] << 8;
	i |= frame->code->code[frame->pc++];
	fieldref = &frame->class->constant_pool[i].info.fieldref_info;
	value = resolvefield(frame->class, fieldref);
	frame_stackpush(frame, value);
	return 0;
}

/* invokevirtual: invoke instance method; dispatch based on class */
int
opinvokevirtual(Frame *frame)
{
	CONSTANT_Methodref_info *methodref;
	U2 i;

	i = frame->code->code[frame->pc++] << 8;
	i |= frame->code->code[frame->pc++];
	methodref = &frame->class->constant_pool[i].info.methodref_info;
	resolvemethod(frame, frame->class, methodref);
	return 0;
}

/* ldc: push item from run-time constant pool */
int
opldc(Frame *frame)
{
	Value value;

	value.u = frame->code->code[frame->pc++];
	frame_stackpush(frame, value);
	return 0;
}

/* nop: do nothing */
int
opnop(Frame *frame)
{
	(void)frame;
	return 0;
}

/* return: return void from method */
int
opreturn(Frame *frame)
{
	(void)frame;
	return 1;
}

/* call method */
void
methodcall(ClassFile *class, Method *method)
{
	static int(*instrtab[])(Frame *) = {
		[NOP]             = opnop,
		[ACONST_NULL]     = opnop,
		[ICONST_M1]       = opnop,
		[ICONST_0]        = opnop,
		[ICONST_1]        = opnop,
		[ICONST_2]        = opnop,
		[ICONST_3]        = opnop,
		[ICONST_4]        = opnop,
		[ICONST_5]        = opnop,
		[LCONST_0]        = opnop,
		[LCONST_1]        = opnop,
		[FCONST_0]        = opnop,
		[FCONST_1]        = opnop,
		[FCONST_2]        = opnop,
		[DCONST_0]        = opnop,
		[DCONST_1]        = opnop,
		[BIPUSH]          = opnop,
		[SIPUSH]          = opnop,
		[LDC]             = opldc,
		[LDC_W]           = opnop,
		[LDC2_W]          = opnop,
		[ILOAD]           = opnop,
		[LLOAD]           = opnop,
		[FLOAD]           = opnop,
		[DLOAD]           = opnop,
		[ALOAD]           = opnop,
		[ILOAD_0]         = opnop,
		[ILOAD_1]         = opnop,
		[ILOAD_2]         = opnop,
		[ILOAD_3]         = opnop,
		[LLOAD_0]         = opnop,
		[LLOAD_1]         = opnop,
		[LLOAD_2]         = opnop,
		[LLOAD_3]         = opnop,
		[FLOAD_0]         = opnop,
		[FLOAD_1]         = opnop,
		[FLOAD_2]         = opnop,
		[FLOAD_3]         = opnop,
		[DLOAD_0]         = opnop,
		[DLOAD_1]         = opnop,
		[DLOAD_2]         = opnop,
		[DLOAD_3]         = opnop,
		[ALOAD_0]         = opnop,
		[ALOAD_1]         = opnop,
		[ALOAD_2]         = opnop,
		[ALOAD_3]         = opnop,
		[IALOAD]          = opnop,
		[LALOAD]          = opnop,
		[FALOAD]          = opnop,
		[DALOAD]          = opnop,
		[AALOAD]          = opnop,
		[BALOAD]          = opnop,
		[CALOAD]          = opnop,
		[SALOAD]          = opnop,
		[ISTORE]          = opnop,
		[LSTORE]          = opnop,
		[FSTORE]          = opnop,
		[DSTORE]          = opnop,
		[ASTORE]          = opnop,
		[ISTORE_0]        = opnop,
		[ISTORE_1]        = opnop,
		[ISTORE_2]        = opnop,
		[ISTORE_3]        = opnop,
		[LSTORE_0]        = opnop,
		[LSTORE_1]        = opnop,
		[LSTORE_2]        = opnop,
		[LSTORE_3]        = opnop,
		[FSTORE_0]        = opnop,
		[FSTORE_1]        = opnop,
		[FSTORE_2]        = opnop,
		[FSTORE_3]        = opnop,
		[DSTORE_0]        = opnop,
		[DSTORE_1]        = opnop,
		[DSTORE_2]        = opnop,
		[DSTORE_3]        = opnop,
		[ASTORE_0]        = opnop,
		[ASTORE_1]        = opnop,
		[ASTORE_2]        = opnop,
		[ASTORE_3]        = opnop,
		[IASTORE]         = opnop,
		[LASTORE]         = opnop,
		[FASTORE]         = opnop,
		[DASTORE]         = opnop,
		[AASTORE]         = opnop,
		[BASTORE]         = opnop,
		[CASTORE]         = opnop,
		[SASTORE]         = opnop,
		[POP]             = opnop,
		[POP2]            = opnop,
		[DUP]             = opnop,
		[DUP_X1]          = opnop,
		[DUP_X2]          = opnop,
		[DUP2]            = opnop,
		[DUP2_X1]         = opnop,
		[DUP2_X2]         = opnop,
		[SWAP]            = opnop,
		[IADD]            = opnop,
		[LADD]            = opnop,
		[FADD]            = opnop,
		[DADD]            = opnop,
		[ISUB]            = opnop,
		[LSUB]            = opnop,
		[FSUB]            = opnop,
		[DSUB]            = opnop,
		[IMUL]            = opnop,
		[LMUL]            = opnop,
		[FMUL]            = opnop,
		[DMUL]            = opnop,
		[IDIV]            = opnop,
		[LDIV]            = opnop,
		[FDIV]            = opnop,
		[DDIV]            = opnop,
		[IREM]            = opnop,
		[LREM]            = opnop,
		[FREM]            = opnop,
		[DREM]            = opnop,
		[INEG]            = opnop,
		[LNEG]            = opnop,
		[FNEG]            = opnop,
		[DNEG]            = opnop,
		[ISHL]            = opnop,
		[LSHL]            = opnop,
		[ISHR]            = opnop,
		[LSHR]            = opnop,
		[IUSHR]           = opnop,
		[LUSHR]           = opnop,
		[IAND]            = opnop,
		[LAND]            = opnop,
		[IOR]             = opnop,
		[LOR]             = opnop,
		[IXOR]            = opnop,
		[LXOR]            = opnop,
		[IINC]            = opnop,
		[I2L]             = opnop,
		[I2F]             = opnop,
		[I2D]             = opnop,
		[L2I]             = opnop,
		[L2F]             = opnop,
		[L2D]             = opnop,
		[F2I]             = opnop,
		[F2L]             = opnop,
		[F2D]             = opnop,
		[D2I]             = opnop,
		[D2L]             = opnop,
		[D2F]             = opnop,
		[I2B]             = opnop,
		[I2C]             = opnop,
		[I2S]             = opnop,
		[LCMP]            = opnop,
		[FCMPL]           = opnop,
		[FCMPG]           = opnop,
		[DCMPL]           = opnop,
		[DCMPG]           = opnop,
		[IFEQ]            = opnop,
		[IFNE]            = opnop,
		[IFLT]            = opnop,
		[IFGE]            = opnop,
		[IFGT]            = opnop,
		[IFLE]            = opnop,
		[IF_ICMPEQ]       = opnop,
		[IF_ICMPNE]       = opnop,
		[IF_ICMPLT]       = opnop,
		[IF_ICMPGE]       = opnop,
		[IF_ICMPGT]       = opnop,
		[IF_ICMPLE]       = opnop,
		[IF_ACMPEQ]       = opnop,
		[IF_ACMPNE]       = opnop,
		[GOTO]            = opnop,
		[JSR]             = opnop,
		[RET]             = opnop,
		[TABLESWITCH]     = opnop,
		[LOOKUPSWITCH]    = opnop,
		[IRETURN]         = opnop,
		[LRETURN]         = opnop,
		[FRETURN]         = opnop,
		[DRETURN]         = opnop,
		[ARETURN]         = opnop,
		[RETURN]          = opreturn,
		[GETSTATIC]       = opgetstatic,
		[PUTSTATIC]       = opnop,
		[GETFIELD]        = opnop,
		[PUTFIELD]        = opnop,
		[INVOKEVIRTUAL]   = opinvokevirtual,
		[INVOKESPECIAL]   = opnop,
		[INVOKESTATIC]    = opnop,
		[INVOKEINTERFACE] = opnop,
		[INVOKEDYNAMIC]   = opnop,
		[NEW]             = opnop,
		[NEWARRAY]        = opnop,
		[ANEWARRAY]       = opnop,
		[ARRAYLENGTH]     = opnop,
		[ATHROW]          = opnop,
		[CHECKCAST]       = opnop,
		[INSTANCEOF]      = opnop,
		[MONITORENTER]    = opnop,
		[MONITOREXIT]     = opnop,
		[WIDE]            = opnop,
		[MULTIANEWARRAY]  = opnop,
		[IFNULL]          = opnop,
		[IFNONNULL]       = opnop,
		[GOTO_W]          = opnop,
		[JSR_W]           = opnop,
	};
	Attribute *cattr;       /* Code_attribute */
	Code_attribute *code;
	Frame *frame;

	if ((cattr = class_getattr(method->attributes, method->attributes_count, Code)) == NULL)
		err(EXIT_FAILURE, "could not find code for method");
	code = &cattr->info.code;
	if ((frame = frame_push(code, class)) == NULL)
		err(EXIT_FAILURE, "out of memory");
	while (frame->pc < code->code_length)
		if ((*instrtab[code->code[frame->pc++]])(frame))
			break;
	frame_pop();
}

/* load and initialize main class, then call main method */
void
java(int argc, char *argv[])
{
	ClassFile *class;
	Method *method;

	(void)argc;
	class = classload(argv[0]);
	if ((method = class_getmethod(class, "main", "([Ljava/lang/String;)V")) == NULL || !(method->access_flags & (ACC_PUBLIC | ACC_STATIC)))
		errx(EXIT_FAILURE, "could not find main method");
	classinit(class);
	methodcall(class, method);
}

/* java: launches a java application */
int
main(int argc, char *argv[])
{
	char *cpath = NULL;
	int i;

	setprogname(argv[0]);
	cpath = getenv("CLASSPATH");
	for (i = 1; i < argc && argv[i][0] == '-'; i++) {
		if (strcmp(argv[i], "-cp") == 0) {
			if (++i >= argc)
				usage();
			cpath = argv[i];
		}
	}
	if (i >= argc)
		usage();
	argc -= i;
	argv += i;
	if (cpath == NULL)
		cpath = ".";
	setclasspath(cpath);
	atexit(classfree);
	java(argc, argv);
	return 0;
}
