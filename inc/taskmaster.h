/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   taskmaster.h                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/16 11:24:42 by tnaton            #+#    #+#             */
/*   Updated: 2023/08/08 17:18:50 by bdetune          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef TASKMASTER_H
# define TASKMASTER_H
# define _GNU_SOURCE
# include <errno.h>
# include <limits.h>
# include <stdbool.h>
# include <sys/time.h>
# include <pthread.h>
# include <stdatomic.h>
# define BUFFER_SIZE PIPE_BUF
# include "yaml.h"
# include "get_next_line.h"


/*
 * ENUM
*/

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

struct s_report {
	bool				critical;
	char				buffer[PIPE_BUF + 1];
	char				stamp[22];
	int					report_fd;
};

struct s_env {
	char*			value;
	struct s_env*	next;
};

struct s_logger {
	char*	logfile;
	int		logfile_maxbytes;
	int		logfile_backups;
	int		logfd;
	int		umask;
};

struct s_socket {
	int		sockfd;
	bool	enable;
	char	*socketpath;
	int		umask;
	char	*uid;
	char	*gid;
	char	*user;
	char	*password;
	void	(*destructor)(struct s_socket*);
};

struct s_priority {
	int					priority;
	struct s_program*	begin;
	struct s_priority*	next;
	struct s_priority*	(*itnext)(struct s_priority*);
	void				(*print_priority)(struct s_priority*);
	void				(*print_priorities)(struct s_priority*);
	void				(*destructor)(struct s_priority*);
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
	bool				stopasgroup;
	bool				stdoutlog;
	struct s_logger		stdout_logger;
	bool				stderrlog;
	struct s_logger		stderr_logger;
	struct s_env*		env;
	char				*workingdir;
	int					umask;
	char				*user;
	char				*group;
	struct s_program*	(*cleaner)(struct s_program*);
	struct s_program*	(*itnext)(struct s_program*);
	void				(*print)(struct s_program*);
	struct s_process	*processes;
	struct s_program	*left;
	struct s_program	*right;
	struct s_program	*parent;
	struct s_program	*next;
};

struct s_process {
	char				*name;
	int					pid;
	struct timeval		start;
	_Atomic int			status;
	struct s_program	*program;
	int					count_restart;
	int					stdin[2];
	int					stdout[2];
	int					stderr[2];
	int					log;
	int					com[2];
	pthread_t			handle;
};

struct s_server {
	char*				config_file;
	int					log_pipe[2];
	enum log_level		loglevel;
	struct s_logger		logger;
	char*				pidfile;
	char*				user;
	char*				workingdir;
	int					umask;
	struct s_env*		env;
	bool				daemon;
	struct s_socket		socket;
	struct s_program*	program_tree;
	struct s_priority*	priorities;
	pid_t				pid;
	pthread_t			logging_thread;
	struct s_server*	(*cleaner)(struct s_server*);
	void				(*insert)(struct s_server*, struct s_program*, struct s_report *reporter);
	void				(*delete_tree)(struct s_server*);
	struct s_program*	(*begin)(struct s_server*);
	void				(*print_tree)(struct s_server*);

};

struct s_server*	parse_config(char* config_file, struct s_report *reporter);
bool				report(struct s_report* reporter, bool critical);
void*				initial_log(void *fds);
void				init_server(struct s_server * server);
void				register_treefn_serv(struct s_server *self);
void				register_treefn_prog(struct s_program *self);
void				default_logger(struct s_logger *logger);
bool				parse_server(struct s_server *server, yaml_document_t *document, int value_index, struct s_report *reporter);
bool				parse_programs(struct s_server *server, yaml_document_t *document, int value_index, struct s_report *reporter);
bool 				add_char(char const *program_name, char const *field_name, char **target, yaml_node_t *value, struct s_report *reporter);
bool				add_number(char const *program_name, char const *field_name, int *target, yaml_node_t *value, long min, long max, struct s_report *reporter);
bool				add_octal(char const *program_name, char const *field_name, int *target, yaml_node_t *value, long min, long max, struct s_report *reporter);
bool				add_bool(char const *program_name, char const *field_name, bool *target, yaml_node_t *value, struct s_report *reporter);
struct s_env*		free_s_env(struct s_env *start);
bool				parse_env(char const *program_name, yaml_node_t *map, yaml_document_t *document, struct s_env **dest, struct s_report *reporter);
void				report_critical(int fd);
struct s_priority*	create_priorities(struct s_server* server, struct s_report *reporter);
void				*administrator(void *arg);
void				launch(struct s_priority *lst, int log_fd);
void				wait_priorities(struct s_priority *lst);
void				prelude(struct s_server *server, struct s_report *reporter);
void				transfer_logs(int tmp_fd, struct s_server *server);
bool				write_log(struct s_logger *logger, char* log_string);
char				*get_stamp(char* stamp_str);
void				*main_logger(void *void_server);
void				init_socket(struct s_socket *socket);
bool				parse_socket(struct s_server *server, yaml_document_t *document, int value_index, struct s_report *reporter);
bool				start_logging_thread(struct s_server *server, bool daemonized);
int					daemonize(struct s_server *server);
bool				end_logging_thread(struct s_report *reporter, pthread_t logger);

int create_server(void);
void handle(int sig);
void check_server(int sock_fd, int efd);

extern int g_sig;

#endif
