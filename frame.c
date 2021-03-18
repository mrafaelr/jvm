#include <stdint.h>
#include <stdlib.h>
#include "class.h"
#include "frame.h"

static Frame *framestack = NULL;

/* allocate frame; push it onto framestack; and return it */
Frame *
frame_push(Code_attribute *code, CP *cp)
{
	Frame *frame = NULL;
	Value *local = NULL;
	Value *stack = NULL;

	frame = malloc(sizeof *frame);
	local = calloc(code->max_locals, sizeof *local);
	stack = calloc(code->max_stack, sizeof *stack);
	if (frame == NULL || local == NULL || stack == NULL) {
		free(frame);
		free(local);
		free(stack);
		return NULL;
	}
	frame->pc = 0;
	frame->cp = cp;
	frame->local = local;
	frame->stack = stack;
	frame->next = framestack;
	framestack = frame;
	return frame;
}

/* pop and free frame from framestack; return -1 on error */
int
frame_pop(void)
{
	Frame *frame;

	if (framestack == NULL)
		return -1;
	frame = framestack;
	framestack = frame->next;
	free(frame->local);
	free(frame->stack);
	free(frame);
	return 0;
}

/* pop and free all frames from framestack */
void
frame_del(void)
{
	while (framestack) {
		frame_pop();
	}
}
