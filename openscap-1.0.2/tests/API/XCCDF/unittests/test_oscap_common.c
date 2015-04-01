/*
 * Copyright 2012 Red Hat Inc., Durham, North Carolina.
 * All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors:
 *	Simon Lukasik <slukasik@redhat.com>
 */

#include <stdio.h>
#include <string.h>
#include "common/list.h"
#include "common/util.h"
#include "../../../assume.h"

static bool _simple_string_filter(void *first, void *second)
{
	return !oscap_strcmp((char *)first, (char *)second);
}

static void _test_first_item_is_not_skipped(void)
{
	// Test that first item is not skipped when using filter.
	struct oscap_stringlist *names = oscap_stringlist_new();
	assume(oscap_stringlist_add_string(names, "Peter"));
	assume(oscap_stringlist_add_string(names, "Tomas"));
	assume(oscap_stringlist_add_string(names, "Peter"));
	struct oscap_string_iterator *it =
		(struct oscap_string_iterator*) oscap_iterator_new_filter(
			(struct oscap_list *) names,
			(oscap_filter_func) _simple_string_filter,
			"Peter");
	assume(oscap_string_iterator_has_more(it));
	assume(strcmp(oscap_string_iterator_next(it), "Peter") == 0);
	assume(oscap_string_iterator_has_more(it));
	assume(strcmp(oscap_string_iterator_next(it), "Peter") == 0);
	assume(oscap_string_iterator_has_more(it) == false);

	oscap_string_iterator_reset(it);
	assume(oscap_string_iterator_has_more(it));
	assume(strcmp(oscap_string_iterator_next(it), "Peter") == 0);
	assume(oscap_string_iterator_has_more(it));
	assume(strcmp(oscap_string_iterator_next(it), "Peter") == 0);
	assume(oscap_string_iterator_has_more(it) == false);
	oscap_string_iterator_free(it);
	oscap_stringlist_free(names);
}

static void _test_not_matching_last_item_is_not_returned(void)
{
	// Test that oscap_iterator_has_more works with filter.
	struct oscap_stringlist *names = oscap_stringlist_new();
	assume(oscap_stringlist_add_string(names, "Peter"));
	struct oscap_string_iterator *it =
		(struct oscap_string_iterator*) oscap_iterator_new_filter(
			(struct oscap_list *) names,
			(oscap_filter_func) _simple_string_filter,
			"Tomas");
	assume(oscap_string_iterator_has_more(it) == false);
	oscap_string_iterator_reset(it);
	assume(oscap_string_iterator_has_more(it) == false);
	oscap_string_iterator_free(it);
	oscap_stringlist_free(names);
}

static void _test_empty_list_has_more(void)
{
	struct oscap_stringlist *names = oscap_stringlist_new();
	struct oscap_string_iterator *it = oscap_stringlist_get_strings(names);
	assume(oscap_string_iterator_has_more(it) == false);
	oscap_string_iterator_reset(it);
	assume(oscap_string_iterator_has_more(it) == false);
	oscap_string_iterator_free(it);
	oscap_stringlist_free(names);
}

static void _test_empty_list_filter_has_more(void)
{
	struct oscap_stringlist *names = oscap_stringlist_new();
	struct oscap_string_iterator *it =
		(struct oscap_string_iterator*) oscap_iterator_new_filter(
			(struct oscap_list *) names,
			(oscap_filter_func) _simple_string_filter,
			NULL);
	assume(oscap_string_iterator_has_more(it) == false);
	oscap_string_iterator_reset(it);
	assume(oscap_string_iterator_has_more(it) == false);
	oscap_string_iterator_free(it);
	oscap_stringlist_free(names);
}

static void _test_hit_empty(void)
{
	struct oscap_htable *h = oscap_htable_new();
	struct oscap_htable_iterator *hit = oscap_htable_iterator_new(h);
	assume(!oscap_htable_iterator_has_more(hit));
	assume(!oscap_htable_iterator_has_more(hit));
	assume(!oscap_htable_iterator_has_more(hit));
	oscap_htable_iterator_free(hit);
	oscap_htable_free0(h);
}

static void _test_hit_single_item(void)
{
	static const char *key = "id-12345";
	static const char *value = "openscap!";
	struct oscap_htable *h = oscap_htable_new();
	assume(oscap_htable_add(h, key, (char *) value));
	struct oscap_htable_iterator *hit = oscap_htable_iterator_new(h);
	assume(oscap_htable_iterator_has_more(hit));
	const struct oscap_htable_item *item = oscap_htable_iterator_next(hit);
	assume(item != NULL);
	assume(strcmp(item->key, key) == 0);
	assume(strcmp(item->value, value) == 0);
	assume(!oscap_htable_iterator_has_more(hit));
	assume(!oscap_htable_iterator_has_more(hit));
	oscap_htable_iterator_free(hit);
	oscap_htable_free0(h);
}

static void _test_hit_multiple_items(void)
{
	static const int n = 9;
	bool seen[n];
	struct oscap_htable *h = oscap_htable_new();
	for (int i = 0; i < n; i++) {
		char key[10];
		snprintf(key, 10, "%d", i);
		assume(oscap_htable_add(h, key, NULL));
		seen[i] = false;
	}

	struct oscap_htable_iterator *hit = oscap_htable_iterator_new(h);
	assume(oscap_htable_iterator_has_more(hit));
	int i = 0;
	while (oscap_htable_iterator_has_more(hit)) {
		const char *key = oscap_htable_iterator_next_key(hit);
		assume(key != NULL);
		assume(strlen(key) == 1);
		seen[key[0] - '0'] = true;
		i++;
	}
	assume(i == n);
	oscap_htable_iterator_free(hit);
	for (i = 0; i < n; i++) {
		assume(seen[i]);
	}
	oscap_htable_free0(h);
}

static int _htable_cmp(const char *s1, const char *s2)
{
        if (s1 == NULL)
                return -1;
        if (s2 == NULL)
                return 1;
        return strcmp(s1, s2);
}

static void _test_hit_empty1(void)
{
	struct oscap_htable *h = oscap_htable_new1(_htable_cmp, 1);
	struct oscap_htable_iterator *hit = oscap_htable_iterator_new(h);
	assume(!oscap_htable_iterator_has_more(hit));
	assume(!oscap_htable_iterator_has_more(hit));
	assume(!oscap_htable_iterator_has_more(hit));
	oscap_htable_iterator_free(hit);
	oscap_htable_free0(h);
}

static void _test_hit_single_item1(void)
{
	static const char *key = "id-12345";
	static const char *value = "openscap!";
	struct oscap_htable *h = oscap_htable_new1(_htable_cmp, 1);
	assume(oscap_htable_add(h, key, (char *) value));
	struct oscap_htable_iterator *hit = oscap_htable_iterator_new(h);
	assume(oscap_htable_iterator_has_more(hit));
	const struct oscap_htable_item *item = oscap_htable_iterator_next(hit);
	assume(item != NULL);
	assume(strcmp(item->key, key) == 0);
	assume(strcmp(item->value, value) == 0);
	assume(!oscap_htable_iterator_has_more(hit));
	assume(!oscap_htable_iterator_has_more(hit));
	oscap_htable_iterator_free(hit);
	oscap_htable_free0(h);
}

static void _test_hit_multiple_items1(void)
{
	static const int n = 9;
	bool seen[n];
	struct oscap_htable *h = oscap_htable_new1(_htable_cmp, 1);
	for (int i = 0; i < n; i++) {
		char key[10];
		snprintf(key, 10, "%d", i);
		assume(oscap_htable_add(h, key, NULL));
		seen[i] = false;
	}

	struct oscap_htable_iterator *hit = oscap_htable_iterator_new(h);
	assume(oscap_htable_iterator_has_more(hit));
	int i = 0;
	while (oscap_htable_iterator_has_more(hit)) {
		const char *key = oscap_htable_iterator_next_key(hit);
		assume(key != NULL);
		assume(strlen(key) == 1);
		seen[key[0] - '0'] = true;
		i++;
	}
	assume(i == n);
	oscap_htable_iterator_free(hit);
	for (i = 0; i < n; i++) {
		assume(seen[i]);
	}
	oscap_htable_free0(h);
}

int main(int argc, char *argv[])
{
	_test_first_item_is_not_skipped();
	_test_not_matching_last_item_is_not_returned();
	_test_empty_list_has_more();
	_test_empty_list_filter_has_more();

	_test_hit_empty();
	_test_hit_single_item();
	_test_hit_multiple_items();
	_test_hit_empty1();
	_test_hit_single_item1();
	_test_hit_multiple_items1();
	return 0;
}
