/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   discord.c                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tnaton <marvin@42.fr>                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/07/18 18:34:20 by tnaton            #+#    #+#             */
/*   Updated: 2023/07/25 14:02:51 by tnaton           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <stdio.h>
#include <stdlib.h>
#include "curl/curl.h"

int main(int ac, char **av) {
	if (ac != 2) {
		printf("Need arg to send\n");
		exit(1);
	}

	char *taskmaster = "TOKEN";
	(void)taskmaster;

	char *message = av[1];

	CURL *handle = curl_easy_init();
	struct curl_slist *slist = NULL;

	if (handle) {
		CURLcode res;
		curl_easy_setopt(handle, CURLOPT_URL, "https://discord.com/api/channels/1130887675730206741/messages");
		curl_easy_setopt(handle, CURLOPT_POST, 1); // make it a post
		curl_easy_setopt(handle, CURLOPT_POSTFIELDS, message);
		slist = curl_slist_append(slist, "Content-Type: application/json");
		slist = curl_slist_append(slist, "Authorization: Bot {TOKEN}");
		curl_easy_setopt(handle, CURLOPT_HTTPHEADER, slist);
		printf("SENDING >%s<\n", message);
		res = curl_easy_perform(handle);
		printf("RESULT : %d\n", res);
		curl_slist_free_all(slist);
	} else {
		printf("HANDLE BROKEN ???\n");
	}
	curl_easy_cleanup(handle);
	return (0);
}
