/* SPDX-License-Identifier: Zlib */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "selection.h"

static zathura_selection_slice_t* slice_new(unsigned int page, double x1, double y1, double x2, double y2) {
  zathura_selection_slice_t* slice = g_try_malloc0(sizeof(zathura_selection_slice_t));
  if (slice == NULL) {
    return NULL;
  }

  slice->page         = page;
  slice->rectangle.x1 = x1;
  slice->rectangle.y1 = y1;
  slice->rectangle.x2 = x2;
  slice->rectangle.y2 = y2;
  return slice;
}

girara_list_t* zathura_selection_slice_range(const zathura_selection_t* selection, const zathura_page_extent_t* extents,
                                             unsigned int n_extents) {
  if (selection == NULL || extents == NULL || n_extents == 0) {
    return NULL;
  }
  if (selection->origin.page >= n_extents || selection->cursor.page >= n_extents) {
    return NULL;
  }

  girara_list_t* list = girara_list_new_with_free(g_free);
  if (list == NULL) {
    return NULL;
  }

  /* The cut follows the page order, so the anchor on the lower-indexed page
   * opens the interval and the other one closes it, whatever the drag
   * direction was. */
  const zathura_selection_anchor_t* first = &selection->origin;
  const zathura_selection_anchor_t* last  = &selection->cursor;
  if (last->page < first->page) {
    first = &selection->cursor;
    last  = &selection->origin;
  }

  if (first->page == last->page) {
    /* Single page. The rectangle is not a region to intersect: the plugin reads
     * (x1,y1) as the start of the selection and (x2,y2) as its end, in reading
     * order, so the two points are passed through as they came. Normalising
     * them into a box would lose the direction of a diagonal drag. */
    if (first->x == last->x && first->y == last->y) {
      /* a click, not a drag: nothing is selected */
      return list;
    }

    zathura_selection_slice_t* slice = slice_new(first->page, first->x, first->y, last->x, last->y);
    if (slice != NULL) {
      girara_list_append(list, slice);
    }
    return list;
  }

  for (unsigned int page = first->page; page <= last->page; page++) {
    const double w = extents[page].width;
    const double h = extents[page].height;

    zathura_selection_slice_t* slice = NULL;
    if (page == first->page) {
      /* from the anchor itself down to the bottom of the page; starting at x=0
       * would pull in the text to the left of the anchor on its own line */
      slice = slice_new(page, first->x, first->y, w, h);
    } else if (page == last->page) {
      /* from the top of the page down to the anchor */
      slice = slice_new(page, 0.0, 0.0, last->x, last->y);
    } else {
      /* the whole page */
      slice = slice_new(page, 0.0, 0.0, w, h);
    }

    if (slice != NULL) {
      girara_list_append(list, slice);
    }
  }

  return list;
}

char* zathura_selection_collect_text(girara_list_t* slices, zathura_selection_text_fn get_text, void* data) {
  if (slices == NULL || get_text == NULL) {
    return NULL;
  }

  GString* text = g_string_new(NULL);
  for (size_t idx = 0; idx != girara_list_size(slices); ++idx) {
    g_autofree char* piece = get_text(girara_list_nth(slices, idx), data);
    if (piece == NULL || *piece == '\0') {
      continue;
    }

    if (text->len > 0 && text->str[text->len - 1] != '\n') {
      g_string_append_c(text, '\n');
    }
    g_string_append(text, piece);
  }

  if (text->len == 0) {
    g_string_free(text, TRUE);
    return NULL;
  }
  return g_string_free(text, FALSE);
}

int zathura_selection_nearest_page(const zathura_rectangle_t* bounds, unsigned int n_bounds, double x, double y,
                                   double* local_x, double* local_y) {
  if (bounds == NULL || local_x == NULL || local_y == NULL) {
    return -1;
  }

  int best          = -1;
  double best_dist2 = 0.0;
  for (unsigned int i = 0; i < n_bounds; i++) {
    const zathura_rectangle_t* r = &bounds[i];
    if (r->x1 > r->x2) {
      continue;
    }

    const double dx    = fmax(fmax(r->x1 - x, x - r->x2), 0.0);
    const double dy    = fmax(fmax(r->y1 - y, y - r->y2), 0.0);
    const double dist2 = dx * dx + dy * dy;
    if (best == -1 || dist2 < best_dist2) {
      best       = (int)i;
      best_dist2 = dist2;
    }
  }

  if (best != -1) {
    const zathura_rectangle_t* r = &bounds[best];
    *local_x                     = fmin(fmax(x, r->x1), r->x2) - r->x1;
    *local_y                     = fmin(fmax(y, r->y1), r->y2) - r->y1;
  }
  return best;
}
