/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   report.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bdetune <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/07/11 19:41:17 by bdetune           #+#    #+#             */
/*   Updated: 2023/07/17 19:26:10 by bdetune          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "taskmaster.h"
#include <unistd.h>
#include <time.h>

char	*add_stamp(struct s_report* reporter)
{
	time_t stamp;
	struct tm *local_stamp;

	time(&stamp);
	local_stamp = localtime(&stamp);
	strftime(reporter->stamp, 22, "[%Y-%m-%d %H:%M:%S]", local_stamp);
	return (reporter->stamp);
}

void	report(struct s_report* reporter, bool critical)
{
	reporter->critical = critical;
	if (write(reporter->report_fd, reporter->buffer, strlen(reporter->buffer)) == -1) {}
}
