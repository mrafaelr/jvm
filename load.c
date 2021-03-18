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

/* path separator */
#ifdef _WIN32
#define PATHSEP ';'
#else
#define PATHSEP ':'
#endif

static char **classpath = NULL;         /* NULL-terminated array of path strings */
static ClassFile *classes = NULL;       /* list of loaded classes */

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
load_setpath(char *cpath)
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

/* free all the classes in the list of classes */
void
load_free(void)
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
load_class(char *classname)
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
		class->super = load_class(class_getclassname(class, class->super_class));
		for (tmp = class->super; tmp; tmp = tmp->super) {
			if (strcmp(class_getclassname(class, class->this_class),
			           class_getclassname(tmp, tmp->this_class)) == 0) {
				errx(EXIT_FAILURE, "class circularity error");
			}
		}
	}
	return class;
}
