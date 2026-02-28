#ifndef RI_OUT_WEB_H
#define RI_OUT_WEB_H

#include "cli.h"

/* Start local HTTP server with browser-based scan UI.
   Blocks until Ctrl-C. Returns 0 on clean shutdown. */
int ri_web_serve(const ri_config_t *defaults);

#endif /* RI_OUT_WEB_H */
