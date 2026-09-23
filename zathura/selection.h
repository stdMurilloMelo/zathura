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

#endif
