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

/* URL'den şema sonrası, ilk '/'/@/: karakterine kadar olan host kısmını çıkarır.
   "https://www.x.com/user/status/1" -> "www.x.com" gibi. */
static void extract_host(const wchar_t *url, wchar_t *out_host, size_t max_len)
{
    const wchar_t *p = url;
    size_t n = 0;

    if (wcsncmp(p, L"https://", 8) == 0) p += 8;
    else if (wcsncmp(p, L"http://", 7) == 0) p += 7;

    /* kullanıcı:şifre@ biçimini atla */
    const wchar_t *at = wcschr(p, L'@');
    const wchar_t *slash = wcschr(p, L'/');
    if (at && (!slash || at < slash)) p = at + 1;

    while (*p && *p != L'/' && *p != L':' && *p != L'?' && *p != L'#' && n + 1 < max_len) {
        out_host[n++] = (wchar_t)towlower(*p++);
    }
    out_host[n] = L'\0';
}

/* host, verilen alan adıyla birebir mi ya da alt alan adı mı?
   "www.x.com" -> "x.com" eşleşir; "dropbox.com" -> "x.com" EŞLEŞMEZ. */
static int host_matches(const wchar_t *host, const wchar_t *domain)
{
    size_t hlen = wcslen(host);
    size_t dlen = wcslen(domain);
    if (hlen < dlen) return 0;

    const wchar_t *suffix = host + (hlen - dlen);
    if (_wcsicmp(suffix, domain) != 0) return 0;

    /* ya birebir aynı ya da nokta ile ayrılmış alt alan adı olmalı */
    return (hlen == dlen) || (suffix[-1] == L'.');
}

PlatformInfo detect_platform(const wchar_t *url)
{
    if (!is_valid_url(url)) {
        return PLATFORMS[PLATFORM_GENERIC];
    }

    wchar_t host[512];
    extract_host(url, host, 512);
    if (host[0] == L'\0') {
        return PLATFORMS[PLATFORM_GENERIC];
    }

    if (host_matches(host, L"youtube.com") || host_matches(host, L"youtu.be")) {
        return PLATFORMS[PLATFORM_YOUTUBE];
    }
    if (host_matches(host, L"tiktok.com")) {
        return PLATFORMS[PLATFORM_TIKTOK];
    }
    if (host_matches(host, L"instagram.com")) {
        return PLATFORMS[PLATFORM_INSTAGRAM];
    }
    if (host_matches(host, L"x.com") || host_matches(host, L"twitter.com")) {
        return PLATFORMS[PLATFORM_TWITTER];
    }

    return PLATFORMS[PLATFORM_GENERIC];
}
