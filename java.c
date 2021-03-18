#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "class.h"
#include "file.h"
#include "util.h"
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

/* path separator */
#ifdef _WIN32
#define PATHSEP ';'
#else
#define PATHSEP ':'
#endif

static char **classpath = NULL;         /* NULL-terminated array of path strings */
static ClassFile *classes = NULL;       /* list of loaded classes */

/* show usage */
static void
usage(void)
{
	(void)fprintf(stderr, "usage: java [-cp classpath] class\n");
	exit(EXIT_FAILURE);
}

/* break cpath into paths and set classpath global variable */
static void
parsecpath(char *cpath)
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

/* check if a class with the given name is loaded */
static ClassFile *
getclass(char *classname)
{
	ClassFile *class;

	for (class = classes; class; class = class->next)
		if (strcmp(classname, getclassname(class, class->this_class)) == 0)
			return class;
	return NULL;
}

/* free all the classes in the list of classes */
static void
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
static ClassFile *
classload(char *classname)
{
	ClassFile *class, *super, *tmp;
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
	if (strcmp(getclassname(class, class->this_class), classname) != 0) {
		free(class);
		errx(EXIT_FAILURE, "could not find class %s", classname);
	}
	class->next = classes;
	classes = class;
	if (!istopclass(class)) {
		super = classload(getclassname(class, class->super_class));
		for (tmp = super; tmp; tmp = getclass(getclassname(tmp, tmp->super_class))) {
			if (strcmp(getclassname(class, class->this_class),
			           getclassname(tmp, tmp->this_class)) == 0) {
				errx(EXIT_FAILURE, "class circularity error");
			}
		}
	}
	return class;
}

/* launches a java application */
int
main(int argc, char *argv[])
{
	ClassFile *class;
	Method *method;
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
	if (cpath == NULL)
		cpath = ".";
	parsecpath(cpath);
	atexit(classfree);
	class = classload(argv[i]);
	if ((method = getmethod(class, "main", "([Ljava/lang/String;)V")) == NULL)
		errx(EXIT_FAILURE, "could not find method main in class %s", argv[i]);
}
