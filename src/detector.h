#ifndef DETECTOR_H
#define DETECTOR_H

#include <wchar.h>

typedef enum {
    PLATFORM_GENERIC = 0,
    PLATFORM_YOUTUBE,
    PLATFORM_TIKTOK,
    PLATFORM_INSTAGRAM,
    PLATFORM_TWITTER
} PlatformType;

typedef struct {
    PlatformType type;
    const wchar_t *key;
    const wchar_t *display_name;
    const wchar_t *badge_label;
    int is_specialized;
} PlatformInfo;

int is_valid_url(const wchar_t *url);
PlatformInfo detect_platform(const wchar_t *url);

#endif /* DETECTOR_H */
