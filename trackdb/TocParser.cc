/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 2026 Denis Leroy <denis@poolshark.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

// Hand-written lexer and recursive-descent parser for the .toc file format.
// This replaces the former PCCTS (ANTLR 1.33) generated parser and accepts
// the same language. The grammar, in EBNF:
//
//   toc            : ( CATALOG string | tocType )*
//                    [ FIRST_TRACK_NO integer ]
//                    [ cdTextGlobal ]
//                    track+ EOF
//   track          : TRACK trackMode [ subChannelMode ]
//                    ( ISRC string | [ NO ] COPY | [ NO ] PRE_EMPHASIS
//                    | TWO_CHANNEL_AUDIO | FOUR_CHANNEL_AUDIO )*
//                    [ cdTextTrack ]
//                    [ PREGAP msf ]
//                    ( subTrack | START [ msf ] | END [ msf ] )+
//                    ( INDEX msf )*
//   subTrack       : ( FILE | AUDIOFILE ) string [ SWAP ] [ "#" sLong ]
//                        samples [ samples ]
//                  | DATAFILE string [ "#" sLong ] [ dataLength ]
//                  | FIFO string dataLength
//                  | SILENCE samples
//                  | ZERO [ dataMode ] [ subChannelMode ] dataLength
//   samples        : msf | uLong
//   dataLength     : msf | uLong
//   msf            : integer ":" integer ":" integer
//   cdTextGlobal   : CD_TEXT "{" [ cdTextLangMap ] cdTextBlock* "}"
//   cdTextTrack    : CD_TEXT "{" cdTextBlock* "}"
//   cdTextLangMap  : LANGUAGE_MAP "{" ( integer ":" ( integer | EN ) )+ "}"
//   cdTextBlock    : LANGUAGE integer "{" [ encoding ] cdTextItem* "}"
//   cdTextItem     : packType ( stringEmpty | binaryData )
//   binaryData     : "{" [ integer ( "," integer )* ] "}"
//
// Lexical rules (kept identical to the old DLG lexer): keywords are
// case-sensitive and matched by longest prefix (no word boundary is
// required, e.g. "TRACKAUDIO" is TRACK followed by AUDIO). Comments start
// with "//" and run to the end of the line. Inside strings, the escapes \",
// \\ and \NNN (octal) are recognized; newline characters are not allowed
// (tabs are, as the old parser accepted them).
//
// Semantic errors are reported and parsing continues so that all of them get
// reported. Lexical and syntax errors are reported and abort the parse.

#include <config.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "CdTextContainer.h"
#include "CdTextItem.h"
#include "Toc.h"
#include "TocParser.h"
#include "log.h"
#include "util.h"

// Maximum length of binary data for CD-TEXT
#define MAX_CD_TEXT_DATA_LEN (256 * 12)

namespace
{

enum class T {
    Eof,
    Integer,
    String,
    Colon,
    Comma,
    LBrace,
    RBrace,
    Hash,

    TrackDef,
    FirstTrackNo,
    Audio,
    Mode0,
    Mode1,
    Mode1Raw,
    Mode2,
    Mode2Raw,
    Mode2Form1,
    Mode2Form2,
    Mode2FormMix,
    SubChanRw,
    SubChanRwRaw,
    Index,
    Catalog,
    Isrc,
    No,
    Copy,
    PreEmphasis,
    TwoChannel,
    FourChannel,
    AudioFile,
    DataFile,
    Fifo,
    Silence,
    Zero,
    Pregap,
    Start,
    End,
    TocTypeCdda,
    TocTypeCdrom,
    TocTypeCdromXa,
    TocTypeCdi,
    Swap,

    CdText,
    Language,
    LanguageMap,
    Title,
    Performer,
    Songwriter,
    Composer,
    Arranger,
    Message,
    DiscId,
    Genre,
    TocInfo1,
    TocInfo2,
    Reserved1,
    Reserved2,
    Reserved3,
    Reserved4,
    Closed,
    UpcEan,
    SizeInfo,
    LangEn,
    EncodingLatin,
    EncodingAscii,
    EncodingJis,
    EncodingKorean,
    EncodingMandarin,
};

struct Keyword {
    const char *text;
    T type;
};

const Keyword keywords[] = {
    {"TRACK", T::TrackDef},
    {"FIRST_TRACK_NO", T::FirstTrackNo},
    {"AUDIO", T::Audio},
    {"MODE0", T::Mode0},
    {"MODE1", T::Mode1},
    {"MODE1_RAW", T::Mode1Raw},
    {"MODE2", T::Mode2},
    {"MODE2_RAW", T::Mode2Raw},
    {"MODE2_FORM1", T::Mode2Form1},
    {"MODE2_FORM2", T::Mode2Form2},
    {"MODE2_FORM_MIX", T::Mode2FormMix},
    {"RW", T::SubChanRw},
    {"RW_RAW", T::SubChanRwRaw},
    {"INDEX", T::Index},
    {"CATALOG", T::Catalog},
    {"ISRC", T::Isrc},
    {"NO", T::No},
    {"COPY", T::Copy},
    {"PRE_EMPHASIS", T::PreEmphasis},
    {"TWO_CHANNEL_AUDIO", T::TwoChannel},
    {"FOUR_CHANNEL_AUDIO", T::FourChannel},
    {"AUDIOFILE", T::AudioFile},
    {"FILE", T::AudioFile},
    {"DATAFILE", T::DataFile},
    {"FIFO", T::Fifo},
    {"SILENCE", T::Silence},
    {"ZERO", T::Zero},
    {"PREGAP", T::Pregap},
    {"START", T::Start},
    {"END", T::End},
    {"CD_DA", T::TocTypeCdda},
    {"CD_ROM", T::TocTypeCdrom},
    {"CD_ROM_XA", T::TocTypeCdromXa},
    {"CD_I", T::TocTypeCdi},
    {"SWAP", T::Swap},
    {"CD_TEXT", T::CdText},
    {"LANGUAGE", T::Language},
    {"LANGUAGE_MAP", T::LanguageMap},
    {"TITLE", T::Title},
    {"PERFORMER", T::Performer},
    {"SONGWRITER", T::Songwriter},
    {"COMPOSER", T::Composer},
    {"ARRANGER", T::Arranger},
    {"MESSAGE", T::Message},
    {"DISC_ID", T::DiscId},
    {"GENRE", T::Genre},
    {"TOC_INFO1", T::TocInfo1},
    {"TOC_INFO2", T::TocInfo2},
    {"RESERVED1", T::Reserved1},
    {"RESERVED2", T::Reserved2},
    {"RESERVED3", T::Reserved3},
    {"RESERVED4", T::Reserved4},
    {"CLOSED", T::Closed},
    {"UPC_EAN", T::UpcEan},
    {"SIZE_INFO", T::SizeInfo},
    {"EN", T::LangEn},
    {"ENCODING_ISO_8859_1", T::EncodingLatin},
    {"ENCODING_ASCII", T::EncodingAscii},
    {"ENCODING_MS_JIS", T::EncodingJis},
    {"ENCODING_KOREAN", T::EncodingKorean},
    {"ENCODING_MANDARIN", T::EncodingMandarin},
};

struct Token {
    T type = T::Eof;
    std::string text; // keyword/integer text, or string contents
    int line = 0;
};

// Thrown after a lexical or syntax error has been reported.
struct ParseAbort {
};

class Lexer
{
  public:
    Lexer(std::string input, const char *filename) : in_(std::move(input)), filename_(filename) {}

    Token next();

  private:
    std::string in_;
    size_t pos_ = 0;
    int line_ = 1;
    const char *filename_;

    int peekc(size_t off = 0) const
    {
        return pos_ + off < in_.size() ? (unsigned char)in_[pos_ + off] : -1;
    }

    [[noreturn]] void illegal(const std::string &text)
    {
        log_message(-2, "%s:%d: Illegal token: %s", filename_, line_, text.c_str());
        throw ParseAbort();
    }

    void lexString(Token &tok);
};

Token Lexer::next()
{
    // Skip whitespace and comments.
    for (;;) {
        int c = peekc();
        if (c == ' ' || c == '\t' || c == '\r') {
            pos_++;
        } else if (c == '\n') {
            pos_++;
            line_++;
        } else if (c == '/' && peekc(1) == '/') {
            while (peekc() != -1 && peekc() != '\n')
                pos_++;
        } else {
            break;
        }
    }

    Token tok;
    tok.line = line_;

    int c = peekc();
    if (c == -1) {
        tok.type = T::Eof;
        return tok;
    }

    switch (c) {
    case ':':
        tok.type = T::Colon;
        break;
    case ',':
        tok.type = T::Comma;
        break;
    case '{':
        tok.type = T::LBrace;
        break;
    case '}':
        tok.type = T::RBrace;
        break;
    case '#':
        tok.type = T::Hash;
        break;
    case '"':
        pos_++;
        lexString(tok);
        return tok;
    default:
        if (isdigit(c)) {
            size_t start = pos_;
            while (isdigit(peekc()))
                pos_++;
            tok.type = T::Integer;
            tok.text = in_.substr(start, pos_ - start);
            return tok;
        }

        // Keyword: longest matching prefix.
        const Keyword *best = nullptr;
        size_t bestLen = 0;
        for (const auto &kw : keywords) {
            size_t len = strlen(kw.text);
            if (len > bestLen && in_.compare(pos_, len, kw.text) == 0) {
                best = &kw;
                bestLen = len;
            }
        }
        if (best) {
            tok.type = best->type;
            tok.text = best->text;
            pos_ += bestLen;
            return tok;
        }

        size_t end = pos_;
        while (end < in_.size() && (isalnum((unsigned char)in_[end]) || in_[end] == '_'))
            end++;
        if (end == pos_)
            end++;
        illegal(in_.substr(pos_, end - pos_));
    }

    tok.text = std::string(1, (char)c);
    pos_++;
    return tok;
}

// Lexes the contents of a string after the opening quote. Escape sequences
// \" and \\ are resolved; octal escapes \NNN are left in place and resolved
// later by Util::processMixedString().
void Lexer::lexString(Token &tok)
{
    tok.type = T::String;

    for (;;) {
        int c = peekc();
        switch (c) {
        case -1:
            log_message(-2, "%s:%d: Unterminated string.", filename_, tok.line);
            throw ParseAbort();
        case '"':
            pos_++;
            return;
        case '\n':
            illegal("newline in string");
        case '\\':
            if (peekc(1) == '"' || peekc(1) == '\\') {
                tok.text += (char)peekc(1);
                pos_ += 2;
            } else if (isdigit(peekc(1)) && isdigit(peekc(2)) && isdigit(peekc(3))) {
                tok.text.append(in_, pos_, 4);
                pos_ += 4;
            } else {
                illegal(in_.substr(pos_, peekc(1) == -1 ? 1 : 2));
            }
            break;
        default:
            tok.text += (char)c;
            pos_++;
            break;
        }
    }
}

} // namespace

class TocParser
{
  public:
    TocParser(std::string input, const char *filename)
        : lexer_(std::move(input), filename), filename_(filename)
    {
    }

    Toc *parse();

  private:
    Lexer lexer_;
    std::deque<Token> la_; // lookahead buffer
    const char *filename_;
    int error_ = 0;

    const Token &peek(size_t k = 0)
    {
        while (la_.size() <= k)
            la_.push_back(lexer_.next());
        return la_[k];
    }
    T type(size_t k = 0) { return peek(k).type; }
    bool is(T t) { return type() == t; }

    Token consume()
    {
        peek();
        Token t = std::move(la_.front());
        la_.pop_front();
        return t;
    }

    [[noreturn]] void syntaxError(const char *expected)
    {
        const Token &t = peek();
        std::string text;
        if (t.type == T::Eof)
            text = "EOF";
        else if (t.type == T::String)
            text = "\"" + t.text + "\"";
        else
            text = t.text;
        log_message(-2, "%s:%d: syntax error at \"%s\", expected %s.", filename_, t.line,
                    text.c_str(), expected);
        throw ParseAbort();
    }

    Token expect(T t, const char *what)
    {
        if (!is(t))
            syntaxError(what);
        return consume();
    }

    std::unique_ptr<Toc> toc();
    std::unique_ptr<Track> track(int &lineNr);
    std::unique_ptr<SubTrack> subTrack(TrackData::Mode trackType,
                                       TrackData::SubChannelMode subChanType, int &lineNr);
    std::string string();
    std::string stringEmpty(bool &is_utf8);
    unsigned long uLong();
    long sLong();
    int integer(int &lineNr);
    Msf msf();
    unsigned long samples();
    unsigned long dataLength(TrackData::Mode mode, TrackData::SubChannelMode sm);
    bool dataMode(TrackData::Mode &m);
    TrackData::Mode trackMode();
    bool subChannelMode(TrackData::SubChannelMode &m);
    bool tocType(Toc::Type &t);
    bool packType(CdTextItem::PackType &t, int &lineNr);
    std::vector<unsigned char> binaryData();
    CdTextItem *cdTextItem(int blockNr, int &lineNr);
    void cdTextBlock(CdTextContainer &container, bool isTrack);
    void cdTextLanguageMap(CdTextContainer &container);
    void cdTextTrack(CdTextContainer &container);
    void cdTextGlobal(CdTextContainer &container);
};

Toc *TocParser::parse()
{
    try {
        std::unique_ptr<Toc> t = toc();
        if (error_)
            return nullptr;
        return t.release();
    } catch (const ParseAbort &) {
        return nullptr;
    }
}

std::unique_ptr<Toc> TocParser::toc()
{
    auto t = std::make_unique<Toc>();
    Toc::Type toctype;

    for (;;) {
        if (is(T::Catalog)) {
            int line = consume().line;
            std::string catalog = string();
            if (!catalog.empty() && t->catalog(catalog.c_str()) != 0) {
                log_message(-2, "%s:%d: Illegal catalog number: %s.\n", filename_, line,
                            catalog.c_str());
                error_ = 1;
            }
        } else if (tocType(toctype)) {
            t->tocType(toctype);
        } else {
            break;
        }
    }

    if (is(T::FirstTrackNo)) {
        consume();
        int firstLine;
        int firsttrack = integer(firstLine);
        if (firsttrack > 0 && firsttrack < 100) {
            t->firstTrackNo(firsttrack);
        } else {
            log_message(-2, "%s:%d: Illegal track number: %d\n", filename_, firstLine, firsttrack);
            error_ = 1;
        }
    }

    if (is(T::CdText))
        cdTextGlobal(t->cdtext_);

    if (!is(T::TrackDef))
        syntaxError("TRACK");

    while (is(T::TrackDef)) {
        int lineNr = 0;
        std::unique_ptr<Track> tr = track(lineNr);
        if (t->append(tr.get()) != 0) {
            log_message(-2, "%s:%d: First track must not have a pregap.\n", filename_, lineNr);
            error_ = 1;
        }
    }

    t->enforceTextEncoding();
    expect(T::Eof, "TRACK or end of file");

    return t;
}

std::unique_ptr<Track> TocParser::track(int &lineNr)
{
    TrackData::SubChannelMode subChanType = TrackData::SUBCHAN_NONE;
    Msf pos;
    Msf startPos; // end of pre-gap
    Msf endPos;   // start of post-gap
    int startPosLine = 0;
    int endPosLine = 0;

    lineNr = expect(T::TrackDef, "TRACK").line;
    TrackData::Mode trackType = trackMode();
    subChannelMode(subChanType);

    auto tr = std::make_unique<Track>(trackType, subChanType);

    for (;;) {
        if (is(T::Isrc)) {
            int line = consume().line;
            std::string isrcCode = string();
            if (!isrcCode.empty() && tr->isrc(isrcCode.c_str()) != 0) {
                log_message(-2, "%s:%d: Illegal ISRC code: %s.\n", filename_, line,
                            isrcCode.c_str());
                error_ = 1;
            }
        } else if (is(T::No) || is(T::Copy) || is(T::PreEmphasis)) {
            int flag = 1;
            if (is(T::No)) {
                consume();
                flag = 0;
            }
            if (is(T::Copy)) {
                consume();
                tr->copyPermitted(flag);
            } else if (is(T::PreEmphasis)) {
                consume();
                tr->preEmphasis(flag);
            } else {
                syntaxError("COPY or PRE_EMPHASIS");
            }
        } else if (is(T::TwoChannel)) {
            consume();
            tr->audioType(0);
        } else if (is(T::FourChannel)) {
            consume();
            tr->audioType(1);
        } else {
            break;
        }
    }

    if (is(T::CdText))
        cdTextTrack(tr->cdtext_);

    if (is(T::Pregap)) {
        int line = consume().line;
        Msf length = msf();
        if (length.lba() == 0) {
            log_message(-2, "%s:%d: Length of pregap is zero.\n", filename_, line);
            error_ = 1;
        } else {
            if (trackType == TrackData::AUDIO) {
                tr->append(SubTrack(SubTrack::DATA, TrackData(length.samples())));
            } else {
                tr->append(SubTrack(
                    SubTrack::DATA,
                    TrackData(trackType, subChanType,
                              length.lba() * TrackData::dataBlockSize(trackType, subChanType))));
            }
            startPos = tr->length();
            startPosLine = line;
        }
    }

    bool any = false;
    for (;;) {
        T tt = type();
        if (tt == T::AudioFile || tt == T::DataFile || tt == T::Fifo || tt == T::Silence ||
            tt == T::Zero) {
            int stLine = 0;
            std::unique_ptr<SubTrack> st = subTrack(trackType, subChanType, stLine);
            if (st && tr->append(*st) == 2) {
                log_message(-2,
                            "%s:%d: Mixing of FILE/AUDIOFILE/SILENCE and DATAFILE/ZERO "
                            "statements not allowed.",
                            filename_, stLine);
                log_message(-2, "%s:%d: PREGAP acts as SILENCE in audio tracks.", filename_,
                            stLine);
                error_ = 1;
            }
        } else if (tt == T::Start || tt == T::End) {
            int line = consume().line;
            bool posGiven = false;
            if (is(T::Integer)) {
                pos = msf();
                posGiven = true;
            }
            if (!posGiven)
                pos = tr->length(); // retrieve current position

            if (tt == T::Start) {
                if (startPosLine != 0) {
                    log_message(-2, "%s:%d: Track start (end of pre-gap) already defined.\n",
                                filename_, line);
                    error_ = 1;
                } else {
                    startPos = pos;
                    startPosLine = line;
                }
            } else {
                if (endPosLine != 0) {
                    log_message(-2, "%s:%d: Track end (start of post-gap) already defined.\n",
                                filename_, line);
                    error_ = 1;
                } else {
                    endPos = pos;
                    endPosLine = line;
                }
            }
            pos = Msf(0);
        } else {
            break;
        }
        any = true;
    }
    if (!any)
        syntaxError("FILE, AUDIOFILE, DATAFILE, FIFO, SILENCE, ZERO, START or END");

    // set track start (end of pre-gap) and check for minimal track length
    if (startPosLine != 0 && tr->start(startPos) != 0) {
        log_message(-2, "%s:%d: START %s behind or at track end.\n", filename_, startPosLine,
                    startPos.str());
        error_ = 1;
    }

    while (is(T::Index)) {
        int line = consume().line;
        Msf indexIncrement = msf();
        switch (tr->appendIndex(indexIncrement)) {
        case 1:
            log_message(-2, "%s:%d: More than 98 index increments.\n", filename_, line);
            error_ = 1;
            break;
        case 2:
            log_message(-2, "%s:%d: Index beyond track end.\n", filename_, line);
            error_ = 1;
            break;
        case 3:
            log_message(-2, "%s:%d: Index at start of track.\n", filename_, line);
            error_ = 1;
            break;
        }
    }

    // set track end (start of post-gap)
    if (endPosLine != 0) {
        switch (tr->end(endPos)) {
        case 1:
            log_message(-2, "%s:%d: END %s behind or at track end.\n", filename_, endPosLine,
                        endPos.str());
            error_ = 1;
            break;
        case 2:
            log_message(-2, "%s:%d: END %s within pre-gap.\n", filename_, endPosLine, endPos.str());
            error_ = 1;
            break;
        case 3:
            log_message(-2, "%s:%d: END %s: Cannot create index mark for post-gap.\n", filename_,
                        endPosLine, endPos.str());
            error_ = 1;
            break;
        }
    }

    return tr;
}

std::unique_ptr<SubTrack> TocParser::subTrack(TrackData::Mode trackType,
                                              TrackData::SubChannelMode subChanType, int &lineNr)
{
    std::unique_ptr<SubTrack> st;
    std::string filename;
    unsigned long len = 0;
    long offset = 0;
    TrackData::Mode dMode;

    lineNr = 0;

    switch (type()) {
    case T::AudioFile: {
        lineNr = consume().line;
        filename = string();
        bool swapSamples = false;
        if (is(T::Swap)) {
            consume();
            swapSamples = true;
        }
        if (is(T::Hash)) {
            consume();
            offset = sLong();
        }
        unsigned long start = samples();
        if (is(T::Integer))
            len = samples();

        st = std::make_unique<SubTrack>(SubTrack::DATA,
                                        TrackData(filename.c_str(), offset, start, len));
        st->swapSamples(swapSamples);

        if (trackType != TrackData::AUDIO) {
            log_message(-2, "%s:%d: FILE/AUDIOFILE statements are only allowed for audio tracks.",
                        filename_, lineNr);
            error_ = 1;
        }
        if (subChanType != TrackData::SUBCHAN_NONE) {
            log_message(-2,
                        "%s:%d: FILE/AUDIOFILE statements are only allowed for audio tracks "
                        "without sub-channel mode.",
                        filename_, lineNr);
            error_ = 1;
        }
        break;
    }

    case T::DataFile:
        lineNr = consume().line;
        filename = string();
        dMode = trackType;
        if (is(T::Hash)) {
            consume();
            offset = sLong();
        }
        if (is(T::Integer))
            len = dataLength(dMode, subChanType);
        st = std::make_unique<SubTrack>(
            SubTrack::DATA, TrackData(dMode, subChanType, filename.c_str(), offset, len));
        break;

    case T::Fifo:
        consume();
        filename = string();
        len = dataLength(trackType, subChanType);
        st = std::make_unique<SubTrack>(SubTrack::DATA,
                                        TrackData(trackType, subChanType, filename.c_str(), len));
        break;

    case T::Silence:
        lineNr = consume().line;
        len = samples();
        st = std::make_unique<SubTrack>(SubTrack::DATA, TrackData(len));
        if (len == 0) {
            log_message(-2, "%s:%d: Length of silence is 0.\n", filename_, lineNr);
            error_ = 1;
        }
        if (trackType != TrackData::AUDIO) {
            log_message(-2, "%s:%d: SILENCE statements are only allowed for audio tracks.",
                        filename_, lineNr);
            error_ = 1;
        }
        if (subChanType != TrackData::SUBCHAN_NONE) {
            log_message(-2,
                        "%s:%d: SILENCE statements are only allowed for audio tracks without "
                        "sub-channel mode.",
                        filename_, lineNr);
            error_ = 1;
        }
        break;

    case T::Zero:
        lineNr = consume().line;
        dMode = trackType;
        dataMode(dMode);
        subChannelMode(subChanType);
        len = dataLength(dMode, subChanType);
        st = std::make_unique<SubTrack>(SubTrack::DATA, TrackData(dMode, subChanType, len));
        if (len == 0) {
            log_message(-2, "%s:%d: Length of zero data is 0.\n", filename_, lineNr);
            error_ = 1;
        }
        break;

    default:
        syntaxError("FILE, AUDIOFILE, DATAFILE, FIFO, SILENCE or ZERO");
    }

    if (st->length() == 0) {
        // try to determine length
        if (st->determineLength() != 0) {
            log_message(-2, "%s:%d: Cannot determine length of track data specification.",
                        filename_, lineNr);
            error_ = 1;
        }
    }

    return st;
}

// Non-empty string.
std::string TocParser::string()
{
    if (is(T::String) && peek().text.empty())
        syntaxError("non-empty string");
    bool is_utf8;
    return stringEmpty(is_utf8);
}

std::string TocParser::stringEmpty(bool &is_utf8)
{
    Token t = expect(T::String, "string");
    is_utf8 = false;
    if (!Util::processMixedString(t.text, is_utf8)) {
        log_message(-2, "%s:%d: Illegal mixed UTF-8 and binary.", filename_, t.line);
        error_ = 1;
    }
    return t.text;
}

unsigned long TocParser::uLong()
{
    return strtoul(expect(T::Integer, "integer").text.c_str(), NULL, 10);
}

long TocParser::sLong()
{
    return strtol(expect(T::Integer, "integer").text.c_str(), NULL, 10);
}

int TocParser::integer(int &lineNr)
{
    Token t = expect(T::Integer, "integer");
    lineNr = t.line;
    return atol(t.text.c_str());
}

Msf TocParser::msf()
{
    int minLine, secLine, fracLine;
    int err = 0;

    int min = integer(minLine);
    expect(T::Colon, "\":\"");
    int sec = integer(secLine);
    expect(T::Colon, "\":\"");
    int frac = integer(fracLine);

    if (min < 0) {
        log_message(-2, "%s:%d: Illegal minute field: %d\n", filename_, minLine, min);
        err = error_ = 1;
    }
    if (sec < 0 || sec > 59) {
        log_message(-2, "%s:%d: Illegal second field: %d\n", filename_, secLine, sec);
        err = error_ = 1;
    }
    if (frac < 0 || frac > 74) {
        log_message(-2, "%s:%d: Illegal fraction field: %d\n", filename_, fracLine, frac);
        err = error_ = 1;
    }

    return err ? Msf(0) : Msf(min, sec, frac);
}

// Either an MSF value (converted to samples) or a plain number of samples.
unsigned long TocParser::samples()
{
    if (is(T::Integer) && type(1) == T::Colon)
        return msf().samples();
    return uLong();
}

// Either an MSF value (converted to bytes) or a plain number of bytes.
unsigned long TocParser::dataLength(TrackData::Mode mode, TrackData::SubChannelMode sm)
{
    if (is(T::Integer) && type(1) == T::Colon)
        return msf().lba() * TrackData::dataBlockSize(mode, sm);
    return uLong();
}

bool TocParser::dataMode(TrackData::Mode &m)
{
    if (is(T::Mode0)) {
        consume();
        m = TrackData::MODE0;
        return true;
    }
    if (is(T::Audio) || is(T::Mode1) || is(T::Mode1Raw) || is(T::Mode2) || is(T::Mode2Raw) ||
        is(T::Mode2Form1) || is(T::Mode2Form2) || is(T::Mode2FormMix)) {
        m = trackMode();
        return true;
    }
    return false;
}

TrackData::Mode TocParser::trackMode()
{
    TrackData::Mode m;
    switch (type()) {
    case T::Audio:
        m = TrackData::AUDIO;
        break;
    case T::Mode1:
        m = TrackData::MODE1;
        break;
    case T::Mode1Raw:
        m = TrackData::MODE1_RAW;
        break;
    case T::Mode2:
        m = TrackData::MODE2;
        break;
    case T::Mode2Raw:
        m = TrackData::MODE2_RAW;
        break;
    case T::Mode2Form1:
        m = TrackData::MODE2_FORM1;
        break;
    case T::Mode2Form2:
        m = TrackData::MODE2_FORM2;
        break;
    case T::Mode2FormMix:
        m = TrackData::MODE2_FORM_MIX;
        break;
    default:
        syntaxError("track mode");
    }
    consume();
    return m;
}

bool TocParser::subChannelMode(TrackData::SubChannelMode &m)
{
    if (is(T::SubChanRw))
        m = TrackData::SUBCHAN_RW;
    else if (is(T::SubChanRwRaw))
        m = TrackData::SUBCHAN_RW_RAW;
    else
        return false;
    consume();
    return true;
}

bool TocParser::tocType(Toc::Type &t)
{
    switch (type()) {
    case T::TocTypeCdda:
        t = Toc::Type::CD_DA;
        break;
    case T::TocTypeCdrom:
        t = Toc::Type::CD_ROM;
        break;
    case T::TocTypeCdromXa:
        t = Toc::Type::CD_ROM_XA;
        break;
    case T::TocTypeCdi:
        t = Toc::Type::CD_I;
        break;
    default:
        return false;
    }
    consume();
    return true;
}

bool TocParser::packType(CdTextItem::PackType &t, int &lineNr)
{
    using P = CdTextItem::PackType;

    switch (type()) {
    case T::Title:
        t = P::TITLE;
        break;
    case T::Performer:
        t = P::PERFORMER;
        break;
    case T::Songwriter:
        t = P::SONGWRITER;
        break;
    case T::Composer:
        t = P::COMPOSER;
        break;
    case T::Arranger:
        t = P::ARRANGER;
        break;
    case T::Message:
        t = P::MESSAGE;
        break;
    case T::DiscId:
        t = P::DISK_ID;
        break;
    case T::Genre:
        t = P::GENRE;
        break;
    case T::TocInfo1:
        t = P::TOC_INFO1;
        break;
    case T::TocInfo2:
        t = P::TOC_INFO2;
        break;
    case T::Reserved1:
        t = P::RES1;
        break;
    case T::Reserved2:
        t = P::RES2;
        break;
    case T::Reserved3:
        t = P::RES3;
        break;
    case T::Reserved4:
    case T::Closed:
        t = P::CLOSED;
        break;
    case T::UpcEan:
    case T::Isrc:
        t = P::UPCEAN_ISRC;
        break;
    case T::SizeInfo:
        t = P::SIZE_INFO;
        break;
    default:
        return false;
    }
    lineNr = consume().line;
    return true;
}

std::vector<unsigned char> TocParser::binaryData()
{
    std::vector<unsigned char> buf;

    expect(T::LBrace, "\"{\"");

    if (is(T::Integer)) {
        for (;;) {
            int lineNr;
            int i = integer(lineNr);
            if (i < 0 || i > 255) {
                log_message(-2, "%s:%d: Illegal binary data: %d", filename_, lineNr, i);
                error_ = 1;
                i = 0;
            }

            if (buf.size() >= MAX_CD_TEXT_DATA_LEN) {
                log_message(-2, "%s:%d: Binary data exceeds maximum length (%d).", filename_,
                            lineNr, MAX_CD_TEXT_DATA_LEN);
                error_ = 1;
            } else {
                buf.push_back(i);
            }

            if (!is(T::Comma))
                break;
            consume();
        }
    }

    expect(T::RBrace, "\"}\"");
    return buf;
}

CdTextItem *TocParser::cdTextItem(int blockNr, int &lineNr)
{
    CdTextItem::PackType type;

    if (!packType(type, lineNr))
        syntaxError("CD-TEXT item");

    if (is(T::String)) {
        bool is_utf8 = false;
        std::string s = stringEmpty(is_utf8);
        auto item = new CdTextItem(type, blockNr);
        if (is_utf8)
            item->setText(s.c_str());
        else
            item->setRawText(s);
        return item;
    }

    if (is(T::LBrace)) {
        std::vector<unsigned char> data = binaryData();
        auto item = new CdTextItem(type, blockNr);
        item->setData(data.data(), data.size());
        return item;
    }

    syntaxError("string or \"{\"");
}

void TocParser::cdTextBlock(CdTextContainer &container, bool isTrack)
{
    int lineNr = 0;

    expect(T::Language, "LANGUAGE");
    int blockNr = integer(lineNr);
    expect(T::LBrace, "\"{\"");

    if (blockNr < 0 || blockNr > 7) {
        log_message(-2, "%s:%d: Invalid block number, allowed range: [0..7].", filename_, lineNr);
        error_ = 1;
        blockNr = 0;
    }

    switch (type()) {
    case T::EncodingLatin:
        container.encoding(blockNr, Util::Encoding::LATIN);
        consume();
        break;
    case T::EncodingAscii:
        container.encoding(blockNr, Util::Encoding::ASCII);
        consume();
        break;
    case T::EncodingJis:
        container.encoding(blockNr, Util::Encoding::MSJIS);
        consume();
        break;
    case T::EncodingKorean:
        container.encoding(blockNr, Util::Encoding::KOREAN);
        consume();
        break;
    case T::EncodingMandarin:
        container.encoding(blockNr, Util::Encoding::MANDARIN);
        consume();
        break;
    default:
        break;
    }

    while (!is(T::RBrace)) {
        std::unique_ptr<CdTextItem> item(cdTextItem(blockNr, lineNr));
        int type = (int)item->packType();

        if (isTrack && ((type > 0x86 && type <= 0x89) || type == 0x8f)) {
            log_message(-2, "%s:%d: Invalid CD-TEXT item for a track.", filename_, lineNr);
            error_ = 1;
        } else {
            container.add(item.release());
        }
    }

    consume(); // "}"
}

void TocParser::cdTextLanguageMap(CdTextContainer &container)
{
    expect(T::LanguageMap, "LANGUAGE_MAP");
    expect(T::LBrace, "\"{\"");

    do {
        int blockNrLine;
        int langLine = 0;
        int lang;

        int blockNr = integer(blockNrLine);
        expect(T::Colon, "\":\"");
        if (is(T::LangEn)) {
            consume();
            lang = 9;
        } else {
            lang = integer(langLine);
        }

        if (blockNr >= 0 && blockNr <= 7) {
            if (lang >= 0 && lang <= 255) {
                container.language(blockNr, lang);
            } else {
                log_message(-2, "%s:%d: Invalid language code, allowed range: [0..255].", filename_,
                            langLine);
                error_ = 1;
            }
        } else {
            log_message(-2, "%s:%d: Invalid language number, allowed range: [0..7].", filename_,
                        blockNrLine);
            error_ = 1;
        }
    } while (is(T::Integer));

    expect(T::RBrace, "\"}\"");
}

void TocParser::cdTextTrack(CdTextContainer &container)
{
    expect(T::CdText, "CD_TEXT");
    expect(T::LBrace, "\"{\"");
    while (is(T::Language))
        cdTextBlock(container, true);
    expect(T::RBrace, "LANGUAGE or \"}\"");
}

void TocParser::cdTextGlobal(CdTextContainer &container)
{
    expect(T::CdText, "CD_TEXT");
    expect(T::LBrace, "\"{\"");
    if (is(T::LanguageMap))
        cdTextLanguageMap(container);
    while (is(T::Language))
        cdTextBlock(container, false);
    expect(T::RBrace, "LANGUAGE or \"}\"");
}

Toc *parseToc(const char *input, const char *filename)
{
    TocParser parser(input, filename);
    return parser.parse();
}

Toc *parseToc(FILE *fp, const char *filename)
{
    std::string input;
    char buf[8192];
    size_t n;

    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0)
        input.append(buf, n);

    TocParser parser(std::move(input), filename);
    return parser.parse();
}
