#ifndef RI_LOG_H
#define RI_LOG_H

typedef enum {
    RI_LOG_ERROR   = 0,
    RI_LOG_WARN    = 1,
    RI_LOG_INFO    = 2,
    RI_LOG_DEBUG   = 3,
    RI_LOG_TRACE   = 4,
    RI_LOG_PACKET  = 5
} ri_log_level_t;

/* Set global verbosity level (0-5) */
void ri_log_set_level(int level);

/* Get current verbosity level */
int ri_log_get_level(void);

/* Log a message at the given level */
void ri_log(ri_log_level_t level, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

/* Convenience macros */
#define LOG_ERROR(...)  ri_log(RI_LOG_ERROR,  __VA_ARGS__)
#define LOG_WARN(...)   ri_log(RI_LOG_WARN,   __VA_ARGS__)
#define LOG_INFO(...)   ri_log(RI_LOG_INFO,   __VA_ARGS__)
#define LOG_DEBUG(...)  ri_log(RI_LOG_DEBUG,   __VA_ARGS__)
#define LOG_TRACE(...)  ri_log(RI_LOG_TRACE,   __VA_ARGS__)
#define LOG_PACKET(...) ri_log(RI_LOG_PACKET,  __VA_ARGS__)

#endif /* RI_LOG_H */
