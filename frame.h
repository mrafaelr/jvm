/* local variable or operand structure */
typedef union Value {
	char    c;
	uint8_t b;
	int16_t s;
	int32_t i;
	int64_t l;
	float   f;
	double  d;
} Value;

/* virtual machine frame structure */
typedef struct Frame {
	struct Frame           *next;
	struct ClassFile       *class;  /* constant pool */
	union  Value           *local;  /* local variable table */
	union  Value           *stack;  /* operand stack */
	struct Code_attribute  *code;   /* array of instructions */
	U2                      pc;     /* program counter */
} Frame;

Frame *frame_push(Code_attribute *code, ClassFile *class);
int frame_pop(void);
void frame_del(void);
