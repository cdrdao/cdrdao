TARGETS = \
	dlg/dlg		\
	antlr/antlr

all: $(TARGETS)

dlg/dlg:
	$(CC) -O -Idlg -Ih -std=c9x \
		-Wno-parentheses-equality -Wno-deprecated-non-prototype \
		-DUSER_ZZSYN  -DZZLEXBUFSIZE=32000 \
		dlg/*.c	-o $@

antlr/antlr:
	$(CC) -O -Iantlr -Ih -std=c9x \
		-Wno-parentheses-equality -Wno-deprecated-non-prototype \
		-DUSER_ZZSYN  -DZZLEXBUFSIZE=32000 \
		antlr/*.c -o $@

clean:
	rm -f $(TARGETS)


regen: $(TARGETS)
	cd ../trackdb && \
	../pccts/antlr/antlr -nopurify -k 3 -CC  -w2 \
		-fl TocLexer.dlg \
		-ft TocParserTokens.h \
		TocParser.g && \
	../pccts/dlg/dlg -C2 -CC -cl TocLexerBase TocLexer.dlg
