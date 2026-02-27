#ifndef RI_STRUTIL_H
#define RI_STRUTIL_H

#include <stddef.h>

/* Safe string copy - always NUL-terminates, returns dst */
char *ri_strlcpy(char *dst, const char *src, size_t size);

/* Safe string concatenation - always NUL-terminates */
char *ri_strlcat(char *dst, const char *src, size_t size);

/* Format MAC address as "aa:bb:cc:dd:ee:ff" */
void ri_mac_to_str(const unsigned char *mac, char *buf, size_t buflen);

/* Parse MAC from string, returns 0 on success */
int ri_str_to_mac(const char *str, unsigned char *mac);

/* Check if a string is empty or NULL */
int ri_str_empty(const char *s);

/* Decode mDNS escape sequences (\032 → space, etc.) in-place */
void ri_str_unescape_mdns(char *s);

#endif /* RI_STRUTIL_H */
