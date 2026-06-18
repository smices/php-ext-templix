/*
 * php-ext-templix
 *
 * Copyright (c) 2026 MISSU.LINK
 * SPDX-License-Identifier: MIT
 *
 * This PHP extension project is released under the MIT License. Portions of
 * the implementation may be based on public network references; please report
 * any infringement concern so the affected code can be rewritten or removed.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "compiler.h"

#include "Zend/zend_smart_str.h"

#include <ctype.h>

zend_string *templix_read_file(zend_string *path)
{
    php_stream *stream;
    zend_string *contents;

    stream = php_stream_open_wrapper(ZSTR_VAL(path), "rb", REPORT_ERRORS, NULL);
    if (!stream) {
        return NULL;
    }

    contents = php_stream_copy_to_mem(stream, PHP_STREAM_COPY_ALL, 0);
    php_stream_close(stream);

    return contents;
}

zend_bool templix_cache_file_exists(zend_string *path)
{
    php_stream_statbuf statbuf;

    return php_stream_stat_path(ZSTR_VAL(path), &statbuf) == 0;
}

static zend_string *templix_trim_expression(const char *start, size_t len)
{
    while (len > 0 && isspace((unsigned char)*start)) {
        start++;
        len--;
    }

    while (len > 0 && isspace((unsigned char)start[len - 1])) {
        len--;
    }

    return zend_string_init(start, len, 0);
}

static const char *templix_find_directive_close(const char *open, const char *end)
{
    const char *cursor = open;

    while (cursor < end && isspace((unsigned char)*cursor)) {
        cursor++;
    }

    if (cursor >= end || *cursor != '(') {
        return NULL;
    }

    while (cursor < end) {
        if (*cursor == ')') {
            return cursor + 1;
        }
        cursor++;
    }

    return NULL;
}

static void templix_append_literal_echo(smart_str *compiled, const char *start, size_t len)
{
    const char *cursor = start;
    const char *end = start + len;

    if (len == 0) {
        return;
    }

    smart_str_appends(compiled, "echo '");

    while (cursor < end) {
        if (*cursor == '\\' || *cursor == '\'') {
            smart_str_appendc(compiled, '\\');
        }
        smart_str_appendc(compiled, *cursor);
        cursor++;
    }

    smart_str_appends(compiled, "';\n");
}

static void templix_append_php_block(smart_str *compiled, const char *start, size_t len)
{
    const char *cursor = start;
    const char *end = start + len;
    const char *last = end;

    smart_str_appendl(compiled, start, len);

    while (last > cursor && isspace((unsigned char)last[-1])) {
        last--;
    }

    if (last > cursor && last[-1] != ';' && last[-1] != ':' && last[-1] != '{' && last[-1] != '}') {
        smart_str_appendc(compiled, ';');
    }

    smart_str_appendc(compiled, '\n');
}

zend_string *templix_compile_source(zend_string *source)
{
    const char *cursor = ZSTR_VAL(source);
    const char *end = cursor + ZSTR_LEN(source);
    smart_str compiled = {0};

    smart_str_appends(&compiled, "<?php /* compiled by Templix */\n");

    while (cursor < end) {
        const char *escaped = php_memnstr(cursor, "{{", 2, end);
        const char *raw = php_memnstr(cursor, "{!!", 3, end);
        const char *directive = php_memnstr(cursor, "@", 1, end);
        const char *php_tag = php_memnstr(cursor, "<?php", 5, end);
        const char *tag;
        zend_bool is_raw = 0;
        zend_bool is_directive = 0;
        zend_bool is_php = 0;

        if (!escaped && !raw && !directive && !php_tag) {
            templix_append_literal_echo(&compiled, cursor, end - cursor);
            break;
        }

        if (php_tag && (!escaped || php_tag < escaped) && (!raw || php_tag < raw) && (!directive || php_tag < directive)) {
            tag = php_tag;
            is_php = 1;
        } else if (directive && (!escaped || directive < escaped) && (!raw || directive < raw)) {
            tag = directive;
            is_directive = 1;
        } else if (raw && (!escaped || raw < escaped)) {
            tag = raw;
            is_raw = 1;
        } else {
            tag = escaped;
        }

        templix_append_literal_echo(&compiled, cursor, tag - cursor);

        if (is_php) {
            const char *close = php_memnstr(tag + 5, "?>", 2, end);

            if (!close) {
                templix_append_literal_echo(&compiled, tag, end - tag);
                break;
            }

            templix_append_php_block(&compiled, tag + 5, close - (tag + 5));
            cursor = close + 2;
        } else if (is_directive) {
            if ((size_t)(end - tag) >= sizeof("@if") - 1 && memcmp(tag, "@if", sizeof("@if") - 1) == 0) {
                const char *open = tag + sizeof("@if") - 1;
                const char *line_end = templix_find_directive_close(open, end);
                if (!line_end) {
                    smart_str_appendc(&compiled, '@');
                    cursor = tag + 1;
                    continue;
                }
                smart_str_appends(&compiled, "if ");
                smart_str_appendl(&compiled, open, line_end - open);
                smart_str_appends(&compiled, ":\n");
                cursor = line_end;
            } else if ((size_t)(end - tag) >= sizeof("@elseif") - 1 && memcmp(tag, "@elseif", sizeof("@elseif") - 1) == 0) {
                const char *open = tag + sizeof("@elseif") - 1;
                const char *line_end = templix_find_directive_close(open, end);
                if (!line_end) {
                    smart_str_appendc(&compiled, '@');
                    cursor = tag + 1;
                    continue;
                }
                smart_str_appends(&compiled, "elseif ");
                smart_str_appendl(&compiled, open, line_end - open);
                smart_str_appends(&compiled, ":\n");
                cursor = line_end;
            } else if ((size_t)(end - tag) >= sizeof("@isset") - 1 && memcmp(tag, "@isset", sizeof("@isset") - 1) == 0) {
                const char *open = tag + sizeof("@isset") - 1;
                const char *line_end = templix_find_directive_close(open, end);
                if (!line_end) {
                    smart_str_appendc(&compiled, '@');
                    cursor = tag + 1;
                    continue;
                }
                smart_str_appends(&compiled, "if (isset");
                smart_str_appendl(&compiled, open, line_end - open);
                smart_str_appends(&compiled, "):\n");
                cursor = line_end;
            } else if ((size_t)(end - tag) >= sizeof("@endisset") - 1 && memcmp(tag, "@endisset", sizeof("@endisset") - 1) == 0) {
                smart_str_appends(&compiled, "endif;\n");
                cursor = tag + sizeof("@endisset") - 1;
            } else if ((size_t)(end - tag) >= sizeof("@empty") - 1 && memcmp(tag, "@empty", sizeof("@empty") - 1) == 0) {
                const char *open = tag + sizeof("@empty") - 1;
                const char *line_end = templix_find_directive_close(open, end);
                if (!line_end) {
                    smart_str_appendc(&compiled, '@');
                    cursor = tag + 1;
                    continue;
                }
                smart_str_appends(&compiled, "if (empty");
                smart_str_appendl(&compiled, open, line_end - open);
                smart_str_appends(&compiled, "):\n");
                cursor = line_end;
            } else if ((size_t)(end - tag) >= sizeof("@endempty") - 1 && memcmp(tag, "@endempty", sizeof("@endempty") - 1) == 0) {
                smart_str_appends(&compiled, "endif;\n");
                cursor = tag + sizeof("@endempty") - 1;
            } else if ((size_t)(end - tag) >= sizeof("@else") - 1 && memcmp(tag, "@else", sizeof("@else") - 1) == 0) {
                smart_str_appends(&compiled, "else:\n");
                cursor = tag + sizeof("@else") - 1;
            } else if ((size_t)(end - tag) >= sizeof("@endif") - 1 && memcmp(tag, "@endif", sizeof("@endif") - 1) == 0) {
                smart_str_appends(&compiled, "endif;\n");
                cursor = tag + sizeof("@endif") - 1;
            } else if ((size_t)(end - tag) >= sizeof("@foreach") - 1 && memcmp(tag, "@foreach", sizeof("@foreach") - 1) == 0) {
                const char *open = tag + sizeof("@foreach") - 1;
                const char *line_end = templix_find_directive_close(open, end);
                if (!line_end) {
                    smart_str_appendc(&compiled, '@');
                    cursor = tag + 1;
                    continue;
                }
                smart_str_appends(&compiled, "foreach ");
                smart_str_appendl(&compiled, open, line_end - open);
                smart_str_appends(&compiled, ":\n");
                cursor = line_end;
            } else if ((size_t)(end - tag) >= sizeof("@endforeach") - 1 && memcmp(tag, "@endforeach", sizeof("@endforeach") - 1) == 0) {
                smart_str_appends(&compiled, "endforeach;\n");
                cursor = tag + sizeof("@endforeach") - 1;
            } else {
                smart_str_appendc(&compiled, '@');
                cursor = tag + 1;
            }
        } else if (is_raw) {
            const char *close = php_memnstr(tag + 3, "!!}", 3, end);
            zend_string *expr;

            if (!close) {
                smart_str_appendl(&compiled, tag, end - tag);
                break;
            }

            expr = templix_trim_expression(tag + 3, close - (tag + 3));
            smart_str_appends(&compiled, "echo ");
            smart_str_append(&compiled, expr);
            smart_str_appends(&compiled, ";\n");
            zend_string_release(expr);
            cursor = close + 3;
        } else {
            const char *close = php_memnstr(tag + 2, "}}", 2, end);
            zend_string *expr;

            if (!close) {
                smart_str_appendl(&compiled, tag, end - tag);
                break;
            }

            expr = templix_trim_expression(tag + 2, close - (tag + 2));
            smart_str_appends(&compiled, "echo htmlspecialchars((string) (");
            smart_str_append(&compiled, expr);
            smart_str_appends(&compiled, "), ENT_QUOTES | ENT_SUBSTITUTE, 'UTF-8');\n");
            zend_string_release(expr);
            cursor = close + 2;
        }
    }

    smart_str_0(&compiled);
    return compiled.s;
}

zend_result templix_compile_file(zend_string *source_path, zend_string *cache_path)
{
    zend_string *source;
    zend_string *compiled;
    php_stream *stream;
    size_t written;
    size_t compiled_len;

    source = templix_read_file(source_path);
    if (!source) {
        return FAILURE;
    }

    compiled = templix_compile_source(source);
    zend_string_release(source);
    compiled_len = ZSTR_LEN(compiled);

    stream = php_stream_open_wrapper(ZSTR_VAL(cache_path), "wb", REPORT_ERRORS, NULL);
    if (!stream) {
        zend_string_release(compiled);
        return FAILURE;
    }

    written = php_stream_write(stream, ZSTR_VAL(compiled), ZSTR_LEN(compiled));
    php_stream_close(stream);

    zend_string_release(compiled);

    return written == compiled_len ? SUCCESS : FAILURE;
}
