/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   taskmaster.h                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/16 11:24:42 by tnaton            #+#    #+#             */
/*   Updated: 2023/06/20 14:39:03 by bdetune          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef TASKMASTER_H
# define TASKMASTER_H
# define _GNU_SOURCE
# include <stdbool.h>
# include <sys/time.h>
# include "yaml.h"

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

enum log_level {
	CRITICAL = 0,
	ERROR = 1,
	WARN = 2,
	INFO = 3,
	DEBUG = 4
};

struct s_logger {
	char*				logfile;
	unsigned int		logfile_maxbytes;
	unsigned char		logfile_backups;
};

struct s_socket {
	char	*socketpath;
	int		chmod;
	char	*uid;
	char	*gid;
	char	*user;
	char	*password;
};


struct s_program {
	char				*name;
	char				*command;
	char				**args;
	int					numprocs;
	int					priority;
	bool				autostart;
	int					startsecs;
	int					startretries;
	enum restart_state	autorestart;
	int					*exitcodes;
	int					stopsignal;
	int					stopwaitsecs;
	bool				stdoutlog;
	struct s_logger		stdout_logger;
	bool				stderrlog;
	struct s_logger		stderr_logger;
	char				**env;
	char				*workingdir;
	int					umask;
	char				*user;
	struct s_process	*processes;
	struct s_program	*left;
	struct s_program	*right;
	struct s_program	*parent;
};

struct s_process {
	char				*name;
	int					pid;
	struct timeval		start;
	int					status;
	struct s_program	*program;
	int					count_restart;
};


struct s_server {
	char *				config_file;
	int					log_pipe[2];
	enum log_level		log_level;
	struct s_logger		logger;
	char				*user;
	int					umask;
	char				**env;
	struct s_socket		socket;
	struct s_program	*program_tree;
	struct s_server*	(*cleaner)(struct s_server*);
};

struct s_server*	parse_config(char* config_file);
bool				init_server(struct s_server * server);
bool				default_logger(struct s_server *server);
bool				parse_server(struct s_server *server, yaml_document_t *document, int value_index);
bool				parse_programs(struct s_server *server, yaml_document_t *document, int value_index);


#endif
