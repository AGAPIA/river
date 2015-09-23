#ifndef _SYMBOP_TRANSLATOR_H
#define _SYMBOP_TRANSLATOR_H

#include "extern.h"
#include "river.h"

/* SYMBOP ABI */
/* - if the instruction uses any flag as input or output no inline code is generated 
   - a symboppushf tracking instruction is generated
*/

/* - if the instruction uses a register as input or output no inline code is generated
   - a symboppushreg <regname> tracking instruction is generated
*/

/* - if the instruction uses a memory location as input or output the following instructions are generated:
		- push imm8/16/32	- push the offset on the stack
		- lea reg, mem		- in order to determine the effective address resolved
		- push reg			- push the reg on the stack
		- push modrm		- push a dword containting scale, index, base
   - a symboppushmem tracking instruction is generated
*/
/* - each instruction generates an inline push opcode instruction
   - each instruction generates a symboptrack instruction
*/

class RiverCodeGen;

class SymbopTranslator {
private :
	BYTE trackedValues;

	RiverCodeGen *codegen;

	void CopyInstruction(RiverInstruction &rOut, const RiverInstruction &rIn);

	DWORD GetMemRepr(const RiverAddress &mem);

	typedef void(SymbopTranslator::*TranslateOpcodeFunc)(const RiverInstruction &rIn, RiverInstruction *&rMainOut, DWORD &instrCount, RiverInstruction *&rTrackOut, DWORD &trackCount);
	static TranslateOpcodeFunc translateOpcodes[2][0x100];
public :
	bool Init(RiverCodeGen *cg);

	bool Translate(const RiverInstruction &rIn, RiverInstruction *rMainOut, DWORD &instrCount, RiverInstruction *rTrackOut, DWORD &trackCount);

private :
	/* Translation helpers */
	void MakeInitTrack(RiverInstruction *&rTrackOut, DWORD &trackCount);
	void MakeCleanTrack(RiverInstruction *&rTrackOut, DWORD &trackCount);

	DWORD MakeTrackFlg(BYTE flags, RiverInstruction *&rMainOut, DWORD &instrCount, RiverInstruction *&rTrackOut, DWORD &trackCount);
	DWORD MakeTrackReg(const RiverRegister &reg, RiverInstruction *&rMainOut, DWORD &instrCount, RiverInstruction *&rTrackOut, DWORD &trackCount);
	DWORD MakeTrackMem(const RiverAddress &mem, WORD specifiers, DWORD addrOffset, RiverInstruction *&rMainOut, DWORD &instrCount, RiverInstruction *&rTrackOut, DWORD &trackCount);
	DWORD MakeTrackAddress(const RiverOperand &op, BYTE optype, RiverInstruction *&rMainOut, DWORD &instrCount, RiverInstruction *&rTrackOut, DWORD &trackCount);

	void MakeMarkFlg(BYTE flags, DWORD offset, RiverInstruction *&rMainOut, DWORD &instrCount, RiverInstruction *&rTrackOut, DWORD &trackCount);
	void MakeMarkReg(const RiverRegister &reg, DWORD addrOffset, DWORD valueOffset, RiverInstruction *&rMainOut, DWORD &instrCount, RiverInstruction *&rTrackOut, DWORD &trackCount);
	void MakeMarkMem(const RiverAddress &mem, WORD specifiers, DWORD addrOffset, DWORD valueOffset, RiverInstruction *&rMainOut, DWORD &instrCount, RiverInstruction *&rTrackOut, DWORD &trackCount);
	void MakeSkipMem(const RiverAddress &mem, RiverInstruction *&rMainOut, DWORD &instrCount, RiverInstruction *&rTrackOut, DWORD &trackCount);
	
	/*  */
	DWORD MakeTrackOp(const BYTE type, const RiverOperand &op, WORD specifiers, DWORD addrOffset, RiverInstruction *&rMainOut, DWORD &instrCount, RiverInstruction *&rTrackOut, DWORD &trackCount);
	void MakeMarkOp(const BYTE type, WORD specifiers, DWORD addrOffset, DWORD valueOffset, const RiverOperand &op, RiverInstruction *&rMainOut, DWORD &instrCount, RiverInstruction *&rTrackOut, DWORD &trackCount);


	/* Translators */
	void TranslateUnk(const RiverInstruction &rIn, RiverInstruction *&rMainOut, DWORD &instrCount, RiverInstruction *&rTrackOut, DWORD &trackCount);
	void TranslateDefault(const RiverInstruction &rIn, RiverInstruction *&rMainOut, DWORD &instrCount, RiverInstruction *&rTrackOut, DWORD &trackCount);
};


#endif
