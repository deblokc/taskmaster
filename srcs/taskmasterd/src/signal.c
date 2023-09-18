/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   signal.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bdetune <marvin@42.fr>                     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/08/10 14:56:29 by bdetune           #+#    #+#             */
/*   Updated: 2023/08/10 18:55:02 by bdetune          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "taskmaster.h"
#include <signal.h>

volatile sig_atomic_t	g_sig = 0;

void	handler(int sig)
{
	g_sig = sig;
}

bool	block_signals(struct s_report *reporter)
{
	sigset_t	set;

	if (sigemptyset(&set))
	{
		strcpy(reporter->buffer, "CRITICAL : Could not empty signal set\n");
		report(reporter, true);
		return (false);
	}
	if (sigaddset(&set, SIGTERM) || sigaddset(&set, SIGHUP)
		|| sigaddset(&set, SIGINT) || sigaddset(&set, SIGQUIT)
		|| sigaddset(&set, SIGUSR1) || sigaddset(&set, SIGUSR2)
		|| sigaddset(&set, SIGTSTP))
	{
		strcpy(reporter->buffer, "CRITICAL : Could not add signal to mask\n");
		report(reporter, true);
		return (false);
	}
	if (sigprocmask(SIG_BLOCK, &set, NULL))
	{
		strcpy(reporter->buffer, "CRITICAL : Could not block signals\n");
		report(reporter, true);
		return (false);
	}
	return (true);
}

bool	block_signals_thread(struct s_report *reporter)
{
	sigset_t	set;

	if (sigemptyset(&set))
	{
		strcpy(reporter->buffer, "CRITICAL : Could not empty signal set\n");
		report(reporter, true);
		return (false);
	}
	if (sigaddset(&set, SIGTERM) || sigaddset(&set, SIGHUP)
		|| sigaddset(&set, SIGINT) || sigaddset(&set, SIGQUIT)
		|| sigaddset(&set, SIGUSR1) || sigaddset(&set, SIGUSR2)
		|| sigaddset(&set, SIGTSTP))
	{
		strcpy(reporter->buffer, "CRITICAL : Could not add signal to mask\n");
		report(reporter, true);
		return (false);
	}
	if (pthread_sigmask(SIG_BLOCK, &set, NULL))
	{
		strcpy(reporter->buffer, "CRITICAL : Could not block signals\n");
		report(reporter, true);
		return (false);
	}
	return (true);
}

bool	unblock_signals_thread(struct s_report *reporter)
{
	sigset_t	set;

	if (sigemptyset(&set))
	{
		strcpy(reporter->buffer, "CRITICAL : Could not empty signal set\n");
		report(reporter, true);
		return (false);
	}
	if (sigaddset(&set, SIGTERM) || sigaddset(&set, SIGHUP)
		|| sigaddset(&set, SIGINT) || sigaddset(&set, SIGQUIT)
		|| sigaddset(&set, SIGUSR1) || sigaddset(&set, SIGUSR2)
		|| sigaddset(&set, SIGTSTP))
	{
		strcpy(reporter->buffer, "CRITICAL : Could not add signal to mask\n");
		report(reporter, true);
		return (false);
	}
	if (pthread_sigmask(SIG_UNBLOCK, &set, NULL))
	{
		strcpy(reporter->buffer, "CRITICAL : Could not block signals\n");
		report(reporter, true);
		return (false);
	}
	return (true);
}

bool	install_signal_handler(struct s_report *reporter)
{
	sigset_t			set;
	struct sigaction	act;
	
	bzero(&act, sizeof(act));
	if (sigemptyset(&set))
	{
		strcpy(reporter->buffer, "CRITICAL : Could not empty signal set\n");
		report(reporter, true);
		return (false);
	}
	if (sigaddset(&set, SIGTERM) || sigaddset(&set, SIGHUP)
		|| sigaddset(&set, SIGINT) || sigaddset(&set, SIGQUIT)
		|| sigaddset(&set, SIGUSR1) || sigaddset(&set, SIGUSR2)
		|| sigaddset(&set, SIGTSTP))
	{
		strcpy(reporter->buffer, "CRITICAL : Could not add signal to mask\n");
		report(reporter, true);
		return (false);
	}
	act.sa_handler = handler;
	act.sa_mask = set;
	if (sigaction(SIGTERM, &act, NULL) || sigaction(SIGHUP, &act, NULL)
		|| sigaction(SIGINT, &act, NULL) || sigaction(SIGQUIT, &act, NULL)
		|| sigaction(SIGUSR1, &act, NULL) || sigaction(SIGUSR2, &act, NULL)
		|| sigaction(SIGTSTP, &act, NULL))
	{
		strcpy(reporter->buffer, "CRITICAL : Could not specify handler for signals\n");
		report(reporter, true);
		return (false);
	}
	return (true);
}
