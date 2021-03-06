/*
 * libdpkg - Debian packaging suite library routines
 * pkg-array.c - primitives for pkg array handling
 *
 * Copyright © 1995,1996 Ian Jackson <ian@chiark.greenend.org.uk>
 * Copyright © 2009 Guillem Jover <guillem@debian.org>
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

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include <dpkg/dpkg.h>
#include <dpkg/dpkg-db.h>
#include <dpkg/pkg-array.h>

/**
 * Initialize a package array from the package database.
 *
 * @param a The array to initialize.
 */
void
pkg_array_init_from_db(struct pkg_array *a)
{
	struct pkgiterator *it;
	struct pkginfo *pkg;
	int i;

	a->n_pkgs = pkg_db_count_pkg();
	a->pkgs = m_malloc(sizeof(a->pkgs[0]) * a->n_pkgs);

	it = pkg_db_iter_new();
	for (i = 0; (pkg = pkg_db_iter_next_pkg(it)); i++)
		a->pkgs[i] = pkg;
	pkg_db_iter_free(it);

	assert(i == a->n_pkgs);
}

/**
 * Sort a package array.
 *
 * @param a The array to sort.
 * @param pkg_sort The function to sort the array.
 */
void
pkg_array_sort(struct pkg_array *a, pkg_sorter_func *pkg_sort)
{
	qsort(a->pkgs, a->n_pkgs, sizeof(a->pkgs[0]), pkg_sort);
}

/**
 * Destroy a package array.
 *
 * Frees the allocated memory and resets the members.
 *
 * @param a The array to destroy.
 */
void
pkg_array_destroy(struct pkg_array *a)
{
	a->n_pkgs = 0;
	free(a->pkgs);
	a->pkgs = NULL;
}
