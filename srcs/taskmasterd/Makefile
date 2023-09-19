# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: tnaton <marvin@42.fr>                      +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2023/06/16 11:17:59 by tnaton            #+#    #+#              #
#    Updated: 2023/09/19 16:40:57 by tnaton           ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

vpath %.h inc

NAME = ../../taskmasterd

OBJDIR := obj

SRCDIR := src

SRCS = main.c \
	   parser/parser.c \
	   parser/server.c \
	   parser/socket.c \
	   parser/logger.c \
	   parser/program.c \
	   parser/tree.c \
	   parser/utils.c \
	   parser/priorities.c \
	   parser/free_errors.c \
	   gnl/get_next_line.c \
	   gnl/get_next_line_utils.c \
	   signal.c \
	   report.c \
	   prelude_server.c \
	   log.c \
	   daemon.c \
	   administrator.c \
	   server.c \
	   launcher.c \
	   update.c

YAML = ../../libs/libyaml.a

YAML_H = inc/yaml.h

SCRYPT = ../../libs/libscrypt.a

SCRYPT_H = inc/libscrypt.h

CURL = ../../libs/libcurl.a

CURL_H = 

INC = taskmaster.h

MOREFLAGS = -Wformat=2				\
			-Wformat-overflow=2		\
			-Wformat-truncation=2	\
			-Wstringop-overflow=4	\
			-Winit-self				\
			-ftrapv					\
			-Wdate-time				\
			-Wconversion			\
			-Wshadow

#	-Wformat=2						Check format when call to printf/scanf...
#	-Wformat-overflow=2				Check overflow of buffer with sprintf/vsprintf
#	-Wformat-truncation=2			Check output truncation with snprintf/vsnprintf
#	-Wstringop-overflow=4			Check overflow when using memcpy and strcpy (which should not happen for obvious reason)
#	-Winit-self						Check variable which initialise themself /* int i; i = i; */
#	-ftrapv							Trap signed overflow for + - * 
#	-Wdate-time						Warn if __TIME__ __DATE or __TIMESTAMP__ are encoutered to prevent bit-wise-identical compilation

CFLAGS = -Wall -Wextra -Werror -Wpedantic -O3 -g $(MOREFLAGS)

CC = gcc

OBJS := $(patsubst %.c,$(OBJDIR)/%.o,$(SRCS))

$(NAME) : $(SCRYPT) $(YAML) $(if $(DISCORD), $(CURL)) $(OBJS) $(INC)
	test -d ../../libs/curl-8.2.1 || (cd ../../libs && tar -xvf curl-8.2.1.tar.gz)
	test -f ../../libs/libcurl.so || ((test -f ../../libs/curl-8.2.1/lib/.libs/libcurl.so.4.8.0 || (cd ../../libs/curl-8.2.1; ./configure --with-openssl --disable-debug --enable-static --disable-ftp --disable-file --disable-ldap --disable-ldaps --disable-rtsp --disable-proxy --disable-dict --disable-telnet --disable-tftp --disable-pop3 --disable-imap --disable-smb --disable-smtp --disable-gopher --disable-mqtt --disable-ipv6 --disable-threaded-resolver --disable-pthreads --disable-sspi --disable-crypto-auth --disable-ntlm --disable-ntlm-wb --disable-tls-srp --disable-unix-sockets --disable-cookies --disable-socketpair --disable-http-auth --disable-doh --disable-dateparse --disable-netrc --disable-progress-meter --disable-dnsshuffle --disable-alt-svc --disable-hsts --disable-websockets --without-hyper --without-zlib --without-brotli --without-libidn2 --without-nghttp2 --without-nghttp3 --without-quiche --without-msh3; make;)) && rm -f ../../libs/libcurl.so; ln -s $(PWD)/libs/curl-8.2.1/lib/.libs/libcurl.so.4.8.0 ../../libs/libcurl.so)
ifndef DISCORD
	$(CC) $(CFLAGS) $(OBJS) $(SCRYPT) $(YAML) -pthread -L ../../libs -lcurl -o $@
else
	$(CC) -D DISCORD $(CFLAGS) $(OBJS) $(SCRYPT) $(YAML) -pthread -L ../../libs -lcurl -o $@
endif

$(OBJS): $(INC) $(SCRYPT_H) $(YAML_H) $(if $(DISCORD), $(CURL_H))

$(OBJS) : | $(OBJDIR)

#
# CREATING LIBS AND LIBS HEADERS
#

$(SCRYPT):
	test -d ../../libs/libscrypt-1.22 || (cd ../../libs && tar -xvf libscrypt-1.22.tar.gz)
	(test -f ../../libs/libscrypt-1.22/libscrypt.a || (cd ../../libs/libscrypt-1.22; make;)) && rm -f $@; ln -s $(PWD)/libs/libscrypt-1.22/libscrypt.a $@

$(SCRYPT_H):
	test -d ../../libs/libscrypt-1.22 || (cd ../../libs && tar -xvf libscrypt-1.22.tar.gz)
	cp ../../libs/libscrypt-1.22/libscrypt.h inc/libscrypt.h	

$(YAML):
	test -d ../../libs/yaml-0.2.5 || (cd ../../libs && tar -xvf libyaml.tar.gz)
	(test -f ../../libs/yaml-0.2.5/src/.libs/libyaml.a || (cd ../../libs/yaml-0.2.5; ./configure; make;)) && rm -f $@; ln -s $(PWD)/libs/yaml-0.2.5/src/.libs/libyaml.a $@

$(YAML_H):
	test -d ../../libs/yaml-0.2.5 || (cd ../../libs && tar -xvf libyaml.tar.gz)
	cp ../../libs/yaml-0.2.5/include/yaml.h inc/yaml.h

$(CURL):
	test -d ../../libs/curl-8.2.1 || (cd ../../libs && tar -xvf curl-8.2.1.tar.gz)
	(test -f ../../libs/curl-8.2.1/lib/.libs/libcurl.a || (cd ../../libs/curl-8.2.1; ./configure --with-openssl --disable-debug --enable-static --disable-ftp --disable-file --disable-ldap --disable-ldaps --disable-rtsp --disable-proxy --disable-dict --disable-telnet --disable-tftp --disable-pop3 --disable-imap --disable-smb --disable-smtp --disable-gopher --disable-mqtt --disable-ipv6 --disable-threaded-resolver --disable-pthreads --disable-sspi --disable-crypto-auth --disable-ntlm --disable-ntlm-wb --disable-tls-srp --disable-unix-sockets --disable-cookies --disable-socketpair --disable-http-auth --disable-doh --disable-dateparse --disable-netrc --disable-progress-meter --disable-dnsshuffle --disable-alt-svc --disable-hsts --disable-websockets --without-hyper --without-zlib --without-brotli --without-libidn2 --without-nghttp2 --without-nghttp3 --without-quiche --without-msh3; make;)) && rm -f $@; ln -s $(PWD)/libs/curl-8.2.1/lib/.libs/libcurl.a $@

$(CURL_H):

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	test -d ../../libs/curl-8.2.1 || (cd ../../libs && tar -xvf curl-8.2.1.tar.gz)
	$(CC) $(CFLAGS) -I inc -I ../../libs/curl-8.2.1/include/curl -o $@ -c $<

$(OBJDIR) :
	mkdir -p $(OBJDIR)/parser $(OBJDIR)/gnl

.SECONDARY: $(OBJS)

.PHONY: all
all : $(NAME)

.PHONY: clean
clean : 
	rm -rf $(OBJS)

.PHONY: fclean
fclean:
	rm -rf $(NAME) $(OBJS) $(OBJDIR)

.PHONY: re
re: fclean all
