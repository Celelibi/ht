/* 
 *	HT Editor
 *	htleimg.cc
 *
 *	Copyright (C) 1999-2002 Stefan Weyergraf (stefan@weyergraf.de)
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License version 2 as
 *	published by the Free Software Foundation.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "htdebug.h"
#include "htdisasm.h"
#include "htnewexe.h"
#include "htle.h"
#include "htleimg.h"
#include "htstring.h"
#include "formats.h"
#include "vxd.h"
#include "x86dis.h"

#include "lestruct.h"

#include <string.h>

class x86dis_le: public x86dis {
protected:
	virtual void str_op(char *opstr, int *opstrlen, x86dis_insn *insn, x86_insn_op *op, bool explicit_params);
public:
	x86dis_le(int opsize, int addrsize);
	virtual ~x86dis_le();

	virtual dis_insn *decode(byte *code, byte maxlen, CPU_ADDR addr);
};

/**/

ht_view *htleimage_init(bounds *b, ht_streamfile *file, ht_format_group *group)
{
	ht_le_shared_data *le_shared=(ht_le_shared_data *)group->get_shared_data();
	ht_streamfile *myfile = le_shared->reloc_file;

	ht_uformat_viewer *v = new ht_uformat_viewer();
	v->init(b, "le/image", VC_EDIT | VC_GOTO | VC_SEARCH, myfile, group);

	for (int i=0; i<le_shared->objmap.count; i++) {
		char t[64];
		char n[5];
		bool use32=le_shared->objmap.header[i].flags & LE_OBJECT_FLAG_USE32;
		memmove(n, le_shared->objmap.header[i].name, 4);
		n[4]=0;
		sprintf(t, "object %d USE%d: %s (psize=%08x, vsize=%08x)", i+1, use32 ? 32 : 16, n, le_shared->objmap.psize[i], le_shared->objmap.vsize[i]);

		Disassembler *disasm;

		if (use32) {
			disasm = new x86dis_le(X86_OPSIZE32, X86_ADDRSIZE32);
		} else {
			disasm = new x86dis_le(X86_OPSIZE16, X86_ADDRSIZE16);
		}

		ht_disasm_sub *d = new ht_disasm_sub();
		d->init(myfile, (le_shared->objmap.header[i].page_map_index-1)*
			le_shared->hdr.pagesize, le_shared->objmap.vsize[i], disasm,
			true, X86DIS_STYLE_OPTIMIZE_ADDR);
		ht_collapsable_sub *cs = new ht_collapsable_sub();
		cs->init(myfile, d, 1, t, 1);
		v->insertsub(cs);
	}

	return v;
}

format_viewer_if htleimage_if = {
	htleimage_init,
	0
};

/*
 *	CLASS x86dis_le
 */

x86dis_le::x86dis_le(int opsize, int addrsize)
: x86dis(opsize, addrsize)
{
}

x86dis_le::~x86dis_le()
{
}

dis_insn *x86dis_le::decode(byte *code, byte maxlen, CPU_ADDR addr)
{
	if ((maxlen>=6) && (code[0]==0xcd) && (code[1]==0x20)) {
		insn.name="VxDCall";
		insn.size=6;
		vxd_t *v=find_vxd(vxds, *(word*)(code+4));

		if (v) {
			insn.op[1].type=X86_OPTYPE_USER;
			insn.op[1].user[0]=*(word*)(code+4);
			insn.op[1].user[1]=(int)v->name;
		
			char *vs=find_vxd_service(v->services, *(word*)(code+2) & 0x7fff);

			if (vs) {
				insn.op[0].type=X86_OPTYPE_USER;
				insn.op[0].user[0]=*(word*)(code+2);
				insn.op[0].user[1]=(int)vs;
			} else {
				insn.op[0].type=X86_OPTYPE_IMM;
				insn.op[0].size=2;
				insn.op[0].imm=*(word*)(code+2);
			}
		} else {
			insn.op[0].type=X86_OPTYPE_IMM;
			insn.op[0].size=2;
			insn.op[0].imm=*(word*)(code+2);
		
			insn.op[1].type=X86_OPTYPE_IMM;
			insn.op[1].size=2;
			insn.op[1].imm=*(word*)(code+4);
		}
		insn.op[2].type=X86_OPTYPE_EMPTY;
		insn.lockprefix=X86_PREFIX_NO;
		insn.repprefix=X86_PREFIX_NO;
		insn.segprefix=X86_PREFIX_NO;
		return &insn;
	}
	return x86dis::decode(code, maxlen, addr);
}

void x86dis_le::str_op(char *opstr, int *opstrlen, x86dis_insn *insn, x86_insn_op *op, bool explicit_params)
{
	if (op->type==X86_OPTYPE_USER) {
		*opstrlen=0;
		strcpy(opstr, (char*)op->user[1]);
	} else {
		x86dis::str_op(opstr, opstrlen, insn, op, explicit_params);
	}
}

