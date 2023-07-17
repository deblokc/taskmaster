/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   complete.c                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/07/17 18:49:49 by tnaton            #+#    #+#             */
/*   Updated: 2023/07/17 19:30:40 by tnaton           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <string.h>
#include "readline.h"
#include <stdlib.h>

void complete_init(char **tab_complete) {
	struct s_readline *global = get_global();
	global->tab_complete = tab_complete;
}

int count_word(char **tab, char *line) {
	int count = 0;

	while (*tab) {
		if (!strncmp(*tab, line, strlen(line))) {
			count++;
		}
		tab++;
	}
	return (count);
}

char **complete(char *line) {
	struct s_readline *global = get_global();

	if (!global->tab_complete) {
		return (NULL);
	}
	char **ret = (char **)calloc(sizeof(char *), count_word(global->tab_complete, line) + 1);
	if (!ret) {
		return (ret);
	}
	char **tab = global->tab_complete;

	int i = 0;
	int j = 0;
	while (tab[i]) {
		if (!strncmp(tab[i], line, strlen(line))) {
			ret[j] = strdup(tab[i]);
			if (!ret[j]) {
				return (NULL);
			}
			j++;
		}
		i++;
	}
	ret[j] = NULL;
	return (ret);
}
