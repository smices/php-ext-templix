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

#ifndef TEMPLIX_RENDERER_H
#define TEMPLIX_RENDERER_H

#include "php.h"

zend_string *templix_render_source(zend_string *source, zval *data, zend_bool auto_escape);

#endif
