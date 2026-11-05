#ifndef SANITIZATION_H
#define SANITIZATION_H

#include <wchar.h>

void sanitize_filename(
    const wchar_t *input,
    wchar_t *output,
    size_t max_out_len,
    const wchar_t *default_name
);

int safe_join_path(
    const wchar_t *base_dir,
    const wchar_t *filename,
    wchar_t *out_path,
    size_t max_out_len
);

#endif /* SANITIZATION_H */
