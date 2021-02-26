#include <stdint.h>

typedef uint8_t  U1;
typedef uint16_t U2;
typedef uint32_t U4;

typedef enum ConstantTag {
	CONSTANT_Untagged           = 0,
	CONSTANT_Utf8               = 1,
	CONSTANT_Integer            = 3,
	CONSTANT_Float              = 4,
	CONSTANT_Long               = 5,
	CONSTANT_Double             = 6,
	CONSTANT_Class              = 7,
	CONSTANT_String             = 8,
	CONSTANT_Fieldref           = 9,
	CONSTANT_Methodref          = 10,
	CONSTANT_InterfaceMethodref = 11,
	CONSTANT_NameAndType        = 12,
	CONSTANT_MethodHandle       = 15,
	CONSTANT_MethodType         = 16,
	CONSTANT_InvokeDynamic      = 18
} ConstantTag;

typedef enum AccessFlags {
	ACC_PUBLIC       = 0x0001,
	ACC_PRIVATE      = 0x0002,
	ACC_PROTECTED    = 0x0004,
	ACC_STATIC       = 0x0008,
	ACC_FINAL        = 0x0010,
	ACC_SUPER        = 0x0020,
	ACC_SYNCHRONIZED = 0x0020,
	ACC_VOLATILE     = 0x0040,
	ACC_BRIDGE       = 0x0040,
	ACC_TRANSIENT    = 0x0080,
	ACC_VARARGS      = 0x0080,
	ACC_NATIVE       = 0x0100,
	ACC_INTERFACE    = 0x0200,
	ACC_ABSTRACT     = 0x0400,
	ACC_STRICT       = 0x0800,
	ACC_SYNTHETIC    = 0x1000,
	ACC_ANNOTATION   = 0x2000,
	ACC_ENUM         = 0x4000
} AccessFlags;

typedef enum AttributeTag {
	UnknownAttribute,
	ConstantValue,
	Code,
	Deprecated,
	Exceptions,
	InnerClasses,
	SourceFile,
	Synthetic,
	LineNumberTable,
	LocalVariableTable
} AttributeTag;

typedef struct CONSTANT_Utf8_info {
	U2      length;
	char   *bytes;
} CONSTANT_Utf8_info;

typedef struct CONSTANT_Integer_info {
	U4      bytes;
} CONSTANT_Integer_info;

typedef struct CONSTANT_Float_info {
	U4      bytes;
} CONSTANT_Float_info;

typedef struct CONSTANT_Long_info {
	U4      high_bytes;
	U4      low_bytes;
} CONSTANT_Long_info;

typedef struct CONSTANT_Double_info {
	U4      high_bytes;
	U4      low_bytes;
} CONSTANT_Double_info;

typedef struct CONSTANT_Class_info {
	U2      name_index;
} CONSTANT_Class_info;

typedef struct CONSTANT_String_info {
	U2      string_index;
} CONSTANT_String_info;

typedef struct CONSTANT_Fieldref_info {
	U2      class_index;
	U2      name_and_type_index;
} CONSTANT_Fieldref_info;

typedef struct CONSTANT_Methodref_info {
	U2      class_index;
	U2      name_and_type_index;
} CONSTANT_Methodref_info;

typedef struct CONSTANT_InterfaceMethodref_info {
	U2      class_index;
	U2      name_and_type_index;
} CONSTANT_InterfaceMethodref_info;

typedef struct CONSTANT_NameAndType_info {
	U2      name_index;
	U2      descriptor_index;
} CONSTANT_NameAndType_info;

typedef struct CONSTANT_MethodHandle_info {
	U1      reference_kind;
	U2      reference_index;
} CONSTANT_MethodHandle_info;

typedef struct CONSTANT_MethodType_info {
	U2      descriptor_index;
} CONSTANT_MethodType_info;

typedef struct CONSTANT_InvokeDynamic_info {
	U2      bootstrap_method_attr_index;
	U2      name_and_type_index;
} CONSTANT_InvokeDynamic_info;

typedef struct ConstantValue_attribute {
	U2      constantvalue_index;
} ConstantValue_attribute;

typedef struct Code_attribute {
	U2                      max_stack;
	U2                      max_locals;
	U4                      code_length;
	U1                     *code;
	U2                      exception_table_length;
	struct Exception       *exception_table;
	U2                      attributes_count;
	struct Attribute       *attributes;
} Code_attribute;

typedef struct Exceptions_attribute {
	U2                      number_of_exceptions;
	U2                     *exception_index_table;
} Exceptions_attribute;

typedef struct InnerClasses_attribute {
	U2                      number_of_classes;
	struct InnerClass      *classes;
} InnerClasses_attribute;

typedef struct SourceFile_attribute {
	U2                      sourcefile_index;
} SourceFile_attribute;

typedef struct LineNumberTable_attribute {
	U2                      line_number_table_length;
	struct LineNumber      *line_number_table;
} LineNumberTable_attribute;

typedef struct LocalVariableTable_attribute {
	U2                      local_variable_table_length;
	struct LocalVariable   *local_variable_table;
} LocalVariableTable_attribute;

typedef struct CP {
	U1      tag;
	union {
		struct CONSTANT_Utf8_info               utf8_info;
		struct CONSTANT_Integer_info            integer_info;
		struct CONSTANT_Float_info              float_info;
		struct CONSTANT_Long_info               long_info;
		struct CONSTANT_Double_info             double_info;
		struct CONSTANT_Class_info              class_info;
		struct CONSTANT_String_info             string_info;
		struct CONSTANT_Fieldref_info           fieldref_info;
		struct CONSTANT_Methodref_info          methodref_info;
		struct CONSTANT_InterfaceMethodref_info interfacemethodref_info;
		struct CONSTANT_NameAndType_info        nameandtype_info;
		struct CONSTANT_MethodHandle_info       methodhandle_info;
		struct CONSTANT_MethodType_info         methodtype_info;
		struct CONSTANT_InvokeDynamic_info      invokedynamic_info;
	}       info;
} CP;

typedef struct Attribute {
	enum AttributeTag                               tag;
	union {
		struct ConstantValue_attribute          constantvalue;
		struct Code_attribute                   code;
		struct Exceptions_attribute             exceptions;
		struct InnerClasses_attribute           innerclasses;
		struct SourceFile_attribute             sourcefile;
		struct LineNumberTable_attribute        linenumbertable;
		struct LocalVariableTable_attribute     localvariabletable;
	}       info;
} Attribute;

typedef struct Field {
	U2                      access_flags;
	U2                      name_index;
	U2                      descriptor_index;
	U2                      attributes_count;
	struct Attribute       *attributes;
} Field;

typedef struct Method {
	U2                      access_flags;
	U2                      name_index;
	U2                      descriptor_index;
	U2                      attributes_count;
	struct Attribute       *attributes;
} Method;

typedef struct Exception {
	U2      start_pc;
	U2      end_pc;
	U2      handler_pc;
	U2      catch_type;
} Exception;

typedef struct InnerClass {
	U2      inner_class_info_index;
	U2      outer_class_info_index;
	U2      inner_name_index;
	U2      inner_class_access_flags;
} InnerClass;

typedef struct LineNumber {
	U2      start_pc;
	U2      line_number;
} LineNumber;

typedef struct LocalVariable {
	U2      start_pc;
	U2      length;
	U2      name_index;
	U2      descriptor_index;
	U2      index;
} LocalVariable;

typedef struct ClassFile {
	U2                minor_version;
	U2                major_version;
	U2                constant_pool_count;
	struct CP        *constant_pool;
	U2                access_flags;
	U2                this_class;
	U2                super_class;
	U2                interfaces_count;
	U2               *interfaces;
	U2                fields_count;
	struct Field     *fields;
	U2                methods_count;
	struct Method    *methods;
	U2                attributes_count;
	struct Attribute *attributes;
} ClassFile;

char *class_strerr(enum ErrorTag errtag);
