#ifndef TocParserTokens_h
#define TocParserTokens_h
/* TocParserTokens.h -- List of labelled tokens and stuff
 *
 * Generated from: ./TocParser.g
 *
 * Terence Parr, Will Cohen, and Hank Dietz: 1989-1999
 * Purdue University Electrical Engineering
 * ANTLR Version 1.33MR20
 */
enum ANTLRTokenType {
	Eof=1,
	Comment=3,
	BeginString=5,
	Integer=6,
	TrackDef=7,
	Audio=8,
	Mode0=9,
	Mode1=10,
	Mode1Raw=11,
	Mode2=12,
	Mode2Raw=13,
	Mode2Form1=14,
	Mode2Form2=15,
	Mode2FormMix=16,
	Index=17,
	Catalog=18,
	Isrc=19,
	No=20,
	Copy=21,
	PreEmphasis=22,
	TwoChannel=23,
	FourChannel=24,
	DataFile=28,
	Silence=29,
	Zero=30,
	Pregap=31,
	Start=32,
	End=33,
	TocTypeCdda=34,
	TocTypeCdrom=35,
	TocTypeCdromXa=36,
	TocTypeCdi=37,
	Swap=38,
	CdText=39,
	Language=40,
	LanguageMap=41,
	Title=42,
	Performer=43,
	Songwriter=44,
	Composer=45,
	Arranger=46,
	Message=47,
	DiscId=48,
	Genre=49,
	TocInfo1=50,
	TocInfo2=51,
	Reserved1=52,
	Reserved2=53,
	Reserved3=54,
	Reserved4=55,
	UpcEan=56,
	SizeInfo=57,
	LangEn=58,
	EndString=59,
	StringQuote=60,
	StringOctal=61,
	String=62,
	DLGminToken=0,
	DLGmaxToken=9999};

#endif
