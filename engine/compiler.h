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

#ifndef TEMPLIX_COMPILER_H
#define TEMPLIX_COMPILER_H

#include "php.h"

zend_string *templix_read_file(zend_string *path);
zend_string *templix_compile_source(zend_string *source);
zend_result templix_compile_file(zend_string *source_path, zend_string *cache_path);
zend_bool templix_cache_file_exists(zend_string *path);

#endif
