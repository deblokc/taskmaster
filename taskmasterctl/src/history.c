/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   history.c                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/22 16:42:46 by tnaton            #+#    #+#             */
/*   Updated: 2023/06/22 16:44:25 by tnaton           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <fcntl.h>
#include <stdio.h>
#include <readline/history.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


char *strjoin(const char* s1, const char* s2)
{
	if (!s1 || !s2) {
		return NULL;
	}
	char* result = (char *)calloc(sizeof(char), (strlen(s1) + strlen(s2) + 1));

	if (result) {
		strcpy(result, s1);
		strcat(result, s2);
	}
	return result;
}

FILE *fopen_history(void) {
	char *history = "/.taskmasterctl_history";
	char *home = getenv("HOME");
	char *path;

	if (!home) {
		return (NULL);
	}
	int size = (strlen(home) + strlen(history) + 1);
	path = (char *)calloc(sizeof(char), size);
	strcpy(path, home);
	strcat(path, history);
	if (access(path, F_OK)) {
		close(open(path, O_CREAT | O_RDWR, 0644));
	}
	FILE *file = fopen(path, "r+");
	free(path);
	return file;
}

void add_old_history(FILE *fd) {
	if (!fd) {
		return ;
	}

	char *line = NULL;
	size_t len = 0;
	int nread = 0;

	while ((nread = getline(&line, &len, fd)) != -1) {
		line[strlen(line) - 1] = '\0';
		add_history(line);
	}
	free(line);
	return ;
}

void add_file_history(char *line, FILE *fd) {
	if (!fd) {
		return ;
	}
	char *tmp = strjoin(line, "\n");
	if (!tmp) {
		return ;
	}
	fwrite(tmp, sizeof(char), strlen(tmp), fd);
	free(tmp);
}

