/* SPDX-License-Identifier: Zlib */

/*
 * Does a GtkGestureDrag on the parent see a drag that starts on one child and
 * ends on another, when every child has the controllers page-widget.c installs?
 *
 * That is the premise alternative B rests on, and it is not verifiable by
 * reading the code: GTK4 made GdkEvent construction private ("gtk4 events
 * cannot be constructed by hand", page-widget.c:1082), so the events have to
 * come from a real display server.
 *
 * The hierarchy mirrors the real one:
 *
 *   parent  (stands for ZathuraDocumentWidget)  <- GtkGestureDrag goes here
 *     +-- child 0 (stands for ZathuraPageWidget) <- GtkGestureClick + motion
 *     +-- child 1                                 <- idem
 *     +-- child 2                                 <- idem
 *
 * It answers three questions, in the order that matters:
 *   Q1. does the parent's drag fire at all, with the children's controllers in place?
 *   Q2. does it keep firing after the pointer crosses into another child?
 *   Q3. do the offsets arrive in the parent's coordinate space?
 *
 * See tests/tools/README.md for how to run it.
 */

#include <glib-unix.h>
#include <gtk/gtk.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>

#define N_CHILDREN 3
#define CHILD_H 120
#define WIN_W 400

typedef struct {
  /* parent gesture */
  bool drag_begin;
  bool drag_update;
  bool drag_end;
  int updates;
  double begin_x, begin_y;
  double last_off_x, last_off_y;
  /* children */
  int pressed_on;  /* index of the child that got the press, -1 if none */
  int released_on; /* index of the child that got the release */
  int leave_count; /* how many times a child saw the pointer leave */
  bool motion_after_leave;
  int last_motion_child;
  /* bookkeeping */
  GtkWidget* window;
  guint timeout_id;
} probe_t;

static probe_t probe = {.pressed_on = -1, .released_on = -1, .last_motion_child = -1};

/* ---------------------------------------------------------------- parent */

static void on_drag_begin(GtkGestureDrag* g, double x, double y, gpointer d) {
  (void)g;
  (void)d;
  probe.drag_begin = true;
  probe.begin_x    = x;
  probe.begin_y    = y;
  g_print("  parent: drag-begin at (%.0f, %.0f)\n", x, y);
}

static void on_drag_update(GtkGestureDrag* g, double ox, double oy, gpointer d) {
  (void)g;
  (void)d;
  probe.drag_update = true;
  probe.updates++;
  probe.last_off_x = ox;
  probe.last_off_y = oy;
}

static void on_drag_end(GtkGestureDrag* g, double ox, double oy, gpointer d) {
  (void)g;
  (void)d;
  probe.drag_end   = true;
  probe.last_off_x = ox;
  probe.last_off_y = oy;
  g_print("  parent: drag-end, offset (%.0f, %.0f), %d update(s)\n", ox, oy, probe.updates);
}

/* ---------------------------------------------------------------- children */

static void on_child_pressed(GtkGestureClick* g, gint n, double x, double y, gpointer data) {
  (void)g;
  (void)n;
  (void)x;
  (void)y;
  const int idx    = GPOINTER_TO_INT(data);
  probe.pressed_on = idx;
  g_print("  child %d: pressed\n", idx);
}

static void on_child_released(GtkGestureClick* g, gint n, double x, double y, gpointer data) {
  (void)g;
  (void)n;
  (void)x;
  (void)y;
  const int idx     = GPOINTER_TO_INT(data);
  probe.released_on = idx;
  g_print("  child %d: released\n", idx);
}

static void on_child_motion(GtkEventControllerMotion* c, double x, double y, gpointer data) {
  (void)c;
  (void)x;
  (void)y;
  const int idx = GPOINTER_TO_INT(data);
  if (probe.leave_count > 0 && idx != probe.pressed_on) {
    probe.motion_after_leave = true;
  }
  probe.last_motion_child = idx;
}

static void on_child_leave(GtkEventControllerMotion* c, gpointer data) {
  (void)c;
  const int idx = GPOINTER_TO_INT(data);
  probe.leave_count++;
  g_print("  child %d: leave\n", idx);
}

/* ---------------------------------------------------------------- report */

static void report_and_quit(void) {
  int failures = 0;

  g_print("\n--- result ---\n");

  const bool q1 = probe.drag_begin && probe.drag_update;
  g_print("Q1 parent gesture fires with the children instrumented ... %s\n", q1 ? "YES" : "NO");
  if (!q1) {
    failures++;
  }

  const bool crossed = probe.leave_count > 0;
  const bool q2      = crossed && probe.drag_end && probe.updates > 1;
  g_print("Q2 keeps firing while crossing between children ...... %s\n",
          q2 ? "YES" : (crossed ? "NO" : "INCONCLUSIVE (pointer did not cross)"));
  if (!q2) {
    failures++;
  }

  const bool q3 = probe.drag_end && (probe.last_off_y > CHILD_H / 2.0);
  g_print("Q3 offsets arrive in the parent coordinate space ..... %s\n", q3 ? "YES" : "NO");
  if (!q3) {
    failures++;
  }

  g_print("\ndetails:\n");
  g_print("  press on child %d, release on child %d\n", probe.pressed_on, probe.released_on);
  g_print("  leave events on children: %d\n", probe.leave_count);
  g_print("  motion on a child other than the first after leave: %s\n", probe.motion_after_leave ? "yes" : "no");
  g_print("  begin (%.0f, %.0f), final offset (%.0f, %.0f)\n", probe.begin_x, probe.begin_y, probe.last_off_x,
          probe.last_off_y);

  if (probe.released_on != -1 && probe.released_on != probe.pressed_on) {
    g_print("\n  note: the release landed on a different child than the press,\n"
            "        which contradicts the implicit grab.\n");
  } else if (probe.pressed_on != -1) {
    g_print("\n  note: press and release on the same child, consistent with the\n"
            "        implicit grab GTK4 takes on button press.\n");
  }

  g_print("\n%s\n", failures == 0 ? "ASSUMPTION HOLDS: the parent gesture sees the whole drag"
                                  : "ASSUMPTION DOES NOT HOLD: see above");

  exit(failures == 0 ? 0 : 1);
}

static gboolean on_timeout(gpointer d) {
  (void)d;
  g_print("\n(timed out with no drag; run it through the driver, see the README)\n");
  report_and_quit();
  return G_SOURCE_REMOVE;
}

/* The driver sends SIGUSR1 when the drag is done. stdin is not usable here:
 * running detached closes it, and the HUP would report before any event. */
static gboolean on_report_signal(gpointer d) {
  (void)d;
  report_and_quit();
  return G_SOURCE_REMOVE;
}

/* ---------------------------------------------------------------- setup */

static void on_activate(GtkApplication* app, gpointer d) {
  (void)d;
  GtkWidget* win = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(win), "gesture-propagation");
  gtk_window_set_default_size(GTK_WINDOW(win), WIN_W, CHILD_H * N_CHILDREN);
  probe.window = win;

  /* The real hierarchy has a grid between the document widget and the pages
   * (document-widget.c:119 parents the grid; :497 attaches each page to it),
   * so the gesture sits two levels above the children, not one. PROBE_FLAT=1
   * collapses it to a single level for comparison. */
  const bool flat = g_strcmp0(g_getenv("PROBE_FLAT"), "1") == 0;

  GtkWidget* outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_window_set_child(GTK_WINDOW(win), outer);

  GtkWidget* box = outer;
  if (!flat) {
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(box, TRUE);
    gtk_widget_set_vexpand(box, TRUE);
    gtk_box_append(GTK_BOX(outer), box);
  }
  g_print("(hierarchy: %s)\n", flat ? "parent -> children" : "parent -> inner -> children");

  for (int i = 0; i < N_CHILDREN; i++) {
    GtkWidget* child = gtk_drawing_area_new();
    gtk_widget_set_size_request(child, WIN_W, CHILD_H);
    gtk_widget_set_hexpand(child, TRUE);
    gtk_box_append(GTK_BOX(box), child);

    /* same controllers page-widget.c installs on every page */
    GtkGesture* click = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), 0);
    g_signal_connect(click, "pressed", G_CALLBACK(on_child_pressed), GINT_TO_POINTER(i));
    g_signal_connect(click, "released", G_CALLBACK(on_child_released), GINT_TO_POINTER(i));
    gtk_widget_add_controller(child, GTK_EVENT_CONTROLLER(click));

    GtkEventController* motion = gtk_event_controller_motion_new();
    g_signal_connect(motion, "motion", G_CALLBACK(on_child_motion), GINT_TO_POINTER(i));
    g_signal_connect(motion, "leave", G_CALLBACK(on_child_leave), GINT_TO_POINTER(i));
    gtk_widget_add_controller(child, motion);
  }

  /* the gesture alternative B would install on ZathuraDocumentWidget */
  GtkGesture* drag = gtk_gesture_drag_new();
  gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(drag), GDK_BUTTON_PRIMARY);
  g_signal_connect(drag, "drag-begin", G_CALLBACK(on_drag_begin), NULL);
  g_signal_connect(drag, "drag-update", G_CALLBACK(on_drag_update), NULL);
  g_signal_connect(drag, "drag-end", G_CALLBACK(on_drag_end), NULL);
  gtk_widget_add_controller(outer, GTK_EVENT_CONTROLLER(drag));

  const char* phase = g_getenv("PROBE_PHASE");
  if (phase != NULL && g_strcmp0(phase, "capture") == 0) {
    gtk_event_controller_set_propagation_phase(GTK_EVENT_CONTROLLER(drag), GTK_PHASE_CAPTURE);
    g_print("(parent gesture phase: CAPTURE)\n");
  } else {
    g_print("(parent gesture phase: BUBBLE, the default)\n");
  }

  gtk_window_present(GTK_WINDOW(win));

  g_unix_signal_add(SIGUSR1, on_report_signal, NULL);
  probe.timeout_id = g_timeout_add_seconds(30, on_timeout, NULL);
}

int main(int argc, char** argv) {
  GtkApplication* app = gtk_application_new("org.pwmt.zathura.probe", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
  return g_application_run(G_APPLICATION(app), argc, argv);
}
