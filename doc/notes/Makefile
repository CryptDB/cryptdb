TEX	 := $(wildcard *.tex)
FIGS_ODG := $(wildcard fig/*.odg)
FIGS_PDF := $(patsubst %.odg,%.pdf,$(FIGS_ODG))
GRAPHS	 := # fig/trantput.pdf fig/queries.pdf fig/microbars.pdf fig/tputbars.pdf
SVG_PDFS := $(patsubst %.svg,%.pdf,$(wildcard fig/*.svg))
SVG_PDFS_BW := $(patsubst %.svg,%-bw.pdf,$(wildcard fig/*.svg))

NAME := internals

all:	$(NAME).pdf 

fig: $(GRAPHS) $(SVG_PDFS) $(SVG_PDFS_BW)
.PHONY: fig

figclean:
	rm -f $(SVG_PDFS) $(SVG_PDFS_BW) $(GRAPHS)
.PHONY: figclean

$(NAME).pdf: $(TEX) $(wildcard *.bib)
	pdflatex $(NAME)
	bibtex -min-crossrefs=100 $(NAME)
	pdflatex $(NAME)
	pdflatex $(NAME)

($NAME)-compat.pdf: ($NAME).pdf
	ps2pdf13 $< $@

clean:
	rm -f $(NAME).aux $(NAME).blg $(NAME).log $(NAME).bbl $(NAME).pdf $(NAME).out $(NAME)-compat.pdf

$(FIGS_PDF): %.pdf: %.odg
	unoconv -f pdf --stdout $< > $@.tmp~
	pdfcrop $@.tmp~ $@
	rm -f $@.tmp~

fig/%.pdf: fig/%.svg
	( inkscape -z --export-pdf=$(patsubst fig/%.pdf,fig/.x.%.pdf,$@) \
	              --export-area-drawing $< \
	  && acroread -toPostScript $(patsubst fig/%.pdf,fig/.x.%.pdf,$@) \
	  && ps2pdf  $(patsubst fig/%.pdf,fig/.x.%.ps,$@) \
	             $(patsubst fig/%.pdf,fig/.x.%.pdfx,$@) \
	  && pdfcrop $(patsubst fig/%.pdf,fig/.x.%.pdfx,$@) $@ \
	  && rm $(patsubst fig/%.pdf,fig/.x.%.pdf,$@) \
	        $(patsubst fig/%.pdf,fig/.x.%.pdfx,$@) \
	        $(patsubst fig/%.pdf,fig/.x.%.ps,$@) \
	  && cp $@ .missing/$@ \
	  && echo NOTE: successfully re-built figure $@ ) \
	|| cp .missing/$@ $@

fig/%.pdf: fig/%.gp fig/%.dat fig/common.gp
	( gnuplot $< \
	  && acroread -toPostScript $@ \
	  && ps2pdf  $(patsubst fig/%.pdf,fig/%.ps,$@) \
		     $(patsubst fig/%.pdf,fig/.x.%.pdf,$@) \
	  && pdfcrop $(patsubst fig/%.pdf,fig/.x.%.pdf,$@) $@ \
	  && rm      $(patsubst fig/%.pdf,fig/%.ps,$@) \
		     $(patsubst fig/%.pdf,fig/.x.%.pdf,$@) \
	  && cp $@ .missing/$@ \
	  && echo NOTE: successfully re-built graph $@ ) \
	|| cp .missing/$@ $@

spell:
	@for i in $(TEX); do aspell --mode=tex -p ./aspell.words -c $$i; done
	@for i in $(TEX); do perl bin/double.pl $$i; done
	@for i in $(TEX); do perl bin/abbrv.pl  $$i; done
	@# bash bin/weasel.sh  $(TEX)
	@# bash bin/passive.sh $(TEX)
	@bash bin/hyphens.sh $(TEX)
	@( head -1 aspell.words ; tail -n +2 aspell.words | sort ) > aspell.words~
	@mv aspell.words~ aspell.words

%-bw.pdf: %.pdf
	( ./bin/pdftobw $< $@ && cp $@ .missing/$@ ) || cp .missing/$@ $@

