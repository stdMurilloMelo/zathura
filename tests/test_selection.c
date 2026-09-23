/* SPDX-License-Identifier: Zlib */

#include "selection.h"

/*
 * The rectangles produced here are not regions to intersect: the plugin reads
 * (x1,y1) as the point where the selection starts and (x2,y2) as where it ends,
 * and collects every glyph between them in reading order. Verified against
 * poppler 26.08 with POPPLER_SELECTION_GLYPH, the style zathura-pdf-poppler
 * passes to poppler_page_get_selected_text.
 */

/* four pages; the second one is taller, to catch per-page extents */
static const zathura_page_extent_t extents[] = {
    {600.0, 800.0},
    {600.0, 1000.0},
    {600.0, 800.0},
    {600.0, 800.0},
};
static const unsigned int n_extents = sizeof(extents) / sizeof(extents[0]);

static zathura_selection_t make(unsigned int p_i, double x_i, double y_i, unsigned int p_f, double x_f, double y_f) {
  zathura_selection_t sel = {
      .origin = {.page = p_i, .x = x_i, .y = y_i},
      .cursor = {.page = p_f, .x = x_f, .y = y_f},
      .active = true,
  };
  return sel;
}

static const zathura_selection_slice_t* nth(girara_list_t* list, size_t i) {
  return girara_list_nth(list, i);
}

static void assert_rect(const zathura_selection_slice_t* s, unsigned int page, double x1, double y1, double x2,
                        double y2) {
  g_assert_nonnull(s);
  g_assert_cmpuint(s->page, ==, page);
  g_assert_cmpfloat(s->rectangle.x1, ==, x1);
  g_assert_cmpfloat(s->rectangle.y1, ==, y1);
  g_assert_cmpfloat(s->rectangle.x2, ==, x2);
  g_assert_cmpfloat(s->rectangle.y2, ==, y2);
}

/* T01: single page, dragged down and to the right */
static void test_single_page(void) {
  zathura_selection_t sel = make(0, 100.0, 200.0, 0, 300.0, 400.0);
  girara_list_t* list     = zathura_selection_slice_range(&sel, extents, n_extents);

  g_assert_nonnull(list);
  g_assert_cmpuint(girara_list_size(list), ==, 1);
  assert_rect(nth(list, 0), 0, 100.0, 200.0, 300.0, 400.0);
  girara_list_free(list);
}

/* T01b: single page, dragged down and to the LEFT. The endpoints must survive:
 * normalising them into a box would start the selection at the wrong x. */
static void test_single_page_diagonal(void) {
  zathura_selection_t sel = make(0, 450.0, 140.0, 0, 150.0, 220.0);
  girara_list_t* list     = zathura_selection_slice_range(&sel, extents, n_extents);

  g_assert_cmpuint(girara_list_size(list), ==, 1);
  assert_rect(nth(list, 0), 0, 450.0, 140.0, 150.0, 220.0);
  girara_list_free(list);
}

/* T01c: single page, dragged upwards. Start and end follow the gesture. */
static void test_single_page_upwards(void) {
  zathura_selection_t sel = make(0, 300.0, 400.0, 0, 100.0, 200.0);
  girara_list_t* list     = zathura_selection_slice_range(&sel, extents, n_extents);

  g_assert_cmpuint(girara_list_size(list), ==, 1);
  assert_rect(nth(list, 0), 0, 300.0, 400.0, 100.0, 200.0);
  girara_list_free(list);
}

/* T02: two adjacent pages */
static void test_two_pages(void) {
  zathura_selection_t sel = make(0, 100.0, 200.0, 1, 300.0, 400.0);
  girara_list_t* list     = zathura_selection_slice_range(&sel, extents, n_extents);

  g_assert_nonnull(list);
  g_assert_cmpuint(girara_list_size(list), ==, 2);
  /* page 0: from the anchor point to the bottom right corner */
  assert_rect(nth(list, 0), 0, 100.0, 200.0, 600.0, 800.0);
  /* page 1: from the top left corner to the cursor */
  assert_rect(nth(list, 1), 1, 0.0, 0.0, 300.0, 400.0);
  girara_list_free(list);
}

/* T02b: the anchor's x must reach the rectangle. Starting the first page at
 * x=0 would pull in the text to the left of the anchor on its own line. */
static void test_first_page_keeps_anchor_x(void) {
  zathura_selection_t sel = make(0, 450.0, 140.0, 1, 100.0, 100.0);
  girara_list_t* list     = zathura_selection_slice_range(&sel, extents, n_extents);

  g_assert_cmpuint(girara_list_size(list), ==, 2);
  g_assert_cmpfloat(nth(list, 0)->rectangle.x1, ==, 450.0);
  girara_list_free(list);
}

/* T03: intermediate pages are taken whole */
static void test_intermediate_pages(void) {
  zathura_selection_t sel = make(0, 100.0, 200.0, 2, 300.0, 400.0);
  girara_list_t* list     = zathura_selection_slice_range(&sel, extents, n_extents);

  g_assert_nonnull(list);
  g_assert_cmpuint(girara_list_size(list), ==, 3);
  assert_rect(nth(list, 0), 0, 100.0, 200.0, 600.0, 800.0);
  assert_rect(nth(list, 1), 1, 0.0, 0.0, 600.0, 1000.0); /* page 1 is taller */
  assert_rect(nth(list, 2), 2, 0.0, 0.0, 300.0, 400.0);
  girara_list_free(list);
}

/* T03b: several intermediate pages, each with its own extent */
static void test_many_intermediate_pages(void) {
  zathura_selection_t sel = make(0, 10.0, 20.0, 3, 30.0, 40.0);
  girara_list_t* list     = zathura_selection_slice_range(&sel, extents, n_extents);

  g_assert_cmpuint(girara_list_size(list), ==, 4);
  assert_rect(nth(list, 1), 1, 0.0, 0.0, 600.0, 1000.0);
  assert_rect(nth(list, 2), 2, 0.0, 0.0, 600.0, 800.0);
  girara_list_free(list);
}

/* T04: a reverse drag yields the same slices as the forward one */
static void test_reverse_drag(void) {
  zathura_selection_t forward = make(0, 100.0, 200.0, 1, 300.0, 400.0);
  zathura_selection_t reverse = make(1, 300.0, 400.0, 0, 100.0, 200.0);

  girara_list_t* a = zathura_selection_slice_range(&forward, extents, n_extents);
  girara_list_t* b = zathura_selection_slice_range(&reverse, extents, n_extents);

  g_assert_cmpuint(girara_list_size(a), ==, girara_list_size(b));
  for (size_t i = 0; i < girara_list_size(a); i++) {
    assert_rect(nth(b, i), nth(a, i)->page, nth(a, i)->rectangle.x1, nth(a, i)->rectangle.y1, nth(a, i)->rectangle.x2,
                nth(a, i)->rectangle.y2);
  }
  girara_list_free(a);
  girara_list_free(b);
}

/* T05: the first page uses its own height, not the cursor page's */
static void test_per_page_extents(void) {
  zathura_selection_t sel = make(1, 10.0, 20.0, 2, 30.0, 40.0);
  girara_list_t* list     = zathura_selection_slice_range(&sel, extents, n_extents);

  g_assert_cmpuint(girara_list_size(list), ==, 2);
  assert_rect(nth(list, 0), 1, 10.0, 20.0, 600.0, 1000.0);
  assert_rect(nth(list, 1), 2, 0.0, 0.0, 30.0, 40.0);
  girara_list_free(list);
}

/* T06: coinciding anchors are a click, and select nothing */
static void test_click_selects_nothing(void) {
  zathura_selection_t sel = make(0, 100.0, 200.0, 0, 100.0, 200.0);
  girara_list_t* list     = zathura_selection_slice_range(&sel, extents, n_extents);

  g_assert_nonnull(list);
  g_assert_cmpuint(girara_list_size(list), ==, 0);
  girara_list_free(list);
}

/* T06b: same point but on different pages is still a selection */
static void test_same_point_across_pages(void) {
  zathura_selection_t sel = make(0, 100.0, 200.0, 1, 100.0, 200.0);
  girara_list_t* list     = zathura_selection_slice_range(&sel, extents, n_extents);

  g_assert_cmpuint(girara_list_size(list), ==, 2);
  girara_list_free(list);
}

/* T07: the list always comes out in increasing page order */
static void test_index_order(void) {
  zathura_selection_t sel = make(3, 10.0, 20.0, 0, 30.0, 40.0);
  girara_list_t* list     = zathura_selection_slice_range(&sel, extents, n_extents);

  g_assert_cmpuint(girara_list_size(list), ==, 4);
  for (size_t i = 0; i < girara_list_size(list); i++) {
    g_assert_cmpuint(nth(list, i)->page, ==, i);
  }
  girara_list_free(list);
}

/* T08: invalid input is rejected instead of crashing */
static void test_invalid_input(void) {
  zathura_selection_t sel = make(0, 0.0, 0.0, 9, 0.0, 0.0);
  g_assert_null(zathura_selection_slice_range(&sel, extents, n_extents));
  g_assert_null(zathura_selection_slice_range(NULL, extents, n_extents));
  g_assert_null(zathura_selection_slice_range(&sel, NULL, n_extents));

  zathura_selection_t ok = make(0, 0.0, 0.0, 0, 1.0, 1.0);
  g_assert_null(zathura_selection_slice_range(&ok, extents, 0));
}

/* T09: the whole document, first page to last */
static void test_whole_document(void) {
  zathura_selection_t sel = make(0, 0.0, 0.0, 3, 600.0, 800.0);
  girara_list_t* list     = zathura_selection_slice_range(&sel, extents, n_extents);

  g_assert_cmpuint(girara_list_size(list), ==, 4);
  assert_rect(nth(list, 0), 0, 0.0, 0.0, 600.0, 800.0);
  assert_rect(nth(list, 3), 3, 0.0, 0.0, 600.0, 800.0);
  girara_list_free(list);
}

int main(int argc, char* argv[]) {
  g_test_init(&argc, &argv, NULL);
  g_test_add_func("/selection/single_page", test_single_page);
  g_test_add_func("/selection/single_page_diagonal", test_single_page_diagonal);
  g_test_add_func("/selection/single_page_upwards", test_single_page_upwards);
  g_test_add_func("/selection/two_pages", test_two_pages);
  g_test_add_func("/selection/first_page_keeps_anchor_x", test_first_page_keeps_anchor_x);
  g_test_add_func("/selection/intermediate_pages", test_intermediate_pages);
  g_test_add_func("/selection/many_intermediate_pages", test_many_intermediate_pages);
  g_test_add_func("/selection/reverse_drag", test_reverse_drag);
  g_test_add_func("/selection/per_page_extents", test_per_page_extents);
  g_test_add_func("/selection/click_selects_nothing", test_click_selects_nothing);
  g_test_add_func("/selection/same_point_across_pages", test_same_point_across_pages);
  g_test_add_func("/selection/index_order", test_index_order);
  g_test_add_func("/selection/invalid_input", test_invalid_input);
  g_test_add_func("/selection/whole_document", test_whole_document);
  return g_test_run();
}
