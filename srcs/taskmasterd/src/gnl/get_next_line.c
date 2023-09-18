/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   get_next_line.c                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bdetune <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2021/12/07 13:27:32 by bdetune           #+#    #+#             */
/*   Updated: 2023/07/25 19:23:59 by bdetune          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "get_next_line.h"

char	*internal_get_str(int fd, char *remainer, int start_index, int *nl_pos)
{
	ssize_t	ret;
	char	*buffer;
	char	*new_remainer;

	if (internal_hasnl(remainer, &start_index, nl_pos))
		return (remainer);
	buffer = (char *)malloc(sizeof(char) * (BUFFER_SIZE + 1));
	if (!buffer)
		return (NULL);
	ret = read(fd, buffer, BUFFER_SIZE);
	if (ret <= 0)
	{
		free (buffer);
		return (remainer);
	}
	buffer[ret] = '\0';
	new_remainer = internal_join(remainer, buffer);
	free(remainer);
	free(buffer);
	return (internal_get_str(fd, new_remainer, start_index, nl_pos));
}

char	*get_next_line(int fd)
{
	static char	*remainer = NULL;
	char		*line;
	char		*new_remainer;
	int			nl_pos;

	line = NULL;
	nl_pos = -1;
	if (fd < 0 || BUFFER_SIZE <= 0)
		return (NULL);
	remainer = internal_get_str(fd, remainer, 0, &nl_pos);
	line = internal_get_line(remainer, &nl_pos);
	new_remainer = internal_getremainer(remainer, (nl_pos + 1));
	free(remainer);
	remainer = new_remainer;
	return (line);
}
