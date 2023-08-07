/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   launcher.c                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/07/07 10:59:08 by tnaton            #+#    #+#             */
/*   Updated: 2023/08/07 15:31:48 by tnaton           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "taskmaster.h"
#include <unistd.h>

char *getname(char *name, int num) {
	size_t size = strlen(name) + 2;
	char *ret = (char *)calloc(size, sizeof(char));
	if (!ret) {
		return NULL;
	}
	strcpy(ret, name);
	ret[size - 1] = '0' + (char)num + 1;
	return (ret);
}

void wait_process(struct s_program *lst) {
	for (int i = 0; lst; i++, lst = lst->next) {
		if (!lst->processes) {
			printf("NO PROCESSES ???\n");
		}
		if (lst->numprocs == 1) {
			pthread_join(lst->processes[0].handle, NULL);
			free(lst->processes[0].name);
			lst->processes[0].name = NULL;
			free(lst->processes);
			lst->processes = NULL;
			printf("joined one proc\n");
		} else {
			int j = 0;
			for (; j < lst->numprocs; j++) {
				pthread_join(lst->processes[j].handle, NULL);
				free(lst->processes[j].name);
				lst->processes[j].name = NULL;
			}
			free(lst->processes);
			lst->processes = NULL;
			printf("joined %d proc\n", j);
		}
	}
}

void wait_priorities(struct s_priority *lst) {
	while (lst) {
		wait_process(lst->begin);
		lst = lst->itnext(lst);
	}
}

static char *strjoin(const char* s1, const char* s2, const char* s3)
{
	if (!s1 || !s2 || !s3) {
		return NULL;
	}
	char* result = (char *)calloc(sizeof(char), (strlen(s1) + strlen(s2) + strlen(s3) + 1));

	if (result) {
		strcpy(result, s1);
		strcat(result, s2);
		strcat(result, s3);
	}
	return result;
}

void create_process(struct s_program *lst, int log_fd) {
	if (!lst) {
		return;
	}

	/*
	 * NEED TO GIVE LOGGER TO THE PROCESS
	 */

	for (int i = 0; lst; i++, lst = lst->next) {
		printf("creating processes for %s\n", lst->name);
		lst->processes = (struct s_process *)calloc(sizeof(struct s_process), (size_t)lst->numprocs);
		if (!lst->processes) {
			printf("FATAL ERROR CALLOC PROCESS\n");
		}
		if (lst->numprocs == 1) {
			lst->processes[0].name = strdup(lst->name);
			if (!lst->processes[0].name) {
				printf("FATAL ERROR STRDUP FAILED\n");
			}
			lst->processes[0].stdoutlog = lst->stdoutlog;
			lst->processes[0].stdout_logger = lst->stdout_logger;
			if (!lst->processes[0].stdout_logger.logfile) {
				lst->processes[0].stdout_logger.logfile = strjoin("/tmp/", lst->processes[0].name, "_stdout.log");
			}
			lst->processes[0].stderrlog = lst->stderrlog;
			lst->processes[0].stderr_logger = lst->stderr_logger;
			if (!lst->processes[0].stderr_logger.logfile) {
				lst->processes[0].stderr_logger.logfile = strjoin("/tmp/", lst->processes[0].name, "_stderr.log");
			}
			lst->processes[0].status = STOPPED;
			lst->processes[0].program = lst;
			if (pipe(lst->processes[0].com)) {
				perror("pipe");
			}
			lst->processes[0].log = log_fd;
			if (pthread_create(&(lst->processes[0].handle), NULL, administrator, &(lst->processes[0]))) {
				printf("FATAL ERROR PTHREAD_CREATE\n");
			}
		} else {
			for (int j = 0; j < lst->numprocs; j++) {
				lst->processes[j].name = getname(lst->name, j);
				if (!lst->processes[j].name) {
					printf("FATAL ERROR GETNAME FAILED\n");
				}
				lst->processes[j].stdoutlog = lst->stdoutlog;
				lst->processes[j].stdout_logger = lst->stdout_logger;
				if (!lst->processes[j].stdout_logger.logfile) {
					lst->processes[j].stdout_logger.logfile = strjoin("/tmp/", lst->processes[j].name, "_stdout.log");
				} else {
					lst->processes[j].stdout_logger.logfile = strjoin(lst->processes[j].stdout_logger.logfile, "_" , lst->processes[j].name);
				}
				lst->processes[j].stderrlog = lst->stderrlog;
				lst->processes[j].stderr_logger = lst->stderr_logger;
				if (!lst->processes[j].stderr_logger.logfile) {
					lst->processes[j].stderr_logger.logfile = strjoin("/tmp/", lst->processes[j].name, "_stderr.log");
				} else {
					lst->processes[j].stderr_logger.logfile = strjoin(lst->processes[j].stderr_logger.logfile, "_" , lst->processes[j].name);
				}
				lst->processes[j].status = STOPPED;
				lst->processes[j].program = lst;
				if (pipe(lst->processes[j].com)) {
					perror("pipe");
				}
				lst->processes[j].log = log_fd;
				if (pthread_create(&(lst->processes[j].handle), NULL, administrator, &(lst->processes[j]))) {
					printf("FATAL ERROR PTHREAD_CREATE\n");
				}
			}
		}
	}
}

void launch(struct s_priority *lst, int log_fd) {
	while (lst) {
		create_process(lst->begin, log_fd);
		lst = lst->itnext(lst);
	}
}
