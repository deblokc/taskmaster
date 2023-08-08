#include "taskmaster.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/epoll.h>

static bool	should_log(enum log_level loglevel, char* line)
{
	if (loglevel == DEBUG)
		return (true);
	else if (loglevel == INFO && strncmp(line, "DEBUG", 5))
		return (true);
	else if (loglevel == WARN && strncmp(line, "DEBUG", 5) && strncmp(line, "INFO", 4))
		return (true);
	else if (loglevel == ERROR && (!strncmp(line, "ERROR", 5) || !strncmp(line, "CRITICAL", 8)))
		return (true);
	else if (loglevel == CRITICAL && !strncmp(line, "CRITICAL", 8))
		return (true);
	return (false);
}


static bool	rotate_log(struct s_logger *logger)
{
	char	old_name[256];
	char	new_name[256];

	close(logger->logfd);
	logger->logfd = -1;
	if (logger->logfile_backups == 0)
	{
		if ((logger->logfd = open(logger->logfile, O_TRUNC | O_RDWR, 0666 & ~logger->umask)) == -1)
			return (false);
		return (true);
	}
	snprintf(old_name, 256, "%s%d", logger->logfile, logger->logfile_backups);
	remove(old_name);
	for (int i = logger->logfile_backups - 1; i > 0; --i)
	{
		snprintf(old_name, 256, "%s%d", logger->logfile, i);
		snprintf(new_name, 256, "%s%d", logger->logfile, i + 1);
		rename(old_name, new_name);
	}
	snprintf(old_name, 256, "%s", logger->logfile);
	snprintf(new_name, 256, "%s%d", logger->logfile, 1);
	rename(old_name, new_name);
	if ((logger->logfd = open(logger->logfile, O_CREAT | O_TRUNC | O_RDWR, 0666 & ~logger->umask)) == -1)
			return (false);
	return (true);
}

bool	write_log(struct s_logger *logger, char* log_string)
{
	size_t		len;
	struct stat	statbuf;

	if ((size_t)logger->logfile_maxbytes < 22)
		return (true);
	if (logger->logfd < 0 && (logger->logfd = open(logger->logfile, O_CREAT | O_APPEND | O_RDWR, 0666 & ~logger->umask)) == -1)
		return (false);
	len = strlen(log_string);
	if ((size_t)logger->logfile_maxbytes < len)
		return (true);
	if (fstat(logger->logfd, &statbuf) == -1)
	{
		close(logger->logfd);
		if ((logger->logfd = open(logger->logfile, O_CREAT | O_APPEND | O_RDWR, 0666 & ~logger->umask)) == -1)
			return (false);
		if (fstat(logger->logfd, &statbuf) == -1)
			return (false);
	}
	if ((statbuf.st_size + (off_t)len) > (off_t)logger->logfile_maxbytes)
	{
		if (!rotate_log(logger))
			return (false);
	}
	if (write(logger->logfd, log_string, len) == -1)
		return (false);
	return (true);
}

bool	write_process_log(struct s_logger *logger, char* log_string)
{
	off_t		remainder;
	size_t		len;
	struct stat	statbuf;

	if ((size_t)logger->logfile_maxbytes == 0)
		return (true);
	if (logger->logfd < 0 && (logger->logfd = open(logger->logfile, O_CREAT | O_APPEND | O_RDWR, 0666 & ~logger->umask)) == -1)
		return (false);
	len = strlen(log_string);
	if (fstat(logger->logfd, &statbuf) == -1)
	{
		close(logger->logfd);
		if ((logger->logfd = open(logger->logfile, O_CREAT | O_APPEND | O_RDWR, 0666 & ~logger->umask)) == -1)
			return (false);
		if (fstat(logger->logfd, &statbuf) == -1)
			return (false);
	}
	if (statbuf.st_size + (off_t)len > (off_t)logger->logfile_maxbytes)
	{
		if (statbuf.st_size > logger->logfile_maxbytes)
		{
			if (!rotate_log(logger))
				return (false);
			return (write_process_log(logger, log_string));
		}
		remainder = logger->logfile_maxbytes - statbuf.st_size;
		if (write(logger->logfd, log_string, (size_t)remainder) == -1)
			return (false);
		if (!rotate_log(logger))
			return (false);
		return (write_process_log(logger, &log_string[remainder]));
	}
	else
	{
		if (write(logger->logfd, log_string, len) == -1)
			return (false);
	}
	return (true);
}

void	transfer_logs(int tmp_fd, struct s_server *server)
{
	bool	ret = true;
	char*	line = NULL;

	if (lseek(tmp_fd, 0, SEEK_SET) == -1)
	{
		if (write(2, "CRITICAL: Could not read temporary log\n", strlen("CRITICAL: Could not read temporary log\n"))) {}
		return ;
	}
	while ((line = get_next_line(tmp_fd)) != NULL)
	{
		if (strlen(line) > 22)
		{
			if (server->loglevel == DEBUG)
			{
				if (!write_log(&server->logger, line))
					ret = false;
				if (server->daemon == false)
				{
					if (write(2, line, strlen(line))) {}
				}
			}
			else if (server->loglevel == INFO && strncmp(&line[22], "DEBUG", 5))
			{
				if (!write_log(&server->logger, line))
					ret = false;
				if (server->daemon == false)
				{
					if (write(2, line, strlen(line))) {}
				}
			}
			else if (server->loglevel == WARN && strncmp(&line[22], "DEBUG", 5) && strncmp(&line[22], "INFO", 4))
			{
				if (!write_log(&server->logger, line))
					ret = false;
				if (server->daemon == false)
				{
					if (write(2, line, strlen(line))) {}
				}
			}
			else if (server->loglevel == ERROR && (!strncmp(&line[22], "ERROR", 5) || !strncmp(&line[22], "CRITICAL", 8)))
			{
				if (!write_log(&server->logger, line))
					ret = false;
				if (server->daemon == false)
				{
					if (write(2, line, strlen(line))) {}
				}
			}
			else if (server->loglevel == CRITICAL && !strncmp(&line[22], "CRITICAL", 8))
			{
				if (!write_log(&server->logger, line))
					ret = false;
				if (server->daemon == false)
				{
					if (write(2, line, strlen(line))) {}
				}
			}
		}
		free(line);
	}
	if (ret == false && server->daemon == false)
	{
		if (write(2, "ERROR: Error while writing to logfile\n", strlen("ERROR: Error while writing to logfile\n")) == -1) {}
	}
	close(tmp_fd);
	remove("/tmp/.taskmasterd_tmp.log");
}

void	*main_logger(void *void_server)
{
	struct s_server		*server;
	int					nb_events;
	int					epoll_fd;
	ssize_t				ret;
	char				buffer[PIPE_BUF + 1];
	struct epoll_event	event;

	server = (struct s_server*)void_server;
	bzero(&event, sizeof(event));
	epoll_fd = epoll_create(1);
	if (epoll_fd == -1)
	{
		get_stamp(buffer);
		strcpy(&buffer[22], "CRITICAL: Could not instantiate epoll, exiting process\n");
		write_log(&server->logger, buffer);
		if (!server->daemon)
		{
			if (write(2, buffer, strlen(buffer)) == -1) {}
		}
		pthread_exit(NULL);
	}
	event.data.fd = server->log_pipe[0];
	event.events = EPOLLIN;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server->log_pipe[0], &event))
	{
		get_stamp(buffer);
		strcpy(&buffer[22], "CRITICAL: Could not add pipe to epoll events, exiting process\n");
		write_log(&server->logger, buffer);
		if (!server->daemon)
		{
			if (write(2, buffer, strlen(buffer)) == -1) {}
		}
		close(epoll_fd);
		pthread_exit(NULL);
	}
	get_stamp(buffer);
	strcpy(&buffer[22], "DEBUG: Ready for logging\n");
	if (should_log(server->loglevel, &buffer[22]))
	{
		write_log(&server->logger, buffer);
		if (!server->daemon)
		{
			if (write(2, buffer, strlen(buffer)) == -1) {}
		}
	}
	while (true)
	{
		if ((nb_events = epoll_wait(epoll_fd, &event, 1, 100000)) == -1)
		{
			if (errno == EINTR)
				continue ;
			get_stamp(buffer);
			strcpy(&buffer[22], "CRITICAL: Error while waiting for epoll event, exiting process\n");
			write_log(&server->logger, buffer);
			if (!server->daemon)
			{
				if (write(2, buffer, strlen(buffer)) == -1) {}
			}
			event.data.fd = server->log_pipe[0];
			event.events = EPOLLIN;
			epoll_ctl(epoll_fd, EPOLL_CTL_DEL, server->log_pipe[0], &event);
			close(epoll_fd);
			pthread_exit(NULL);
		}
		if (nb_events)
		{
			get_stamp(buffer);
			ret = read(server->log_pipe[0], &buffer[22], PIPE_BUF - 22);
			if (ret > 0)
			{
				buffer[ret + 22] = '\0';
				if (!strncmp("ENDLOG\n", &buffer[22], 7))
				{
					event.data.fd = server->log_pipe[0];
					event.events = EPOLLIN;
					epoll_ctl(epoll_fd, EPOLL_CTL_DEL, server->log_pipe[0], &event);
					close(epoll_fd);
					get_stamp(buffer);
					strcpy(&buffer[22], "DEBUG: Terminating logging thread\n");
					if (should_log(server->loglevel, &buffer[22]))
					{
						write_log(&server->logger, buffer);
						if (!server->daemon)
						{
							if (write(2, buffer, strlen(buffer)) == -1) {}
						}
					}
					pthread_exit(NULL);
				}
				else
				{
					if (should_log(server->loglevel, &buffer[22]))
					{
						write_log(&server->logger, buffer);
						if (!server->daemon)
						{
							if (write(2, buffer, (size_t)ret + 22) == -1) {}
						}
					}
				}
			}
		}
	}
	return (NULL);
}

bool	start_logging_thread(struct s_server *server, bool daemonized)
{
	struct s_report	reporter;

	if (pipe2(server->log_pipe, O_DIRECT | O_NONBLOCK) == -1)
	{
		get_stamp(reporter.buffer);
		strcpy(&reporter.buffer[22], "CRITICAL: Could not open pipe for logging\n");
		if (!daemonized && write(2, reporter.buffer, strlen(reporter.buffer)) <= 0)
		{
		}
		write_log(&server->logger, reporter.buffer);
		return (false);
	}
	if (pthread_create(&server->logging_thread, NULL, main_logger, server))
	{
		get_stamp(reporter.buffer);
		strcpy(&reporter.buffer[22], "CRITICAL: Could not initiate logging thread\n");
		if (!daemonized && write(2, reporter.buffer, strlen(reporter.buffer)) <= 0)
		{
		}
		write_log(&server->logger, reporter.buffer);
		return (false);
	}
	return (true);
}
