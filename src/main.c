/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/16 11:25:17 by tnaton            #+#    #+#             */
/*   Updated: 2023/08/24 16:24:43 by bdetune          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "taskmaster.h"
#include <limits.h>
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>

int	early_error(char* msg, int* reporter_pipe, char* tmp_log, struct s_server *server)
{
	if (write(2, msg, strlen(msg))) {}
	if (reporter_pipe[0] > 0)
		close(reporter_pipe[0]);
	if (reporter_pipe[1] > 0)
		close(reporter_pipe[1]);
	if (reporter_pipe[2] > 0)
		close(reporter_pipe[2]);
	if (tmp_log)
		unlink(tmp_log);
	if (server)
		server->cleaner(server);
	return (1);
}

int	reload_early_error(int* reporter_pipe, char* tmp_log, struct s_server *server)
{
	if (reporter_pipe[0] > 0)
		close(reporter_pipe[0]);
	if (reporter_pipe[1] > 0)
		close(reporter_pipe[1]);
	if (reporter_pipe[2] > 0)
		close(reporter_pipe[2]);
	if (tmp_log)
		unlink(tmp_log);
	if (server)
		server->cleaner(server);
	return (1);
}

static int	tmp_log(char tmp_log_file[1024])
{
	int				fd;
	ssize_t			ret;
	char			base[] = "0123456789-abcdefghijklmnopqrstuvwxyz_ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	unsigned char	tmp;
	

	bzero(tmp_log_file, sizeof(char) * 1024);
	fd = open("/dev/urandom", O_RDONLY);
	if (fd == -1)
		return (-1);
	strcpy(tmp_log_file, "/tmp/.taskmaster_tmp_");
	ret = read(fd, &tmp_log_file[21], 200);
	close(fd);
	if (ret < 0)
		return (-1);
	for (ssize_t i = 0; i < ret; ++i)
	{
		tmp = (unsigned char)tmp_log_file[21 + i];
		tmp_log_file[21 + i] = base[tmp % 64];
	}
	strncat(tmp_log_file, ".log", 1023);
	return (open(tmp_log_file, O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP));
}

bool	tmp_logging(int reporter_pipe[4], char tmp_log_file[1024], struct s_report *reporter, bool is_first)
{
	if (pipe2(reporter_pipe, O_DIRECT | O_NONBLOCK) == -1)
	{
		if (is_first)
		{
			get_stamp(reporter->buffer);
			snprintf(&reporter->buffer[22], PIPE_BUF - 22, "CRITICAL: Could not create pipe, exiting process\n");
			early_error(reporter->buffer, reporter_pipe, NULL, NULL);
		}
		else
		{
			strcpy(reporter->buffer, "CRITICAL: Could not create pipe, exiting process\n");
			report(reporter, true);
			reload_early_error(reporter_pipe, NULL, NULL);
		}
		return (false);
	}
	reporter_pipe[2] = tmp_log(tmp_log_file);
	if (reporter_pipe[2] < 0)
	{
		if (is_first)
		{
			get_stamp(reporter->buffer);
			snprintf(&reporter->buffer[22], PIPE_BUF - 22, "CRITICAL: could not create temporary log file, exiting process\n");
			early_error(reporter->buffer, reporter_pipe, NULL, NULL);
		}
		else
		{
			strcpy(reporter->buffer, "CRITICAL: could not create temporary log file, exiting process\n");
			report(reporter, true);
			reload_early_error(reporter_pipe, NULL, NULL);
		}
		return (false);
	}
	return (true);
}


static void	cleanup(struct s_server *server, bool remove_pidfile, struct s_report *reporter)
{
	if (!end_logging_thread(reporter, server->logging_thread) && !server->daemon)
	{
		if (write(2, reporter->buffer, strlen(reporter->buffer))) {}
	}
	if (remove_pidfile)
	{
		if (!server->pidfile)
			unlink("taskmasterd.pid");
		else
			unlink(server->pidfile);
	}
	if (efd > 0)
		close(efd);
	efd = 0;
	if (server->socket.enable && server->socket.socketpath)
		unlink(server->socket.socketpath);
	server->cleaner(server);
}

static void	soft_cleanup(struct s_server *server)
{
	if (!server->pidfile)
		unlink("taskmasterd.pid");
	else
		unlink(server->pidfile);
	if (efd > 0)
		close(efd);
	efd = 0;
	if (server->socket.enable && server->socket.socketpath)
		unlink(server->socket.socketpath);
}

static bool reload_error(struct s_server *server, struct s_report *reporter)
{
	strcpy(reporter->buffer, "CRITICAL: Failed to reload configuration, exiting process\n");
	report(reporter, true);
	if (!end_logging_thread(reporter, server->logging_thread) && !server->daemon)
	{
		if (write(2, reporter->buffer, strlen(reporter->buffer))) {}
	}
	server->cleaner(server);
	return (false);
}

static bool	reload_configuration(struct s_server **server, struct s_report *reporter)
{
	char			tmp_log_file[1024];
	void			*thread_ret;
	int				reporter_pipe[4];
	int				ret = 0;
	pthread_t		initial_logger;
	struct s_server	*new_server = NULL;
	struct s_report	tmp_reporter;

	soft_cleanup(*server);
	bzero(reporter_pipe, sizeof(int) * 4);
	bzero(tmp_reporter.buffer, PIPE_BUF + 1);
	bzero(tmp_log_file, sizeof(char) * 1024);
	tmp_reporter.critical = false;
	if (!block_signals_thread(reporter) || !tmp_logging(reporter_pipe, tmp_log_file, reporter, false))
		return (reload_error(*server, reporter));
	tmp_reporter.report_fd = reporter_pipe[1];
	printf("############ RELOADING ############\n");
	if (pthread_create(&initial_logger, NULL, initial_log, reporter_pipe))
	{
		snprintf(reporter->buffer, PIPE_BUF - 22, "CRITICAL: could not create initial log thread, exiting process\n");
		report(reporter, true);
		reload_early_error(reporter_pipe, tmp_log_file, NULL);
		return (reload_error(*server, reporter));
	}
	snprintf(reporter->buffer, PIPE_BUF - 22, "############ Initial thread created ############\n");
	report(reporter, false);
	new_server = parse_config((*server)->bin_path, (*server)->config_file, &tmp_reporter);
	if (!new_server || tmp_reporter.critical)
	{
		if (end_initial_log(&tmp_reporter, &thread_ret, initial_logger) && *(int *)thread_ret != 1)
			report_critical(reporter_pipe[2], reporter->report_fd);
		reload_early_error(reporter_pipe, tmp_log_file, new_server);
		return (reload_error(*server, reporter));
	}
	snprintf(reporter->buffer, PIPE_BUF - 22, "############ Valid new server ############\n");
	report(reporter, false);
	if (new_server->socket.enable)
	{
		create_socket(new_server, &tmp_reporter);
		if (tmp_reporter.critical)
		{
			if (end_initial_log(&tmp_reporter, &thread_ret, initial_logger) && *(int *)thread_ret != 1)
				report_critical(reporter_pipe[2], reporter->report_fd);
			reload_early_error(reporter_pipe, tmp_log_file, new_server);
			return (reload_error(*server, reporter));
		}
		snprintf(reporter->buffer, PIPE_BUF - 22, "############ Socket created ############\n");
		report(reporter, false);
	}
	prelude(new_server, &tmp_reporter);
	if (tmp_reporter.critical)
	{
		if (end_initial_log(&tmp_reporter, &thread_ret, initial_logger) && *(int *)thread_ret != 1)
			report_critical(reporter_pipe[2], reporter->report_fd);
		reload_early_error(reporter_pipe, tmp_log_file, new_server);
		return (reload_error(*server, reporter));
	}
	snprintf(reporter->buffer, PIPE_BUF - 22, "############ Prelude successful ############\n");
	report(reporter, false);
	if (!end_initial_log(&tmp_reporter, &thread_ret, initial_logger))
	{
		snprintf(reporter->buffer, PIPE_BUF - 22, "CRITICAL: could not exit initial logging thread, exiting process\n");
		report(reporter, true);
		reload_early_error(reporter_pipe, tmp_log_file, new_server);
		return (reload_error(*server, reporter));
	}
	close(reporter_pipe[0]);
	close(reporter_pipe[1]);
	reporter_pipe[0] = 0;
	reporter_pipe[1] = 0;
	snprintf(reporter->buffer, PIPE_BUF - 22, "############ End initial log successful ############\n");
	report(reporter, false);
	if ((*server)->daemon && !new_server->daemon)
	{
		snprintf(reporter->buffer, PIPE_BUF - 22, "CRITICAL: Cannot undaemonize process\n");
		report(reporter, true);
		new_server->daemon = true;
	}
	if (!transfer_logs(reporter_pipe[2], tmp_log_file, new_server, &tmp_reporter))
	{
		reload_early_error(reporter_pipe, tmp_log_file, new_server);
		return (reload_error(*server, reporter));
	}
	reporter_pipe[2] = 0;
	strcpy(reporter->buffer, "INFO: Successfully reloaded configuration\n");
	report(reporter, false);
	if (!end_logging_thread(reporter, (*server)->logging_thread) && !(*server)->daemon)
	{
		if (write(2, reporter->buffer, strlen(reporter->buffer))) {}
	}
	(*server)->cleaner(*server);
	*server = new_server;
	reporter->critical = false;
	reporter->report_fd = tmp_reporter.report_fd;
	if (!start_logging_thread(*server, (*server)->daemon))
	{
		*server = (*server)->cleaner(*server);
		return (false);
	}
	reporter->report_fd = (*server)->log_pipe[1];
	(*server)->priorities = create_priorities(*server, reporter);
	if (reporter->critical)
	{
		strcpy(reporter->buffer, "CRITICAL: Could not build priorities, exiting taskmasterd\n");
		report(reporter, true);
		if (!end_logging_thread(reporter, (*server)->logging_thread) && !(*server)->daemon)
		{
			if (write(2, reporter->buffer, strlen(reporter->buffer))) {}
		}	
		*server = (*server)->cleaner(*server);
		return (false);
	}
	if ((*server)->daemon)
	{
		ret = daemonize(*server);
		if (ret != -1)
		{
			(*server)->cleaner(*server);
			return (false);
		}
	}
	else
	{
		(*server)->pid = getpid();
	}
	reporter->report_fd = (*server)->log_pipe[1];
	create_pid_file(*server, reporter);
	if (reporter->critical)
	{
		cleanup(*server, false, reporter);
		return (false);
	}
	strcpy(reporter->buffer, "INFO: Starting taskmasterd\n");
	report(reporter, false);
	if (!(*server)->priorities)
	{
		strcpy(reporter->buffer, "DEBUG: No priorities to start\n");
		report(reporter, false);
	}
	else
	{
		strcpy(reporter->buffer, "DEBUG: Launching priorities\n");
		report(reporter, false);
		launch((*server)->priorities, (*server)->log_pipe[1]);
	}
	return (true);
}



int	main_routine(struct s_server *server, struct s_report *reporter)
{
	int					nb_events;
	struct s_client		*clients = NULL;
	struct epoll_event	events[10];
	struct s_program	*program_tree = NULL;
	struct s_program	*tmp_tree = NULL;

	if (!install_signal_handler(reporter) || !unblock_signals_thread(reporter)
		|| (server->socket.enable && !init_epoll(server, reporter)))
	{
		exit_admins(server->priorities);
		wait_priorities(server->priorities);
		cleanup(server, true, reporter);
		return (1);
	}
	bzero(events, sizeof(*events) * 10);
	while (true)
	{
		if (!server->socket.enable)
		{
			pause();
		}
		else
		{
			while ((nb_events = epoll_wait(efd, events, 10, 5000)) >= 0)
			{
				check_server(server, events, nb_events, &clients, reporter);
				if (g_sig)
					break ;
			}
		}
		if (g_sig)
		{
			if (g_sig == SIGHUP)
			{
				strcpy(reporter->buffer, "INFO: taskmasterd received signal to reload configuration\n");
				report(reporter, false);
				delete_clients(&clients);
				exit_admins(server->priorities);
				wait_priorities(server->priorities);
				if (!reload_configuration(&server, reporter))
					return (1);
				if (!install_signal_handler(reporter) || !unblock_signals_thread(reporter)
					|| (server->socket.enable && !init_epoll(server, reporter)))
				{
					exit_admins(server->priorities);
					wait_priorities(server->priorities);
					cleanup(server, true, reporter);
					return (1);
				}
				g_sig = 0;
			}
			else if (g_sig == -1)
			{
				strcpy(reporter->buffer, "INFO: taskmasterd received signal to update configuration\n");
				report(reporter, false);
				program_tree = get_current_programs(server, reporter);
				if (reporter->critical)
				{
					strcpy(reporter->buffer, "CRITICAL: Could not update configuration, taskmasterd will continue with current configuration\n");
					report(reporter, false);
					if (program_tree)
					{
						tmp_tree = server->program_tree;
						server->program_tree = program_tree;
						server->delete_tree(server);
						server->program_tree = tmp_tree;
						program_tree = NULL;
					}
				}
				else
				{
					update_configuration(server, program_tree, reporter);
				}
				g_sig = 0;
			}
			else
			{
				strcpy(reporter->buffer, "INFO: taskmasterd received signal to shutdown\n");
				report(reporter, false);
				delete_clients(&clients);
				exit_admins(server->priorities);
				wait_priorities(server->priorities);
				cleanup(server, true, reporter);
				return (0);
			}
		}
	}
	return (0);
}


int	main(int ac, char **av)
{
	char			tmp_log_file[1024];
	void			*thread_ret;
	int				ret = 0;
	int				reporter_pipe[4];
	pthread_t		initial_logger;
	struct s_report reporter;
	struct s_server *server = NULL;

	bzero(reporter_pipe, sizeof(int) * 4);
	bzero(reporter.buffer, PIPE_BUF + 1);
	bzero(tmp_log_file, sizeof(char) * 1024);
	reporter.critical = false;
	reporter.report_fd = 2;
	if (ac != 2)
	{
		snprintf(reporter.buffer, PIPE_BUF, "Usage: %s CONFIGURATION-FILE\n", av[0]);
		return (1);
	}
	if (!block_signals(&reporter))
	{
		get_stamp(reporter.buffer);
		snprintf(&reporter.buffer[22], PIPE_BUF - 22, "CRITICAL: Could not block signals, exiting process\n");
		return (1);
	}
	if (!tmp_logging(reporter_pipe, tmp_log_file, &reporter, true))
		return (1);
	reporter.report_fd = reporter_pipe[1];
	if (pthread_create(&initial_logger, NULL, initial_log, reporter_pipe))
	{
		get_stamp(reporter.buffer);
		snprintf(&reporter.buffer[22], PIPE_BUF - 22, "CRITICAL: could not create initial log thread, exiting process\n");
		return (early_error(reporter.buffer, reporter_pipe, tmp_log_file, NULL));
	}
	server = parse_config(av[0], av[1], &reporter);
	if (!server || reporter.critical)
	{
		if (end_initial_log(&reporter, &thread_ret, initial_logger) && *(int *)thread_ret != 1)
			report_critical(reporter_pipe[2], 2);
		get_stamp(reporter.buffer);
		snprintf(&reporter.buffer[22], PIPE_BUF - 22, "CRITICAL: could not start taskmasterd, exiting process\n");
		return (early_error(reporter.buffer, reporter_pipe, tmp_log_file, server));
	}
	if (server->socket.enable)
	{
		create_socket(server, &reporter);
		if (reporter.critical)
		{
			if (end_initial_log(&reporter, &thread_ret, initial_logger) && *(int *)thread_ret != 1)
				report_critical(reporter_pipe[2], 2);
			get_stamp(reporter.buffer);
			snprintf(&reporter.buffer[22], PIPE_BUF - 22, "CRITICAL: could not start taskmasterd, exiting process\n");
			return (early_error(reporter.buffer, reporter_pipe, tmp_log_file, server));
		}
	}
	prelude(server, &reporter);
	if (reporter.critical)
	{
		if (end_initial_log(&reporter, &thread_ret, initial_logger) && *(int *)thread_ret != 1)
			report_critical(reporter_pipe[2], 2);
		get_stamp(reporter.buffer);
		snprintf(&reporter.buffer[22], PIPE_BUF - 22, "CRITICAL: could not start taskmasterd, exiting process\n");
		return (early_error(reporter.buffer, reporter_pipe, tmp_log_file, server));
	}
	if (!end_initial_log(&reporter, &thread_ret, initial_logger))
		return (early_error(reporter.buffer, reporter_pipe, tmp_log_file, server));
	close(reporter_pipe[0]);
	close(reporter_pipe[1]);
	reporter_pipe[0] = 0;
	reporter_pipe[1] = 0;
	if (!transfer_logs(reporter_pipe[2], tmp_log_file, server, &reporter))
		return (early_error(reporter.buffer, reporter_pipe, tmp_log_file, server));
	reporter_pipe[2] = 0;
	if (!start_logging_thread(server, false))
	{
		server = server->cleaner(server);
		return (1);
	}
	reporter.report_fd = server->log_pipe[1];
	server->priorities = create_priorities(server, &reporter);
	if (reporter.critical)
	{
		strcpy(reporter.buffer, "CRITICAL: Could not build priorities, exiting taskmasterd\n");
		report(&reporter, true);
		if (!end_logging_thread(&reporter, server->logging_thread))
		{
			if (write(2, reporter.buffer, strlen(reporter.buffer))) {}
		}	
		server = server->cleaner(server);
		return (1);
	}
	if (server->daemon)
	{
		ret = daemonize(server);
		if (ret != -1)
		{
			server->cleaner(server);
			exit(ret);
		}
	}
	else
	{
		server->pid = getpid();
	}
	reporter.report_fd = server->log_pipe[1];
	create_pid_file(server, &reporter);
	if (reporter.critical)
	{
		cleanup(server, false, &reporter);
		return (1);
	}
	strcpy(reporter.buffer, "INFO: Starting taskmasterd\n");
	report(&reporter, false);
	if (!server->priorities)
	{
		strcpy(reporter.buffer, "DEBUG: No priorities to start\n");
		report(&reporter, false);
	}
	else
	{
		strcpy(reporter.buffer, "DEBUG: Launching priorities\n");
		report(&reporter, false);
		launch(server->priorities, server->log_pipe[1]);
	}
	return (main_routine(server, &reporter));
}
