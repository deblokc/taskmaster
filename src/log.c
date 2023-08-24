#include "taskmaster.h"
#include "curl.h"
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

bool	transfer_logs(int tmp_fd, char tmp_log_file[1024], struct s_server *server, struct s_report *reporter)
{
	bool	ret = true;
	char*	line = NULL;

	if (lseek(tmp_fd, 0, SEEK_SET) == -1)
	{
		get_stamp(reporter->buffer);
		strcpy(&reporter->buffer[22], "CRITICAL: Could not read temporary log, exiting process\n");
		return (false);
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
		get_stamp(reporter->buffer);
		strcpy(&reporter->buffer[22], "ERROR: Error while writing to logfile, initial logs might be incomplete\n");
		if (write(2, reporter->buffer, strlen(reporter->buffer))) {}
	}
	close(tmp_fd);
	remove(tmp_log_file);
	return (true);
}

static void	curl_cleanup(struct curl_slist *slist, CURL *handle)
{
	curl_slist_free_all(slist);
	curl_easy_cleanup(handle);
}

static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	(void)ptr;
	(void)size;
	(void)userdata;
	return (nmemb);
}

static bool	register_curl_opt(CURL *handle, char *channel, struct curl_slist *slist, char *data)
{
	return (curl_easy_setopt(handle, CURLOPT_URL, channel) == CURLE_OK
		&& curl_easy_setopt(handle, CURLOPT_POST, 1) == CURLE_OK
		&& curl_easy_setopt(handle, CURLOPT_FAILONERROR, 1L) == CURLE_OK
		&& curl_easy_setopt(handle, CURLOPT_TIMEOUT, 3L) == CURLE_OK
		&& curl_easy_setopt(handle, CURLOPT_POSTFIELDS, data) == CURLE_OK
		&& curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_callback) == CURLE_OK
		&& curl_easy_setopt(handle, CURLOPT_HTTPHEADER, slist) == CURLE_OK);
}

static CURL	*init_curl(struct curl_slist **slist, char *token)
{
	char				buf[PIPE_BUF];
	CURL				*handle = NULL;
	struct curl_slist	*tmp = NULL;

	bzero(buf, PIPE_BUF);

	// Get handle for CURL
	handle = curl_easy_init();
	if (!handle)
		return (NULL);

	// Prepare HTTP headers
	snprintf(buf, PIPE_BUF, "Authorization: Bot %s", token);
	*slist = curl_slist_append(*slist, "Content-Type: application/json");
	if (!*slist)
	{
		curl_easy_cleanup(handle);
		return (NULL);
	}
	tmp = curl_slist_append(*slist, buf);
	if (!tmp)
	{
		curl_cleanup(*slist, handle);
		return (NULL);
	}
	*slist = tmp;
	return (handle);
}

void	log_discord(struct s_discord_logger *discord_logger, char *data)
{
	bool		has_error = false;
	char		payload[PIPE_BUF + 15];		
	static int	failed_attempts = 0;

	data[strlen(data) - 1] = '\0';
	snprintf(payload, PIPE_BUF + 15, "{\"content\": \"%s\"}", data);
	if (!register_curl_opt(discord_logger->handle, discord_logger->channel, discord_logger->slist, payload))
	{
		++failed_attempts;
		strcpy(discord_logger->reporter.buffer, "CRITICAL: Could not register curl options, message will be discarded\n");
		report(&discord_logger->reporter, false);
		has_error = true;
	}
	else
	{
		for (int i = 0; i < 10; ++i)
		{
			if (curl_easy_perform(discord_logger->handle) == CURLE_OK)
			{
				has_error = false;
				break ;
			}
			has_error = true;
			usleep(500);
		}
		if (has_error)
		{
			++failed_attempts;
			strcpy(discord_logger->reporter.buffer, "CRITICAL: Could not log to discord, message will be discarded\n");
			report(&discord_logger->reporter, false);
		}
		else
			failed_attempts = 0;
	}
	if (failed_attempts > 10)
	{
		strcpy(discord_logger->reporter.buffer, "CRITICAL: Logging to discord failed too many times, disabling feature\n");
		report(&discord_logger->reporter, false);
		discord_logger->logging = false;
	}
}

void	*discord_logger_thread(void *void_discord_logger)
{
	struct s_discord_logger	*discord_logger;
	int						nb_events;
	int						epoll_fd;
	ssize_t					ret;
	char					buffer[PIPE_BUF + 1];
	struct epoll_event		event;

	discord_logger = (struct s_discord_logger*)void_discord_logger;
	bzero(&event, sizeof(event));
	epoll_fd = epoll_create(1);
	if (epoll_fd == -1)
	{
		discord_logger->logging = false;
		strcpy(discord_logger->reporter.buffer, "CRITICAL: Could not instantiate epoll, exiting discord logging thread\n");
		report(&discord_logger->reporter, true);
		pthread_exit(NULL);
	}
	event.data.fd = discord_logger->com[0];
	event.events = EPOLLIN;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, discord_logger->com[0], &event))
	{
		discord_logger->logging = false;
		strcpy(discord_logger->reporter.buffer, "CRITICAL: Could not add pipe to epoll events in Discord logging thread\n");
		report(&discord_logger->reporter, true);
		close(epoll_fd);
		pthread_exit(NULL);
	}
	strcpy(discord_logger->reporter.buffer, "DEBUG: Discord thread ready for logging\n");
	report(&discord_logger->reporter, false);
	while (true)
	{
		if ((nb_events = epoll_wait(epoll_fd, &event, 1, 100000)) == -1)
		{
			if (errno == EINTR)
				continue ;
			discord_logger->logging = false;
			strcpy(discord_logger->reporter.buffer, "CRITICAL: Error while waiting for epoll event in Discord logging thread\n");
			report(&discord_logger->reporter, true);
			event.events = EPOLLIN;
			epoll_ctl(epoll_fd, EPOLL_CTL_DEL, discord_logger->com[0], &event);
			close(epoll_fd);
			pthread_exit(NULL);
		}
		if (nb_events)
		{
			ret = read(discord_logger->com[0], buffer, PIPE_BUF);
			if (ret > 0)
			{
				buffer[ret] = '\0';
				if (!strncmp("ENDLOG\n", buffer, 7))
				{
					event.data.fd = discord_logger->com[0];
					event.events = EPOLLIN;
					epoll_ctl(epoll_fd, EPOLL_CTL_DEL, discord_logger->com[0], &event);
					close(epoll_fd);
					pthread_exit(NULL);
				}
				else if (discord_logger->logging)
					log_discord(discord_logger, buffer);
			}
		}
	}
	return (NULL);
}

void	*main_logger(void *void_server)
{
	struct s_report			reporter;
	struct s_discord_logger	discord_logger;
	struct s_server			*server;
	pthread_t				discord_thread;
	int						nb_events;
	int						epoll_fd;
	ssize_t					ret;
	struct epoll_event		event;

	server = (struct s_server*)void_server;
	bzero(&discord_logger, sizeof(discord_logger));
	bzero(&reporter, sizeof(reporter));
	discord_logger.logging = false;
	discord_logger.running = false;
	if (server->log_discord)
	{
		snprintf(discord_logger.channel, PIPE_BUF, "https://discord.com/api/channels/%s/messages", server->discord_channel);
		discord_logger.handle = init_curl(&discord_logger.slist, server->discord_token);
		if (!discord_logger.handle)
		{
			get_stamp(reporter.buffer);
			strcpy(&reporter.buffer[22], "CRITICAL: Could not instantiate curl, discord logging will be disabled\n");
			write_log(&server->logger, reporter.buffer);
			if (!server->daemon)
			{
				if (write(2, reporter.buffer, strlen(reporter.buffer)) == -1) {}
			}
			discord_logger.logging = false;
		}
		else
		{
			discord_logger.loglevel = server->loglevel_discord;
			discord_logger.logging = true;
			discord_logger.reporter.report_fd = server->log_pipe[1];
		}
	}
	if (discord_logger.logging)
	{
		if (pipe2(discord_logger.com, O_DIRECT | O_NONBLOCK) == -1)
		{
			get_stamp(reporter.buffer);
			strcpy(&reporter.buffer[22], "CRITICAL: Could not open pipe to communicate with discord logging thread\n");
			if (!server->daemon && write(2, reporter.buffer, strlen(reporter.buffer)) <= 0)
			{
			}
			write_log(&server->logger, reporter.buffer);
			discord_logger.logging = false;
			curl_cleanup(discord_logger.slist, discord_logger.handle);
		}
		else if (pthread_create(&discord_thread, NULL, discord_logger_thread, &discord_logger))
		{
			get_stamp(reporter.buffer);
			strcpy(&reporter.buffer[22], "CRITICAL: Could not initiate discord logging thread\n");
			if (!server->daemon && write(2, reporter.buffer, strlen(reporter.buffer)) <= 0)
			{
			}
			write_log(&server->logger, reporter.buffer);
			discord_logger.logging = false;
			curl_cleanup(discord_logger.slist, discord_logger.handle);
			close(discord_logger.com[0]);
			close(discord_logger.com[1]);
		}
		else
		{
			reporter.report_fd = discord_logger.com[1];
			discord_logger.running = true;
		}
	}
	bzero(&event, sizeof(event));
	epoll_fd = epoll_create(1);
	if (epoll_fd == -1)
	{
		get_stamp(reporter.buffer);
		strcpy(&reporter.buffer[22], "CRITICAL: Could not instantiate epoll, exiting process\n");
		write_log(&server->logger, reporter.buffer);
		if (!server->daemon)
		{
			if (write(2, reporter.buffer, strlen(reporter.buffer)) == -1) {}
		}
		if (discord_logger.logging)
			report(&reporter, false);
		if (discord_logger.running)
		{
			strcpy(reporter.buffer, "ENDLOG\n");
			if (report(&reporter, false))
				pthread_join(discord_thread, NULL);
			curl_cleanup(discord_logger.slist, discord_logger.handle);
			close(discord_logger.com[0]);
			close(discord_logger.com[1]);
		}
		pthread_exit(NULL);
	}
	event.data.fd = server->log_pipe[0];
	event.events = EPOLLIN;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server->log_pipe[0], &event))
	{
		get_stamp(reporter.buffer);
		strcpy(&reporter.buffer[22], "CRITICAL: Could not add pipe to epoll events, exiting process\n");
		write_log(&server->logger, reporter.buffer);
		if (!server->daemon)
		{
			if (write(2, reporter.buffer, strlen(reporter.buffer)) == -1) {}
		}
		if (discord_logger.logging)
			report(&reporter, false);
		if (discord_logger.running)
		{
			strcpy(reporter.buffer, "ENDLOG\n");
			if (report(&reporter, false))
				pthread_join(discord_thread, NULL);
			curl_cleanup(discord_logger.slist, discord_logger.handle);
			close(discord_logger.com[0]);
			close(discord_logger.com[1]);
		}
		close(epoll_fd);
		pthread_exit(NULL);
	}
	get_stamp(reporter.buffer);
	strcpy(&reporter.buffer[22], "DEBUG: Ready for logging\n");
	if (should_log(server->loglevel, &reporter.buffer[22]))
	{
		write_log(&server->logger, reporter.buffer);
		if (!server->daemon)
		{
			if (write(2, reporter.buffer, strlen(reporter.buffer)) == -1) {}
		}
	}
	if (discord_logger.logging && should_log(discord_logger.loglevel, &reporter.buffer[22]))
		report(&reporter, false);
	while (true)
	{
		if ((nb_events = epoll_wait(epoll_fd, &event, 1, 100000)) == -1)
		{
			if (errno == EINTR)
				continue ;
			get_stamp(reporter.buffer);
			strcpy(&reporter.buffer[22], "CRITICAL: Error while waiting for epoll event, exiting process\n");
			write_log(&server->logger, reporter.buffer);
			if (!server->daemon)
			{
				if (write(2, reporter.buffer, strlen(reporter.buffer)) == -1) {}
			}
			if (discord_logger.logging)
				report(&reporter, false);
			if (discord_logger.running)
			{
				strcpy(reporter.buffer, "ENDLOG\n");
				if (report(&reporter, false))
					pthread_join(discord_thread, NULL);
				curl_cleanup(discord_logger.slist, discord_logger.handle);
				close(discord_logger.com[0]);
				close(discord_logger.com[1]);
			}
			event.data.fd = server->log_pipe[0];
			event.events = EPOLLIN;
			epoll_ctl(epoll_fd, EPOLL_CTL_DEL, server->log_pipe[0], &event);
			close(epoll_fd);
			pthread_exit(NULL);
		}
		if (nb_events)
		{
			get_stamp(reporter.buffer);
			ret = read(server->log_pipe[0], &reporter.buffer[22], PIPE_BUF - 22);
			if (ret > 0)
			{
				reporter.buffer[ret + 22] = '\0';
				if (!strncmp("ENDLOG\n", &reporter.buffer[22], 7))
				{
					if (discord_logger.running)
					{
						get_stamp(reporter.buffer);
						strcpy(&reporter.buffer[22], "DEBUG: Asking Discord logging thread to terminate\n");
						if (should_log(server->loglevel, &reporter.buffer[22]))
						{
							write_log(&server->logger, reporter.buffer);
							if (!server->daemon)
							{
								if (write(2, reporter.buffer, strlen(reporter.buffer)) == -1) {}
							}
						}
						strcpy(reporter.buffer, "ENDLOG\n");
						if (report(&reporter, false))
						{
							get_stamp(reporter.buffer);
							strcpy(&reporter.buffer[22], "DEBUG: Joining Discord logging thread to terminate\n");
							if (should_log(server->loglevel, &reporter.buffer[22]))
							{
								write_log(&server->logger, reporter.buffer);
								if (!server->daemon)
								{
									if (write(2, reporter.buffer, strlen(reporter.buffer)) == -1) {}
								}
							}
							if (pthread_join(discord_thread, NULL))
							{
								get_stamp(reporter.buffer);
								strcpy(&reporter.buffer[22], "CRITICAL: Could not join Discord logging thread to terminate\n");
								if (should_log(server->loglevel, &reporter.buffer[22]))
								{
									write_log(&server->logger, reporter.buffer);
									if (!server->daemon)
									{
										if (write(2, reporter.buffer, strlen(reporter.buffer)) == -1) {}
									}
								}
							}
							else
							{
								get_stamp(reporter.buffer);
								ret = read(server->log_pipe[0], &reporter.buffer[22], PIPE_BUF - 22);
								while (ret > 0)
								{
									reporter.buffer[ret + 22] = '\0';
									if (should_log(server->loglevel, &reporter.buffer[22]))
									{
										write_log(&server->logger, reporter.buffer);
										if (!server->daemon)
										{
											if (write(2, reporter.buffer, strlen(reporter.buffer)) == -1) {}
										}
									}
									get_stamp(reporter.buffer);
									ret = read(server->log_pipe[0], &reporter.buffer[22], PIPE_BUF - 22);
								}
								get_stamp(reporter.buffer);
								strcpy(&reporter.buffer[22], "DEBUG: Successfully joined Discord logging thread\n");
								if (should_log(server->loglevel, &reporter.buffer[22]))
								{
									write_log(&server->logger, reporter.buffer);
									if (!server->daemon)
									{
										if (write(2, reporter.buffer, strlen(reporter.buffer)) == -1) {}
									}
								}
							}
						}
						else
						{
							get_stamp(reporter.buffer);
							strcpy(&reporter.buffer[22], "CRITICAL: Could not join discord thread\n");
							write_log(&server->logger, reporter.buffer);
						}
						curl_cleanup(discord_logger.slist, discord_logger.handle);
						close(discord_logger.com[0]);
						close(discord_logger.com[1]);
					}
					event.data.fd = server->log_pipe[0];
					event.events = EPOLLIN;
					epoll_ctl(epoll_fd, EPOLL_CTL_DEL, server->log_pipe[0], &event);
					close(epoll_fd);
					get_stamp(reporter.buffer);
					strcpy(&reporter.buffer[22], "DEBUG: Terminating logging thread\n");
					if (should_log(server->loglevel, &reporter.buffer[22]))
					{
						write_log(&server->logger, reporter.buffer);
						if (!server->daemon)
						{
							if (write(2, reporter.buffer, strlen(reporter.buffer)) == -1) {}
						}
					}
					pthread_exit(NULL);
				}
				else
				{
					if (should_log(server->loglevel, &reporter.buffer[22]))
					{
						write_log(&server->logger, reporter.buffer);
						if (!server->daemon)
						{
							if (write(2, reporter.buffer, (size_t)ret + 22) == -1) {}
						}
					}
					if (discord_logger.logging && should_log(discord_logger.loglevel, &reporter.buffer[22]))
						report(&reporter, false);
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

bool end_initial_log(struct s_report *reporter, void **thread_ret, pthread_t initial_logger)
{
	strcpy(reporter->buffer, "ENDLOG\n");
	if (!report(reporter, false))
	{
		get_stamp(reporter->buffer);
		strcpy(&reporter->buffer[22], "CRITICAL: Could not stop initial logger, exiting process\n");
		return (false);
	}
	if (pthread_join(initial_logger, thread_ret))
	{
		get_stamp(reporter->buffer);
		strcpy(&reporter->buffer[22], "CRITICAL: Could not join initial logger, exiting process\n");
		return (false);
	}
	return (true);
}

bool end_logging_thread(struct s_report *reporter, pthread_t logger)
{
	strcpy(reporter->buffer, "ENDLOG\n");
	if (!report(reporter, false))
	{
		get_stamp(reporter->buffer);
		strcpy(&reporter->buffer[22], "CRITICAL: Could not stop logging thread, exiting process\n");
		return (false);
	}
	if (pthread_join(logger, NULL))
	{
		get_stamp(reporter->buffer);
		strcpy(&reporter->buffer[22], "CRITICAL: Could not join logging thread, exiting process\n");
		return (false);
	}
	return (true);
}
