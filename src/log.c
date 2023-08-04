#include "taskmaster.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <stdio.h>

static bool	rotate_log(struct s_logger *logger)
{
	char	old_name[256];
	char	new_name[256];

	close(logger->logfd);
	logger->logfd = -1;
	if (logger->logfile_backups == 0)
	{
		if ((logger->logfd = open(logger->logfile, O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP)) == -1)
			return (false);
		return (true);
	}
	snprintf(old_name, PIPE_BUF, "%s%d", logger->logfile, logger->logfile_backups);
	remove(old_name);
	for (int i = logger->logfile_backups - 1; i > 0; --i)
	{
		snprintf(old_name, PIPE_BUF, "%s%d", logger->logfile, i);
		snprintf(new_name, PIPE_BUF, "%s%d", logger->logfile, i + 1);
		rename(old_name, new_name);
	}
	snprintf(old_name, PIPE_BUF, "%s", logger->logfile);
	snprintf(new_name, PIPE_BUF, "%s%d", logger->logfile, 1);
	rename(old_name, new_name);
	if ((logger->logfd = open(logger->logfile, O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP)) == -1)
			return (false);
	return (true);
}

bool	write_log(struct s_logger *logger, char* log_string)
{
	size_t		len;
	struct stat	statbuf;

	if ((size_t)logger->logfile_maxbytes < 22)
		return (true);
	len = strlen(log_string);
	if ((size_t)logger->logfile_maxbytes < len)
		return (true);
	if (fstat(logger->logfd, &statbuf) == -1)
		return (false);
	if ((statbuf.st_size + (off_t)len) > (off_t)logger->logfile_maxbytes)
	{
		if (!rotate_log(logger))
			return (false);
	}
	if (write(logger->logfd, log_string, len) == -1)
		return (false);
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
