#include "da_main.h"

static void print_store_triple() {
    for (int i = 0; i < store_count; i++) {
       VG_(printf)("Store Triple [%d]: 0x%08lx (iaddr: 0x%08lx)\n",
          i,
          STORE_INFO[i].addr, STORE_INFO[i].iaddr);
    }
 }
 
 static void print_shadow_memory() {
    for (int i = 0; i < shadow_count; i++) {
       if (SHADOW_MEM[i].info.varname != NULL) {
          VG_(printf)("Shadow Memory [%d]: %s[%d] 0x%08lx %s:%u (iaddr: 0x%08lx)\n",
             i,
             SHADOW_MEM[i].info.varname, SHADOW_MEM[i].info.offset, SHADOW_MEM[i].addr, SHADOW_MEM[i].info.filename, SHADOW_MEM[i].info.linenum, SHADOW_MEM[i].iaddr);
       }
    }
 }
 
 static void print_data_memory() {
    for (int i = 0; i < data_count; i++) {
       if (DEPEND_INFO[i].src_addr == 0 || DEPEND_INFO[i].tmp_count == 0) continue;
       VG_(printf)("Data Memory [%d] - src_addr: 0x%08lx ", i, DEPEND_INFO[i].src_addr);
       VG_(printf)("iaddr: 0x%08lx ", DEPEND_INFO[i].instr_addr);
       VG_(printf)("dest_addr: 0x%08lx ", DEPEND_INFO[i].dest_addr);
       for (int j = 0; j < DEPEND_INFO[i].tmp_count; j++) {
          if (j == 0) {VG_(printf)("0x%08lx ", DEPEND_INFO[i].tmp[j]);}
          else VG_(printf)(",%d ", DEPEND_INFO[i].tmp[j]);
       }
       VG_(printf)("\n");
    }
 }

 static void get_store_triples(IRExpr* mem_addr, Addr iaddr, IRExpr* data){
    switch (mem_addr->tag) {
       case Iex_Const:
          if (mem_addr->Iex.Const.con != NULL && mem_addr->Iex.Const.con->tag == Ico_U64) {
                STORE_INFO[store_count].is_array = False;
          }
          break;
       default:
          STORE_INFO[store_count].is_array = True;
          break;
    }
    switch (data->tag) {
       case Iex_RdTmp:
          STORE_INFO[store_count].addr = (ULong) data->Iex.RdTmp.tmp;
          STORE_INFO[store_count].iaddr = iaddr;
          store_count++;
          append_data_dependency((ULong) data->Iex.RdTmp.tmp, data);
          break;
       case Iex_Const:
          if (data->Iex.Const.con != NULL) {
             if (data->Iex.Const.con->tag == Ico_U64) {
                STORE_INFO[store_count].addr = (ULong) data->Iex.Const.con->Ico.U64;
                STORE_INFO[store_count].iaddr = iaddr;
                store_count++;
             }
          }
          else {
             STORE_INFO[store_count].addr = -1;
             STORE_INFO[store_count].iaddr = iaddr;
             store_count++;
          }
          break;
       default:
          STORE_INFO[store_count].addr = -1;
          STORE_INFO[store_count].iaddr = iaddr;
          store_count++;
          break;
    }
 }