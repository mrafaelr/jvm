/* local variable or operand structure */
typedef union Value {
	U2      u;
	int32_t i;
	char **a;
	char ***mat;
	int64_t l;
	float   f;
	double  d;
	void   *v;
	int   *in;
	char *a1;
} Value;

/* virtual machine frame structure */
typedef struct Frame {
	struct Frame           *next;
	struct ClassFile       *class;  /* constant pool */
	union  Value           *local;  /* local variable table */
	union  Value           *stack;  /* operand stack */
	size_t                  nstack; /* number of values on operand stack */
	struct Code_attribute  *code;   /* array of instructions */
	U2                      pc;     /* program counter */
} Frame;

Frame *frame_push(Code_attribute *code, ClassFile *class);
int frame_pop(void);
void frame_del(void);
void frame_stackpush(Frame *frame, Value value);
Value frame_stackpop(Frame *frame);
void frame_localstore(Frame *frame, U2 i, Value v);
Value frame_localload(Frame *frame, U2 i);
Value frame_localload4(Frame *frame, U4 i);
