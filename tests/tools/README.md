# tests/tools

Manual verification tools. **They are not part of the meson test suite**, on
purpose: they need poppler, a generated PDF and an X server, and the unit tests
depend on none of those.

They exist because `tests/test_selection.c` checks the arithmetic of
`zathura_selection_slice_range` but not the two assumptions that arithmetic
rests on. Both assumptions are about components outside zathura, so a change in
either would sail past the unit tests and break selection silently.

## selection-semantics

Checks what the plugin does with the rectangle handed to `page_get_text`.

The rectangle is **not a region to intersect**. The plugin reads `(x1,y1)` as
the point where the selection starts and `(x2,y2)` as where it ends, then picks
up every glyph between them in reading order.

    make check

Expected output, exit status 0:

    ASSUMPTIONS HOLD (0 failures)

### What it checks

- **P1.** A rectangle whose right corner falls mid-line also brings in the text
  to the right on the lines above. Strict intersection would drop it.
- **P2.** The page that opens the range must start at `a_x`. Starting at the
  margin drags in the text to the left of the anchor on its own line.
- **P3.** The two points of a single-page drag must not be normalised into a
  box: a leftward diagonal drag would lose its starting point.

P2 and P3 are the two rules in `zathura/selection.c`, guarded in the unit tests
by T02b and T01b.

### Selection style

The tool uses `POPPLER_SELECTION_GLYPH`, which is what `zathura-pdf-poppler`
passes. To re-check against a different plugin:

    objdump -d /usr/lib/zathura/libpdf-poppler.so | grep -B8 poppler_page_get_selected_text

The second argument goes in `esi`: `xor %esi,%esi` is `GLYPH` (0),
`mov $0x1,%esi` is `WORD`, `mov $0x2,%esi` is `LINE`.

### fixture.tex

Builds a two page PDF with text at known coordinates: lines every 40bp starting
at y=50, left block at x 50..207, right block at x 402..550. `make` builds it;
the PDF is not versioned.

## gesture-propagation

Answers the question the document-level gesture rests on: **does a
`GtkGestureDrag` on a parent widget see a drag that starts on one child and
ends on another, with every child carrying the controllers `page-widget.c`
installs?**

Reading the code cannot answer it. GTK4 made `GdkEvent` construction private,
which zathura itself notes at `page-widget.c:1082`, so the events have to come
from a real X server.

The program mirrors the real hierarchy: an outer container standing for
`ZathuraDocumentWidget`, an inner one standing for the grid
(`document-widget.c:119`), and three children standing for the page widgets
(`document-widget.c:497`), each with `GtkGestureClick` and
`GtkEventControllerMotion`.

    make check-gesture

### Measured result (GTK 4.22.4)

    Q1 parent gesture fires with the children instrumented ... YES
    Q2 keeps firing while crossing between children ...... YES
    Q3 offsets arrive in the parent coordinate space ..... YES

    press on child 0, release on child 0
    leave events on children: 1
    begin (200, 60), final offset (30, 260)

Three things follow:

1. The parent gesture gets `drag-begin`, ten `drag-update` and `drag-end` with
   the pointer crossing two children. Moving the gesture up one level works.
2. Press and release land on the *same* child even though the cursor ends over
   another. That is GTK4's implicit grab, and it is why selection today cannot
   leave the page where the drag started.
3. The children keep getting `leave` during the drag, and that does not
   interrupt the parent's gesture. Both levels see the same gesture, so the
   per-page selection state has to go when the gesture moves up, or the two
   would fight over what to draw.

`PROBE_PHASE=capture` runs the same checks with the gesture in
`GTK_PHASE_CAPTURE`; all three still pass, with the parent's `drag-begin`
arriving before the child's `pressed` instead of after. `PROBE_FLAT=1` drops
the intermediate container, to confirm the extra level makes no difference.

### Running it

Preferably under Xvfb, so the drag does not fight the real pointer:

    sudo pacman -S xorg-server-xvfb     # Arch
    sudo apt install xvfb               # Debian/Ubuntu

Without Xvfb it runs on the current session and moves the pointer for ~2s:

    PROBE_USE_REAL_DISPLAY=1 ./run-gesture-probe.sh

On Wayland the driver forces `GDK_BACKEND=x11`, because xdotool injects through
XTEST, which only exists on X. The window needs focus to receive the clicks,
hence the `windowactivate --sync` in the driver.
