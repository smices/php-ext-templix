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
#include "renderer.h"

#include "Zend/zend_smart_str.h"

#include <ctype.h>

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

static zval *templix_find_simple_variable(zend_string *expr, zval *data)
{
    const char *cursor = ZSTR_VAL(expr);
    const char *end = cursor + ZSTR_LEN(expr);
    const char *name_start;
    const char *name_end;
    zend_string *name;
    zval *value;

    if (Z_TYPE_P(data) != IS_ARRAY || cursor == end || *cursor != '$') {
        return NULL;
    }

    cursor++;
    name_start = cursor;

    while (cursor < end && (isalnum((unsigned char)*cursor) || *cursor == '_')) {
        cursor++;
    }

    name_end = cursor;

    while (cursor < end && isspace((unsigned char)*cursor)) {
        cursor++;
    }

    if (name_start == name_end || cursor != end) {
        return NULL;
    }

    name = zend_string_init(name_start, name_end - name_start, 0);
    value = zend_hash_find(Z_ARRVAL_P(data), name);
    zend_string_release(name);

    return value;
}

static void templix_append_escaped(smart_str *output, zend_string *value)
{
    const char *cursor = ZSTR_VAL(value);
    const char *end = cursor + ZSTR_LEN(value);

    while (cursor < end) {
        switch (*cursor) {
            case '&':
                smart_str_appends(output, "&amp;");
                break;
            case '<':
                smart_str_appends(output, "&lt;");
                break;
            case '>':
                smart_str_appends(output, "&gt;");
                break;
            case '"':
                smart_str_appends(output, "&quot;");
                break;
            case '\'':
                smart_str_appends(output, "&#039;");
                break;
            default:
                smart_str_appendc(output, *cursor);
                break;
        }
        cursor++;
    }
}

static void templix_append_expression(smart_str *output, const char *start, size_t len, zval *data, zend_bool escape)
{
    zend_string *expr = templix_trim_expression(start, len);
    zval *value = templix_find_simple_variable(expr, data);

    if (value) {
        zend_string *string_value = zval_get_string(value);

        if (escape) {
            templix_append_escaped(output, string_value);
        } else {
            smart_str_append(output, string_value);
        }

        zend_string_release(string_value);
    }

    zend_string_release(expr);
}

static zend_bool templix_parse_foreach(
    const char *start,
    const char *end,
    const char **body_start,
    const char **body_end,
    const char **after_block,
    zend_string **collection_name,
    zend_string **item_name
) {
    const char *cursor = start;
    const char *name_start;
    const char *close_tag;
    const char *endforeach_tag;
    const char *endforeach = "<?php endforeach ?>";
    size_t endforeach_len = strlen(endforeach);

    if ((size_t)(end - cursor) < 5 || memcmp(cursor, "<?php", 5) != 0) {
        return 0;
    }
    cursor += 5;

    while (cursor < end && isspace((unsigned char)*cursor)) {
        cursor++;
    }
    if ((size_t)(end - cursor) < sizeof("foreach") - 1 || memcmp(cursor, "foreach", sizeof("foreach") - 1) != 0) {
        return 0;
    }
    cursor += sizeof("foreach") - 1;

    while (cursor < end && isspace((unsigned char)*cursor)) {
        cursor++;
    }
    if (cursor >= end || *cursor != '(') {
        return 0;
    }
    cursor++;

    while (cursor < end && isspace((unsigned char)*cursor)) {
        cursor++;
    }
    if (cursor >= end || *cursor != '$') {
        return 0;
    }
    cursor++;
    name_start = cursor;
    while (cursor < end && (isalnum((unsigned char)*cursor) || *cursor == '_')) {
        cursor++;
    }
    if (name_start == cursor) {
        return 0;
    }
    *collection_name = zend_string_init(name_start, cursor - name_start, 0);

    while (cursor < end && isspace((unsigned char)*cursor)) {
        cursor++;
    }
    if ((size_t)(end - cursor) < sizeof("as") - 1 || memcmp(cursor, "as", sizeof("as") - 1) != 0) {
        zend_string_release(*collection_name);
        return 0;
    }
    cursor += sizeof("as") - 1;

    while (cursor < end && isspace((unsigned char)*cursor)) {
        cursor++;
    }
    if (cursor >= end || *cursor != '$') {
        zend_string_release(*collection_name);
        return 0;
    }
    cursor++;
    name_start = cursor;
    while (cursor < end && (isalnum((unsigned char)*cursor) || *cursor == '_')) {
        cursor++;
    }
    if (name_start == cursor) {
        zend_string_release(*collection_name);
        return 0;
    }
    *item_name = zend_string_init(name_start, cursor - name_start, 0);

    while (cursor < end && isspace((unsigned char)*cursor)) {
        cursor++;
    }
    if (cursor >= end || *cursor != ')') {
        zend_string_release(*collection_name);
        zend_string_release(*item_name);
        return 0;
    }
    cursor++;

    while (cursor < end && isspace((unsigned char)*cursor)) {
        cursor++;
    }
    if (cursor >= end || *cursor != ':') {
        zend_string_release(*collection_name);
        zend_string_release(*item_name);
        return 0;
    }
    cursor++;

    while (cursor < end && isspace((unsigned char)*cursor)) {
        cursor++;
    }
    if ((size_t)(end - cursor) < 2 || memcmp(cursor, "?>", 2) != 0) {
        zend_string_release(*collection_name);
        zend_string_release(*item_name);
        return 0;
    }
    close_tag = cursor + 2;

    endforeach_tag = php_memnstr(close_tag, endforeach, endforeach_len, end);
    if (!endforeach_tag) {
        zend_string_release(*collection_name);
        zend_string_release(*item_name);
        return 0;
    }

    *body_start = close_tag;
    *body_end = endforeach_tag;
    *after_block = endforeach_tag + endforeach_len;

    return 1;
}

static void templix_append_foreach(
    smart_str *output,
    const char *body_start,
    const char *body_end,
    zend_string *collection_name,
    zend_string *item_name,
    zval *data,
    zend_bool auto_escape
) {
    zval *collection;
    zval *item;
    zend_string *body;

    if (Z_TYPE_P(data) != IS_ARRAY) {
        return;
    }

    collection = zend_hash_find(Z_ARRVAL_P(data), collection_name);
    if (!collection || Z_TYPE_P(collection) != IS_ARRAY) {
        return;
    }

    body = zend_string_init(body_start, body_end - body_start, 0);

    ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(collection), item) {
        zval loop_data;
        zval *value;
        zend_string *key;
        zend_string *rendered;

        array_init_size(&loop_data, zend_hash_num_elements(Z_ARRVAL_P(data)) + 1);

        ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(data), key, value) {
            if (key) {
                Z_TRY_ADDREF_P(value);
                zend_hash_update(Z_ARRVAL(loop_data), key, value);
            }
        } ZEND_HASH_FOREACH_END();

        Z_TRY_ADDREF_P(item);
        zend_hash_update(Z_ARRVAL(loop_data), item_name, item);

        rendered = templix_render_source(body, &loop_data, auto_escape);
        smart_str_append(output, rendered);
        zend_string_release(rendered);
        zval_ptr_dtor(&loop_data);
    } ZEND_HASH_FOREACH_END();

    zend_string_release(body);
}

zend_string *templix_render_source(zend_string *source, zval *data, zend_bool auto_escape)
{
    const char *cursor = ZSTR_VAL(source);
    const char *end = cursor + ZSTR_LEN(source);
    smart_str output = {0};

    while (cursor < end) {
        const char *escaped = php_memnstr(cursor, "{{", 2, end);
        const char *raw = php_memnstr(cursor, "{!!", 3, end);
        const char *foreach_tag = php_memnstr(cursor, "<?php foreach", sizeof("<?php foreach") - 1, end);
        const char *tag;
        zend_bool is_raw = 0;
        zend_bool is_foreach = 0;

        if (!escaped && !raw && !foreach_tag) {
            smart_str_appendl(&output, cursor, end - cursor);
            break;
        }

        if (foreach_tag && (!escaped || foreach_tag < escaped) && (!raw || foreach_tag < raw)) {
            tag = foreach_tag;
            is_foreach = 1;
        } else if (raw && (!escaped || raw < escaped)) {
            tag = raw;
            is_raw = 1;
        } else {
            tag = escaped;
        }

        smart_str_appendl(&output, cursor, tag - cursor);

        if (is_foreach) {
            const char *body_start;
            const char *body_end;
            const char *after_block;
            zend_string *collection_name;
            zend_string *item_name;

            if (!templix_parse_foreach(tag, end, &body_start, &body_end, &after_block, &collection_name, &item_name)) {
                smart_str_appendl(&output, tag, sizeof("<?php foreach") - 1);
                cursor = tag + sizeof("<?php foreach") - 1;
                continue;
            }

            templix_append_foreach(&output, body_start, body_end, collection_name, item_name, data, auto_escape);
            zend_string_release(collection_name);
            zend_string_release(item_name);
            cursor = after_block;
        } else if (is_raw) {
            const char *close = php_memnstr(tag + 3, "!!}", 3, end);
            if (!close) {
                smart_str_appendl(&output, tag, end - tag);
                break;
            }
            templix_append_expression(&output, tag + 3, close - (tag + 3), data, 0);
            cursor = close + 3;
        } else {
            const char *close = php_memnstr(tag + 2, "}}", 2, end);
            if (!close) {
                smart_str_appendl(&output, tag, end - tag);
                break;
            }
            templix_append_expression(&output, tag + 2, close - (tag + 2), data, auto_escape);
            cursor = close + 2;
        }
    }

    smart_str_0(&output);

    if (!output.s) {
        return ZSTR_EMPTY_ALLOC();
    }

    return output.s;
}
