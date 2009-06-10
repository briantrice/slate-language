
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#ifdef WIN32
// The following must be obtained from http://code.google.com/p/msinttypes/ for Windows:
#include "stdint.h"
#include "inttypes.h"
#else
#include <stdint.h>
#include <inttypes.h>
#endif
#include <fcntl.h>
#include <time.h>
#include <limits.h>
#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
//typedef size_t socklen_t;
typedef signed int ssize_t;
typedef SOCKADDR sockaddr_un;
#else
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/un.h>
#include <pthread.h>
#endif

#include <dirent.h>  // on Windows, download from http://www.softagalleria.net/dirent.php

/* SLATE_BUILD_TYPE should be set by the build system (Makefile, VS project): */
#ifndef SLATE_BUILD_TYPE
#define SLATE_BUILD_TYPE "UNDEFINED"
#endif

typedef uintptr_t uword_t;
typedef intptr_t word_t;
typedef uint8_t byte_t;
typedef word_t bool_t;
typedef float float_type;

#define WORDT_MAX INTPTR_MAX

#define KB 1024
#define MB 1024 * 1024
#define GB 1024 * 1024 * 1024

#define ASSERT(x) assert(x)
#define str(s) #s
#define xstr(s) str(s)
#define MAX(a, b) (a > b ? a : b)
#define MIN(a, b) (a < b ? a : b)

struct SlotTable;
struct Symbol;
struct CompiledMethod;
struct LexicalContext;
struct RoleTable;
struct OopArray;
struct MethodDefinition;
struct Object;
struct ByteArray;
struct MethodCacheEntry;
struct Closure;
struct Interpreter;
struct ForwardedObject;
struct Map;
struct BreakEntry;
struct PrimitiveMethod;
struct RoleEntry;
struct SlotEntry;

struct slate_image_header {
  word_t magic;
  word_t size;
  word_t next_hash;
  word_t special_objects_oop;
  word_t current_dispatch_id;
};
struct Object
{
  word_t header;
  word_t objectSize; /*in words... used for slot storage*/
  word_t payloadSize; /*in bytes... used for ooparray and bytearray elements*/
  /*put the map after*/
  struct Map * map;

};
/* when we clone objects, make sure we don't overwrite the things within headersize. 
 * we might clone with a new size, and we don't want to set the size back.
 */
#define HEADER_SIZE (sizeof(struct Object) - sizeof(struct Map*))
#define HEADER_SIZE_WORDS (HEADER_SIZE/sizeof(word_t))
#define SLATE_IMAGE_MAGIC (word_t)0xABCDEF43

/*this doesn't seem to ensure anything*/
#define GC_VOLATILE /*volatile*/

#ifdef _MSC_VER
	#define SLATE_INLINE 
	#pragma warning(disable : 4996)
#else
	#define SLATE_INLINE inline
#endif

#define METHOD_CACHE_ARITY 6

struct MethodCacheEntry
{
  struct MethodDefinition * method;
  struct Symbol* selector;
  struct Map * maps[METHOD_CACHE_ARITY];
};
struct ForwardedObject
{
  word_t header;
  word_t objectSize; 
  word_t payloadSize; 
  struct Object * target; /*rather than the map*/
};
struct ForwardPointerEntry /*for saving images*/
{
  struct Object* fromObj;
  struct Object* toObj;
};
struct Map
{
  struct Object base;
  struct Object* flags;
  struct Object* representative;
  struct OopArray * delegates;
  struct Object* slotCount;
  struct SlotTable * slotTable;
  struct RoleTable * roleTable;
  word_t dispatchID;
  word_t visitedPositions;
};
struct BreakEntry
{
  word_t oldAddress;
  word_t newAddress;
};
struct PrimitiveMethod
{
  struct Object base;
  struct Object* index;
  struct Object* selector;
  struct Object* inputVariables;
};
struct RoleEntry
{
  struct Symbol* name;
  struct Object* rolePositions; /*smallint?*/
  struct MethodDefinition * methodDefinition;
  struct Object* nextRole; /*smallint?*/
};
struct SlotEntry
{
  struct Symbol* name;
  struct Object* offset; /*smallint?*/
};
struct SlotTable
{
  struct Object base;
  struct SlotEntry slots[];
};
struct Symbol
{
  struct Object base;
  struct Object* cacheMask;
  byte_t elements[];
};
struct CompiledMethod
{
  struct Object base;
  struct CompiledMethod * method;
  struct Symbol* selector;
  struct Object* inputVariables;
  struct Object* localVariables;
  struct Object* restVariable;
  struct OopArray * optionalKeywords;
  struct Object* heapAllocate;
  struct Object* environment;
  struct OopArray * literals;
  struct OopArray * selectors;
  struct OopArray * code;
  word_t sourceTree;
  word_t debugMap;
  struct Object* isInlined;
  struct OopArray* oldCode;
  struct Object* callCount;
  /* calleeCount is the polymorphic inline cache (PIC), see #defines/method_pic_add_callee below for details */
  struct OopArray* calleeCount;
  struct Object* registerCount;
  struct OopArray* cachedInCallers; /*struct Object* reserved2;*/
  struct Object* cachedInCallersCount; /*struct Object* reserved3;*/
  struct Object* reserved4;
  struct Object* reserved5;
  struct Object* reserved6;
};
struct LexicalContext
{
  struct Object base;
  struct Object* framePointer;
  struct Object* variables[];
};
struct RoleTable
{
  struct Object base;
  struct RoleEntry roles[];
};
struct OopArray
{
  struct Object base;
  struct Object* elements[];
};
struct MethodDefinition
{
  struct Object base;
  struct Object* method;
  struct Object* slotAccessor;
  word_t dispatchID;
  word_t dispatchPositions;
  word_t foundPositions;
  word_t dispatchRank;
};

struct ByteArray
{
  struct Object base;
  byte_t elements[];
};
struct Closure
{
  struct Object base;
  struct CompiledMethod * method;
  struct LexicalContext * lexicalWindow[];
};
struct Interpreter /*note the bottom fields are treated as contents in a bytearray so they don't need to be converted from objects to get the actual smallint value*/
{
  struct Object base;
  struct OopArray * stack;
  struct CompiledMethod * method;
  struct Closure * closure;
  struct LexicalContext * lexicalContext;
  struct Object* ensureHandlers;
  /*everything below here is in the interpreter's "byte array" so we don't need to translate from oop to int*/
  word_t framePointer;
  word_t codePointer;
  word_t codeSize;
  word_t stackPointer;
  word_t stackSize;
};

#define SLATE_ERROR_RETURN (-1)
#define SLATE_FILE_NAME_LENGTH 512
#define DELEGATION_STACK_SIZE 256
#define PROFILER_ENTRY_COUNT 4096
#define MAX_FIXEDS 64
#define MARK_MASK 1
#define METHOD_CACHE_SIZE 1024*64
#define PINNED_CARD_SIZE (sizeof(word_t) * 8)
#define SLATE_MEMS_MAXIMUM 1024
#define SLATE_NETTICKET_MAXIMUM 1024
#define SLATE_FILES_MAXIMUM 256
#define SLATE_DIRECTORIES_MAXIMUM 256
#define MAX_PLATFORM_STRING_LENGTH 256

#ifndef WIN32
#include <sys/utsname.h>
#else
typedef struct utsname {
  char sysname[MAX_PLATFORM_STRING_LENGTH];
  char nodename[MAX_PLATFORM_STRING_LENGTH];
  char release[MAX_PLATFORM_STRING_LENGTH];
  char version[MAX_PLATFORM_STRING_LENGTH];
  char machine[MAX_PLATFORM_STRING_LENGTH];
};
int uname(struct utsname *un);
#endif

#ifdef WIN32
int getpid();
#endif


/*these things below never exist in slate land (so word_t types are their actual value)*/

struct object_heap;

struct slate_addrinfo_request {

  /*these first two must be freed individually also*/
  char* hostname;
  char* service;

  word_t family, type, protocol, flags;

  /*above are filled in by primitive requester, belowe are filled in by slate's socket_getaddrinfo*/
  struct object_heap* oh;
  word_t result, ticketNumber;
  word_t inUse; /*whether this whole thing is garbage*/
  word_t finished; /*whether getaddrinfo has returned... set this to zero before spawning the thread*/
  struct addrinfo* addrResult; /*this needs to be freed with freeaddrinfo*/
};


struct slate_profiler_entry {
  struct Object* method; /*null if non-active entry.. this must point to an old generation object (since they don't move)*/
  word_t callCount; /*this is not a small int... it needs to be converted*/
  word_t callTime; /*total time spent in this method.... fixme add code for this*/
};


struct object_heap
{
  byte_t mark_color;
  byte_t * memoryOld;
  byte_t * memoryYoung;
  word_t memoryOldSize;
  word_t memoryYoungSize;
  word_t memoryOldLimit;
  word_t memoryYoungLimit;
  struct OopArray* special_objects_oop; /*root for gc*/
  word_t current_dispatch_id;
  bool_t interrupt_flag;
  bool_t quiet; /*suppress excess stdout*/
  word_t lastHash;
  word_t method_cache_hit, method_cache_access;
  word_t gcTenureCount;

  char** envp;

  /* contains all the file handles */
  FILE** file_index;
  word_t file_index_size;

  /* containts all the directory handles */
  DIR** dir_index;
  word_t dir_index_size;

  struct Object *nextFree;
  struct Object *nextOldFree;
  struct Object* delegation_stack[DELEGATION_STACK_SIZE];
  struct MethodCacheEntry methodCache[METHOD_CACHE_SIZE];
  struct Object* fixedObjects[MAX_FIXEDS];

  struct CompiledMethod** optimizedMethods;
  word_t optimizedMethodsSize;
  word_t optimizedMethodsLimit;

  /* memory areas for the primitive memory functions */
  void* memory_areas [SLATE_MEMS_MAXIMUM];
  int memory_sizes [SLATE_MEMS_MAXIMUM];
  int memory_num_refs [SLATE_MEMS_MAXIMUM];

  struct slate_addrinfo_request*  socketTickets;
  int socketTicketCount;
#ifdef WIN32
  HANDLE WINAPI socketThreadMutex;
#if 0
  pthread_mutex_t socketTicketMutex;
#endif
#endif

  int argcSaved;
  char** argvSaved;

  struct utsname platform_info;

  struct Object** markStack;
  size_t markStackSize;
  size_t markStackPosition;

  word_t* pinnedYoungObjects; /* scan the C stack for things we can't move */
  word_t* rememberedYoungObjects; /* old gen -> new gen pointers for incremental GC */
  void* stackBottom;

  bool_t currentlyProfiling;
  word_t currentlyProfilingIndex;
  int64_t profilerTimeStart, profilerTimeEnd;
  struct slate_profiler_entry profiler_entries[PROFILER_ENTRY_COUNT];

  /*
   * I call this cached because originally these could move around
   * memory, but now the old objects memory block doesn't move any of
   * its objects around.  The old GC did compaction. The new GC only
   * compacts YoungSpace.
   */

  struct {
    struct Interpreter* interpreter;
    struct Object* true_object;
    struct Object* false_object;
    struct Object* nil;
    struct Object* primitive_method_window;
    struct Object* compiled_method_window;
    struct Object* closure_method_window;
  } cached;
};




#define SMALLINT_MASK 0x1
#define ID_HASH_RESERVED 0x7FFFF0
#define ID_HASH_FORWARDED ID_HASH_RESERVED
#define ID_HASH_FREE 0x7FFFFF
#define ID_HASH_MAX ID_HASH_FREE


#define FLOAT_SIGNIFICAND 0x7FFFFF
#define FLOAT_EXPONENT_OFFSET 23



/*obj map flags is a smallint oop, so we start after the smallint flag*/
#define MAP_FLAG_RESTRICT_DELEGATION 2
#define MAP_FLAG_IMMUTABLE 4


#define WORD_BYTES_MINUS_ONE (sizeof(word_t)-1)
#define ROLE_ENTRY_WORD_SIZE ((sizeof(struct RoleEntry) + WORD_BYTES_MINUS_ONE) / sizeof(word_t))
#define SLOT_ENTRY_WORD_SIZE ((sizeof(struct SlotEntry) + WORD_BYTES_MINUS_ONE) / sizeof(word_t))
#define FUNCTION_FRAME_SIZE 6

#define TRUE 1
#define FALSE 0

#define TYPE_OBJECT 0
#define TYPE_OOP_ARRAY  1
#define TYPE_BYTE_ARRAY 2

#define CALLER_PIC_SETUP_AFTER 500
#define CALLEE_OPTIMIZE_AFTER 1000
#define CALLER_PIC_SIZE 64
#define CALLER_PIC_ENTRY_SIZE 4 /*calleeCompiledMethod, calleeArity, oopArrayOfMaps, count*/
#define PIC_CALLEE 0 
#define PIC_CALLEE_ARITY 1
#define PIC_CALLEE_MAPS 2
#define PIC_CALLEE_COUNT 3

#define SPECIAL_OOP_LOBBY 0
#define SPECIAL_OOP_NO_ROLE 1
#define SPECIAL_OOP_NIL 2
#define SPECIAL_OOP_TRUE 3
#define SPECIAL_OOP_FALSE 4
#define SPECIAL_OOP_ARRAY_PROTO 5
#define SPECIAL_OOP_BYTE_ARRAY_PROTO 6
#define SPECIAL_OOP_ASCII_PROTO 7
#define SPECIAL_OOP_MAP_PROTO 8
#define SPECIAL_OOP_METHOD_DEF_PROTO 9
#define SPECIAL_OOP_SMALL_INT_PROTO 10
#define SPECIAL_OOP_FLOAT_PROTO 11
#define SPECIAL_OOP_CLOSURE_WINDOW 12
#define SPECIAL_OOP_COMPILED_METHOD_WINDOW 13
#define SPECIAL_OOP_PRIMITIVE_METHOD_WINDOW 14
#define SPECIAL_OOP_CLOSURE_PROTO 15
#define SPECIAL_OOP_LEXICAL_CONTEXT_PROTO 16
#define SPECIAL_OOP_INTERPRETER 17
#define SPECIAL_OOP_ENSURE_MARKER 18
#define SPECIAL_OOP_NOT_FOUND_ON 19
#define SPECIAL_OOP_NOT_FOUND_ON_AFTER 20
#define SPECIAL_OOP_WRONG_INPUTS_TO 21
#define SPECIAL_OOP_MAY_NOT_RETURN_TO 22
#define SPECIAL_OOP_SLOT_NOT_FOUND_NAMED 23
#define SPECIAL_OOP_KEY_NOT_FOUND_ON 24
#define SPECIAL_OOP_IMMUTABLE 25
#define SPECIAL_OOP_BIT_SHIFT_OVERFLOW 26
#define SPECIAL_OOP_ADD_OVERFLOW 27 
#define SPECIAL_OOP_SUBTRACT_OVERFLOW 28
#define SPECIAL_OOP_MULTIPLY_OVERFLOW 29
#define SPECIAL_OOP_DIVIDE_BY_ZERO 30
#define SPECIAL_OOP_NOT_A_BOOLEAN 31
#define SPECIAL_OOP_APPLY_TO 32
#define SPECIAL_OOP_OPTIONALS 33
#define SPECIAL_OOP_TYPE_ERROR_ON 34
#define SPECIAL_OOP_COUNT 35

#define SF_READ				1
#define SF_WRITE			1 << 1
#define SF_CREATE			1 << 2
#define SF_CLEAR			1 << 3


#define SLATE_DOMAIN_LOCAL  1
#define SLATE_DOMAIN_IPV4   2
#define SLATE_DOMAIN_IPV6   3

#define SLATE_TYPE_STREAM   1

#define SLATE_PROTOCOL_DEFAULT 0



#ifdef PRINT_DEBUG_OPCODES
#define PRINTOP(X) printf(X)
#else
#define PRINTOP(X)
#endif

#define inc_ptr(xxx, yyy)     ((byte_t*)xxx + yyy)




#define OP_SEND                         ((0 << 1) | SMALLINT_MASK)
#define OP_INDIRECT_SEND                ((1 << 1) | SMALLINT_MASK) /*unused now*/
/*#define OP_ALLOCATE_REGISTERS           ((2 << 1) | SMALLINT_MASK)*/
#define OP_LOAD_LITERAL                 ((3 << 1) | SMALLINT_MASK)
#define OP_STORE_LITERAL                ((4 << 1) | SMALLINT_MASK)
#define OP_SEND_MESSAGE_WITH_OPTS       ((5 << 1) | SMALLINT_MASK)
/*?? profit??*/
#define OP_NEW_CLOSURE                  ((7 << 1) | SMALLINT_MASK)
#define OP_NEW_ARRAY_WITH               ((8 << 1) | SMALLINT_MASK)
#define OP_RESEND_MESSAGE               ((9 << 1) | SMALLINT_MASK)
#define OP_RETURN_FROM                  ((10 << 1) | SMALLINT_MASK)
#define OP_LOAD_ENVIRONMENT             ((11 << 1) | SMALLINT_MASK)
#define OP_LOAD_VARIABLE                ((12 << 1) | SMALLINT_MASK)
#define OP_STORE_VARIABLE               ((13 << 1) | SMALLINT_MASK)
#define OP_LOAD_FREE_VARIABLE           ((14 << 1) | SMALLINT_MASK)
#define OP_STORE_FREE_VARIABLE          ((15 << 1) | SMALLINT_MASK)
#define OP_IS_IDENTICAL_TO              ((16 << 1) | SMALLINT_MASK)
#define OP_BRANCH_KEYED                 ((17 << 1) | SMALLINT_MASK)
#define OP_JUMP_TO                      ((18 << 1) | SMALLINT_MASK)
#define OP_MOVE_REGISTER                ((19 << 1) | SMALLINT_MASK)
#define OP_BRANCH_IF_TRUE               ((20 << 1) | SMALLINT_MASK)
#define OP_BRANCH_IF_FALSE              ((21 << 1) | SMALLINT_MASK)
#define OP_RETURN_REGISTER              ((22 << 1) | SMALLINT_MASK)
#define OP_RETURN_VALUE                 ((23 << 1) | SMALLINT_MASK)
#define OP_RESUME                       ((24 << 1) | SMALLINT_MASK)
#define OP_                             ((25 << 1) | SMALLINT_MASK)

#define SSA_REGISTER(X)                 (i->stack->elements[i->framePointer + (X)])
#define REG_STACK_POINTER(X)            (i->framePointer + (X))
#define SSA_NEXT_PARAM_SMALLINT         ((word_t)i->method->code->elements[i->codePointer++]>>1)
#define SSA_NEXT_PARAM_OBJECT           (i->method->code->elements[i->codePointer++])
#define ASSERT_VALID_REGISTER(X)        (assert((X) < (word_t)i->method->registerCount>>1))

#define SOCKET_RETURN(x) (smallint_to_object(socket_return((x < 0)? -errno : x)))

extern int globalInterrupt; /*if this is set to 1, we should break so the user can stop an infinite loop*/
extern void (*primitives[]) (struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);

#define ASSURE_SMALLINT_ARG(XXX) \
  if (!object_is_smallint(args[XXX])) { \
    interpreter_signal_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_TYPE_ERROR_ON), args[XXX], NULL, resultStackPointer); \
    return; \
  }

#define ASSURE_NOT_SMALLINT_ARG(XXX) \
  if (object_is_smallint(args[XXX])) { \
    interpreter_signal_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_TYPE_ERROR_ON), args[XXX], NULL, resultStackPointer); \
    return; \
  }

#define ASSURE_TYPE_ARG(XXX, TYPEXXX) \
  if (object_type(args[XXX]) != TYPEXXX) { \
    interpreter_signal_with(oh, oh->cached.interpreter, get_special(oh, SPECIAL_OOP_TYPE_ERROR_ON), args[XXX], NULL, resultStackPointer); \
    return; \
  }


/* for any assignment where an old gen object points to a new gen
object.  call it before the next GC happens. !Before any stack pushes! */

void heap_store_into(struct object_heap* oh, struct Object* src, struct Object* dest);


void heap_gc(struct object_heap* oh);
void heap_full_gc(struct object_heap* oh);
void interpreter_apply_to_arity_with_optionals(struct object_heap* oh, struct Interpreter * i, struct Closure * closure,
                                               struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);

/* 
 * Args are the actual arguments to the function. dispatchers are the
 * objects we find the dispatch function on. Usually they should be
 * the same.
 */

void send_to_through_arity_with_optionals(struct object_heap* oh,
                                                  struct Symbol* selector, struct Object* args[],
                                                  struct Object* dispatchers[], word_t arity, struct OopArray* opts,
                                          word_t resultStackPointer/*where to put the return value in the stack*/);



word_t object_to_smallint(struct Object* xxx);
struct Object* smallint_to_object(word_t xxx);
int64_t getTickCount();

bool_t oop_is_object(word_t xxx);
bool_t oop_is_smallint(word_t xxx);
bool_t object_is_smallint(struct Object* xxx);
bool_t object_is_special(struct object_heap* oh, struct Object* obj);
word_t object_markbit(struct Object* xxx);
word_t object_hash(struct Object* xxx);
word_t object_size(struct Object* xxx);
word_t payload_size(struct Object* xxx);
word_t object_type(struct Object* xxx);

void copy_bytes_into(byte_t * src, word_t n, byte_t * dst);
void copy_words_into(void * src, word_t n, void * dst);
void fill_words_with(word_t* dst, word_t n, word_t value);
void fill_bytes_with(byte_t* dst, word_t n, byte_t value);

void print_object(struct Object* oop);
void print_symbol(struct Symbol* name);
void print_byte_array(struct Object* o);
void print_object_with_depth(struct object_heap* oh, struct Object* o, word_t depth, word_t max_depth);
void print_detail(struct object_heap* oh, struct Object* o);
bool_t print_printname(struct object_heap* oh, struct Object* o);
void print_type(struct object_heap* oh, struct Object* o);
void print_stack(struct object_heap* oh);
void print_stack_types(struct object_heap* oh, word_t last_count);
void print_backtrace(struct object_heap* oh);
void heap_print_objects(struct object_heap* oh, byte_t* memory, word_t memorySize);
word_t heap_what_points_to_in(struct object_heap* oh, struct Object* x, byte_t* memory, word_t memorySize, bool_t print);
word_t heap_what_points_to(struct object_heap* oh, struct Object* x, bool_t print);
void heap_print_marks(struct object_heap* oh, byte_t* memory, word_t memorySize);
void heap_integrity_check(struct object_heap* oh, byte_t* memory, word_t memorySize);
struct Object* heap_new_cstring(struct object_heap* oh, byte_t *input);
bool_t object_is_old(struct object_heap* oh, struct Object* oop);
bool_t object_is_young(struct object_heap* oh, struct Object* obj);
bool_t object_in_memory(struct object_heap* oh, struct Object* oop, byte_t* memory, word_t memorySize);
struct Object* object_after(struct object_heap* heap, struct Object* o);
bool_t object_is_marked(struct object_heap* heap, struct Object* o);
bool_t object_is_free(struct Object* o);
void method_flush_cache(struct object_heap* oh, struct Symbol* selector);
struct Object* heap_make_free_space(struct object_heap* oh, struct Object* obj, word_t words);
struct Object* heap_make_used_space(struct object_heap* oh, struct Object* obj, word_t words);
bool_t heap_initialize(struct object_heap* oh, word_t size, word_t limit, word_t young_limit, word_t next_hash, word_t special_oop, word_t cdid);
void heap_close(struct object_heap* oh);
bool_t object_is_pinned(struct object_heap* oh, struct Object* x);
bool_t object_is_remembered(struct object_heap* oh, struct Object* x);
struct Object* heap_find_first_young_free(struct object_heap* oh, struct Object* obj, word_t bytes);
struct Object* heap_find_first_old_free(struct object_heap* oh, struct Object* obj, word_t bytes);
struct Object* gc_allocate_old(struct object_heap* oh, word_t bytes);
struct Object* gc_allocate(struct object_heap* oh, word_t bytes);
void object_forward_pointers_to(struct object_heap* oh, struct Object* o, struct Object* x, struct Object* y);
void heap_free_object(struct object_heap* oh, struct Object* obj);
void heap_finish_gc(struct object_heap* oh);
void heap_finish_full_gc(struct object_heap* oh);
void heap_start_gc(struct object_heap* oh);
void heap_pin_young_object(struct object_heap* oh, struct Object* x);
void heap_remember_young_object(struct object_heap* oh, struct Object* x);
void heap_mark(struct object_heap* oh, struct Object* obj);
void heap_mark_specials(struct object_heap* oh, bool_t mark_old);
void heap_mark_fixed(struct object_heap* oh, bool_t mark_old);
void heap_mark_interpreter_stack(struct object_heap* oh, bool_t mark_old);
void heap_mark_fields(struct object_heap* oh, struct Object* o);
void heap_mark_recursively(struct object_heap* oh, bool_t mark_old);
void heap_free_and_coalesce_unmarked(struct object_heap* oh, byte_t* memory, word_t memorySize);
void heap_unmark_all(struct object_heap* oh, byte_t* memory, word_t memorySize);
void heap_update_forwarded_pointers(struct object_heap* oh, byte_t* memory, word_t memorySize);
void heap_tenure(struct object_heap* oh);
void heap_mark_pinned_young(struct object_heap* oh);
void heap_mark_remembered_young(struct object_heap* oh);
void heap_sweep_young(struct object_heap* oh);
void heap_pin_c_stack(struct object_heap* oh);
void heap_full_gc(struct object_heap* oh);
void heap_gc(struct object_heap* oh);
void heap_fixed_add(struct object_heap* oh, struct Object* x);
void heap_fixed_remove(struct object_heap* oh, struct Object* x);
void heap_forward_from(struct object_heap* oh, struct Object* x, struct Object* y, byte_t* memory, word_t memorySize);
void heap_forward(struct object_heap* oh, struct Object* x, struct Object* y);
void heap_store_into(struct object_heap* oh, struct Object* src, struct Object* dest);
struct Object* heap_allocate_with_payload(struct object_heap* oh, word_t words, word_t payload_size);
struct Object* heap_allocate(struct object_heap* oh, word_t words);
struct Object* heap_clone(struct object_heap* oh, struct Object* proto);
struct Object* heap_clone_special(struct object_heap* oh, word_t special_index);
struct Map* heap_clone_map(struct object_heap* oh, struct Map* map);
struct ByteArray* heap_new_float(struct object_heap* oh);
struct OopArray* heap_clone_oop_array_sized(struct object_heap* oh, struct Object* proto, word_t size);
struct ByteArray* heap_clone_byte_array_sized(struct object_heap* oh, struct Object* proto, word_t bytes);
struct ByteArray* heap_new_byte_array_with(struct object_heap* oh, word_t byte_size, byte_t* bytes);
struct ByteArray* heap_new_string_with(struct object_heap* oh, word_t byte_size, byte_t* bytes);
void interpreter_grow_stack(struct object_heap* oh, struct Interpreter* i, word_t minimum);
void interpreter_stack_allocate(struct object_heap* oh, struct Interpreter* i, word_t n);
void interpreter_stack_push(struct object_heap* oh, struct Interpreter* i, struct Object* value);
struct Object* interpreter_stack_pop(struct object_heap* oh, struct Interpreter* i);
void interpreter_stack_pop_amount(struct object_heap* oh, struct Interpreter* i, word_t amount);
void unhandled_signal(struct object_heap* oh, struct Symbol* selector, word_t n, struct Object* args[]);
void interpreter_signal(struct object_heap* oh, struct Interpreter* i, struct Object* signal, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void interpreter_signal_with(struct object_heap* oh, struct Interpreter* i, struct Object* signal, struct Object* arg, struct OopArray* opts, word_t resultStackPointer);
void interpreter_signal_with_with(struct object_heap* oh, struct Interpreter* i, struct Object* signal, struct Object* arg, struct Object* arg2, struct OopArray* opts, word_t resultStackPointer);
void interpreter_signal_with_with_with(struct object_heap* oh, struct Interpreter* i, struct Object* signal, struct Object* arg, struct Object* arg2, struct Object* arg3, struct OopArray* opts, word_t resultStackPointer);
bool_t interpreter_dispatch_optional_keyword(struct object_heap* oh, struct Interpreter * i, struct Object* key, struct Object* value);
void interpreter_dispatch_optionals(struct object_heap* oh, struct Interpreter * i, struct OopArray* opts);
bool_t interpreter_return_result(struct object_heap* oh, struct Interpreter* i, word_t context_depth, struct Object* result, word_t prevCodePointer);
void interpreter_resend_message(struct object_heap* oh, struct Interpreter* i, word_t n, word_t resultStackPointer);
void interpreter_branch_keyed(struct object_heap* oh, struct Interpreter * i, struct OopArray* table, struct Object* oop);
void interpret(struct object_heap* oh);
void method_save_cache(struct object_heap* oh, struct MethodDefinition* md, struct Symbol* name, struct Object* arguments[], word_t n);
struct MethodDefinition* method_check_cache(struct object_heap* oh, struct Symbol* selector, struct Object* arguments[], word_t n);
void method_add_optimized(struct object_heap* oh, struct CompiledMethod* method);
void method_unoptimize(struct object_heap* oh, struct CompiledMethod* method);
void method_remove_optimized_sending(struct object_heap* oh, struct Symbol* symbol);
void method_optimize(struct object_heap* oh, struct CompiledMethod* method);
void method_pic_setup(struct object_heap* oh, struct CompiledMethod* caller);
void method_pic_flush_caller_pics(struct object_heap* oh, struct CompiledMethod* callee);
struct MethodDefinition* method_is_on_arity(struct object_heap* oh, struct Object* method, struct Symbol* selector, struct Object* args[], word_t n);
struct MethodDefinition* method_define(struct object_heap* oh, struct Object* method, struct Symbol* selector, struct Object* args[], word_t n);
void error(char* str);
void cache_specials(struct object_heap* heap);
#ifndef WIN32
word_t max(word_t x, word_t y);
#endif
word_t write_args_into(struct object_heap* oh, char* buffer, word_t limit);
word_t hash_selector(struct object_heap* oh, struct Symbol* name, struct Object* arguments[], word_t n);
void object_set_mark(struct object_heap* oh, struct Object* xxx);
void object_unmark(struct object_heap* oh, struct Object* xxx) ;
void object_set_format(struct Object* xxx, word_t type);
void object_set_size(struct Object* xxx, word_t size);
void object_set_idhash(struct Object* xxx, word_t hash);
void payload_set_size(struct Object* xxx, word_t size);
word_t heap_new_hash(struct object_heap* oh);
struct Object* get_special(struct object_heap* oh, word_t special_index);
struct Map* object_get_map(struct object_heap* oh, struct Object* o);
word_t object_word_size(struct Object* o);
word_t object_array_offset(struct Object* o);
struct Object* object_array_get_element(struct Object* o, word_t i);
struct Object* object_array_set_element(struct object_heap* oh, struct Object* o, word_t i, struct Object* val);
struct Object** object_array_elements(struct Object* o);
struct Object** array_elements(struct OopArray* o);
word_t object_array_size(struct Object* o);
word_t byte_array_size(struct ByteArray* o);
word_t array_size(struct OopArray* x);
word_t slot_table_capacity(struct SlotTable* roles);
word_t role_table_capacity(struct RoleTable* roles);
word_t object_byte_size(struct Object* o);
word_t object_total_size(struct Object* o);
word_t object_first_slot_offset(struct Object* o);
word_t object_last_slot_offset(struct Object* o);
word_t object_last_oop_offset(struct Object* o);
struct Object* object_slot_value_at_offset(struct Object* o, word_t offset);
struct Object* object_slot_value_at_offset_put(struct object_heap* oh, struct Object* o, word_t offset, struct Object* value);
struct RoleEntry* role_table_entry_for_name(struct object_heap* oh, struct RoleTable* roles, struct Symbol* name);
struct RoleEntry* role_table_entry_for_inserting_name(struct object_heap* oh, struct RoleTable* roles, struct Symbol* name);
struct RoleEntry* role_table_insert(struct object_heap* oh, struct RoleTable* roles, struct Symbol* name);
struct SlotEntry* slot_table_entry_for_name(struct object_heap* oh, struct SlotTable* slots, struct Symbol* name);
struct SlotEntry* slot_table_entry_for_inserting_name(struct object_heap* oh, struct SlotTable* slots, struct Symbol* name);
word_t role_table_accommodate(struct RoleTable* roles, word_t n);
word_t slot_table_accommodate(struct SlotTable* roles, word_t n);
word_t role_table_empty_space(struct object_heap* oh, struct RoleTable* roles);
word_t slot_table_empty_space(struct object_heap* oh, struct SlotTable* slots);
struct RoleTable* role_table_grow_excluding(struct object_heap* oh, struct RoleTable* roles, word_t n, struct MethodDefinition* method);
struct SlotTable* slot_table_grow_excluding(struct object_heap* oh, struct SlotTable* slots, word_t n, struct Symbol* excluding);
void slot_table_relocate_by(struct object_heap* oh, struct SlotTable* slots, word_t offset, word_t amount);
struct MethodDefinition* object_has_role_named_at(struct Object* obj, struct Symbol* selector, word_t position, struct Object* method);
void object_change_map(struct object_heap* oh, struct Object* obj, struct Map* map);
void object_represent(struct object_heap* oh, struct Object* obj, struct Map* map);
word_t object_add_role_at(struct object_heap* oh, struct Object* obj, struct Symbol* selector, word_t position, struct MethodDefinition* method);
word_t object_remove_role(struct object_heap* oh, struct Object* obj, struct Symbol* selector, struct MethodDefinition* method);
struct Object* object_add_slot_named_at(struct object_heap* oh, struct Object* obj, struct Symbol* name, struct Object* value, word_t offset);
struct Object* object_add_slot_named(struct object_heap* oh, struct Object* obj, struct Symbol* name, struct Object* value);
struct Object* object_remove_slot(struct object_heap* oh, struct Object* obj, struct Symbol* name);
void adjust_fields_by(struct object_heap* oh, struct Object* o, word_t shift_amount);
void adjust_oop_pointers_from(struct object_heap* oh, word_t shift_amount, byte_t* memory, word_t memorySize);
void prim_fixme(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_closePipe(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_readFromPipe(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_writeToPipe(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_selectOnWritePipesFor(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_cloneSystem(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_socketCreate(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_socketListen(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_socketAccept(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_socketBind(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_socketConnect(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_socketGetError(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_getAddrInfo(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_getAddrInfoResult(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_freeAddrInfoResult(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_socketCreateIP(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_write_to_starting_at(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_close(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_readConsole_from_into_starting_at(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_read_from_into_starting_at(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_write_to_from_starting_at(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_reposition_to(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_positionOf(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_bytesPerWord(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_timeSinceEpoch(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_atEndOf(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_sizeOf(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_flush_output(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_handle_for(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_handleForNew(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_handle_for_input(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_addressOf(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_library_open(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_library_close(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_procAddressOf(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_extlibError(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_applyExternal(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_memory_new(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_memory_close(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_memory_size(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_memory_addRef(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_memory_read(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_memory_write(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_memory_resizeTo(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
void prim_smallint_at_slot_named(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_bytesize(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_findon(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_ensure(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_frame_pointer_of(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_bytearray_newsize(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_byteat_put(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_byteat(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_map(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_set_map(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_run_args_into(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_exit(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_isIdenticalTo(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_identity_hash(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_identity_hash_univ(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_clone(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_applyto(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* optionals, word_t resultStackPointer);
void prim_at(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_at_put(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_ooparray_newsize(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_equals(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_less_than(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_size(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_bitand(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_bitor(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_bitxor(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_bitnot(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_smallIntegerMinimum(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_smallIntegerMaximum(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_plus(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_removefrom(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_exponent(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_significand(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_getcwd(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_setcwd(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_float_equals(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_float_less_than(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_float_plus(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_float_minus(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_float_times(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_float_divide(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_float_raisedTo(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_float_ln(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_float_exp(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_float_sin(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_withSignificand_exponent(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_bitshift(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_minus(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_times(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_quo(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_forward_to(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_clone_setting_slots(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_as_method_on(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_at_slot_named(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_at_slot_named_put(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_clone_with_slot_valued(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_clone_without_slot(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_send_to(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* optionals, word_t resultStackPointer);
void prim_send_to_through(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* optionals, word_t resultStackPointer);
void prim_as_accessor(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);
void prim_save_image(struct object_heap* oh, struct Object* args[], word_t n, struct OopArray* opts, word_t resultStackPointer);

void socket_module_init(struct object_heap* oh);
word_t socket_return(word_t ret);
int socket_select_setup(struct OopArray* selectOn, fd_set* fdList, int* maxFD);
void socket_select_find_available(struct OopArray* selectOn, fd_set* fdList, struct OopArray* readyPipes, word_t readyCount);
void prim_selectOnReadPipesFor(struct object_heap* oh, struct Object* args[], word_t arity, struct OopArray* opts, word_t resultStackPointer);
int socket_lookup_domain(word_t domain);
int socket_reverse_lookup_domain(word_t domain);
int socket_lookup_type(word_t type);
int socket_reverse_lookup_type(word_t type);
int socket_lookup_protocol(word_t protocol);
int socket_reverse_lookup_protocol(word_t protocol);
int socket_getaddrinfo(struct object_heap* oh, struct ByteArray* hostname, word_t hostnameSize, struct ByteArray* service, word_t serviceSize, word_t family, word_t type, word_t protocol, word_t flags);

word_t memory_string_to_bytes(char* str);

byte_t* byte_array_elements(struct ByteArray* o);
byte_t byte_array_get_element(struct Object* o, word_t i);
byte_t byte_array_set_element(struct ByteArray* o, word_t i, byte_t val);
int fork2();

void file_module_init(struct object_heap* oh);
bool_t file_handle_isvalid(struct object_heap* oh, word_t file);
word_t file_allocate(struct object_heap* oh);
void file_close(struct object_heap* oh, word_t file);
word_t file_open(struct object_heap* oh, struct ByteArray * name, word_t flags);
word_t file_write(struct object_heap* oh, word_t file, word_t n, char * bytes);
word_t file_read(struct object_heap* oh, word_t file, word_t n, char * bytes);
word_t file_sizeof(struct object_heap* oh, word_t file);
word_t file_seek(struct object_heap* oh, word_t file, word_t offset);
word_t file_tell(struct object_heap* oh, word_t file);
bool_t file_isatend(struct object_heap* oh, word_t file);
bool_t file_delete(struct object_heap* oh, char* filename);
bool_t file_touch(struct object_heap* oh, char* filename);


void dir_module_init(struct object_heap* oh);
int dir_open(struct object_heap* oh, struct ByteArray *dirName);
int dir_close(struct object_heap* oh, int dirHandle);
int dir_read(struct object_heap* oh, int dirHandle, struct ByteArray *entNameBuffer);
int dir_getcwd(struct ByteArray *wdBuffer);
int dir_setcwd(struct ByteArray *newWd);

void memarea_module_init (struct object_heap* oh);
int memarea_handle_isvalid (struct object_heap* oh, int memory);
void memarea_close (struct object_heap* oh, int memory);
void memarea_addref (struct object_heap* oh, int memory);
int memarea_open (struct object_heap* oh, int size);
int memarea_write (struct object_heap* oh, int memory, int memStart, int n, byte_t* bytes);
int memarea_read (struct object_heap* oh, int memory, int memStart, int n, byte_t* bytes);
int memarea_sizeof (struct object_heap* oh, int memory);
int memarea_resize (struct object_heap* oh, int memory, int size);
int memarea_addressof (struct object_heap* oh, int memory, int offset, byte_t* addressBuffer);

void profiler_start(struct object_heap* oh);
void profiler_stop(struct object_heap* oh);
void profiler_enter_method(struct object_heap* oh, struct Object* method);
void profiler_leave_current(struct object_heap* oh);
/*void profiler_leave_method(struct object_heap* oh, struct Object* method);*/
void profiler_delete_method(struct object_heap* oh, struct Object* method);


bool_t openExternalLibrary(struct object_heap* oh, struct ByteArray *libname, struct ByteArray *handle);
bool_t closeExternalLibrary(struct object_heap* oh, struct ByteArray *handle);
bool_t lookupExternalLibraryPrimitive(struct object_heap* oh, struct ByteArray *handle, struct ByteArray *symname, struct ByteArray *ptr);
int readExternalLibraryError(struct ByteArray *messageBuffer);
word_t extractBigInteger(struct ByteArray* bigInt);
struct Object* injectBigInteger(struct object_heap* oh, word_t value);
struct Object* heap_new_cstring(struct object_heap* oh, byte_t *input);
struct Object* applyExternalLibraryPrimitive(struct object_heap* oh, struct ByteArray * fnHandle, struct OopArray * argsFormat, struct Object* callFormat, struct Object* resultFormat, struct OopArray * argsArr);

struct MethodDefinition* method_dispatch_on(struct object_heap* oh, struct Symbol* name,
                                            struct Object* arguments[], word_t arity, struct Object* resendMethod);

word_t object_is_immutable(struct Object* o);
word_t smallint_fits_object(word_t i);
float_type* float_part(struct ByteArray* o);
void copy_used_objects(struct object_heap* oh, struct Object** writeObject,  byte_t* memoryStart, word_t memorySize,
                       struct ForwardPointerEntry* table, word_t forwardPointerEntryCount);

void adjust_object_fields_with_table(struct object_heap* oh, byte_t* memory, word_t memorySize,
                                     struct ForwardPointerEntry* table, word_t forwardPointerEntryCount);


struct ForwardPointerEntry* forward_pointer_hash_add(struct ForwardPointerEntry* table,
                                                    word_t forwardPointerEntryCount,
                                                     struct Object* fromObj, struct Object* toObj);

struct ForwardPointerEntry* forward_pointer_hash_get(struct ForwardPointerEntry* table,
                                                    word_t forwardPointerEntryCount,
                                                     struct Object* fromObj);


int socket_set_nonblocking(int fd);

word_t extractCString(struct ByteArray * array, byte_t* buffer, word_t bufferSize);


struct MethodDefinition* method_pic_find_callee(struct object_heap* oh, struct CompiledMethod* callerMethod,
                                                struct Symbol* selector, word_t arity, struct Object* args[]);

void method_pic_add_callee(struct object_heap* oh, struct CompiledMethod* callerMethod, struct MethodDefinition* def,
                           word_t arity, struct Object* args[]);


/*when a function is redefined, we need to know what PICs to flush. Here each method will
keep a list of all the pics that it is in */
void method_pic_add_callee_backreference(struct object_heap* oh,
                                         struct CompiledMethod* caller, struct CompiledMethod* callee);
