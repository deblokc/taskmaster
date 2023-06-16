/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   taskmaster.h                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/16 11:24:42 by tnaton            #+#    #+#             */
/*   Updated: 2023/06/16 12:42:45 by tnaton           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef TASKMASTER_H
# define TASKMASTER_H

# include <stdbool.h>
# include <sys/time.h>

enum restart_state {
	NEVER = 0,
	ONERROR = 1,
	ALWAYS = 2
};

enum status {
	STOPPED = 0,
	STARTING = 1,
	RUNNING = 2,
	BACKOFF = 3,
	STOPPING = 4,
	EXITED = 5,
	FATAL = 6
};

struct s_program {
	char	*name;
	char	*command;
	char	**args;
	int		numprocs;
	int		priority;
	bool	autostart;
	int		startsecs;
	int		startretries;
	int		autorestart;
	int		*exitcodes;
	int		stopsignal;
	int		stopwaitsecs;
	bool	stdoutlog;
	char	*stdoutlogpath;
	bool	stderrlog;
	char	*stderrlogpath;
	char	**env;
	char	*workingdir;
	int		umask;
	char	*user;
};

struct s_process {
	char				*name;
	int					pid;
	struct timeval		start;
	int					status;
	struct s_program	*program;
	int					count_restart;
};

#endif