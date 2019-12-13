# Generates new website documentation via LyX and latex2html

HOST_SYSTEM := $(shell uname -s)

LYX_BIN=lyx
ifeq ($(HOST_SYSTEM), Darwin)
  LYX_BIN=/Applications/LyX.app/Contents/MacOS/lyx
endif
OUT=progman
DIAGRAMS= #core-web collection-web stream-web
DIAGRAM_OUTS=$(foreach dgm, $(DIAGRAMS), $(dgm).eps)
HTML_CONVERT=latex2html -no_subdir -reuse 1 -local_icons -split 5 -show_section_numbers

default: $(OUT)

#Call Lyx to output LaTeX, then replace \tabularnewline's with \\'s but not
#the first one (that creates a circularity which breaks TeX).
$(OUT).tex: $(OUT).lyx
	$(LYX_BIN) -e latex $< -E latex $@
	mv $@ $@.pre
	sed     -e '24,30s/tabularnewline/ZANZIBAR/' \
		-e 's/\\tabularnewline/\\\\/g' \
		-e 's/ZANZIBAR/tabularnewline/1' $@.pre > $@

%.pdf: %.lyx
	$(LYX_BIN) -e pdf $< $@

%.ps: %.lyx
	$(LYX_BIN) -e ps $< $@

%.eps: %.fig %.png $(OUT)-dir
	fig2dev -L eps $< $@
	cp $@ $(OUT)/$@

%.png: %.fig $(OUT)-dir
	fig2dev -L png $< $@
	cp $@ $(OUT)/$@

$(OUT)-dir:
	install -d $(OUT)/

diagrams: $(DIAGRAM_OUTS)

$(OUT): $(OUT).tex diagrams $(OUT)-dir
	cp $< $(OUT)/
	-cp $(OUT).bib $(OUT)/
	-cp $(OUT).aux $(OUT)/
	$(HTML_CONVERT) $(OUT)/$<
	-rm $(OUT)/*.pl
	-rm $(OUT)/images.*
	-rm $(OUT)/*.tex
	-rm $(OUT)/WARNINGS
	-rm $(OUT).tar.gz
	tar czf $(OUT).tar.gz $(OUT)/

clean:
	-rm -Rf $(OUT)/
	-rm $(DIAGRAM_OUTS)
	-rm $@.pre

fullclean: clean
	-rm -Rf $(OUT).tex
