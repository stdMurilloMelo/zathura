# Manual probe. Not wired into meson on purpose: it needs poppler and a
# generated fixture, which the unit tests do not depend on.

CFLAGS += -Wall -Wextra

all: selection-semantics fixture.pdf gesture-propagation

selection-semantics: selection-semantics.c
	$(CC) $(CFLAGS) $(shell pkg-config --cflags poppler-glib) $< -o $@ $(shell pkg-config --libs poppler-glib)

gesture-propagation: gesture-propagation.c
	$(CC) $(CFLAGS) $(shell pkg-config --cflags gtk4) $< -o $@ $(shell pkg-config --libs gtk4)

fixture.pdf: fixture.tex
	pdflatex -interaction=nonstopmode fixture.tex >/dev/null
	rm -f fixture.aux fixture.log

check: selection-semantics fixture.pdf
	./selection-semantics fixture.pdf

check-gesture: gesture-propagation
	./run-gesture-probe.sh

clean:
	rm -f selection-semantics gesture-propagation fixture.pdf fixture.aux fixture.log probe.out probe.err

.PHONY: all check check-gesture clean
