/*
 * LinCLI - A lightweight C command-line interaction framework for embedded/MCU.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "init_d.h"

#define INIT_D_MAX 32

void call_init_d(void)
{
	struct init_d *arr[INIT_D_MAX];
	int n = 0;
	struct init_d *init;

	_FOR_EACH_INIT_D(init)
	{
		if (init && n < INIT_D_MAX)
			arr[n++] = init;
	}

	/* simple bubble sort by ascending priority */
	for (int i = 0; i < n - 1; i++) {
		for (int j = 0; j < n - 1 - i; j++) {
			if (arr[j]->priority > arr[j + 1]->priority) {
				struct init_d *tmp = arr[j];
				arr[j] = arr[j + 1];
				arr[j + 1] = tmp;
			}
		}
	}

	for (int i = 0; i < n; i++) {
		if (arr[i] && arr[i]->_init_entry)
			arr[i]->_init_entry(arr[i]->_private);
	}
}
