/*
 * libcompat - system compatibility library
 *
 * Copyright © 2011 Guillem Jover <guillem@debian.org>
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <compat.h>

#include <string.h>
#include <stdlib.h>

#undef strndup

char *
strndup(const char *s, size_t n)
{
	size_t len;
	char *str;

	len = strnlen(s, n);
	str = malloc(len + 1);
	if (str == NULL)
		return NULL;

	memcpy(str, s, len);
	str[len] = '\0';

	return str;
}
