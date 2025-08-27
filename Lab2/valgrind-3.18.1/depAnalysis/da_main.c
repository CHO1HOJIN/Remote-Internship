
/*--------------------------------------------------------------------*/
/*--- Nulgrind: The minimal Valgrind tool.               da_main.c ---*/
/*--------------------------------------------------------------------*/

/*
   This file is part of Nulgrind, the minimal Valgrind tool,
   which does no instrumentation or analysis.

   Copyright (C) 2002-2017 Nicholas Nethercote
      njn@valgrind.org

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.

   The GNU General Public License is contained in the file COPYING.
*/
#include "da_main.h"


/* Trace File */
static HChar* TRACE_FILE_NAME = NULL;
static VgFile* out_fp = NULL;

static Int find_shadow_index(Addr addr) {
   for (int i = 0; i < shadow_count; i++) {
       if (SHADOW_MEM[i].addr == addr)
           return i;
   }
   return -1;
}

static Bool is_array_var(const HChar* varname) {
   for (int i = 0; i < shadow_count; i++) {
      if (varname != NULL && SHADOW_MEM[i].info.varname != NULL && VG_(strcmp)(SHADOW_MEM[i].info.varname, varname) == 0) {
         if (SHADOW_MEM[i].info.offset != 0) return True;
      }
   }
   return False;
}

static void print_trace_info() {
   for (int i = 0; i < trace_count; i++) {
      if (!TRACE_INFO[i].traced) continue;
      if (TRACE_INFO[i].dest_info.info.varname == NULL || TRACE_INFO[i].src_info.info.varname == NULL) continue;

      HChar dest_varname[200], src_varname[200], src_info[200];

      if (is_array_var(TRACE_INFO[i].dest_info.info.varname))
         VG_(sprintf)(dest_varname, "%s[%d]", TRACE_INFO[i].dest_info.info.varname, TRACE_INFO[i].dest_info.info.offset / TRACE_INFO[i].dest_info.info.size);
      else 
         VG_(sprintf)(dest_varname, "%s", TRACE_INFO[i].dest_info.info.varname);

      if (is_array_var(TRACE_INFO[i].src_info.info.varname))
         VG_(sprintf)(src_varname, "%s[%d]", TRACE_INFO[i].src_info.info.varname, TRACE_INFO[i].src_info.info.offset / TRACE_INFO[i].src_info.info.size);
      else 
         VG_(sprintf)(src_varname, "%s", TRACE_INFO[i].src_info.info.varname);

      if (TRACE_INFO[i].src_info.info.filename == NULL) 
         VG_(sprintf)(src_info, "(null)");
      else
         VG_(sprintf)(src_info, "%s:%u", TRACE_INFO[i].src_info.info.filename, TRACE_INFO[i].src_info.info.linenum);

      if (VG_(strlen)(src_varname) > 0) {
         VG_(fprintf)(out_fp, "%s 0x%08lx %s:%u %s 0x%08lx %s \n",
            dest_varname, TRACE_INFO[i].dest_info.addr, TRACE_INFO[i].dest_info.info.filename, 
            TRACE_INFO[i].dest_info.info.linenum, src_varname, TRACE_INFO[i].src_info.addr, src_info);
      }
   }
}

static Bool is_in_load_varname(const HChar* varname, int offset) {
   for (int i = 0; i < load_count; i++) {
      if (LOAD_VAR[i].varname != NULL && VG_(strcmp)(LOAD_VAR[i].varname, varname) == 0 && LOAD_VAR[i].offset == offset) {
         return True;
      }
   }
   return False;
}

static void update_trace_info(Addr src_addr, Addr dest_addr, Bool is_array) {

   UInt sc = find_shadow_index(dest_addr);
   if (sc == -1) return;
   
   for(int i = 0; i < trace_count; i++){
      if (!TRACE_INFO[i].traced){
         if (!is_in_load_varname(TRACE_INFO[i].src_info.info.varname, TRACE_INFO[i].src_info.info.offset)) {
            TRACE_INFO[i].dest_info.addr = SHADOW_MEM[sc].addr;                TRACE_INFO[i].dest_info.info.varname = SHADOW_MEM[sc].info.varname;
            TRACE_INFO[i].dest_info.info.offset = SHADOW_MEM[sc].info.offset;  TRACE_INFO[i].dest_info.info.is_array = is_array;
            TRACE_INFO[i].dest_info.info.size = SHADOW_MEM[sc].info.size;      TRACE_INFO[i].src_info.iaddr = SHADOW_MEM[sc].iaddr;
            TRACE_INFO[i].dest_info.info.filename = SHADOW_MEM[sc].info.filename; TRACE_INFO[i].dest_info.info.linenum = SHADOW_MEM[sc].info.linenum;
            TRACE_INFO[i].traced = True;                                       
            
            LOAD_VAR[load_count].varname = TRACE_INFO[i].src_info.info.varname;  LOAD_VAR[load_count].offset = TRACE_INFO[i].src_info.info.offset;
            load_count++;
         }
         else TRACE_INFO[i].traced = True;
      }
   }
}

static Long find_store_triple(Addr iaddr) {
   for (int i = 0; i < store_count; i++) {
      if (STORE_INFO[i].iaddr == iaddr) 
         return STORE_INFO[i].addr;
   }

   return -1;
}

static Bool is_array_element(Addr iaddr){
   for (int i = 0; i < store_count; i++) {
      if (STORE_INFO[i].iaddr == iaddr)
         return STORE_INFO[i].is_array;
   }

   return False;
}

static void find_and_store_trace_info(Addr iaddr, Addr dest_addr) {
   Long data = find_store_triple(iaddr);
   Bool is_array = is_array_element(iaddr);

   for (int i = 0; i < data_count; i++) {
      for (int j = 1; j < DEPEND_INFO[i].tmp_count; j++) {
         if (DEPEND_INFO[i].tmp[j] == data || DEPEND_INFO[i].dest_addr == dest_addr) {
            load_count = 0;
            update_trace_info(DEPEND_INFO[i].tmp[0], dest_addr, is_array);
         }
      }
   }
}

static void write_trace_info(const HChar* src_varname, UInt src_offset, const HChar* src_filename, Addr src_addr, \
   UInt src_lineno, Addr iaddr, UInt size){
   if (trace_count >= MAX_SHADOW_ENTRIES) {
      VG_(printf)("Trace memory full\n");
      return;
   }

   TRACE_INFO[trace_count].src_info.info.varname = src_varname;   TRACE_INFO[trace_count].src_info.info.offset = src_offset;
   TRACE_INFO[trace_count].src_info.addr = src_addr;              TRACE_INFO[trace_count].src_info.info.filename = src_filename;
   TRACE_INFO[trace_count].src_info.info.linenum = src_lineno;    TRACE_INFO[trace_count].dest_info.addr = 0;
   TRACE_INFO[trace_count].src_info.iaddr = iaddr;                TRACE_INFO[trace_count].dest_info.iaddr = iaddr;
   TRACE_INFO[trace_count].src_info.info.size = size;
   
   //DEBUG
   // VG_(printf) ("==========================\n");
   // VG_(printf) ("Trace Count: %d ", trace_count);
   // VG_(printf) ("src_varname: %s ", src_varname);
   // VG_(printf) ("src_offset: %d ", src_offset);
   // VG_(printf) ("src_addr: 0x%08lx ", src_addr);
   // VG_(printf) ("src_filename: %s ", src_filename);
   // VG_(printf) ("src_lineno: %u ", src_lineno);
   // VG_(printf) ("iaddr: 0x%08lx ", iaddr);
   // VG_(printf) ("size: %u ", size);
   // VG_(printf) ("==========================\n");

}

static void update_shadow_memory(Addr mem_addr, Addr instr_addr, UInt size, UWord data) {
   
   DiEpoch ep = VG_(current_DiEpoch)();
   const HChar* varname = NULL;
   const HChar* filename = NULL;
   UInt linenum = 0;
   PtrdiffT offset = 0;

   VG_(get_datasym_and_offset)(ep, mem_addr, &varname, &offset);
   VG_(get_filename_linenum)(ep, instr_addr, &filename, NULL, &linenum);

   int idx = find_shadow_index(mem_addr);

   if (idx == -1 && shadow_count < MAX_SHADOW_ENTRIES && varname != NULL) {
      SHADOW_MEM[shadow_count].addr = mem_addr;          SHADOW_MEM[shadow_count].iaddr = instr_addr;
      SHADOW_MEM[shadow_count].info.varname = varname;   SHADOW_MEM[shadow_count].info.offset = offset;
      SHADOW_MEM[shadow_count].info.filename = filename; SHADOW_MEM[shadow_count].info.linenum = linenum;
      SHADOW_MEM[shadow_count].info.size = size;         shadow_count++;
      if (offset != 0) SHADOW_MEM[shadow_count - 1].info.is_array = True;
      else SHADOW_MEM[shadow_count - 1].info.is_array = False;
      
      find_and_store_trace_info(instr_addr, mem_addr);
   }

   else if (idx != -1 && varname != NULL) {
      SHADOW_MEM[idx].info.varname = varname;   SHADOW_MEM[idx].info.filename = filename;
      SHADOW_MEM[idx].info.linenum = linenum;   SHADOW_MEM[idx].iaddr = instr_addr;
      SHADOW_MEM[idx].addr = mem_addr;          SHADOW_MEM[idx].info.size = size;

      find_and_store_trace_info(instr_addr, mem_addr);
   } 

   else if (idx == -1 && shadow_count >= MAX_SHADOW_ENTRIES) {
      VG_(printf)("Full shadow memory\n");
      return;
   }
}

static void handle_store(Addr mem_addr, Addr instr_addr, UInt size, UWord data, UInt tmp_id) {
   if (instr_addr >= MAX_USER_ADDR || mem_addr >= MAX_USER_ADDR) return;

   if (tmp_id != -1) {
      for (int i = 0; i < data_count; i++) {
         if (DEPEND_INFO[i].tmp_count > 0 && DEPEND_INFO[i].tmp[DEPEND_INFO[i].tmp_count - 1] == tmp_id) {
            DEPEND_INFO[i].dest_addr = mem_addr;
         }
      }
   }
   
   update_shadow_memory(mem_addr, instr_addr, size, data);
}

static void append_data_dependency(IRTemp tmp, IRExpr* data) {
   switch (data->tag) {
      case Iex_RdTmp:
         for (int i = data_count - 1; i >= 0; i--) {
            for (int j = 0; j < DEPEND_INFO[i].tmp_count; j++) {
               if (DEPEND_INFO[i].tmp[j] == data->Iex.RdTmp.tmp) {
                  DEPEND_INFO[i].tmp[DEPEND_INFO[i].tmp_count] = tmp;
                  DEPEND_INFO[i].tmp_count++;
                  return;
               }
            }
         }
         break;
      case Iex_GetI:
         append_data_dependency(tmp, data->Iex.GetI.ix);
         break;
      case Iex_Qop:
         append_data_dependency(tmp, data->Iex.Qop.details->arg1);
         append_data_dependency(tmp, data->Iex.Qop.details->arg2);
         append_data_dependency(tmp, data->Iex.Qop.details->arg3);
         append_data_dependency(tmp, data->Iex.Qop.details->arg4);
         break;
      case Iex_Triop:
         append_data_dependency(tmp, data->Iex.Triop.details->arg1);
         append_data_dependency(tmp, data->Iex.Triop.details->arg2);
         append_data_dependency(tmp, data->Iex.Triop.details->arg3);
         break;
      case Iex_Binop:
         append_data_dependency(tmp, data->Iex.Binop.arg1);
         append_data_dependency(tmp, data->Iex.Binop.arg2);
         break;
      case Iex_Unop:
         append_data_dependency(tmp, data->Iex.Unop.arg);
         break;
      case Iex_Load:
         append_data_dependency(tmp, data->Iex.Load.addr);
         break;
      case Iex_CCall:
         for (int i = 0; data->Iex.CCall.args[i] != NULL; i++) {
            append_data_dependency(tmp, data->Iex.CCall.args[i]);
         }
         break;
      case Iex_ITE:
         append_data_dependency(tmp, data->Iex.ITE.cond);
         append_data_dependency(tmp, data->Iex.ITE.iftrue);
         append_data_dependency(tmp, data->Iex.ITE.iffalse);
         break;
      case Iex_Const:
         if (data->Iex.Const.con != NULL) {
            if (data->Iex.Const.con->tag == Ico_U64) {
               data_count++;
            }
         }
      default:
         break;
   }
}

static void handle_load(Addr load_addr, Addr instr_addr, UInt size, Addr temp_addr, UInt temp_id) {
   if (instr_addr >= MAX_USER_ADDR || load_addr >= MAX_USER_ADDR) return;

   DiEpoch ep = VG_(current_DiEpoch)();
   const HChar* varname = NULL;
   PtrdiffT offset = 0;

   varInfo src_info;
   int idx = find_shadow_index(load_addr);

   if (idx == -1) {
      VG_(get_datasym_and_offset)(ep, load_addr, &varname, &offset);
      src_info.varname = varname;   src_info.offset = offset;
      src_info.filename = NULL;     src_info.linenum = 0;
   } 
   else {
      src_info = SHADOW_MEM[idx].info;
   }

   if (temp_id != -1) {
      for (int i = 0; i < data_count; i++) {
         if (DEPEND_INFO[i].src_addr == temp_id) {
            DEPEND_INFO[i].tmp[0] = temp_addr;
         }
      }
   }
   
   write_trace_info(src_info.varname, src_info.offset, src_info.filename, load_addr,\
      src_info.linenum, instr_addr, size);

   trace_count++;
}

static Bool da_process_cmd_line_option(const HChar* arg) {
   if (VG_STR_CLO(arg, "--trace-file", TRACE_FILE_NAME)) return True;
   VG_(printf)("Unknown option: %s\n", arg);
   return False;
}

static void da_post_clo_init(void) {
   out_fp = VG_(fopen)(TRACE_FILE_NAME, VKI_O_WRONLY | VKI_O_CREAT | VKI_O_TRUNC, 0644);
   if (out_fp == NULL) {
      VG_(printf)("Error opening trace file: %s\n", TRACE_FILE_NAME);
      VG_(exit)(1);
   }
}

static void get_store_triple(IRExpr* mem_addr, Addr iaddr, IRExpr* data) {
   Bool is_array = True;
   Long addr_value = -1;

   if (mem_addr->tag == Iex_Const && mem_addr->Iex.Const.con != NULL && mem_addr->Iex.Const.con->tag == Ico_U64)
      is_array = False;
   
   STORE_INFO[store_count].is_array = is_array;

   if (data->tag == Iex_RdTmp) {
      addr_value = (ULong)data->Iex.RdTmp.tmp;
      append_data_dependency((ULong)data->Iex.RdTmp.tmp, data);
   }
   else if (data->tag == Iex_Const && data->Iex.Const.con != NULL) {
      if (data->Iex.Const.con->tag == Ico_U64) {
         addr_value = (ULong)data->Iex.Const.con->Ico.U64;
      }
   }
   
   STORE_INFO[store_count].addr = addr_value;
   STORE_INFO[store_count].iaddr = iaddr;
   store_count++;

}

static void insert_dirty_call(IRSB* bbOut, const HChar* name, void* handler, IRTemp addr, Addr iaddr, UInt size, IRTemp data, UInt tmp_id) {

   IRExpr** args = mkIRExprVec_5(
      IRExpr_RdTmp(addr),
      mkIRExpr_HWord((HWord)iaddr),
      mkIRExpr_HWord((HWord)size),
      IRExpr_RdTmp(data),
      mkIRExpr_HWord((HWord)tmp_id)
   );
   
   IRDirty* di = unsafeIRDirty_0_N(0, name, VG_(fnptr_to_fnentry)(handler), args);
   addStmtToIRSB(bbOut, IRStmt_Dirty(di));
}

static IRSB* da_instrument(VgCallbackClosure* closure,
                           IRSB* bb,
                           const VexGuestLayout* layout,
                           const VexGuestExtents* vge,
                           const VexArchInfo* archinfo_host,
                           IRType gWordTy, IRType hWordTy) {
   IRDirty* di;
   Int i;
   IRSB* bbOut;
   IRTypeEnv* tyenv = bb->tyenv;
   Addr iaddr = 0;
   UInt ilen = 0;
   DiEpoch ep = VG_(current_DiEpoch)();

   if (gWordTy != hWordTy)
      VG_(tool_panic)("host/guest word size mismatch");

   bbOut = deepCopyIRSBExceptStmts(bb);
   i = 0;

   while (i < bb->stmts_used && bb->stmts[i]->tag != Ist_IMark) {
      addStmtToIRSB(bbOut, bb->stmts[i]);
      i++;
   }

   for ( ; i < bb->stmts_used; i++) {
      IRStmt* st = bb->stmts[i];
      if (!st || st->tag == Ist_NoOp) continue;
      
      switch (st->tag) {
         case Ist_IMark: {
            iaddr = st->Ist.IMark.addr;
            ilen  = st->Ist.IMark.len;
            addStmtToIRSB(bbOut, st);
            break;
         }

         case Ist_Store: {
            IRExpr* addr = st->Ist.Store.addr;
            IRExpr* data = st->Ist.Store.data;

            /* Create temporary to hold the store address */
            IRTemp mem_addr = newIRTemp(bbOut->tyenv, Ity_I64);
            addStmtToIRSB(bbOut, IRStmt_WrTmp(mem_addr, addr));

            IRType type = typeOfIRExpr(bbOut->tyenv, data);
            UInt size =  sizeofIRType(type);  
            UInt tmp_id = -1;
            if(addr->tag == Iex_RdTmp) tmp_id = addr->Iex.RdTmp.tmp;

            if (type == Ity_I64) {
               IRTemp tmp_data = newIRTemp(bbOut->tyenv, type);
               addStmtToIRSB(bbOut, IRStmt_WrTmp(tmp_data, data));

               insert_dirty_call(bbOut, "handlestore", handle_store,
                  mem_addr, iaddr, size, tmp_data, tmp_id);
            }

            else {

               IROp cast_op;
               switch (type) {
                  case Ity_I8:  cast_op = Iop_8Uto64; break;
                  case Ity_I16: cast_op = Iop_16Uto64; break;
                  case Ity_I32: cast_op = Iop_32Uto64; break;
                  default:
                     cast_op = -1;
               }

               if (cast_op != -1) {
                  IRExpr* cast_data = IRExpr_Unop(cast_op, data);
                  IRTemp tmp_data = newIRTemp(bbOut->tyenv, Ity_I64);
                  addStmtToIRSB(bbOut, IRStmt_WrTmp(tmp_data, cast_data));

                  insert_dirty_call(bbOut, "handle_store", handle_store,
                     mem_addr, iaddr, size, tmp_data, tmp_id);

               }
            }
            if (iaddr < MAX_USER_ADDR) {
               get_store_triple(addr, iaddr, data);
            }
            addStmtToIRSB(bbOut, st);
            break;
         }

         case Ist_WrTmp: {
            IRTemp tmp = st->Ist.WrTmp.tmp;
            IRExpr* data = st->Ist.WrTmp.data;

            if (data->tag == Iex_Load) {
               IRExpr* load_addr = data->Iex.Load.addr;
               IRTemp tmp_load_addr = newIRTemp(bbOut->tyenv, Ity_I64);

               addStmtToIRSB(bbOut, IRStmt_WrTmp(tmp_load_addr, load_addr));

               IRType type = typeOfIRExpr(bbOut->tyenv, data);
               UInt size = sizeofIRType(type);
               IRTemp tmp_addr = (load_addr->tag == Iex_RdTmp) ? load_addr->Iex.RdTmp.tmp : (IRTemp) -1;
               UInt tmp_id = (load_addr->tag == Iex_RdTmp) ? load_addr->Iex.RdTmp.tmp : (UInt) -1;

               IRExpr** args = mkIRExprVec_5(
                  IRExpr_RdTmp(tmp_load_addr),
                  mkIRExpr_HWord((HWord)iaddr),
                  mkIRExpr_HWord((HWord)size),
                  (tmp_addr != (IRTemp)-1) ? IRExpr_RdTmp(tmp_addr) : mkIRExpr_HWord(0),
                  mkIRExpr_HWord((HWord)tmp_id)
               );
               
               IRDirty* di = unsafeIRDirty_0_N(0, "handle_load",
                                               VG_(fnptr_to_fnentry)((void*)handle_load), args);
               addStmtToIRSB(bbOut, IRStmt_Dirty(di));

               if (iaddr < MAX_USER_ADDR) {
                  if (load_addr->tag == Iex_Const && load_addr->Iex.Const.con->tag == Ico_U64) {
                     DEPEND_INFO[data_count].src_addr = (ULong)(load_addr->Iex.Const.con->Ico.U64);
                  } 
                  else if (load_addr->tag == Iex_RdTmp) {
                     DEPEND_INFO[data_count].src_addr = load_addr->Iex.RdTmp.tmp;
                  }
                  
                  DEPEND_INFO[data_count].tmp[0] = DEPEND_INFO[data_count].src_addr;
                  DEPEND_INFO[data_count].instr_addr = iaddr;  DEPEND_INFO[data_count].tmp[1] = tmp;
                  DEPEND_INFO[data_count].tmp_count += 2;
                  data_count++;
               }
            } 
            else {
               if (iaddr < MAX_USER_ADDR) {
                  append_data_dependency(tmp, data);
               }
            }

            addStmtToIRSB(bbOut, st);
            break;
         }

         default:
            addStmtToIRSB(bbOut, st);
            break;
      }
   }
   return bbOut; 
}

static void update_array_info(){
   for(int i = 0; i < shadow_count; i++){
      if (SHADOW_MEM[i].info.varname != NULL) {
         for(int j = 0; j < shadow_count; j++){
            if (SHADOW_MEM[i].info.varname == SHADOW_MEM[j].info.varname && SHADOW_MEM[i].info.offset != SHADOW_MEM[j].info.offset){
               SHADOW_MEM[i].info.is_array = True;
            }
         }
      }
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

/*================ Finalisation ==================*/
static void da_fini(Int exitcode) {
   print_shadow_memory();
   update_array_info();
   print_trace_info();
   if (out_fp != NULL) {
       VG_(fclose)(out_fp);
       out_fp = NULL;
   }
}

static void da_pre_clo_init(void)
{
   VG_(details_name)            ("depAnalysis");
   VG_(details_version)         (NULL);
   VG_(details_description)     ("the minimal Valgrind tool");
   VG_(details_copyright_author)(
      "Copyright (C) 2002-2017, and GNU GPL'd, by Nicholas Nethercote.");
   VG_(details_bug_reports_to)  (VG_BUGS_TO);

   VG_(details_avg_translation_sizeB) ( 275 );

   VG_(basic_tool_funcs)        (da_post_clo_init,
                                 da_instrument,
                                 da_fini);
   VG_(needs_command_line_options)(da_process_cmd_line_option, NULL, NULL);
   /* No needs, no core events to track */
}

VG_DETERMINE_INTERFACE_VERSION(da_pre_clo_init)
