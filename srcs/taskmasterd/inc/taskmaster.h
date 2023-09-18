/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   taskmaster.h                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/16 11:24:42 by tnaton            #+#    #+#             */
/*   Updated: 2023/09/18 19:28:51 by bdetune          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef TASKMASTER_H
#define TASKMASTER_H
#define _GNU_SOURCE
#include <errno.h>
#include <sys/epoll.h>
#include <limits.h>
#include <stdbool.h>
#include <sys/time.h>
#include <pthread.h>
#include <stdatomic.h>
#include <signal.h>
#include <stddef.h>
#define BUFFER_SIZE PIPE_BUF
#define PATH_SIZE 2048
#include "yaml.h"
#include "curl.h"
#include "get_next_line.h"

/*
 * ENUM
 */

enum restart_state
{
	NEVER = 0,
	ONERROR = 1,
	ALWAYS = 2
};

enum status
{
	STOPPED = 0,
	STARTING = 1,
	RUNNING = 2,
	BACKOFF = 3,
	STOPPING = 4,
	EXITED = 5,
	FATAL = 6
};

enum log_level
{
	CRITICAL = 0,
	ERROR = 1,
	WARN = 2,
	INFO = 3,
	DEBUG = 4
};

struct s_report
{
	_Alignas(max_align_t) char	buffer[PIPE_BUF + 1];
	int							report_fd;
	bool						critical;
};

struct s_env
{
	struct s_env*	next;
	char*			key;
	char*			value;
};

struct s_logger
{
	char*	logfile;
	int		logfile_maxbytes;
	int		logfile_backups;
	int		logfd;
	int		umask;
};

struct s_discord_logger
{
	_Alignas(max_align_t) struct s_report	reporter;
	_Alignas(max_align_t) char				channel[PIPE_BUF + 1];
	CURL*									handle;
	struct curl_slist*						slist;
	int										com[2];
	_Atomic bool							logging;
	bool									running;
	enum log_level							loglevel;
};

struct s_client
{
	_Alignas(max_align_t) char					buf[PIPE_BUF + 1];
	_Alignas(max_align_t) struct epoll_event	poll;
	char*										log;
	struct s_client*							next;
	bool										tail;
	bool										auth;
};

struct s_logging_client
{
	_Alignas(max_align_t) char					buf[PIPE_BUF + 1];
	_Alignas(max_align_t) struct epoll_event	poll;
	struct s_logging_client*					next;
	char*										log;
	bool										fg;
};

struct s_socket
{
	char*	socketpath;
	char*	uid;
	char*	gid;
	char*	user;
	char*	password;
	void	(*destructor)(struct s_socket *);
	int		sockfd;
	int		umask;
	bool	enable;
};

struct s_priority
{

	struct s_program*	begin;
	struct s_priority*	next;
	struct s_priority*	(*itnext)(struct s_priority *);
	void				(*print_priority)(struct s_priority *);
	void				(*print_priorities)(struct s_priority *);
	void				(*destructor)(struct s_priority *);
	int					priority;
};

struct s_program
{
	_Alignas(max_align_t) struct s_logger	stdout_logger;
	_Alignas(max_align_t) struct s_logger	stderr_logger;
	char**									args;
	char*									name;
	char*									command;
	char*									workingdir;
	char*									user;
	char*									group;
	int*									exitcodes;
	void									(*print)(struct s_program *);
	struct s_program*						(*cleaner)(struct s_program *);
	struct s_program*						(*itnext)(struct s_program *);
	struct s_process*						processes;
	struct s_program*						left;
	struct s_program*						right;
	struct s_program*						parent;
	struct s_program*						next;
	struct s_env*							env;
	int										numprocs;
	int										priority;
	int										startsecs;
	int										startretries;
	int										stopsignal;
	int										stopwaitsecs;
	int										umask;
	bool									autostart;
	bool									stopasgroup;
	bool									stdoutlog;
	bool									stderrlog;
	enum restart_state						autorestart;
};

struct s_process
{
	_Alignas(max_align_t) struct s_logger	stdout_logger;
	_Alignas(max_align_t) struct s_logger	stderr_logger;
	_Alignas(max_align_t) struct timeval	start;
	_Alignas(max_align_t) struct timeval	stop;
	struct s_logging_client*				list;
	struct s_program*						program;
	char*									name;
	int										stdin[2];
	int										stdout[2];
	int										stderr[2];
	int										com[2];
	int										count_restart;
	int										log;
	_Atomic int								status;
	_Atomic int								pid;
	pthread_t								handle;
	bool									bool_start;
	bool									bool_exit;
	bool									stdoutlog;
	bool									stderrlog;
};

struct s_server
{
	_Alignas(max_align_t) struct s_logger	logger;
	_Alignas(max_align_t) struct s_socket	socket;
	struct s_server*						(*cleaner)(struct s_server *);
	void									(*insert)(struct s_server *, struct s_program *, struct s_report *reporter);
	void									(*delete_tree)(struct s_server *);
	struct s_program*						(*begin)(struct s_server *);
	void									(*print_tree)(struct s_server *);
	struct s_program*						program_tree;
	struct s_priority*						priorities;
	struct s_env*							env;
	char*									bin_path;
	char*									config_file;
	char*									discord_channel;
	char*									discord_token;
	char*									pidfile;
	char*									user;
	char*									workingdir;
	int										log_pipe[2];
	int										umask;
	pthread_t								logging_thread;
	pid_t									pid;
	bool									log_discord;
	bool									daemon;
	enum log_level							loglevel;
	enum log_level							loglevel_discord;
};

struct s_server*	parse_config(char *bin_path, char *config_file, struct s_report *reporter);
bool				report(struct s_report *reporter, bool critical);
void*				initial_log(void *fds);
void				init_server(struct s_server *server);
void				register_treefn_serv(struct s_server *self);
void				register_treefn_prog(struct s_program *self);
void				default_logger(struct s_logger *logger);
bool				parse_server(struct s_server *server, yaml_document_t *document, int value_index, struct s_report *reporter);
bool				parse_programs(struct s_server *server, yaml_document_t *document, int value_index, struct s_report *reporter);
bool				add_char(char const *program_name, char const *field_name, char **target, yaml_node_t *value, struct s_report *reporter, int max_size);
bool				add_number(char const *program_name, char const *field_name, int *target, yaml_node_t *value, long min, long max, struct s_report *reporter);
bool				add_octal(char const *program_name, char const *field_name, int *target, yaml_node_t *value, long min, long max, struct s_report *reporter);
bool				add_bool(char const *program_name, char const *field_name, bool *target, yaml_node_t *value, struct s_report *reporter);
struct s_env*		free_s_env(struct s_env *start);
bool				parse_env(char const *program_name, yaml_node_t *map, yaml_document_t *document, struct s_env **dest, struct s_report *reporter);
void				report_critical(int fd, int report_fd);
struct s_priority*	create_priorities(struct s_server *server, struct s_report *reporter);
void*				administrator(void *arg);
void				launch(struct s_priority *lst, int log_fd);
void				wait_priorities(struct s_priority *lst);
void prelude(struct s_server *server, struct s_report *reporter);
bool transfer_logs(int tmp_fd, char tmp_log_file[1024], struct s_server *server, struct s_report *reporter);
bool write_log(struct s_logger *logger, char *log_string);
bool write_process_log(struct s_logger *logger, char *log_string);
char *get_stamp(char *stamp_str);
void *main_logger(void *void_server);
void init_socket(struct s_socket *socket);
bool parse_socket(struct s_server *server, yaml_document_t *document, int value_index, struct s_report *reporter);
bool start_logging_thread(struct s_server *server, bool daemonized);
int daemonize(struct s_server *server);
bool end_logging_thread(struct s_report *reporter, pthread_t logger);
bool block_signals(struct s_report *reporter);
bool block_signals_thread(struct s_report *reporter);
bool unblock_signals_thread(struct s_report *reporter);
void handler(int sig);
bool end_initial_log(struct s_report *reporter, void **thread_ret, pthread_t initial_logger);
bool end_logging_thread(struct s_report *reporter, pthread_t logger);
bool install_signal_handler(struct s_report *reporter);
void create_pid_file(struct s_server *server, struct s_report *reporter);
bool init_epoll(struct s_server *server, struct s_report *reporter);

void exit_admins(struct s_priority *priorities);
void create_socket(struct s_server *server, struct s_report *reporter);
void handle(int sig);
void check_server(struct s_server *server, struct epoll_event *events, int nb_events, struct s_client **clients_lst, struct s_report *reporter);
void delete_clients(struct s_client **clients_lst, struct s_report *reporter);
void update_configuration(struct s_server *server, struct s_program *program_tree, struct s_report *reporter);
struct s_program *get_current_programs(struct s_server *server, struct s_report *reporter);
void update_umask(struct s_server *server);
bool next_string(char *buffer, char *log, size_t *current_index, bool add_stamp);

struct s_logging_client *new_logging_client(struct s_logging_client **list, int client_fd, struct s_report *reporter);

extern volatile sig_atomic_t g_sig;
extern volatile _Atomic int efd;

#endif
