#ifndef CueLexerBase_h
#define CueLexerBase_h
/*
 * D L G L e x e r  C l a s s  D e f i n i t i o n
 *
 * Generated from: CueLexer.dlg
 *
 * 1989-1999 by  Will Cohen, Terence Parr, and Hank Dietz
 * Purdue University Electrical Engineering
 * DLG Version 1.33MR20
 */


#include "DLexerBase.h"

class CueLexerBase : public DLGLexerBase {
public:
public:
	static const int MAX_MODE;
	static const int DfaStates;
	static const int START;
	static const int STRING;
	static const int FILE;
	static const int CATALOG;
	static const int ISRC;
	typedef unsigned char DfaState;

	CueLexerBase(DLGInputStream *in,
		unsigned bufsize=2000)
		: DLGLexerBase(in, bufsize, 0)
	{
	;
	}
	void	  mode(int);
	ANTLRTokenType nextTokenType(void);
	void     advance(void);
protected:
	ANTLRTokenType act1();
	ANTLRTokenType act2();
	ANTLRTokenType act3();
	ANTLRTokenType act4();
	ANTLRTokenType act5();
	ANTLRTokenType act6();
	ANTLRTokenType act7();
	ANTLRTokenType act8();
	ANTLRTokenType act9();
	ANTLRTokenType act10();
	ANTLRTokenType act11();
	ANTLRTokenType act12();
	ANTLRTokenType act13();
	ANTLRTokenType act14();
	ANTLRTokenType act15();
	ANTLRTokenType act16();
	ANTLRTokenType act17();
	ANTLRTokenType act18();
	ANTLRTokenType act19();
	ANTLRTokenType act20();
	ANTLRTokenType act21();
	ANTLRTokenType act22();
	ANTLRTokenType act23();
	ANTLRTokenType act24();
	ANTLRTokenType act25();
	ANTLRTokenType act26();
	ANTLRTokenType act27();
	ANTLRTokenType act28();
	ANTLRTokenType act29();
	ANTLRTokenType act30();
	ANTLRTokenType act31();
	ANTLRTokenType act32();
	ANTLRTokenType act33();
	ANTLRTokenType act34();
	ANTLRTokenType act35();
	ANTLRTokenType act36();
	ANTLRTokenType act37();
	ANTLRTokenType act38();
	ANTLRTokenType act39();
	ANTLRTokenType act40();
	ANTLRTokenType act41();
	ANTLRTokenType act42();
	static DfaState st0[37];
	static DfaState st1[37];
	static DfaState st2[37];
	static DfaState st3[37];
	static DfaState st4[37];
	static DfaState st5[37];
	static DfaState st6[37];
	static DfaState st7[37];
	static DfaState st8[37];
	static DfaState st9[37];
	static DfaState st10[37];
	static DfaState st11[37];
	static DfaState st12[37];
	static DfaState st13[37];
	static DfaState st14[37];
	static DfaState st15[37];
	static DfaState st16[37];
	static DfaState st17[37];
	static DfaState st18[37];
	static DfaState st19[37];
	static DfaState st20[37];
	static DfaState st21[37];
	static DfaState st22[37];
	static DfaState st23[37];
	static DfaState st24[37];
	static DfaState st25[37];
	static DfaState st26[37];
	static DfaState st27[37];
	static DfaState st28[37];
	static DfaState st29[37];
	static DfaState st30[37];
	static DfaState st31[37];
	static DfaState st32[37];
	static DfaState st33[37];
	static DfaState st34[37];
	static DfaState st35[37];
	static DfaState st36[37];
	static DfaState st37[37];
	static DfaState st38[37];
	static DfaState st39[37];
	static DfaState st40[37];
	static DfaState st41[37];
	static DfaState st42[37];
	static DfaState st43[37];
	static DfaState st44[37];
	static DfaState st45[37];
	static DfaState st46[37];
	static DfaState st47[37];
	static DfaState st48[37];
	static DfaState st49[37];
	static DfaState st50[37];
	static DfaState st51[37];
	static DfaState st52[37];
	static DfaState st53[37];
	static DfaState st54[37];
	static DfaState st55[37];
	static DfaState st56[37];
	static DfaState st57[37];
	static DfaState st58[37];
	static DfaState st59[37];
	static DfaState st60[37];
	static DfaState st61[37];
	static DfaState st62[37];
	static DfaState st63[37];
	static DfaState st64[37];
	static DfaState st65[37];
	static DfaState st66[37];
	static DfaState st67[37];
	static DfaState st68[37];
	static DfaState st69[37];
	static DfaState st70[37];
	static DfaState st71[37];
	static DfaState st72[37];
	static DfaState st73[37];
	static DfaState st74[37];
	static DfaState st75[37];
	static DfaState st76[37];
	static DfaState st77[37];
	static DfaState st78[37];
	static DfaState st79[37];
	static DfaState st80[37];
	static DfaState st81[37];
	static DfaState st82[37];
	static DfaState st83[37];
	static DfaState st84[37];
	static DfaState st85[37];
	static DfaState st86[37];
	static DfaState st87[37];
	static DfaState st88[37];
	static DfaState st89[37];
	static DfaState st90[37];
	static DfaState st91[7];
	static DfaState st92[7];
	static DfaState st93[7];
	static DfaState st94[7];
	static DfaState st95[7];
	static DfaState st96[7];
	static DfaState st97[7];
	static DfaState st98[7];
	static DfaState st99[7];
	static DfaState st100[7];
	static DfaState st101[5];
	static DfaState st102[5];
	static DfaState st103[5];
	static DfaState st104[5];
	static DfaState st105[5];
	static DfaState st106[5];
	static DfaState st107[5];
	static DfaState st108[5];
	static DfaState st109[5];
	static DfaState st110[5];
	static DfaState st111[5];
	static DfaState st112[5];
	static DfaState st113[5];
	static DfaState st114[5];
	static DfaState st115[5];
	static DfaState *dfa[116];
	static DfaState dfa_base[];
	static unsigned char *b_class_no[];
	static DfaState accepts[117];
	static DLGChar alternatives[117];
	static ANTLRTokenType (CueLexerBase::*actions[43])();
	static unsigned char shift0[257];
	static unsigned char shift1[257];
	static unsigned char shift2[257];
	static unsigned char shift3[257];
	static unsigned char shift4[257];
	int ZZSHIFT(int c) { return b_class_no[automaton][1+c]; }
//
// 133MR1 Deprecated feature to allow inclusion of user-defined code in DLG class header
//
#ifdef DLGLexerIncludeFile
#include DLGLexerIncludeFile
#endif
};
typedef ANTLRTokenType (CueLexerBase::*PtrCueLexerBaseMemberFunc)();
#endif
