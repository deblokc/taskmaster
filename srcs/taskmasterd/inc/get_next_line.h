/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   get_next_line.h                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bdetune <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2021/12/02 12:19:31 by bdetune           #+#    #+#             */
/*   Updated: 2023/01/30 20:14:25 by bdetune          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef GET_NEXT_LINE_H
#define GET_NEXT_LINE_H
#ifndef BUFFER_SIZE
#define BUFFER_SIZE 42
#endif
#include <sys/uio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>

char *get_next_line(int fd);
char *internal_get_str(int fd, char *remainer, int start_index, int *nl_pos);
char *internal_join(char *dst, char *src);
char *internal_getremainer(char *remainer, int nl_pos);
char *internal_get_line(char *remainer, int *nl_pos);
int internal_hasnl(char *str, int *start_index, int *nl_pos);

#endif
