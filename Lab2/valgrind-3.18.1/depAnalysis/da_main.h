
#include "pub_tool_basics.h"
#include "pub_tool_tooliface.h"
#include "pub_tool_libcassert.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_debuginfo.h"
#include "pub_tool_libcbase.h"
#include "pub_tool_options.h"
#include "pub_tool_machine.h"
#include "pub_tool_vki.h"
#include "pub_tool_addrinfo.h"

/*Shadow Memory*/
#define MAX_SHADOW_ENTRIES 1000
#define MAX_USER_ADDR 0x01000000

typedef struct {
   const HChar* varname;
   UInt offset;
   UInt size;
   const HChar* filename;
   UInt linenum;
   Bool is_array;
} varInfo;

typedef struct {
   Addr addr;
   Addr iaddr;
   varInfo info;
} shadow_memory;

typedef struct {
   IRTemp tmp[200];
   UInt tmp_count;
   Addr src_addr;
   Addr instr_addr;
   Addr dest_addr;
} data_dependency;

typedef struct {
   shadow_memory dest_info;
   shadow_memory src_info;
   Bool traced;
} trace_info;

typedef struct {
   Addr iaddr;
   Long addr;
   Bool is_array;
} iaddr_tmp_triple;

static shadow_memory SHADOW_MEM[MAX_SHADOW_ENTRIES];
static trace_info TRACE_INFO[MAX_SHADOW_ENTRIES];
static data_dependency DEPEND_INFO[MAX_SHADOW_ENTRIES];
static iaddr_tmp_triple STORE_INFO[MAX_SHADOW_ENTRIES];
static varInfo LOAD_VAR[MAX_SHADOW_ENTRIES];

static int trace_count = 0;
static int data_count = -1;
static int shadow_count = 0;
static int current_shadow_index = 0;
static int store_count = 0;
static int load_count = 0;