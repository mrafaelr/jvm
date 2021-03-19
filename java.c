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

/* ldc: push item from run-time constant pool */
int
opldc(Frame *frame)
{
	Value value;

	value.u = frame->code->code[frame->pc++];
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
	Attribute *cattr;       /* Code_attribute */
	Code_attribute *code;
	Frame *frame;
	int ret = 0;

	if ((cattr = class_getattr(method->attributes, method->attributes_count, Code)) == NULL)
		err(EXIT_FAILURE, "could not find code for method");
	code = &cattr->info.code;
	if ((frame = frame_push(code, class)) == NULL)
		err(EXIT_FAILURE, "out of memory");
	while (frame->pc < code->code_length) {
		switch (code->code[frame->pc++]) {
		case GETSTATIC:         ret = opgetstatic(frame);       break;
		case LDC:               ret = opldc(frame);             break;
		case INVOKEVIRTUAL:     ret = opinvokevirtual(frame);   break;
		case RETURN:            ret = opreturn(frame);          break;
		default:                printf("ASDA\n");               break;
		}
		if (ret) {
			break;
		}
	}
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
