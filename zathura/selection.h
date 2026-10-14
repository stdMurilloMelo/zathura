/* SPDX-License-Identifier: Zlib */

#ifndef ZATHURA_SELECTION_H
#define ZATHURA_SELECTION_H

#include <girara/datastructures.h>
#include <stdbool.h>

#include "types.h"

/**
 * A selection anchor: a page index plus a point in that page's coordinates.
 */
typedef struct zathura_selection_anchor_s {
  unsigned int page; /**< page index */
  double x;          /**< x in page coordinates */
  double y;          /**< y in page coordinates */
} zathura_selection_anchor_t;

/**
 * A selection over the document: the closed interval delimited by two anchors.
 */
typedef struct zathura_selection_s {
  zathura_selection_anchor_t origin; /**< anchor fixed on button press */
  zathura_selection_anchor_t cursor; /**< anchor following the pointer */
  bool active;                       /**< true while the gesture is in progress */
} zathura_selection_t;

/**
 * One page's share of a selection.
 */
typedef struct zathura_selection_slice_s {
  unsigned int page;             /**< page index */
  zathura_rectangle_t rectangle; /**< rectangle in page coordinates */
} zathura_selection_slice_t;

/**
 * Size of a page, in page coordinates.
 */
typedef struct zathura_page_extent_s {
  double width;
  double height;
} zathura_page_extent_t;

/**
 * Slice a selection into one rectangle per page of the interval.
 *
 * Pure function: it depends only on its arguments and allocates nothing but the
 * returned list, so it can be exercised without a GTK main loop or a document.
 *
 * The interval is [min(origin.page, cursor.page), max(...)] and the cut of each
 * page follows the page order, not the gesture order: dragging upwards yields
 * the same slices as dragging downwards between the same two points.
 *
 * The rectangles are not regions to intersect. The plugin reads (x1,y1) as the
 * point where the selection starts and (x2,y2) as where it ends, and picks up
 * every glyph between them in reading order, so a rectangle whose corners fall
 * on different lines still selects the full width of the lines in between.
 *
 * @param selection the selection to slice
 * @param extents   array of page sizes, indexed by page
 * @param n_extents number of entries in @p extents
 * @return a girara list of zathura_selection_slice_t*, empty when the two
 *         anchors coincide; NULL on invalid input. The caller owns the list.
 */
girara_list_t* zathura_selection_slice_range(const zathura_selection_t* selection, const zathura_page_extent_t* extents,
                                             unsigned int n_extents);

/**
 * Extract the text of one slice. Returns a newly allocated string, freed with
 * g_free, or NULL when the page has no text there.
 */
typedef char* (*zathura_selection_text_fn)(const zathura_selection_slice_t* slice, void* data);

/**
 * Join the text of every slice, in list order, into one string.
 *
 * Pieces are separated by a newline, as the plugin does not end a page's text
 * with one. Slices that yield no text are skipped, so a page with no text
 * layer does not leave a blank line behind.
 *
 * The text itself comes from @p get_text, which keeps this function free of
 * plugins and documents and lets it be tested with a stub.
 *
 * @param slices   list of zathura_selection_slice_t*, as returned by
 *                 zathura_selection_slice_range
 * @param get_text extracts the text of one slice
 * @param data     passed through to @p get_text
 * @return the joined text, freed with g_free; NULL when no slice yields text
 */
char* zathura_selection_collect_text(girara_list_t* slices, zathura_selection_text_fn get_text, void* data);

/**
 * Find the page a point belongs to, given the bounds of each page in one
 * common coordinate space (for instance the document widget's).
 *
 * A point inside a page picks that page. A point in the gap between pages, or
 * outside all of them, picks the nearest page; on a tie the lower index wins.
 * Either way the returned point is clamped to the page, so a drag that leaves
 * the pages still yields a valid anchor.
 *
 * Entries with x1 > x2 are skipped, which lets the caller mark pages that have
 * no widget yet or are hidden.
 *
 * @param bounds   array of page bounds, indexed by page
 * @param n_bounds number of entries in @p bounds
 * @param x        x of the point
 * @param y        y of the point
 * @param local_x  x relative to the picked page's top-left corner
 * @param local_y  y relative to the picked page's top-left corner
 * @return index of the picked page, or -1 when no entry is usable
 */
int zathura_selection_nearest_page(const zathura_rectangle_t* bounds, unsigned int n_bounds, double x, double y,
                                   double* local_x, double* local_y);

#endif
