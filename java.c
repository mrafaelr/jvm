#include <math.h>
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

/* resolve constant reference */
Value
resolveconstant(ClassFile *class, U2 index)
{
	Value v;

	v.i = 0;
	switch (class->constant_pool[index].tag) {
	case CONSTANT_Integer:
		v.i = class_getinteger(class, index);
		break;
	case CONSTANT_Float:
		v.f = class_getfloat(class, index);
		break;
	case CONSTANT_Long:
		v.l = class_getlong(class, index);
		break;
	case CONSTANT_Double:
		v.d = class_getdouble(class, index);
		break;
	case CONSTANT_String:
		v.v = class_getstring(class, index);
		break;
	}
	return v;
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

/* dadd: add double */
int
opdadd(Frame *frame)
{
	Value v1, v2;

	v2 = frame_stackpop(frame);
	v1 = frame_stackpop(frame);
	v1.d += v2.d;
	frame_stackpush(frame, v1);
	return 0;
}

/* ddiv: divide double */
int
opddiv(Frame *frame)
{
	Value v1, v2;

	v2 = frame_stackpop(frame);
	v1 = frame_stackpop(frame);
	v1.d /= v2.d;
	frame_stackpush(frame, v1);
	return 0;
}

/* dmul: multiply double */
int
opdmul(Frame *frame)
{
	Value v1, v2;

	v2 = frame_stackpop(frame);
	v1 = frame_stackpop(frame);
	v1.d *= v2.d;
	frame_stackpush(frame, v1);
	return 0;
}

/* dneg: negate double */
int
opdneg(Frame *frame)
{
	Value v;

	v = frame_stackpop(frame);
	v.d = -v.d;
	frame_stackpush(frame, v);
	return 0;
}

/* drem: remainder double */
int
opdrem(Frame *frame)
{
	Value v1, v2;

	v2 = frame_stackpop(frame);
	v1 = frame_stackpop(frame);
	v1.d = fmod(v1.d, v2.d);
	frame_stackpush(frame, v1);
	return 0;
}

/* dsub: subtract double */
int
opdsub(Frame *frame)
{
	Value v1, v2;

	v2 = frame_stackpop(frame);
	v1 = frame_stackpop(frame);
	v1.d -= v2.d;
	frame_stackpush(frame, v1);
	return 0;
}

/* dload: load double from local variable */
int
opdload(Frame *frame)
{
	Value v;
	U2 i;

	i = frame->code->code[frame->pc++];
	v = frame_localload(frame, i);
	frame_stackpush(frame, v);
	return 0;
}

/* dload_0: load double from local variable */
int
opdload_0(Frame *frame)
{
	Value v;

	v = frame_localload(frame, 0);
	frame_stackpush(frame, v);
	return 0;
}

/* dload_1: load double from local variable */
int
opdload_1(Frame *frame)
{
	Value v;

	v = frame_localload(frame, 1);
	frame_stackpush(frame, v);
	return 0;
}

/* dload_2: load double from local variable */
int
opdload_2(Frame *frame)
{
	Value v;

	v = frame_localload(frame, 2);
	frame_stackpush(frame, v);
	return 0;
}

/* dload_3: load double from local variable */
int
opdload_3(Frame *frame)
{
	Value v;

	v = frame_localload(frame, 3);
	frame_stackpush(frame, v);
	return 0;
}

/* dstore: store double into local variable */
int
opdstore(Frame *frame)
{
	Value v;
	U2 i;

	i = frame->code->code[frame->pc++];
	v = frame_stackpop(frame);
	frame_localstore(frame, i, v);
	frame_localstore(frame, i + 1, v);
	return 0;
}

/* dstore_0: store double into local variable */
int
opdstore_0(Frame *frame)
{
	Value v;

	v = frame_stackpop(frame);
	frame_localstore(frame, 0, v);
	frame_localstore(frame, 1, v);
	return 0;
}

/* dstore_1: store double into local variable */
int
opdstore_1(Frame *frame)
{
	Value v;

	v = frame_stackpop(frame);
	frame_localstore(frame, 1, v);
	frame_localstore(frame, 2, v);
	return 0;
}

/* dstore_2: store double into local variable */
int
opdstore_2(Frame *frame)
{
	Value v;

	v = frame_stackpop(frame);
	frame_localstore(frame, 2, v);
	frame_localstore(frame, 3, v);
	return 0;
}

/* dstore_3: store double into local variable */
int
opdstore_3(Frame *frame)
{
	Value v;

	v = frame_stackpop(frame);
	frame_localstore(frame, 3, v);
	frame_localstore(frame, 4, v);
	return 0;
}

/* getstatic: get static field from class */
int
opgetstatic(Frame *frame)
{
	CONSTANT_Fieldref_info *fieldref;
	Value v;
	U2 i;

	i = frame->code->code[frame->pc++] << 8;
	i |= frame->code->code[frame->pc++];
	fieldref = &frame->class->constant_pool[i].info.fieldref_info;
	v = resolvefield(frame->class, fieldref);
	frame_stackpush(frame, v);
	return 0;
}

/* iadd: add int */
int
opiadd(Frame *frame)
{
	Value v1, v2;

	v2 = frame_stackpop(frame);
	v1 = frame_stackpop(frame);
	v1.i += v2.i;
	frame_stackpush(frame, v1);
	return 0;
}

/* _______________________________________________ int ___________________________________________________________________*/

/* idiv: divide int */
int
opidiv(Frame *frame)
{
	Value v1, v2;

	v2 = frame_stackpop(frame);
	v1 = frame_stackpop(frame);
	v1.i /= v2.i;
	frame_stackpush(frame, v1);
	return 0;
}

/* imul: multiply int */
int
opimul(Frame *frame)
{
	Value v1, v2;

	v2 = frame_stackpop(frame);
	v1 = frame_stackpop(frame);
	v1.i *= v2.i;
	frame_stackpush(frame, v1);
	return 0;
}

/* ineg: negate int */
int
opineg(Frame *frame)
{
	Value v;

	v = frame_stackpop(frame);
	v.i = -v.i;
	frame_stackpush(frame, v);
	return 0;
}

/* irem: remainder int */
int
opirem(Frame *frame)
{
	Value v1, v2;

	v2 = frame_stackpop(frame);
	v1 = frame_stackpop(frame);
	v1.d = fmod(v1.i, v2.i);
	frame_stackpush(frame, v1);
	return 0;
}

/* isub: subtract int */
int
opisub(Frame *frame)
{
	Value v1, v2;

	v2 = frame_stackpop(frame);
	v1 = frame_stackpop(frame);
	v1.i -= v2.i;
	frame_stackpush(frame, v1);
	return 0;
}

/* iload: load int from local variable */
int
opiload(Frame *frame)
{
	Value v;
	U2 i;

	i = frame->code->code[frame->pc++];
	v = frame_localload(frame, i);
	frame_stackpush(frame, v);
	return 0;
}

/* iload_0: load double from local variable */
int
opiload_0(Frame *frame)
{
	Value v;

	v = frame_localload(frame, 0);
	frame_stackpush(frame, v);
	return 0;
}

/* iload_1: load int from local variable */
int
opiload_1(Frame *frame)
{
	Value v;

	v = frame_localload(frame, 1);
	frame_stackpush(frame, v);
	return 0;
}

/* iload_2: load int from local variable */
int
opiload_2(Frame *frame)
{
	Value v;

	v = frame_localload(frame, 2);
	frame_stackpush(frame, v);
	return 0;
}

/* iload_3: load int from local variable */
int
opiload_3(Frame *frame)
{
	Value v;

	v = frame_localload(frame, 3);
	frame_stackpush(frame, v);
	return 0;
}

/* istore: store int into local variable */
int
opistore(Frame *frame)
{
	Value v;
	U2 i;

	i = frame->code->code[frame->pc++];
	v = frame_stackpop(frame);
	frame_localstore(frame, i, v);
	return 0;
}

/* istore_0: store int into local variable */
int
opistore_0(Frame *frame)
{
	Value v;

	v = frame_stackpop(frame);
	frame_localstore(frame, 0, v);
	return 0;
}

/* istore_1: store int into local variable */
int
opistore_1(Frame *frame)
{
	Value v;

	v = frame_stackpop(frame);
	frame_localstore(frame, 1, v);
	return 0;
}

/* istore_2: store int into local variable */
int
opistore_2(Frame *frame)
{
	Value v;

	v = frame_stackpop(frame);
	frame_localstore(frame, 2, v);
	return 0;
}

/* istore_3: store int into local variable */
int
opistore_3(Frame *frame)
{
	Value v;

	v = frame_stackpop(frame);
	frame_localstore(frame, 3, v);
	return 0;
}
/*_______________________________________________________________________________________________________________*/

/* _______________________________________________ float ___________________________________________________________________*/

/* fadd: add float */
int
opfadd(Frame *frame)
{
	Value v1, v2;

	v2 = frame_stackpop(frame);
	v1 = frame_stackpop(frame);
	v1.f += v2.f;
	frame_stackpush(frame, v1);
	return 0;
}

/* fdiv: divide float */
int
opfdiv(Frame *frame)
{
	Value v1, v2;

	v2 = frame_stackpop(frame);
	v1 = frame_stackpop(frame);
	v1.f /= v2.f;
	frame_stackpush(frame, v1);
	return 0;
}

/* fmul: multiply float */
int
opfmul(Frame *frame)
{
	Value v1, v2;

	v2 = frame_stackpop(frame);
	v1 = frame_stackpop(frame);
	v1.f *= v2.f;
	frame_stackpush(frame, v1);
	return 0;
}

/* fneg: negate float */
int
opfneg(Frame *frame)
{
	Value v;

	v = frame_stackpop(frame);
	v.f = -v.f;
	frame_stackpush(frame, v);
	return 0;
}

/* frem: remainder float */
int
opfrem(Frame *frame)
{
	Value v1, v2;

	v2 = frame_stackpop(frame);
	v1 = frame_stackpop(frame);
	v1.d = fmod(v1.f, v2.f);
	frame_stackpush(frame, v1);
	return 0;
}

/* fsub: subtract float */
int
opfsub(Frame *frame)
{
	Value v1, v2;

	v2 = frame_stackpop(frame);
	v1 = frame_stackpop(frame);
	v1.f -= v2.f;
	frame_stackpush(frame, v1);
	return 0;
}

/* fload: load float from local variable */
int
opfload(Frame *frame)
{
	Value v;
	U2 i;

	i = frame->code->code[frame->pc++];
	v = frame_localload(frame, i);
	frame_stackpush(frame, v);
	return 0;
}

/* fload_0: load float from local variable */
int
opfload_0(Frame *frame)
{
	Value v;

	v = frame_localload(frame, 0);
	frame_stackpush(frame, v);
	return 0;
}

/* fload_1: load float from local variable */
int
opfload_1(Frame *frame)
{
	Value v;

	v = frame_localload(frame, 1);
	frame_stackpush(frame, v);
	return 0;
}

/* fload_2: load float from local variable */
int
opfload_2(Frame *frame)
{
	Value v;

	v = frame_localload(frame, 2);
	frame_stackpush(frame, v);
	return 0;
}

/* fload_3: load float from local variable */
int
opfload_3(Frame *frame)
{
	Value v;

	v = frame_localload(frame, 3);
	frame_stackpush(frame, v);
	return 0;
}

/* fstore: store float into local variable */
int
opfstore(Frame *frame)
{
	Value v;
	U2 i;

	i = frame->code->code[frame->pc++];
	v = frame_stackpop(frame);
	frame_localstore(frame, i, v);
	return 0;
}

/* fstore_0: store float into local variable */
int
opfstore_0(Frame *frame)
{
	Value v;

	v = frame_stackpop(frame);
	frame_localstore(frame, 0, v);
	return 0;
}

/* fstore_1: store float into local variable */
int
opfstore_1(Frame *frame)
{
	Value v;

	v = frame_stackpop(frame);
	frame_localstore(frame, 1, v);
	return 0;
}

/* istore_2: store float into local variable */
int
opfstore_2(Frame *frame)
{
	Value v;

	v = frame_stackpop(frame);
	frame_localstore(frame, 2, v);
	return 0;
}

/* fstore_3: store float into local variable */
int
opfstore_3(Frame *frame)
{
	Value v;

	v = frame_stackpop(frame);
	frame_localstore(frame, 3, v);
	return 0;
}

/*_______________________________________________________________________________________________________________*/

/* _______________________________________________ long ___________________________________________________________________*/

/* ladd: add long */
int
opladd(Frame *frame)
{
	Value v1, v2;

	v2 = frame_stackpop(frame);
	v1 = frame_stackpop(frame);
	v1.l += v2.l;
	frame_stackpush(frame, v1);
	return 0;
}

/* ldiv: divide long */
int
opldiv(Frame *frame)
{
	Value v1, v2;

	v2 = frame_stackpop(frame);
	v1 = frame_stackpop(frame);
	v1.l /= v2.l;
	frame_stackpush(frame, v1);
	return 0;
}

/* lmul: multiply long */
int
oplmul(Frame *frame)
{
	Value v1, v2;

	v2 = frame_stackpop(frame);
	v1 = frame_stackpop(frame);
	v1.l *= v2.l;
	frame_stackpush(frame, v1);
	return 0;
}

/* lneg: negate long */
int
oplneg(Frame *frame)
{
	Value v;

	v = frame_stackpop(frame);
	v.l = -v.l;
	frame_stackpush(frame, v);
	return 0;
}

/* lrem: remainder long */
int
oplrem(Frame *frame)
{
	Value v1, v2;

	v2 = frame_stackpop(frame);
	v1 = frame_stackpop(frame);
	v1.l = fmod(v1.l, v2.l);
	frame_stackpush(frame, v1);
	return 0;
}

/* lsub: subtract long */
int
oplsub(Frame *frame)
{
	Value v1, v2;

	v2 = frame_stackpop(frame);
	v1 = frame_stackpop(frame);
	v1.l -= v2.l;
	frame_stackpush(frame, v1);
	return 0;
}

/* lload: load long from local variable */
int
oplload(Frame *frame)
{
	Value v;
	U2 i;

	i = frame->code->code[frame->pc++];
	v = frame_localload(frame, i);
	frame_stackpush(frame, v);
	return 0;
}

/* lload_0: load long from local variable */
int
oplload_0(Frame *frame)
{
	Value v;

	v = frame_localload(frame, 0);
	frame_stackpush(frame, v);
	return 0;
}

/* lload_1: load long from local variable */
int
oplload_1(Frame *frame)
{
	Value v;

	v = frame_localload(frame, 1);
	frame_stackpush(frame, v);
	return 0;
}

/* lload_2: load long from local variable */
int
oplload_2(Frame *frame)
{
	Value v;

	v = frame_localload(frame, 2);
	frame_stackpush(frame, v);
	return 0;
}

/* lload_3: load long from local variable */
int
oplload_3(Frame *frame)
{
	Value v;

	v = frame_localload(frame, 3);
	frame_stackpush(frame, v);
	return 0;
}

/* lstore: store long into local variable */
int
oplstore(Frame *frame)
{
	Value v;
	U2 i;

	i = frame->code->code[frame->pc++];
	v = frame_stackpop(frame);
	frame_localstore(frame, i, v);
	frame_localstore(frame, i + 1, v);
	return 0;
}

/* lstore_0: store long into local variable */
int
oplstore_0(Frame *frame)
{
	Value v;

	v = frame_stackpop(frame);
	frame_localstore(frame, 0, v);
	frame_localstore(frame, 1, v);
	return 0;
}

/* lstore_1: store long into local variable */
int
oplstore_1(Frame *frame)
{
	Value v;

	v = frame_stackpop(frame);
	frame_localstore(frame, 1, v);
	frame_localstore(frame, 2, v);
	return 0;
}

/* lstore_2: store long into local variable */
int
oplstore_2(Frame *frame)
{
	Value v;

	v = frame_stackpop(frame);
	frame_localstore(frame, 2, v);
	frame_localstore(frame, 3, v);
	return 0;
}

/* lstore_3: store long into local variable */
int
oplstore_3(Frame *frame)
{
	Value v;

	v = frame_stackpop(frame);
	frame_localstore(frame, 3, v);
	frame_localstore(frame, 4, v);
	return 0;
}

/*____________________________________________________________________________________________________________________*/

/*____________________________________________________ const ________________________________________________________________*/

/* iconst_m1: push int -1 into stack */
int
opiconst_m1(Frame *frame)
{
	Value v;

	v.i = (-1);
	frame_stackpush(frame, v);
	return 0;
}

/* iconst_0: push int 0 into stack */
int
opiconst_0(Frame *frame)
{
	Value v;

	v.i = 0;
	frame_stackpush(frame, v);
	return 0;
}

/* iconst_1: push int 1 into stack */
int
opiconst_1(Frame *frame)
{
	Value v;

	v.i = 1;
	frame_stackpush(frame, v);
	return 0;
}

/* iconst_2: push int 2 into stack */
int
opiconst_2(Frame *frame)
{
	Value v;

	v.i = 2;
	frame_stackpush(frame, v);
	return 0;
}

/* iconst_3: push int 3 into stack */
int
opiconst_3(Frame *frame)
{
	Value v;

	v.i = 3;
	frame_stackpush(frame, v);
	return 0;
}

/* iconst_4: push int 4 into stack */
int
opiconst_4(Frame *frame)
{
	Value v;

	v.i = 4;
	frame_stackpush(frame, v);
	return 0;
}

/* iconst_5: push int 5 into stack */
int
opiconst_5(Frame *frame)
{
	Value v;

	v.i = 5;
	frame_stackpush(frame, v);
	return 0;
}

/* lconst_0: push long 0 into stack */
int
oplconst_0(Frame *frame)
{
	Value v;

	v.l = 0;
	frame_stackpush(frame, v);
	return 0;
}

/* lconst_1: push long 1 into stack */
int
oplconst_1(Frame *frame)
{
	Value v;

	v.l = 1;
	frame_stackpush(frame, v);
	return 0;
}

/* fconst_0: push float 0 into stack */
int
opfconst_0(Frame *frame)
{
	Value v;

	v.f = 0.0;
	frame_stackpush(frame, v);
	return 0;
}

/* fconst_1: push float 1 into stack */
int
opfconst_1(Frame *frame)
{
	Value v;

	v.f = 1.0;
	frame_stackpush(frame, v);
	return 0;
}

/* fconst_2: push float 2 into stack */
int
opfconst_2(Frame *frame)
{
	Value v;

	v.f = 2.0;
	frame_stackpush(frame, v);
	return 0;
}

/* dconst_0: push double 0 into stack */
int
opdconst_0(Frame *frame)
{
	Value v;

	v.d = 0.0;
	frame_stackpush(frame, v);
	return 0;
}

/* dconst_1: push double 1 into stack */
int
opdconst_1(Frame *frame)
{
	Value v;

	v.d = 1.0;
	frame_stackpush(frame, v);
	return 0;
}

/* bipush: push byte */
int
opbipush(Frame *frame)
{
	Value v;

	v.i = frame->code->code[frame->pc++];
	frame_stackpush(frame, v);
	return 0;
}

/*____________________________________________________________________________________________________________________*/

/*________________________________________________________pop____________________________________________________________*/

int
oppop(Frame *frame)
{
	Value v;
	v = frame_stackpop(frame);
	if (v.d || v.l) {
		frame_stackpush(frame, v);
		return 1; /*Needs to throw exception!!*/
	}
	return 0;   
}

int
oppop2(Frame *frame)
{
	frame_stackpop(frame);
	return 0;   
}

/*____________________________________________________________________________________________________________________*/

/*________________________________________________________dup____________________________________________________________*/

int
opdup(Frame *frame)
{
	Value v;
	v = frame_stackpop(frame);
	if (v.d || v.l) {
		frame_stackpush(frame, v);
		return 1; /*Needs to throw exception!!*/
	}
	frame_stackpush(frame, v);
	frame_stackpush(frame, v);
	return 0;   
}

int
opdup_x1(Frame *frame)
{
	Value v1, v2;
	v1 = frame_stackpop(frame);
	v2 = frame_stackpop(frame);
	if ((v1.d || v1.l) || (v2.d || v2.l)) {
		frame_stackpush(frame, v2);
		frame_stackpush(frame, v1);
		return 1; /*Needs to throw exception!!*/
	}
	frame_stackpush(frame, v2);
	frame_stackpush(frame, v1);
	frame_stackpush(frame, v2);
	frame_stackpush(frame, v1);
	return 0;   
}

int
opdup_x2(Frame *frame)
{
	Value v1, v2, v3;
	v1 = frame_stackpop(frame);
	v2 = frame_stackpop(frame);
	v3 = frame_stackpop(frame);
	if (((v1.d || v1.l) || (v2.d || v2.l) || (v3.d || v3.l))) {
		frame_stackpush(frame, v3);
		frame_stackpush(frame, v2);
		frame_stackpush(frame, v1);
		frame_stackpush(frame, v2);
		frame_stackpush(frame, v1);
	}
	frame_stackpush(frame, v3);
	frame_stackpush(frame, v2);
	frame_stackpush(frame, v1);
	frame_stackpush(frame, v3);
	frame_stackpush(frame, v2);
	frame_stackpush(frame, v1);
	return 0;   
}

int
opdup2(Frame *frame)
{
	Value v1, v2;
	v1 = frame_stackpop(frame);
	v2 = frame_stackpop(frame);
	if (v1.d || v2.l) {
		frame_stackpush(frame, v1);
		frame_stackpush(frame, v2);
		frame_stackpush(frame, v1);
	}
	frame_stackpush(frame, v2);
	frame_stackpush(frame, v1);
	frame_stackpush(frame, v2);
	frame_stackpush(frame, v1);
	return 0;   
}

int
opdup2_x1(Frame *frame)
{
	Value v1, v2, v3;
	v1 = frame_stackpop(frame);
	v2 = frame_stackpop(frame);
	v3 = frame_stackpop(frame);
	if (((v1.d || v1.l) || (v2.d || v2.l) || (v3.d || v3.l))) {
		frame_stackpush(frame, v3);
		frame_stackpush(frame, v2);
		frame_stackpush(frame, v1);
		frame_stackpush(frame, v2);
		frame_stackpush(frame, v1);
	}
	frame_stackpush(frame, v3);
	frame_stackpush(frame, v2);
	frame_stackpush(frame, v1);
	frame_stackpush(frame, v3);
	frame_stackpush(frame, v2);
	frame_stackpush(frame, v1);
	return 0; 
}

int
opdup2_x2(Frame *frame)
{
	Value v1, v2, v3, v4;
	v1 = frame_stackpop(frame);
	v2 = frame_stackpop(frame);
	v3 = frame_stackpop(frame);
	v4 = frame_stackpop(frame);
	
	if (((v1.d || v1.l) && (v2.d || v2.l) && (v3.d || v3.l))) {
		frame_stackpush(frame, v4);
		frame_stackpush(frame, v3);
		frame_stackpush(frame, v2);
		frame_stackpush(frame, v1);
		frame_stackpush(frame, v2);
		frame_stackpush(frame, v1);
		return 0;
	}
	else if ((v1.d || v1.l)){
		frame_stackpush(frame, v4);
		frame_stackpush(frame, v3);
		frame_stackpush(frame, v2);
		frame_stackpush(frame, v1);
		frame_stackpush(frame, v3);
		frame_stackpush(frame, v2);
		frame_stackpush(frame, v1);
		return 0;
		
	}
	else if ((v3.d || v3.l)){
		frame_stackpush(frame, v4);
		frame_stackpush(frame, v3);
		frame_stackpush(frame, v2);
		frame_stackpush(frame, v1);
		frame_stackpush(frame, v3);
		frame_stackpush(frame, v2);
		frame_stackpush(frame, v1);
		return 0;
		
	} else if ((!(v1.d || v1.l) && !(v2.d || v2.l) && !(v3.d || v3.l))){
	
		frame_stackpush(frame, v4);
		frame_stackpush(frame, v3);
		frame_stackpush(frame, v2);
		frame_stackpush(frame, v1);
		frame_stackpush(frame, v4);
		frame_stackpush(frame, v3);
		frame_stackpush(frame, v2);
		frame_stackpush(frame, v1);
		return 0;
	} else {
		frame_stackpush(frame, v4);
		frame_stackpush(frame, v3);
		frame_stackpush(frame, v2);
		frame_stackpush(frame, v1);
		return 1; /*Needs to throw exception*/
	}
}


/*____________________________________________________________________________________________________________________*/

/*_____________________________________________________type a_______________________________________________________________*/

int
opaaload(Frame *frame)
{
	Value v1;
	Value *v2;
	U4 i;
	U4 *a;

	i = frame->code->code[frame->pc++];
	v1 = frame_localload4(frame, i);
	*a = frame->code->code[frame->pc++];
	*v2 = frame_localload4(frame, *a);
	//if pointer null exception
	// if index not in bound in array, exception
	frame_stackpush(frame, v2[v1.i]);
	return 0;
} 

int
opaastore(Frame *frame)
{
	Value v1;
	U2 i;

	i = frame->code->code[frame->pc++];
	v1 = frame_stackpop(frame);
	//if pointer null exception
	// if index not in bound in array, exception
	frame_localstore(frame, i, v1);
	return 0;
}

/* aload: load address from local variable */
int
opaload(Frame *frame)
{
	Value v;
	U2 i;

	i = frame->code->code[frame->pc++];
	v = frame_localload(frame, i);
	frame_stackpush(frame, v);
	return 0;
}

/* aload_0: load address from local variable */
int
opaload_0(Frame *frame)
{
	Value v;

	v = frame_localload(frame, 0);
	frame_stackpush(frame, v);
	return 0;
}

/* aload_1: load address from local variable */
int
opaload_1(Frame *frame)
{
	Value v;

	v = frame_localload(frame, 1);
	frame_stackpush(frame, v);
	return 0;
}

/* aload_2: load address from local variable */
int
opaload_2(Frame *frame)
{
	Value v;

	v = frame_localload(frame, 2);
	frame_stackpush(frame, v);
	return 0;
}

/* aload_3: load address from local variable */
int
opaload_3(Frame *frame)
{
	Value v;

	v = frame_localload(frame, 3);
	frame_stackpush(frame, v);
	return 0;
}

/* astore: address into local variable */
int
opastore(Frame *frame)
{
	Value v;
	U2 i;

	i = frame->code->code[frame->pc++];
	v = frame_stackpop(frame);
	frame_localstore(frame, i, v);
	return 0;
}

/* astore_0: store address into local variable */
int
opastore_0(Frame *frame)
{
	Value v;

	v = frame_stackpop(frame);
	frame_localstore(frame, 0, v);
	return 0;
}

/* astore_1: store address into local variable */
int
opastore_1(Frame *frame)
{
	Value v;

	v = frame_stackpop(frame);
	frame_localstore(frame, 1, v);
	return 0;
}

/* astore_2: store address into local variable */
int
opastore_2(Frame *frame)
{
	Value v;

	v = frame_stackpop(frame);
	frame_localstore(frame, 2, v);
	return 0;
}

/* astore_3: store address into local variable */
int
opastore_3(Frame *frame)
{
	Value v;

	v = frame_stackpop(frame);
	frame_localstore(frame, 3, v);
	return 0;
}

/*____________________________________________________________________________________________________________________*/

/*__________________________________________________________array__________________________________________________________*/

typedef enum array_type {
	T_BOOLEAN   = 4,
	T_CHAR      = 5,
	T_FLOAT     = 6,
	T_DOUBLE    = 7,
	T_BYTE      = 8,
	T_SHORT     = 9,
	T_INT       = 10,
	T_LONG      = 11,
} arraytype;

U1*
initialize_zerou1(U1 *p, int tamanho)
{
	for (int i = 0; i < tamanho; i++) {
        	*(p + i) = 0;
    	}
    	return p;
}

U2*
initialize_zerou2(U2 *p, int tamanho)
{
	for (int i = 0; i < tamanho; i++) {
        	*(p + i) = 0;
    	}
    	return p;
}

U4*
initialize_zerou4(U4 *p, int tamanho)
{
	for (int i = 0; i < tamanho; i++) {
        	*(p + i) = 0;
    	}
    	return p;
}

U8* 
initialize_zerou8(U8 *p, int tamanho)
{
	for (int i = 0; i < tamanho; i++) {
        	*(p + i) = 0;
    	}
    	return p;
}

char* 
initialize_zerochar(char *p, int tamanho)
{
	for (int i = 0; i < tamanho; i++) {
        	*(p + i) = '\0';
    	}
    	return p;
}

int
opnewarray(Frame *frame)
{
	Value v1;
	v1.i = frame->code->code[frame->pc++];
	int atype = v1.i;
	Value v;
	v = frame_stackpop(frame);
	
	Value x;
	void *p;
	
	switch (atype){
		case T_BOOLEAN :
			p = ecalloc(v.i, sizeof (U1));
			p = initialize_zerou1(p, v.i); 
			x.a = p;
			frame_stackpush(frame, x);
			return 0;
			break;
		case T_CHAR :
			p = ecalloc(v.i, sizeof (U2));
			p = initialize_zerochar(p, v.i); 
			x.a = p;
			frame_stackpush(frame, x);
			return 0;
			break;
		case T_FLOAT :
			p = ecalloc(v.i, sizeof (U4));
			p = initialize_zerou4(p, v.i); 
			x.a = p;
			frame_stackpush(frame, x);
			return 0;
			break;
		case T_DOUBLE :
			p = ecalloc(v.i, sizeof (U8));
			p = initialize_zerou8(p, v.i); 
			x.a = p;
			frame_stackpush(frame, x);
			return 0;
			break;
		case T_BYTE :
			p = ecalloc(v.i, sizeof (U1));
			p = initialize_zerou1(p, v.i); 
			x.a = p;
			frame_stackpush(frame, x);
			return 0;
			break;
		case T_SHORT :
			p = ecalloc(v.i, sizeof (U2));
			p = initialize_zerou2(p, v.i); 
			x.a = p;
			frame_stackpush(frame, x);
			return 0;
			break;
		case T_INT :
			p = ecalloc(v.i, sizeof (U4));
			p = initialize_zerou4(p, v.i); 
			x.a = p;
			frame_stackpush(frame, x);
			return 0;
			break;
		case T_LONG :
			p = ecalloc(v.i, sizeof (U8));
			p = initialize_zerou8(p, v.i); 
			x.a = p;
			frame_stackpush(frame, x);
			return 0;
			break;
	}
	return 0;
}

void*
initialize_zerov(void *p, int tamanho)
{
	for (int i = 0; i < tamanho; i++) {
        	*(p + i).class_index = 0;
        	*(p + i).name_and_type_index = 0;
    	}
    	return p;
}

int
anewarray(Frame *frame){
	
	CP *cp;
	
	Value v;
	U2 i;
	Value v1;
	v1.i = frame->code->code[frame->pc++];

	i = frame->code->code[frame->pc++] << 8;
	i = frame->code->code[frame->pc++];
	cp = &frame->class->constant_pool[i].info;
	switch (cp){
		case CONSTANT_Methodref_info: 
			cp = &frame->class->constant_pool[i].info.methodref;
		case CONSTANT_InterfaceMethodref_info: 
			cp = &frame->class->constant_pool[i].info.interfacemethodref;
				
	}
	v = resolvefield(frame->class, cp);
	
	Value x;
	void *p;
	p = ecalloc(v.i, sizeof (v));
	p = initialize_zerov(p, v); 
	x.a = p;
	frame_stackpush(frame, x);

}

int 
opmultianewarray(Frame *frame)
{
	U2 index;
	unsigned int dimension;

	index = frame->code->code[frame->pc++] << 8;
	index = frame->code->code[frame->pc++];
	
	dimension = frame->code->code[frame->pc++];
	if (dimension == 0){
		return 1;
		//exception;
	}
	
	else if (dimension == 1){
		opnewarray(frame);
	}
	else {
		while (dimension > 1){
			
		} 
	
	}
	return 0;
		
	
	
}

/*____________________________________________________________________________________________________________________*/


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
	Value v;
	U2 i;

	i = frame->code->code[frame->pc++];
	v = resolveconstant(frame->class, i);
	frame_stackpush(frame, v);
	return 0;
}

/* ldc_w: push item from run-time constant pool (wide index) */
int
opldc_w(Frame *frame)
{
	Value v;
	U2 i;

	i = frame->code->code[frame->pc++] << 8;
	i |= frame->code->code[frame->pc++];
	v = resolveconstant(frame->class, i);
	frame_stackpush(frame, v);
	return 0;
}

/* ldc2_w: push long or double from run-time constant pool (wide index) */
int
opldc2_w(Frame *frame)
{
	Value v;
	U2 i;

	i = frame->code->code[frame->pc++] << 8;
	i |= frame->code->code[frame->pc++];
	v = resolveconstant(frame->class, i);
	frame_stackpush(frame, v);
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
		[ICONST_M1]       = opiconst_m1,
		[ICONST_0]        = opiconst_0,
		[ICONST_1]        = opiconst_1,
		[ICONST_2]        = opiconst_2,
		[ICONST_3]        = opiconst_3,
		[ICONST_4]        = opiconst_4,
		[ICONST_5]        = opiconst_5,
		[LCONST_0]        = oplconst_0,
		[LCONST_1]        = oplconst_1,
		[FCONST_0]        = opfconst_0,
		[FCONST_1]        = opfconst_1,
		[FCONST_2]        = opfconst_2,
		[DCONST_0]        = opdconst_0,
		[DCONST_1]        = opdconst_1,
		[BIPUSH]          = opbipush,
		[SIPUSH]          = opnop,
		[LDC]             = opldc,
		[LDC_W]           = opldc_w,
		[LDC2_W]          = opldc2_w,
		[ILOAD]           = opiload,
		[LLOAD]           = oplload,
		[FLOAD]           = opfload,
		[DLOAD]           = opdload,
		[ALOAD]           = opnop,
		[ILOAD_0]         = opiload_0,
		[ILOAD_1]         = opiload_1,
		[ILOAD_2]         = opiload_2,
		[ILOAD_3]         = opiload_3,
		[LLOAD_0]         = oplload_0,
		[LLOAD_1]         = oplload_1,
		[LLOAD_2]         = oplload_2,
		[LLOAD_3]         = oplload_3,
		[FLOAD_0]         = opfload_0,
		[FLOAD_1]         = opfload_1,
		[FLOAD_2]         = opfload_2,
		[FLOAD_3]         = opfload_3,
		[DLOAD_0]         = opdload_0,
		[DLOAD_1]         = opdload_1,
		[DLOAD_2]         = opdload_2,
		[DLOAD_3]         = opdload_3,
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
		[ISTORE]          = opistore,
		[LSTORE]          = oplstore,
		[FSTORE]          = opfstore,
		[DSTORE]          = opdstore,
		[ASTORE]          = opnop,
		[ISTORE_0]        = opistore_0,
		[ISTORE_1]        = opistore_1,
		[ISTORE_2]        = opistore_2,
		[ISTORE_3]        = opistore_3,
		[LSTORE_0]        = oplstore_0,
		[LSTORE_1]        = oplstore_1,
		[LSTORE_2]        = oplstore_2,
		[LSTORE_3]        = oplstore_3,
		[FSTORE_0]        = opfstore_0,
		[FSTORE_1]        = opfstore_1,
		[FSTORE_2]        = opfstore_2,
		[FSTORE_3]        = opfstore_3,
		[DSTORE_0]        = opdstore_0,
		[DSTORE_1]        = opdstore_1,
		[DSTORE_2]        = opdstore_2,
		[DSTORE_3]        = opdstore_3,
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
		[POP]             = oppop,
		[POP2]            = oppop2,
		[DUP]             = opdup,
		[DUP_X1]          = opdup_x1,
		[DUP_X2]          = opdup_x2,
		[DUP2]            = opdup2,
		[DUP2_X1]         = opdup2_x1,
		[DUP2_X2]         = opdup2_x2,
		[SWAP]            = opnop,
		[IADD]            = opiadd,
		[LADD]            = opladd,
		[FADD]            = opfadd,
		[DADD]            = opdadd,
		[ISUB]            = opisub,
		[LSUB]            = oplsub,
		[FSUB]            = opfsub,
		[DSUB]            = opdsub,
		[IMUL]            = opimul,
		[LMUL]            = oplmul,
		[FMUL]            = opfmul,
		[DMUL]            = opdmul,
		[IDIV]            = opidiv,
		[LDIV]            = opldiv,
		[FDIV]            = opfdiv,
		[DDIV]            = opddiv,
		[IREM]            = opirem,
		[LREM]            = oplrem,
		[FREM]            = opfrem,
		[DREM]            = opdrem,
		[INEG]            = opineg,
		[LNEG]            = oplneg,
		[FNEG]            = opfneg,
		[DNEG]            = opdneg,
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
		[NEWARRAY]        = opnewarray,
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
		} else {
			usage();
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
