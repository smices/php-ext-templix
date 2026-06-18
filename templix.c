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
#include "ext/standard/info.h"
#include "Zend/zend_smart_str.h"
#include "php_templix.h"
#include "php_compat.h"
#include "engine/compiler.h"
#include "engine/renderer.h"

typedef struct _templix_engine_object {
    zend_string *mode;
    zend_string *path;
    zend_string *cache;
    zend_string *extension;
    zend_object std;
} templix_engine_object;

static zend_class_entry *templix_engine_ce;
static zend_object_handlers templix_engine_handlers;

PHP_FUNCTION(templix_escape)
{
    zend_string *input;
    const char *cursor;
    const char *end;
    smart_str output = {0};
    zend_bool changed = 0;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STR(input)
    ZEND_PARSE_PARAMETERS_END();

    cursor = ZSTR_VAL(input);
    end = cursor + ZSTR_LEN(input);

    while (cursor < end) {
        switch (*cursor) {
            case '&':
                smart_str_appends(&output, "&amp;");
                changed = 1;
                break;
            case '<':
                smart_str_appends(&output, "&lt;");
                changed = 1;
                break;
            case '>':
                smart_str_appends(&output, "&gt;");
                changed = 1;
                break;
            case '"':
                smart_str_appends(&output, "&quot;");
                changed = 1;
                break;
            case '\'':
                smart_str_appends(&output, "&#039;");
                changed = 1;
                break;
            default:
                smart_str_appendc(&output, *cursor);
                break;
        }
        cursor++;
    }

    if (!changed) {
        smart_str_free(&output);
        RETURN_STR_COPY(input);
    }

    smart_str_0(&output);
    RETURN_STR(output.s);
}

static inline templix_engine_object *templix_engine_from_obj(zend_object *obj)
{
    return (templix_engine_object *)((char *)(obj) - XtOffsetOf(templix_engine_object, std));
}

#define Z_TEMPLIX_ENGINE_P(zv) templix_engine_from_obj(Z_OBJ_P((zv)))

static zend_string *templix_config_string(HashTable *config, const char *key, size_t key_len, const char *fallback)
{
    zval *value;

    if (config && (value = zend_hash_str_find(config, key, key_len)) != NULL) {
        if (Z_TYPE_P(value) == IS_STRING) {
            return zend_string_copy(Z_STR_P(value));
        }
        if (Z_TYPE_P(value) == IS_NULL) {
            return fallback ? zend_string_init(fallback, strlen(fallback), 0) : NULL;
        }
    }

    return fallback ? zend_string_init(fallback, strlen(fallback), 0) : NULL;
}

static zend_string *templix_join_path(zend_string *left, zend_string *right)
{
    smart_str path = {0};

    smart_str_append(&path, left);
    if (ZSTR_LEN(left) > 0 && ZSTR_VAL(left)[ZSTR_LEN(left) - 1] != DEFAULT_SLASH) {
        smart_str_appendc(&path, DEFAULT_SLASH);
    }
    smart_str_append(&path, right);
    smart_str_0(&path);

    return path.s;
}

static zend_string *templix_source_path(templix_engine_object *intern, zend_string *name)
{
    smart_str relative = {0};
    zend_string *relative_path;
    zend_string *source_path;

    smart_str_append(&relative, name);
    smart_str_append(&relative, intern->extension);
    smart_str_0(&relative);

    relative_path = relative.s;
    source_path = templix_join_path(intern->path, relative_path);
    zend_string_release(relative_path);

    return source_path;
}

static zend_string *templix_cache_path(templix_engine_object *intern, zend_string *name)
{
    smart_str basename = {0};
    const char *cursor = ZSTR_VAL(name);
    const char *end = cursor + ZSTR_LEN(name);
    zend_string *cache_name;
    zend_string *cache_path;

    while (cursor < end) {
        char c = *cursor;
        if (c == '/' || c == '\\' || c == '.') {
            smart_str_appendc(&basename, '_');
        } else {
            smart_str_appendc(&basename, c);
        }
        cursor++;
    }

    smart_str_append(&basename, intern->extension);
    smart_str_appends(&basename, ".php");
    smart_str_0(&basename);

    cache_name = basename.s;
    cache_path = templix_join_path(intern->cache, cache_name);
    zend_string_release(cache_name);

    return cache_path;
}

static zend_string *templix_execute_compiled(zend_string *compiled, zval *data)
{
    smart_str code = {0};
    zval result;
    zval data_var;
    zend_string *output = NULL;
    zend_array *symbol_table;
    const char *compiled_code = ZSTR_VAL(compiled);
    size_t compiled_len = ZSTR_LEN(compiled);

    if (compiled_len >= 5 && memcmp(compiled_code, "<?php", 5) == 0) {
        compiled_code += 5;
        compiled_len -= 5;
    }

    smart_str_appends(&code, "(static function($__templix_data) { extract($__templix_data, EXTR_SKIP); ob_start();\n");
    smart_str_appendl(&code, compiled_code, compiled_len);
    smart_str_appends(&code, "\nreturn ob_get_clean(); })($__templix_data);");
    smart_str_0(&code);

    symbol_table = zend_rebuild_symbol_table();
    ZVAL_COPY(&data_var, data);
    zend_hash_str_update(symbol_table, "__templix_data", sizeof("__templix_data") - 1, &data_var);

    ZVAL_UNDEF(&result);
    if (zend_eval_stringl(ZSTR_VAL(code.s), ZSTR_LEN(code.s), &result, "Templix compiled template") == SUCCESS && !EG(exception)) {
        output = zval_get_string(&result);
    }

    if (!Z_ISUNDEF(result)) {
        zval_ptr_dtor(&result);
    }
    zend_hash_str_del(symbol_table, "__templix_data", sizeof("__templix_data") - 1);
    zend_string_release(code.s);

    return output;
}

static zend_object *templix_engine_create_object(zend_class_entry *class_type)
{
    templix_engine_object *intern = zend_object_alloc(sizeof(templix_engine_object), class_type);

    zend_object_std_init(&intern->std, class_type);
    object_properties_init(&intern->std, class_type);

    intern->mode = zend_string_init("dev", sizeof("dev") - 1, 0);
    intern->path = NULL;
    intern->cache = NULL;
    intern->extension = zend_string_init(".blade.php", sizeof(".blade.php") - 1, 0);

    intern->std.handlers = &templix_engine_handlers;

    return &intern->std;
}

static void templix_engine_free_object(zend_object *object)
{
    templix_engine_object *intern = templix_engine_from_obj(object);

    if (intern->mode) {
        zend_string_release(intern->mode);
    }
    if (intern->path) {
        zend_string_release(intern->path);
    }
    if (intern->cache) {
        zend_string_release(intern->cache);
    }
    if (intern->extension) {
        zend_string_release(intern->extension);
    }

    zend_object_std_dtor(&intern->std);
}

PHP_METHOD(Templix_Engine, __construct)
{
    HashTable *config = NULL;
    templix_engine_object *intern = Z_TEMPLIX_ENGINE_P(ZEND_THIS);
    zend_string *mode;
    zend_string *path;
    zend_string *cache;
    zend_string *extension;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY_HT(config)
    ZEND_PARSE_PARAMETERS_END();

    mode = templix_config_string(config, "mode", sizeof("mode") - 1, "dev");
    path = templix_config_string(config, "path", sizeof("path") - 1, NULL);
    cache = templix_config_string(config, "cache", sizeof("cache") - 1, NULL);
    extension = templix_config_string(config, "extension", sizeof("extension") - 1, ".blade.php");

    if (intern->mode) {
        zend_string_release(intern->mode);
    }
    if (intern->path) {
        zend_string_release(intern->path);
    }
    if (intern->cache) {
        zend_string_release(intern->cache);
    }
    if (intern->extension) {
        zend_string_release(intern->extension);
    }

    intern->mode = mode;
    intern->path = path;
    intern->cache = cache;
    intern->extension = extension;
}

PHP_METHOD(Templix_Engine, render)
{
    zend_string *name;
    zval *data = NULL;
    templix_engine_object *intern = Z_TEMPLIX_ENGINE_P(ZEND_THIS);
    zend_string *source_path;
    zend_string *cache_path;
    zend_string *compiled;
    zend_string *output;
    zval empty_data;
    zend_bool prod_mode;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_STR(name)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY(data)
    ZEND_PARSE_PARAMETERS_END();

    if (!intern->path || ZSTR_LEN(intern->path) == 0) {
        zend_throw_error(NULL, "Templix\\Engine requires a non-empty path configuration");
        RETURN_THROWS();
    }
    if (!intern->cache || ZSTR_LEN(intern->cache) == 0) {
        zend_throw_error(NULL, "Templix\\Engine requires a non-empty cache configuration");
        RETURN_THROWS();
    }

    if (!data) {
        array_init(&empty_data);
        data = &empty_data;
    } else {
        ZVAL_DEREF(data);
    }

    source_path = templix_source_path(intern, name);
    cache_path = templix_cache_path(intern, name);
    prod_mode = zend_string_equals_literal(intern->mode, "prod");

    if (!prod_mode || !templix_cache_file_exists(cache_path)) {
        if (templix_compile_file(source_path, cache_path) != SUCCESS) {
            if (&empty_data == data) {
                zval_ptr_dtor(&empty_data);
            }
            zend_string_release(source_path);
            zend_string_release(cache_path);
            zend_throw_error(NULL, "Failed to compile Templix template");
            RETURN_THROWS();
        }
    }

    compiled = templix_read_file(cache_path);
    if (!compiled) {
        if (&empty_data == data) {
            zval_ptr_dtor(&empty_data);
        }
        zend_string_release(source_path);
        zend_string_release(cache_path);
        RETURN_FALSE;
    }

    output = templix_execute_compiled(compiled, data);

    zend_string_release(compiled);
    zend_string_release(source_path);
    zend_string_release(cache_path);

    if (&empty_data == data) {
        zval_ptr_dtor(&empty_data);
    }

    if (!output) {
        RETURN_THROWS();
    }

    RETURN_STR(output);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_templix_engine_construct, 0, 0, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, config, IS_ARRAY, 0, "[]")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_templix_engine_render, 0, 1, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, template, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, data, IS_ARRAY, 0, "[]")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_templix_escape, 0, 1, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, value, IS_STRING, 0)
ZEND_END_ARG_INFO()

static const zend_function_entry templix_engine_methods[] = {
    PHP_ME(Templix_Engine, __construct, arginfo_templix_engine_construct, ZEND_ACC_PUBLIC)
    PHP_ME(Templix_Engine, render, arginfo_templix_engine_render, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

static const zend_function_entry templix_functions[] = {
    ZEND_NS_FENTRY("Templix", escape, PHP_FN(templix_escape), arginfo_templix_escape, 0)
    PHP_FE_END
};

PHP_MINIT_FUNCTION(templix)
{
    zend_class_entry ce;

    INIT_NS_CLASS_ENTRY(ce, "Templix", "Engine", templix_engine_methods);
    templix_engine_ce = zend_register_internal_class(&ce);
    templix_engine_ce->create_object = templix_engine_create_object;

    memcpy(&templix_engine_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    templix_engine_handlers.offset = XtOffsetOf(templix_engine_object, std);
    templix_engine_handlers.free_obj = templix_engine_free_object;

    return SUCCESS;
}

PHP_MINFO_FUNCTION(templix)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "Templix support", "enabled");
    php_info_print_table_row(2, "Version", PHP_TEMPLIX_VERSION);
    php_info_print_table_end();
}

zend_module_entry templix_module_entry = {
    STANDARD_MODULE_HEADER,
    "templix",
    templix_functions,
    PHP_MINIT(templix),
    NULL,
    NULL,
    NULL,
    PHP_MINFO(templix),
    PHP_TEMPLIX_VERSION,
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_TEMPLIX
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(templix)
#endif
