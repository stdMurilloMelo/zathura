/* SPDX-License-Identifier: Zlib */

/*
 * Checks the assumption that zathura/selection.c is built on: that the
 * rectangle handed to page_get_text is read as a pair of flow endpoints and
 * not as a region to intersect.
 *
 * This is deliberately NOT part of the meson test suite. It needs poppler and a
 * generated fixture, neither of which the unit tests depend on. Run it by hand
 * when poppler or the plugin is upgraded, or when the slicing rule is touched.
 *
 * See tests/tools/README.md for how to build and run it.
 */

#include <poppler.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;

static char* select_text(PopplerPage* page, double x1, double y1, double x2, double y2) {
  PopplerRectangle r = {.x1 = x1, .y1 = y1, .x2 = x2, .y2 = y2};
  char* text         = poppler_page_get_selected_text(page, POPPLER_SELECTION_GLYPH, &r);
  if (text == NULL) {
    return g_strdup("");
  }
  for (char* p = text; *p != '\0'; p++) {
    if (*p == '\n') {
      *p = ' ';
    }
  }
  return text;
}

static void check(const char* what, bool ok, const char* got) {
  printf("  [%s] %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) {
    printf("         returned: %s\n", got);
    failures++;
  }
}

int main(int argc, char** argv) {
  const char* path     = (argc > 1) ? argv[1] : "fixture.pdf";
  g_autofree char* abs = g_canonicalize_filename(path, NULL);
  g_autofree char* uri = g_strdup_printf("file://%s", abs);

  GError* error        = NULL;
  PopplerDocument* doc = poppler_document_new_from_file(uri, NULL, &error);
  if (doc == NULL) {
    fprintf(stderr, "could not open %s: %s\n", path, error->message);
    fprintf(stderr, "build the fixture first: make fixture.pdf\n");
    return 2;
  }
  PopplerPage* page = poppler_document_get_page(doc, 0);

  printf("poppler %s, GLYPH style, fixture %s\n\n", poppler_get_version(), path);

  /* P1. O retangulo nao e uma regiao de intersecao. Com o canto direito em
   * x=200 na terceira linha, o texto a direita de x=200 nas linhas ACIMA tem
   * de vir junto: e isso que uma selecao de fluxo faz. */
  printf("P1. the rectangle is a pair of flow endpoints, not a region to intersect\n");
  {
    g_autofree char* t = select_text(page, 0, 0, 200, 140);
    check("text to the right on the lines above the cursor is included",
          strstr(t, "DIR1") != NULL && strstr(t, "DIR2") != NULL, t);
    check("text below the cursor is left out", strstr(t, "ESQ4") == NULL, t);
  }

  /* P2. The page that opens the range must start at the anchor's x. */
  printf("\nP2. the first page of the range starts at the anchor, not at the margin\n");
  {
    g_autofree char* with_anchor = select_text(page, 450, 140, 600, 800);
    g_autofree char* from_margin = select_text(page, 0, 140, 600, 800);
    check("with a_x=450 the selection starts past ESQ3", strstr(with_anchor, "ESQ3") == NULL, with_anchor);
    check("with x=0 it would start too early", strstr(from_margin, "ESQ3") != NULL, from_margin);
  }

  /* P3. The two points of a single-page drag must not be normalised. */
  printf("\nP3. a leftward diagonal drag loses its start if normalised\n");
  {
    g_autofree char* as_dragged = select_text(page, 450, 140, 150, 220);
    g_autofree char* normalised = select_text(page, 150, 140, 450, 220);
    check("unnormalised, it starts at DIR3", strstr(as_dragged, "ESQ3") == NULL, as_dragged);
    check("normalised, it would start at ESQ3", strstr(normalised, "ESQ3") != NULL, normalised);
  }

  g_object_unref(page);
  g_object_unref(doc);

  printf("\n%s (%d failure%s)\n", failures == 0 ? "ASSUMPTIONS HOLD" : "ASSUMPTIONS BROKEN", failures,
         failures == 1 ? "" : "s");
  return failures == 0 ? 0 : 1;
}
