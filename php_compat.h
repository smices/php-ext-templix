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

#ifndef PHP_TEMPLIX_COMPAT_H
#define PHP_TEMPLIX_COMPAT_H

#include "php.h"

#if PHP_VERSION_ID < 80000
# error "Templix requires PHP 8.0 or newer"
#endif

#endif
