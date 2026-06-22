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
    size_t depth = 0;
    char quote = '\0';
    zend_bool escaped = 0;

    while (cursor < end && isspace((unsigned char)*cursor)) {
        cursor++;
    }

    if (cursor >= end || *cursor != '(') {
        return NULL;
    }

    while (cursor < end) {
        char c = *cursor++;

        if (quote) {
            if (escaped) {
                escaped = 0;
            } else if (c == '\\') {
                escaped = 1;
            } else if (c == quote) {
                quote = '\0';
            }
            continue;
        }

        if (c == '\'' || c == '"') {
            quote = c;
            continue;
        }

        if (c == '(') {
            depth++;
            continue;
        }

        if (c == ')') {
            if (depth == 0) {
                return NULL;
            }
            depth--;
            if (depth == 0) {
                return cursor;
            }
        }
    }

    return NULL;
}

static void templix_close_echo(smart_str *compiled, zend_bool *echo_open)
{
    if (*echo_open) {
        smart_str_appends(compiled, ";\n");
        *echo_open = 0;
    }
}

static void templix_open_echo(smart_str *compiled, zend_bool *echo_open, zend_bool *echo_has_arg)
{
    if (!*echo_open) {
        smart_str_appends(compiled, "echo ");
        *echo_open = 1;
        *echo_has_arg = 0;
    }
}

static void templix_append_echo_separator(smart_str *compiled, zend_bool *echo_has_arg)
{
    if (*echo_has_arg) {
        smart_str_appends(compiled, ", ");
    }
    *echo_has_arg = 1;
}

static void templix_append_literal_echo(smart_str *compiled, const char *start, size_t len, zend_bool *echo_open, zend_bool *echo_has_arg)
{
    const char *cursor = start;
    const char *end = start + len;

    if (len == 0) {
        return;
    }

    templix_open_echo(compiled, echo_open, echo_has_arg);
    templix_append_echo_separator(compiled, echo_has_arg);
    smart_str_appendc(compiled, '\'');

    while (cursor < end) {
        if (*cursor == '\\' || *cursor == '\'') {
            smart_str_appendc(compiled, '\\');
        }
        smart_str_appendc(compiled, *cursor);
        cursor++;
    }

    smart_str_appendc(compiled, '\'');
}

static void templix_append_raw_echo(smart_str *compiled, zend_string *expr, zend_bool *echo_open, zend_bool *echo_has_arg)
{
    templix_open_echo(compiled, echo_open, echo_has_arg);
    templix_append_echo_separator(compiled, echo_has_arg);
    smart_str_append(compiled, expr);
}

static void templix_append_escaped_echo(smart_str *compiled, zend_string *expr, zend_bool *echo_open, zend_bool *echo_has_arg)
{
    templix_open_echo(compiled, echo_open, echo_has_arg);
    templix_append_echo_separator(compiled, echo_has_arg);
    smart_str_appends(compiled, "\\Templix\\escape(");
    smart_str_append(compiled, expr);
    smart_str_appendc(compiled, ')');
}

static zend_bool templix_is_safe_number_format_expression(zend_string *expr)
{
    const char *cursor = ZSTR_VAL(expr);
    const char *end = cursor + ZSTR_LEN(expr);
    size_t depth = 0;
    size_t comma_count = 0;
    char quote = '\0';
    zend_bool escaped = 0;

    while (cursor < end && isspace((unsigned char)*cursor)) {
        cursor++;
    }

    if (cursor < end && *cursor == '\\') {
        cursor++;
    }

    if ((size_t)(end - cursor) < sizeof("number_format") - 1 ||
        memcmp(cursor, "number_format", sizeof("number_format") - 1) != 0) {
        return 0;
    }
    cursor += sizeof("number_format") - 1;

    while (cursor < end && isspace((unsigned char)*cursor)) {
        cursor++;
    }
    if (cursor >= end || *cursor != '(') {
        return 0;
    }

    while (cursor < end) {
        char c = *cursor++;

        if (quote) {
            if (escaped) {
                escaped = 0;
            } else if (c == '\\') {
                escaped = 1;
            } else if (c == quote) {
                quote = '\0';
            }
            continue;
        }

        if (c == '\'' || c == '"') {
            quote = c;
            continue;
        }

        if (c == '(' || c == '[' || c == '{') {
            depth++;
            continue;
        }

        if (c == ')' || c == ']' || c == '}') {
            if (depth == 0) {
                return 0;
            }
            depth--;
            if (depth == 0) {
                while (cursor < end && isspace((unsigned char)*cursor)) {
                    cursor++;
                }
                return cursor == end && comma_count <= 1;
            }
            continue;
        }

        if (c == ',' && depth == 1) {
            comma_count++;
        }
    }

    return 0;
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

static zend_bool templix_php_block_starts_with_newline(const char *start, size_t len)
{
    const char *cursor = start;
    const char *end = start + len;

    while (cursor < end && (*cursor == ' ' || *cursor == '\t')) {
        cursor++;
    }

    return cursor < end && (*cursor == '\n' || *cursor == '\r');
}

static const char *templix_skip_one_line_ending(const char *cursor, const char *end)
{
    if (cursor < end && *cursor == '\r') {
        cursor++;
        if (cursor < end && *cursor == '\n') {
            cursor++;
        }
        return cursor;
    }

    if (cursor < end && *cursor == '\n') {
        return cursor + 1;
    }

    return cursor;
}

static zend_bool templix_is_whitespace_only(const char *start, size_t len)
{
    const char *cursor = start;
    const char *end = start + len;

    while (cursor < end) {
        if (!isspace((unsigned char)*cursor)) {
            return 0;
        }
        cursor++;
    }

    return 1;
}

static void templix_append_directive_inner(smart_str *compiled, const char *open, const char *line_end)
{
    const char *cursor = open;
    const char *inner_end = line_end - 1;

    while (cursor < line_end && isspace((unsigned char)*cursor)) {
        cursor++;
    }
    if (cursor < line_end && *cursor == '(') {
        cursor++;
    }
    while (inner_end > cursor && isspace((unsigned char)inner_end[-1])) {
        inner_end--;
    }

    smart_str_appendl(compiled, cursor, inner_end - cursor);
}

static void templix_append_function_directive(
    smart_str *compiled,
    const char *function_name,
    const char *open,
    const char *line_end,
    zend_bool *echo_open,
    zend_bool *echo_has_arg
) {
    smart_str expr = {0};

    smart_str_appends(&expr, function_name);
    smart_str_appendc(&expr, '(');
    templix_append_directive_inner(&expr, open, line_end);
    smart_str_appendc(&expr, ')');
    smart_str_0(&expr);

    templix_append_raw_echo(compiled, expr.s, echo_open, echo_has_arg);
    zend_string_release(expr.s);
}

static void templix_append_boolean_attribute_directive(
    smart_str *compiled,
    const char *attribute,
    const char *open,
    const char *line_end,
    zend_bool *echo_open,
    zend_bool *echo_has_arg
) {
    smart_str expr = {0};

    smart_str_appends(&expr, "((");
    templix_append_directive_inner(&expr, open, line_end);
    smart_str_appends(&expr, ") ? '");
    smart_str_appends(&expr, attribute);
    smart_str_appends(&expr, "' : '')");
    smart_str_0(&expr);

    templix_append_raw_echo(compiled, expr.s, echo_open, echo_has_arg);
    zend_string_release(expr.s);
}

static void templix_append_conditional_loop_control(
    smart_str *compiled,
    const char *statement,
    const char *open,
    const char *line_end
) {
    if (line_end) {
        smart_str_appends(compiled, "if (");
        templix_append_directive_inner(compiled, open, line_end);
        smart_str_appends(compiled, "): ");
        smart_str_appends(compiled, statement);
        smart_str_appends(compiled, "; endif;\n");
    } else {
        smart_str_appends(compiled, statement);
        smart_str_appends(compiled, ";\n");
    }
}

zend_string *templix_compile_source(zend_string *source)
{
    const char *cursor = ZSTR_VAL(source);
    const char *end = cursor + ZSTR_LEN(source);
    smart_str compiled = {0};
    zend_bool echo_open = 0;
    zend_bool echo_has_arg = 0;
    char forelse_stack[64][32];
    size_t forelse_depth = 0;
    unsigned int forelse_counter = 0;
    size_t switch_depth = 0;
    zend_bool suppress_switch_whitespace = 0;

    smart_str_appends(&compiled, "<?php /* compiled by Templix */\n");

    while (cursor < end) {
        const char *comment = php_memnstr(cursor, "{{--", 4, end);
        const char *escaped = php_memnstr(cursor, "{{", 2, end);
        const char *raw = php_memnstr(cursor, "{!!", 3, end);
        const char *directive = php_memnstr(cursor, "@", 1, end);
        const char *php_tag = php_memnstr(cursor, "<?php", 5, end);
        const char *tag;
        zend_bool is_raw = 0;
        zend_bool is_directive = 0;
        zend_bool is_php = 0;
        zend_bool is_comment = 0;

        if (!comment && !escaped && !raw && !directive && !php_tag) {
            templix_append_literal_echo(&compiled, cursor, end - cursor, &echo_open, &echo_has_arg);
            break;
        }

        if (comment && (!escaped || comment <= escaped) && (!raw || comment < raw) && (!directive || comment < directive) && (!php_tag || comment < php_tag)) {
            tag = comment;
            is_comment = 1;
        } else if (php_tag && (!escaped || php_tag < escaped) && (!raw || php_tag < raw) && (!directive || php_tag < directive)) {
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

        if (suppress_switch_whitespace &&
            is_directive &&
            templix_is_whitespace_only(cursor, tag - cursor)) {
            /* PHP alternative switch syntax does not allow output before case/default labels. */
        } else {
            templix_append_literal_echo(&compiled, cursor, tag - cursor, &echo_open, &echo_has_arg);
            if (tag > cursor) {
                suppress_switch_whitespace = 0;
            }
        }

        if (is_comment) {
            const char *close = php_memnstr(tag + 4, "--}}", 4, end);
            if (!close) {
                templix_append_literal_echo(&compiled, tag, end - tag, &echo_open, &echo_has_arg);
                break;
            }
            cursor = close + 4;
        } else if (is_php) {
            const char *close = php_memnstr(tag + 5, "?>", 2, end);

            if (!close) {
                templix_append_literal_echo(&compiled, tag, end - tag, &echo_open, &echo_has_arg);
                break;
            }

            templix_close_echo(&compiled, &echo_open);
            templix_append_php_block(&compiled, tag + 5, close - (tag + 5));
            cursor = close + 2;
        } else if (is_directive) {
            if ((size_t)(end - tag) >= sizeof("@php") - 1 && memcmp(tag, "@php", sizeof("@php") - 1) == 0) {
                const char *close = php_memnstr(tag + sizeof("@php") - 1, "@endphp", sizeof("@endphp") - 1, end);
                const char *block_start = tag + sizeof("@php") - 1;
                size_t block_len;
                if (!close) {
                    templix_append_literal_echo(&compiled, tag, 1, &echo_open, &echo_has_arg);
                    cursor = tag + 1;
                    continue;
                }
                block_len = close - block_start;
                templix_close_echo(&compiled, &echo_open);
                templix_append_php_block(&compiled, block_start, block_len);
                cursor = close + sizeof("@endphp") - 1;
                if (templix_php_block_starts_with_newline(block_start, block_len)) {
                    cursor = templix_skip_one_line_ending(cursor, end);
                }
            } else if ((size_t)(end - tag) >= sizeof("@verbatim") - 1 && memcmp(tag, "@verbatim", sizeof("@verbatim") - 1) == 0) {
                const char *close = php_memnstr(tag + sizeof("@verbatim") - 1, "@endverbatim", sizeof("@endverbatim") - 1, end);
                if (!close) {
                    templix_append_literal_echo(&compiled, tag, 1, &echo_open, &echo_has_arg);
                    cursor = tag + 1;
                    continue;
                }
                templix_append_literal_echo(&compiled, tag + sizeof("@verbatim") - 1, close - (tag + sizeof("@verbatim") - 1), &echo_open, &echo_has_arg);
                cursor = close + sizeof("@endverbatim") - 1;
            } else if ((size_t)(end - tag) >= sizeof("@unless") - 1 && memcmp(tag, "@unless", sizeof("@unless") - 1) == 0) {
                const char *open = tag + sizeof("@unless") - 1;
                const char *line_end = templix_find_directive_close(open, end);
                if (!line_end) {
                    templix_append_literal_echo(&compiled, tag, 1, &echo_open, &echo_has_arg);
                    cursor = tag + 1;
                    continue;
                }
                templix_close_echo(&compiled, &echo_open);
                smart_str_appends(&compiled, "if (!");
                smart_str_appendl(&compiled, open, line_end - open);
                smart_str_appends(&compiled, "):\n");
                cursor = line_end;
            } else if ((size_t)(end - tag) >= sizeof("@endunless") - 1 && memcmp(tag, "@endunless", sizeof("@endunless") - 1) == 0) {
                templix_close_echo(&compiled, &echo_open);
                smart_str_appends(&compiled, "endif;\n");
                cursor = tag + sizeof("@endunless") - 1;
            } else if ((size_t)(end - tag) >= sizeof("@if") - 1 && memcmp(tag, "@if", sizeof("@if") - 1) == 0) {
                const char *open = tag + sizeof("@if") - 1;
                const char *line_end = templix_find_directive_close(open, end);
                if (!line_end) {
                    templix_append_literal_echo(&compiled, tag, 1, &echo_open, &echo_has_arg);
                    cursor = tag + 1;
                    continue;
                }
                templix_close_echo(&compiled, &echo_open);
                smart_str_appends(&compiled, "if ");
                smart_str_appendl(&compiled, open, line_end - open);
                smart_str_appends(&compiled, ":\n");
                cursor = line_end;
            } else if ((size_t)(end - tag) >= sizeof("@elseif") - 1 && memcmp(tag, "@elseif", sizeof("@elseif") - 1) == 0) {
                const char *open = tag + sizeof("@elseif") - 1;
                const char *line_end = templix_find_directive_close(open, end);
                if (!line_end) {
                    templix_append_literal_echo(&compiled, tag, 1, &echo_open, &echo_has_arg);
                    cursor = tag + 1;
                    continue;
                }
                templix_close_echo(&compiled, &echo_open);
                smart_str_appends(&compiled, "elseif ");
                smart_str_appendl(&compiled, open, line_end - open);
                smart_str_appends(&compiled, ":\n");
                cursor = line_end;
            } else if ((size_t)(end - tag) >= sizeof("@isset") - 1 && memcmp(tag, "@isset", sizeof("@isset") - 1) == 0) {
                const char *open = tag + sizeof("@isset") - 1;
                const char *line_end = templix_find_directive_close(open, end);
                if (!line_end) {
                    templix_append_literal_echo(&compiled, tag, 1, &echo_open, &echo_has_arg);
                    cursor = tag + 1;
                    continue;
                }
                templix_close_echo(&compiled, &echo_open);
                smart_str_appends(&compiled, "if (isset");
                smart_str_appendl(&compiled, open, line_end - open);
                smart_str_appends(&compiled, "):\n");
                cursor = line_end;
            } else if ((size_t)(end - tag) >= sizeof("@endisset") - 1 && memcmp(tag, "@endisset", sizeof("@endisset") - 1) == 0) {
                templix_close_echo(&compiled, &echo_open);
                smart_str_appends(&compiled, "endif;\n");
                cursor = tag + sizeof("@endisset") - 1;
            } else if ((size_t)(end - tag) >= sizeof("@empty") - 1 &&
                       memcmp(tag, "@empty", sizeof("@empty") - 1) == 0 &&
                       forelse_depth > 0 &&
                       !templix_find_directive_close(tag + sizeof("@empty") - 1, end)) {
                templix_close_echo(&compiled, &echo_open);
                smart_str_appends(&compiled, "endforeach; if (");
                smart_str_appends(&compiled, forelse_stack[forelse_depth - 1]);
                smart_str_appends(&compiled, "):\n");
                cursor = tag + sizeof("@empty") - 1;
            } else if ((size_t)(end - tag) >= sizeof("@empty") - 1 && memcmp(tag, "@empty", sizeof("@empty") - 1) == 0) {
                const char *open = tag + sizeof("@empty") - 1;
                const char *line_end = templix_find_directive_close(open, end);
                if (!line_end) {
                    templix_append_literal_echo(&compiled, tag, 1, &echo_open, &echo_has_arg);
                    cursor = tag + 1;
                    continue;
                }
                templix_close_echo(&compiled, &echo_open);
                smart_str_appends(&compiled, "if (empty");
                smart_str_appendl(&compiled, open, line_end - open);
                smart_str_appends(&compiled, "):\n");
                cursor = line_end;
            } else if ((size_t)(end - tag) >= sizeof("@endempty") - 1 && memcmp(tag, "@endempty", sizeof("@endempty") - 1) == 0) {
                templix_close_echo(&compiled, &echo_open);
                smart_str_appends(&compiled, "endif;\n");
                cursor = tag + sizeof("@endempty") - 1;
            } else if ((size_t)(end - tag) >= sizeof("@forelse") - 1 && memcmp(tag, "@forelse", sizeof("@forelse") - 1) == 0) {
                const char *open = tag + sizeof("@forelse") - 1;
                const char *line_end = templix_find_directive_close(open, end);
                if (!line_end || forelse_depth >= 64) {
                    templix_append_literal_echo(&compiled, tag, 1, &echo_open, &echo_has_arg);
                    cursor = tag + 1;
                    continue;
                }
                snprintf(forelse_stack[forelse_depth], sizeof(forelse_stack[forelse_depth]), "$__templix_empty_%u", ++forelse_counter);
                templix_close_echo(&compiled, &echo_open);
                smart_str_appends(&compiled, forelse_stack[forelse_depth]);
                smart_str_appends(&compiled, " = true;\nforeach ");
                smart_str_appendl(&compiled, open, line_end - open);
                smart_str_appends(&compiled, ":\n");
                smart_str_appends(&compiled, forelse_stack[forelse_depth]);
                smart_str_appends(&compiled, " = false;\n");
                forelse_depth++;
                cursor = line_end;
            } else if ((size_t)(end - tag) >= sizeof("@endforelse") - 1 && memcmp(tag, "@endforelse", sizeof("@endforelse") - 1) == 0) {
                templix_close_echo(&compiled, &echo_open);
                smart_str_appends(&compiled, "endif;\n");
                if (forelse_depth > 0) {
                    forelse_depth--;
                }
                cursor = tag + sizeof("@endforelse") - 1;
            } else if ((size_t)(end - tag) >= sizeof("@else") - 1 && memcmp(tag, "@else", sizeof("@else") - 1) == 0) {
                templix_close_echo(&compiled, &echo_open);
                smart_str_appends(&compiled, "else:\n");
                cursor = tag + sizeof("@else") - 1;
            } else if ((size_t)(end - tag) >= sizeof("@endif") - 1 && memcmp(tag, "@endif", sizeof("@endif") - 1) == 0) {
                templix_close_echo(&compiled, &echo_open);
                smart_str_appends(&compiled, "endif;\n");
                cursor = tag + sizeof("@endif") - 1;
            } else if ((size_t)(end - tag) >= sizeof("@foreach") - 1 && memcmp(tag, "@foreach", sizeof("@foreach") - 1) == 0) {
                const char *open = tag + sizeof("@foreach") - 1;
                const char *line_end = templix_find_directive_close(open, end);
                if (!line_end) {
                    templix_append_literal_echo(&compiled, tag, 1, &echo_open, &echo_has_arg);
                    cursor = tag + 1;
                    continue;
                }
                templix_close_echo(&compiled, &echo_open);
                smart_str_appends(&compiled, "foreach ");
                smart_str_appendl(&compiled, open, line_end - open);
                smart_str_appends(&compiled, ":\n");
                cursor = line_end;
            } else if ((size_t)(end - tag) >= sizeof("@endforeach") - 1 && memcmp(tag, "@endforeach", sizeof("@endforeach") - 1) == 0) {
                templix_close_echo(&compiled, &echo_open);
                smart_str_appends(&compiled, "endforeach;\n");
                cursor = tag + sizeof("@endforeach") - 1;
            } else if ((size_t)(end - tag) >= sizeof("@for") - 1 && memcmp(tag, "@for", sizeof("@for") - 1) == 0) {
                const char *open = tag + sizeof("@for") - 1;
                const char *line_end = templix_find_directive_close(open, end);
                if (!line_end) {
                    templix_append_literal_echo(&compiled, tag, 1, &echo_open, &echo_has_arg);
                    cursor = tag + 1;
                    continue;
                }
                templix_close_echo(&compiled, &echo_open);
                smart_str_appends(&compiled, "for ");
                smart_str_appendl(&compiled, open, line_end - open);
                smart_str_appends(&compiled, ":\n");
                cursor = line_end;
            } else if ((size_t)(end - tag) >= sizeof("@endfor") - 1 && memcmp(tag, "@endfor", sizeof("@endfor") - 1) == 0) {
                templix_close_echo(&compiled, &echo_open);
                smart_str_appends(&compiled, "endfor;\n");
                cursor = tag + sizeof("@endfor") - 1;
            } else if ((size_t)(end - tag) >= sizeof("@while") - 1 && memcmp(tag, "@while", sizeof("@while") - 1) == 0) {
                const char *open = tag + sizeof("@while") - 1;
                const char *line_end = templix_find_directive_close(open, end);
                if (!line_end) {
                    templix_append_literal_echo(&compiled, tag, 1, &echo_open, &echo_has_arg);
                    cursor = tag + 1;
                    continue;
                }
                templix_close_echo(&compiled, &echo_open);
                smart_str_appends(&compiled, "while ");
                smart_str_appendl(&compiled, open, line_end - open);
                smart_str_appends(&compiled, ":\n");
                cursor = line_end;
            } else if ((size_t)(end - tag) >= sizeof("@endwhile") - 1 && memcmp(tag, "@endwhile", sizeof("@endwhile") - 1) == 0) {
                templix_close_echo(&compiled, &echo_open);
                smart_str_appends(&compiled, "endwhile;\n");
                cursor = tag + sizeof("@endwhile") - 1;
            } else if ((size_t)(end - tag) >= sizeof("@switch") - 1 && memcmp(tag, "@switch", sizeof("@switch") - 1) == 0) {
                const char *open = tag + sizeof("@switch") - 1;
                const char *line_end = templix_find_directive_close(open, end);
                if (!line_end) {
                    templix_append_literal_echo(&compiled, tag, 1, &echo_open, &echo_has_arg);
                    cursor = tag + 1;
                    continue;
                }
                templix_close_echo(&compiled, &echo_open);
                smart_str_appends(&compiled, "switch ");
                smart_str_appendl(&compiled, open, line_end - open);
                smart_str_appends(&compiled, ":\n");
                switch_depth++;
                suppress_switch_whitespace = 1;
                cursor = line_end;
            } else if ((size_t)(end - tag) >= sizeof("@case") - 1 && memcmp(tag, "@case", sizeof("@case") - 1) == 0) {
                const char *open = tag + sizeof("@case") - 1;
                const char *line_end = templix_find_directive_close(open, end);
                if (!line_end) {
                    templix_append_literal_echo(&compiled, tag, 1, &echo_open, &echo_has_arg);
                    cursor = tag + 1;
                    continue;
                }
                templix_close_echo(&compiled, &echo_open);
                smart_str_appends(&compiled, "case ");
                templix_append_directive_inner(&compiled, open, line_end);
                smart_str_appends(&compiled, ":\n");
                suppress_switch_whitespace = 0;
                cursor = line_end;
            } else if ((size_t)(end - tag) >= sizeof("@default") - 1 && memcmp(tag, "@default", sizeof("@default") - 1) == 0) {
                templix_close_echo(&compiled, &echo_open);
                smart_str_appends(&compiled, "default:\n");
                suppress_switch_whitespace = 0;
                cursor = tag + sizeof("@default") - 1;
            } else if ((size_t)(end - tag) >= sizeof("@endswitch") - 1 && memcmp(tag, "@endswitch", sizeof("@endswitch") - 1) == 0) {
                templix_close_echo(&compiled, &echo_open);
                smart_str_appends(&compiled, "endswitch;\n");
                if (switch_depth > 0) {
                    switch_depth--;
                }
                suppress_switch_whitespace = 0;
                cursor = tag + sizeof("@endswitch") - 1;
            } else if ((size_t)(end - tag) >= sizeof("@break") - 1 && memcmp(tag, "@break", sizeof("@break") - 1) == 0) {
                const char *open = tag + sizeof("@break") - 1;
                const char *line_end = templix_find_directive_close(open, end);
                templix_close_echo(&compiled, &echo_open);
                templix_append_conditional_loop_control(&compiled, "break", open, line_end);
                suppress_switch_whitespace = switch_depth > 0;
                cursor = line_end ? line_end : tag + sizeof("@break") - 1;
            } else if ((size_t)(end - tag) >= sizeof("@continue") - 1 && memcmp(tag, "@continue", sizeof("@continue") - 1) == 0) {
                const char *open = tag + sizeof("@continue") - 1;
                const char *line_end = templix_find_directive_close(open, end);
                templix_close_echo(&compiled, &echo_open);
                templix_append_conditional_loop_control(&compiled, "continue", open, line_end);
                cursor = line_end ? line_end : tag + sizeof("@continue") - 1;
            } else if ((size_t)(end - tag) >= sizeof("@class") - 1 && memcmp(tag, "@class", sizeof("@class") - 1) == 0) {
                const char *open = tag + sizeof("@class") - 1;
                const char *line_end = templix_find_directive_close(open, end);
                if (!line_end) {
                    templix_append_literal_echo(&compiled, tag, 1, &echo_open, &echo_has_arg);
                    cursor = tag + 1;
                    continue;
                }
                templix_append_function_directive(&compiled, "\\Templix\\class_attr", open, line_end, &echo_open, &echo_has_arg);
                cursor = line_end;
            } else if ((size_t)(end - tag) >= sizeof("@style") - 1 && memcmp(tag, "@style", sizeof("@style") - 1) == 0) {
                const char *open = tag + sizeof("@style") - 1;
                const char *line_end = templix_find_directive_close(open, end);
                if (!line_end) {
                    templix_append_literal_echo(&compiled, tag, 1, &echo_open, &echo_has_arg);
                    cursor = tag + 1;
                    continue;
                }
                templix_append_function_directive(&compiled, "\\Templix\\style_attr", open, line_end, &echo_open, &echo_has_arg);
                cursor = line_end;
            } else if ((size_t)(end - tag) >= sizeof("@checked") - 1 && memcmp(tag, "@checked", sizeof("@checked") - 1) == 0) {
                const char *open = tag + sizeof("@checked") - 1;
                const char *line_end = templix_find_directive_close(open, end);
                if (!line_end) { templix_append_literal_echo(&compiled, tag, 1, &echo_open, &echo_has_arg); cursor = tag + 1; continue; }
                templix_append_boolean_attribute_directive(&compiled, "checked", open, line_end, &echo_open, &echo_has_arg);
                cursor = line_end;
            } else if ((size_t)(end - tag) >= sizeof("@selected") - 1 && memcmp(tag, "@selected", sizeof("@selected") - 1) == 0) {
                const char *open = tag + sizeof("@selected") - 1;
                const char *line_end = templix_find_directive_close(open, end);
                if (!line_end) { templix_append_literal_echo(&compiled, tag, 1, &echo_open, &echo_has_arg); cursor = tag + 1; continue; }
                templix_append_boolean_attribute_directive(&compiled, "selected", open, line_end, &echo_open, &echo_has_arg);
                cursor = line_end;
            } else if ((size_t)(end - tag) >= sizeof("@disabled") - 1 && memcmp(tag, "@disabled", sizeof("@disabled") - 1) == 0) {
                const char *open = tag + sizeof("@disabled") - 1;
                const char *line_end = templix_find_directive_close(open, end);
                if (!line_end) { templix_append_literal_echo(&compiled, tag, 1, &echo_open, &echo_has_arg); cursor = tag + 1; continue; }
                templix_append_boolean_attribute_directive(&compiled, "disabled", open, line_end, &echo_open, &echo_has_arg);
                cursor = line_end;
            } else if ((size_t)(end - tag) >= sizeof("@readonly") - 1 && memcmp(tag, "@readonly", sizeof("@readonly") - 1) == 0) {
                const char *open = tag + sizeof("@readonly") - 1;
                const char *line_end = templix_find_directive_close(open, end);
                if (!line_end) { templix_append_literal_echo(&compiled, tag, 1, &echo_open, &echo_has_arg); cursor = tag + 1; continue; }
                templix_append_boolean_attribute_directive(&compiled, "readonly", open, line_end, &echo_open, &echo_has_arg);
                cursor = line_end;
            } else if ((size_t)(end - tag) >= sizeof("@required") - 1 && memcmp(tag, "@required", sizeof("@required") - 1) == 0) {
                const char *open = tag + sizeof("@required") - 1;
                const char *line_end = templix_find_directive_close(open, end);
                if (!line_end) { templix_append_literal_echo(&compiled, tag, 1, &echo_open, &echo_has_arg); cursor = tag + 1; continue; }
                templix_append_boolean_attribute_directive(&compiled, "required", open, line_end, &echo_open, &echo_has_arg);
                cursor = line_end;
            } else if ((size_t)(end - tag) >= sizeof("@json") - 1 && memcmp(tag, "@json", sizeof("@json") - 1) == 0) {
                const char *open = tag + sizeof("@json") - 1;
                const char *line_end = templix_find_directive_close(open, end);
                smart_str expr = {0};
                if (!line_end) {
                    templix_append_literal_echo(&compiled, tag, 1, &echo_open, &echo_has_arg);
                    cursor = tag + 1;
                    continue;
                }
                smart_str_appends(&expr, "json_encode(");
                templix_append_directive_inner(&expr, open, line_end);
                smart_str_appends(&expr, ", JSON_HEX_TAG | JSON_HEX_APOS | JSON_HEX_AMP | JSON_HEX_QUOT)");
                smart_str_0(&expr);
                templix_append_raw_echo(&compiled, expr.s, &echo_open, &echo_has_arg);
                zend_string_release(expr.s);
                cursor = line_end;
            } else {
                templix_append_literal_echo(&compiled, tag, 1, &echo_open, &echo_has_arg);
                cursor = tag + 1;
            }
        } else if (is_raw) {
            const char *close = php_memnstr(tag + 3, "!!}", 3, end);
            zend_string *expr;

            if (!close) {
                templix_append_literal_echo(&compiled, tag, end - tag, &echo_open, &echo_has_arg);
                break;
            }

            expr = templix_trim_expression(tag + 3, close - (tag + 3));
            templix_append_raw_echo(&compiled, expr, &echo_open, &echo_has_arg);
            zend_string_release(expr);
            cursor = close + 3;
        } else {
            const char *close = php_memnstr(tag + 2, "}}", 2, end);
            zend_string *expr;

            if (!close) {
                templix_append_literal_echo(&compiled, tag, end - tag, &echo_open, &echo_has_arg);
                break;
            }

            expr = templix_trim_expression(tag + 2, close - (tag + 2));
            if (templix_is_safe_number_format_expression(expr)) {
                templix_append_raw_echo(&compiled, expr, &echo_open, &echo_has_arg);
            } else {
                templix_append_escaped_echo(&compiled, expr, &echo_open, &echo_has_arg);
            }
            zend_string_release(expr);
            cursor = close + 2;
        }
    }

    templix_close_echo(&compiled, &echo_open);
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
