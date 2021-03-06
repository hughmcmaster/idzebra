SUBDIRS = common

INKSCAPE=inkscape

docdir=$(datadir)/doc/$(PACKAGE)$(PACKAGE_SUFFIX)

XMLMAN = zebraidx.xml zebrasrv.xml idzebra-config.xml idzebra-abs2dom.xml

XMLFILES =  \
   administration.xml \
   architecture.xml \
   field-structure.xml \
   gpl-2.0.xml \
   idzebra-abs2dom.xml \
   indexdata.xml \
   installation.xml \
   introduction.xml \
   license.xml \
   marc_indexing.xml \
   querymodel.xml \
   recordmodel-domxml.xml \
   recordmodel-alvisxslt.xml \
   recordmodel-grs.xml \
   manref.xml \
   tutorial.xml \
   idzebra.xml \
   zebrasrv-options.xml \
   zebrasrv-synopsis.xml \
   zebrasrv-virtual.xml \
   local.ent

#examples.xml 
#quickstart.xml


HTMLFILES = index.html

PNGFILES = zebra.png domfilter.png

MANFILES=zebraidx$(PACKAGE_SUFFIX).1 \
	zebrasrv$(PACKAGE_SUFFIX).8 \
	idzebra-config$(PACKAGE_SUFFIX).1 \
	idzebra-abs2dom.1

doc_DATA = $(HTMLFILES) $(PNGFILES)
man_MANS = $(MANFILES)

$(MANFILES): local.ent

EXTRA_DIST = $(XMLFILES) $(XMLMAN) \
	$(doc_DATA) zebra.eps $(man_MANS) \
	marc_indexing.xml entities.ent domfilter.svg

zebraidx$(PACKAGE_SUFFIX).1: zebraidx.xml
	$(MAN_COMPILE) $(srcdir)/zebraidx.xml
	mv zebraidx.1 zebraidx$(PACKAGE_SUFFIX).1

zebrasrv$(PACKAGE_SUFFIX).8: zebrasrv.xml  zebrasrv-options.xml \
            zebrasrv-synopsis.xml zebrasrv-virtual.xml
	$(MAN_COMPILE) $(srcdir)/zebrasrv.xml
	mv zebrasrv.8 zebrasrv$(PACKAGE_SUFFIX).8

idzebra-config$(PACKAGE_SUFFIX).1: idzebra-config.xml
	$(MAN_COMPILE) $(srcdir)/idzebra-config.xml
	mv idzebra-config.1 idzebra-config$(PACKAGE_SUFFIX).1

idzebra-abs2dom.1: idzebra-abs2dom.xml
	$(MAN_COMPILE) $(srcdir)/idzebra-abs2dom.xml

$(HTMLFILES): $(XMLFILES) $(PNGFILES)
	rm -f *.html
	$(HTML_COMPILE) $(srcdir)/idzebra.xml

idzebra.pdf: $(XMLFILES) domfilter.pdf
	for i in $(PNGFILES); do \
		if test ! -f $$i; then cp $(srcdir)/$$i .; fi; \
	done
	$(PDF_COMPILE) $(srcdir)/idzebra.xml

manref.xml: $(XMLMAN) $(srcdir)/common/ref2dbinc.xsl local.ent
	rm -f manref.xml
	for i in $(XMLMAN); do \
		xsltproc $(srcdir)/common/stripref.xsl $(srcdir)/$$i | sed 1d >> manref.xml; \
	done

gils.txt: gils.sgml
	sgml2txt -f gils.sgml

gils.ps: gils.sgml
	sgml2latex -o ps -p a4 gils.sgml

gils.html: gils.sgml
	sgml2html gils.sgml

clean-data-hook:
	rm -f [0-9]* *.bak

dist-hook:
	if test -f index.html; then d=.; else d="$(srcdir)"; fi; \
	for p in $$d/*.html; do \
		cp $$p $(distdir); \
	done

install-data-hook:
	cd $(DESTDIR)$(man1dir) && ln -sf zebraidx$(PACKAGE_SUFFIX).1 zebraidx.1
	cd $(DESTDIR)$(man8dir) && ln -sf zebrasrv$(PACKAGE_SUFFIX).8 zebrasrv.8
	cd $(DESTDIR)$(man1dir) && ln -sf idzebra-config$(PACKAGE_SUFFIX).1 idzebra-config.1
	if test -f index.html; then d=.; else d="$(srcdir)"; fi; \
	for p in $$d/*.html; do \
		$(INSTALL_DATA) $$p $(DESTDIR)$(docdir); \
	done

uninstall-hook:
	rm -r $(DESTDIR)$(docdir)

domfilter.png: domfilter.svg
	unset DISPLAY; ${INKSCAPE} --export-png=$@ --export-width=600 $?

domfilter.eps: domfilter.svg
	unset DISPLAY; ${INKSCAPE} --export-eps=$@ $?

doc-clean:
	rm -f manref.xml *.html *.pdf *.[0-9] domfilter.png domfilter.eps

.eps.pdf:
	epstopdf -hires $?

