#ifndef CueParserTokens_h
#define CueParserTokens_h
/* CueParserTokens.h -- List of labelled tokens and stuff
 *
 * Generated from: ./CueParser.g
 *
 * Terence Parr, Will Cohen, and Hank Dietz: 1989-1999
 * Purdue University Electrical Engineering
 * ANTLR Version 1.33MR20
 */
enum ANTLRTokenType {
	Eof=1,
	BeginString=4,
	TrackDef=5,
	Audio=6,
	Mode1_2048=7,
	Mode1_2352=8,
	Mode2_2336=9,
	Mode2_2352=10,
	Index=11,
	File=12,
	Pregap=13,
	Postgap=14,
	Binary=15,
	Motorola=16,
	Flags=17,
	Catalog=18,
	Isrc=19,
	Integer=21,
	EndString=22,
	StringQuote=23,
	StringOctal=24,
	String=25,
	FileName=28,
	CatalogNr=31,
	IsrcCode=34,
	DLGminToken=0,
	DLGmaxToken=9999};

#endif
