#include "sanitization.h"
#include <wchar.h>
#include <wctype.h>
#include <string.h>
#include <windows.h>

static int is_forbidden_char(wchar_t c)
{
    if (c >= 0 && c <= 31) return 1;
    switch (c) {
        case L'<':
        case L'>':
        case L':':
        case L'\"':
        case L'/':
        case L'\\':
        case L'|':
        case L'?':
        case L'*':
            return 1;
        default:
            return 0;
    }
}

static int is_reserved_stem(const wchar_t *stem)
{
    static const wchar_t *reserved[] = {
        L"CON", L"PRN", L"AUX", L"NUL",
        L"COM1", L"COM2", L"COM3", L"COM4", L"COM5", L"COM6", L"COM7", L"COM8", L"COM9",
        L"LPT1", L"LPT2", L"LPT3", L"LPT4", L"LPT5", L"LPT6", L"LPT7", L"LPT8", L"LPT9",
        NULL
    };

    for (int i = 0; reserved[i] != NULL; i++) {
        if (_wcsicmp(stem, reserved[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

void sanitize_filename(
    const wchar_t *input,
    wchar_t *output,
    size_t max_out_len,
    const wchar_t *default_name
)
{
    if (!output || max_out_len == 0) return;
    if (!default_name) default_name = L"indirilen_medya";

    if (!input || *input == L'\0') {
        wcsncpy(output, default_name, max_out_len - 1);
        output[max_out_len - 1] = L'\0';
        return;
    }

    wchar_t temp[512];
    size_t in_len = wcslen(input);
    if (in_len >= sizeof(temp) / sizeof(temp[0])) {
        in_len = (sizeof(temp) / sizeof(temp[0])) - 1;
    }

    size_t out_idx = 0;
    int prev_was_space = 0;
    int prev_was_underscore = 0;

    for (size_t i = 0; i < in_len; i++) {
        wchar_t c = input[i];

        /* Prevent path traversal .. */
        if (c == L'.' && i + 1 < in_len && input[i + 1] == L'.') {
            i++; /* Skip second dot */
            continue;
        }

        if (is_forbidden_char(c)) {
            c = L'_';
        }

        if (iswspace(c)) {
            if (!prev_was_space && out_idx > 0) {
                temp[out_idx++] = L' ';
                prev_was_space = 1;
                prev_was_underscore = 0;
            }
        } else if (c == L'_') {
            if (!prev_was_underscore && out_idx > 0) {
                temp[out_idx++] = L'_';
                prev_was_underscore = 1;
                prev_was_space = 0;
            }
        } else {
            temp[out_idx++] = c;
            prev_was_space = 0;
            prev_was_underscore = 0;
        }
    }

    /* Trim trailing dots, spaces, underscores */
    while (out_idx > 0 && (temp[out_idx - 1] == L' ' || temp[out_idx - 1] == L'.' || temp[out_idx - 1] == L'_')) {
        out_idx--;
    }
    temp[out_idx] = L'\0';

    /* Leading dots/spaces/underscores */
    size_t start = 0;
    while (start < out_idx && (temp[start] == L' ' || temp[start] == L'.' || temp[start] == L'_')) {
        start++;
    }

    if (start >= out_idx) {
        wcsncpy(output, default_name, max_out_len - 1);
        output[max_out_len - 1] = L'\0';
        return;
    }

    wchar_t cleaned[512];
    wcscpy(cleaned, &temp[start]);

    /* Extract stem and extension */
    const wchar_t *dot = wcsrchr(cleaned, L'.');
    wchar_t stem[512];
    wchar_t ext[64] = {0};

    if (dot && dot != cleaned) {
        size_t stem_len = dot - cleaned;
        wcsncpy(stem, cleaned, stem_len);
        stem[stem_len] = L'\0';
        wcsncpy(ext, dot, sizeof(ext)/sizeof(ext[0]) - 1);
    } else {
        wcscpy(stem, cleaned);
    }

    /* Check reserved stems */
    if (is_reserved_stem(stem)) {
        wchar_t new_stem[512];
        new_stem[0] = L'_';
        wcscpy(&new_stem[1], stem);
        wcscpy(stem, new_stem);
    }

    /* Max length handling: truncate stem while preserving extension */
    size_t ext_len = wcslen(ext);
    size_t stem_len = wcslen(stem);
    size_t max_stem = (max_out_len > ext_len + 1) ? (max_out_len - ext_len - 1) : 1;
    if (max_stem > 200) max_stem = 200;

    if (stem_len > max_stem) {
        stem[max_stem] = L'\0';
        while (max_stem > 0 && (stem[max_stem - 1] == L' ' || stem[max_stem - 1] == L'.' || stem[max_stem - 1] == L'_')) {
            stem[--max_stem] = L'\0';
        }
    }

    _snwprintf(output, max_out_len, L"%ls%ls", stem, ext);
    output[max_out_len - 1] = L'\0';
}

int safe_join_path(
    const wchar_t *base_dir,
    const wchar_t *filename,
    wchar_t *out_path,
    size_t max_out_len
)
{
    if (!base_dir || !filename || !out_path || max_out_len == 0) return 0;

    wchar_t clean_name[260];
    sanitize_filename(filename, clean_name, sizeof(clean_name)/sizeof(clean_name[0]), L"medya");

    wchar_t combined[1024];
    _snwprintf(combined, sizeof(combined)/sizeof(combined[0]), L"%ls\\%ls", base_dir, clean_name);

    wchar_t full_base[1024];
    wchar_t full_target[1024];

    if (GetFullPathNameW(base_dir, sizeof(full_base)/sizeof(full_base[0]), full_base, NULL) == 0) {
        return 0;
    }
    if (GetFullPathNameW(combined, sizeof(full_target)/sizeof(full_target[0]), full_target, NULL) == 0) {
        return 0;
    }

    /* Ensure full_target starts with full_base to block path traversal */
    size_t base_len = wcslen(full_base);
    if (_wcsnicmp(full_target, full_base, base_len) != 0) {
        return 0;
    }

    wcsncpy(out_path, full_target, max_out_len - 1);
    out_path[max_out_len - 1] = L'\0';
    return 1;
}
