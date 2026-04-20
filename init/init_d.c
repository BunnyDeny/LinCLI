/*
 * LinCLI - A lightweight C command-line interaction framework for embedded/MCU.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "init_d.h"
#include "rbtree.h"
#include <string.h>

static struct rb_root init_d_tree = RB_ROOT;

static int init_d_insert(struct init_d *init)
{
	struct rb_node **new = &(init_d_tree.rb_node), *parent = NULL;

	while (*new) {
		struct init_d *this = rb_entry(*new, struct init_d, node);
		int p1 = init->priority;
		int p2 = this->priority;

		parent = *new;
		if (p1 < p2)
			new = &((*new)->rb_left);
		else
			new = &((*new)->rb_right);
	}

	rb_link_node(&init->node, parent, new);
	rb_insert_color(&init->node, &init_d_tree);
	return 0;
}

void call_init_d(void)
{
	struct init_d *init;
	struct rb_node *node;

	_FOR_EACH_INIT_D(_init_d_start, _init_d_end, init)
	{
		if (init) {
			init_d_insert(init);
		}
	}

	for (node = rb_first(&init_d_tree); node; node = rb_next(node)) {
		init = rb_entry(node, struct init_d, node);
		if (init && init->_init_entry) {
			init->_init_entry(init->_private);
		}
	}
}
