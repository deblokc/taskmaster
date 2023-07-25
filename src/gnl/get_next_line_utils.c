/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   get_next_line_utils.c                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bdetune <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2021/12/07 13:35:52 by bdetune           #+#    #+#             */
/*   Updated: 2023/07/25 19:26:48 by bdetune          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "get_next_line.h"

int	internal_hasnl(char *str, int *start_index, int *nl_pos)
{
	while (str && str[*start_index])
	{
		if (str[*start_index] == '\n')
		{
			*nl_pos = *start_index;
			return (1);
		}
		*start_index += 1;
	}
	return (0);
}

char	*internal_get_line(char *remainer, int *nl_pos)
{
	int		i;
	char	*line;

	i = 0;
	if (!remainer)
		return (NULL);
	if (*nl_pos == -1)
	{
		while (remainer[i])
			i++;
		*nl_pos = --i;
	}
	line = (char *)malloc(sizeof(char) * (size_t)(*nl_pos + 2));
	if (!line)
		return (NULL);
	i = 0;
	while (i <= *nl_pos)
	{
		line[i] = remainer[i];
		i++;
	}
	line[i] = '\0';
	return (line);
}

char	*internal_getremainer(char *remainer, int nl_pos)
{
	int		i;
	char	*new_remainer;

	if (!remainer || !remainer[nl_pos])
		return (NULL);
	i = 0;
	while (remainer[nl_pos + i])
		i++;
	new_remainer = (char *)malloc(sizeof(char) * (size_t)(i + 1));
	if (!new_remainer)
		return (NULL);
	i = 0;
	while (remainer[nl_pos + i])
	{
		new_remainer[i] = remainer[nl_pos + i];
		i++;
	}
	new_remainer[i] = '\0';
	return (new_remainer);
}

char	*internal_join(char *dst, char *src)
{
	int		i;
	int		j;
	char	*new_str;

	i = 0;
	j = 0;
	while (dst && dst[i])
		i++;
	while (src && src[j])
		j++;
	if (!(i + j))
		return (NULL);
	new_str = (char *)malloc(sizeof(char) * (size_t)(i + j + 1));
	if (!new_str)
		return (NULL);
	i = -1;
	while (++i >= 0 && dst && dst[i])
		new_str[i] = dst[i];
	j = -1;
	while (++j >= -1 && src && src[j])
		new_str[i + j] = src[j];
	new_str[i + j] = '\0';
	return (new_str);
}
