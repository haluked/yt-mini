#include "detector.h"
#include <wchar.h>
#include <wctype.h>
#include <string.h>

static const PlatformInfo PLATFORMS[] = {
    { PLATFORM_GENERIC,   L"generic",   L"Web Medya",   L"WEB",     0 },
    { PLATFORM_YOUTUBE,   L"youtube",   L"YouTube",     L"YOUTUBE", 1 },
    { PLATFORM_TIKTOK,    L"tiktok",    L"TikTok",      L"TIKTOK",  1 },
    { PLATFORM_INSTAGRAM, L"instagram", L"Instagram",   L"INSTA",   1 },
    { PLATFORM_TWITTER,   L"twitter",   L"X (Twitter)", L"X",       1 },
};

int is_valid_url(const wchar_t *url)
{
    if (!url) return 0;
    while (*url && iswspace(*url)) url++;
    if (*url == L'\0') return 0;

    size_t len = wcslen(url);
    if (len == 0 || len >= 1024) return 0;

    /* Komut satırına "%ls" olarak verildiği için tırnak, ters bölü ve
       kontrol karakterleri enjeksiyona yol açabilir. */
    for (const wchar_t *p = url; *p; p++) {
        if (*p == L'"' || *p == L'\\' || iswcntrl(*p)) return 0;
    }

    /* Scheme check: must start with http:// or https:// */
    if (wcsncmp(url, L"http://", 7) == 0) {
        return url[7] != L'\0';
    }
    if (wcsncmp(url, L"https://", 8) == 0) {
        return url[8] != L'\0';
    }
    return 0;
}

static int wcs_contains_case_insensitive(const wchar_t *haystack, const wchar_t *needle)
{
    size_t nlen, hlen, i, j;
    if (!haystack || !needle) return 0;
    nlen = wcslen(needle);
    hlen = wcslen(haystack);
    if (nlen > hlen) return 0;

    for (i = 0; i <= hlen - nlen; i++) {
        int match = 1;
        for (j = 0; j < nlen; j++) {
            if (towlower(haystack[i + j]) != towlower(needle[j])) {
                match = 0;
                break;
            }
        }
        if (match) return 1;
    }
    return 0;
}

PlatformInfo detect_platform(const wchar_t *url)
{
    if (!is_valid_url(url)) {
        return PLATFORMS[PLATFORM_GENERIC];
    }

    /* YouTube check */
    if (wcs_contains_case_insensitive(url, L"youtube.com") ||
        wcs_contains_case_insensitive(url, L"youtu.be")) {
        return PLATFORMS[PLATFORM_YOUTUBE];
    }

    /* TikTok check */
    if (wcs_contains_case_insensitive(url, L"tiktok.com")) {
        return PLATFORMS[PLATFORM_TIKTOK];
    }

    /* Instagram check */
    if (wcs_contains_case_insensitive(url, L"instagram.com")) {
        return PLATFORMS[PLATFORM_INSTAGRAM];
    }

    /* Twitter / X check */
    if (wcs_contains_case_insensitive(url, L"twitter.com") ||
        wcs_contains_case_insensitive(url, L"x.com")) {
        return PLATFORMS[PLATFORM_TWITTER];
    }

    return PLATFORMS[PLATFORM_GENERIC];
}
