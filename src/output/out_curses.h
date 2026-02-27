#ifndef RI_OUT_CURSES_H
#define RI_OUT_CURSES_H

#include "core/graph.h"

/* Display graph as color-coded BFS tree in terminal using ncurses.
   Returns 0 on success. */
int ri_out_curses(const ri_graph_t *g);

#endif /* RI_OUT_CURSES_H */
