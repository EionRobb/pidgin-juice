/*
 * Release Notification Plugin
 *
 * Copyright (C) 2003, Nathan Walp <faceprint@faceprint.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02111-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef PURPLE_PLUGINS
#define PURPLE_PLUGINS
#endif

#include "internal.h"

#include <string.h>

#include "connection.h"
#include "core.h"
#include "debug.h"
#include "gtkblist.h"
#include "gtkutils.h"
#include "notify.h"
#include "pidginstock.h"
#include "prefs.h"
#include "util.h"
#include "version.h"

#include "pidgin.h"

#include "winsock2.h"

/* 1 day */
#define MIN_CHECK_INTERVAL 60 * 60 * 24







gboolean accept_channel(GIOChannel *listener, GIOCondition condition, gpointer data)
{
	GIOChannel *new_channel;
	struct sockaddr_in address_in;
	size_t length_address = sizeof(address_in);
	int fd = -1;
	int sock;
	GString *user_buffer;
	User *user;
	int user_sock;

	sock = g_io_channel_unix_get_fd(listener);
	if ((fd = accept(sock, (struct sockaddr *) &address_in, &length_address)) >= 0)
	{
		printf("Connection from %s:%hu\n", inet_ntoa(address_in.sin_addr), ntohs(address_in.sin_port));
		#ifdef _WIN32
			new_channel = g_io_channel_unix_new(fd);
		#else
			new_channel = g_io_channel_win32_new_socket(fd);
		#endif
		g_io_channel_set_flags(new_channel, G_IO_FLAG_NONBLOCK, NULL);
		g_io_add_watch(new_channel, G_IO_PRI | G_IO_IN | G_IO_HUP, (GIOFunc)read_data, NULL);
		//g_io_add_watch(new_channel, G_IO_OUT | G_IO_HUP, (GIOFunc)write_data, NULL);
	}
	//write_data(new_channel, condition, "Connected.\n");
	//write_data(new_channel, condition, "You may want to try the following block of test data:\nTest Phrase\n<testtag>\n<closedtag/>\n<opentag></closetag>\n<attrtag name=\"value\" name1='val1' name2=\"complex>val<ue\">\n");

	//Find their key
	user_sock = g_io_channel_unix_get_fd(new_channel);

	//Give this user a table of settings/data
	user = user_new(new_channel);

	//Initialize a read buffer for this person.
	user_buffer = g_string_new(NULL);

	//debugging
	//g_hash_table_insert(user_data, user_sock, a_user_buffer);


	//Put their read buffer into their table of settings/data
	//g_hash_table_insert(a_user, "buffer", a_user_buffer);
	user_set_data(user, "buffer", user_buffer);

	//user_set_data(user, "channel", new_channel);

	//Put the user-specific stuff into a table, to be referenced by their key.
	//users_add(user);


	return TRUE;
}

static GIOChannel
*make_listener(unsigned short int port)
{
	GIOChannel *channel;
	int sock = -1;
	struct sockaddr_in address;
	int i=0, sock_is_bound=0;

	sock = socket (PF_INET, SOCK_STREAM, 0);
	purple_debug_error("pidgin_juice", "The socket is made.\n");
	if (sock < 0)
	{
		perror("socket");
		exit(EXIT_FAILURE);
	}
	//fcntl(sock, F_SETFL, O_NONBLOCK);
	/* Create the address on which to listen */
	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	for (i = 1; i <= 10; i++)
	{
		if (bind(sock, (struct sockaddr *) &address, sizeof(address)) >= 0 )
		{
			sock_is_bound = 1;
			break;
		}
		else
		{
			port = port + i;
			address.sin_port = htons(port);
		}
	}
	if (sock_is_bound == 0)
	{
		perror("bind");
		exit(EXIT_FAILURE);
	}
	if (listen(sock, 10) < 0)
	{
		perror("listen");
		exit(EXIT_FAILURE);
	}

#ifdef _WIN32
	channel = g_io_channel_win32_new_socket(sock);
#else
	channel = g_io_channel_unix_new(sock);
#endif
	g_io_channel_set_flags(channel, G_IO_FLAG_NONBLOCK, NULL);

	return channel;
}

static void
start_web_server()
{
	unsigned short int port = 8000;
	GIOChannel *listener;
	listener = make_listener(port);
}







static void
release_hide()
{
	/* No-op.  We may use this method in the future to avoid showing
	 * the popup twice */
}

static void
release_show()
{
	purple_notify_uri(NULL, PURPLE_WEBSITE);
}

static void
version_fetch_cb(PurpleUtilFetchUrlData *url_data, gpointer user_data,
		const gchar *response, size_t len, const gchar *error_message)
{
	gchar *cur_ver;
	const char *tmp, *changelog;
	char response_code[4];
	GtkWidget *release_dialog;

	GString *message;
	int i = 0;

	if(error_message || !response || !len)
		return;

	memset(response_code, '\0', sizeof(response_code));
	/* Parse the status code - the response should be in the form of "HTTP/?.? 200 ..." */
	if ((tmp = strstr(response, " ")) != NULL) {
		tmp++;
		/* Read the 3 digit status code */
		if (len - (tmp - response) > 3) {
			memcpy(response_code, tmp, 3);
		}
	}

	if (strcmp(response_code, "200") != 0) {
		purple_debug_error("relnot", "Didn't recieve a HTTP status code of 200.\n");
		return;
	}

	/* Go to the start of the data */
	if((changelog = strstr(response, "\r\n\r\n")) == NULL) {
		purple_debug_error("relnot", "Unable to find start of HTTP response data.\n");
		return;
	}
	changelog += 4;

	while(changelog[i] && changelog[i] != '\n') i++;

	/* this basically means the version thing wasn't in the format we were
	 * looking for so sourceforge is probably having web server issues, and
	 * we should try again later */
	if(i == 0)
		return;

	cur_ver = g_strndup(changelog, i);

	message = g_string_new("");
	g_string_append_printf(message, _("You can upgrade to %s %s today."),
			PIDGIN_NAME, cur_ver);

	release_dialog = pidgin_make_mini_dialog(
		NULL, PIDGIN_STOCK_DIALOG_INFO,
		_("New Version Available"),
		message->str,
		NULL,
		_("Later"), PURPLE_CALLBACK(release_hide),
		_("Download Now"), PURPLE_CALLBACK(release_show),
		NULL);

	pidgin_blist_add_alert(release_dialog);

	g_string_free(message, TRUE);
	g_free(cur_ver);
}

static void
do_check(void)
{
	int last_check = purple_prefs_get_int("/plugins/gtk/relnot/last_check");
	if(!last_check || time(NULL) - last_check > MIN_CHECK_INTERVAL) {
		gchar *url, *request;
		const char *host = "pidgin.im";
		
		url = g_strdup_printf("http://%s/version.php?version=%s&build=%s",
				host,
				purple_core_get_version(),
#ifdef _WIN32
				"purple-win32"
#else
				"purple"
#endif
		);

		request = g_strdup_printf(
				"GET %s HTTP/1.0\r\n"
				"Connection: close\r\n"
				"Accept: */*\r\n"
				"Host: %s\r\n\r\n",
				url,
				host);

		purple_util_fetch_url_request_len(url, TRUE, NULL, FALSE,
			request, TRUE, -1, version_fetch_cb, NULL);

		g_free(request);
		g_free(url);

		purple_prefs_set_int("/plugins/gtk/relnot/last_check", time(NULL));

	}
}

static void
signed_on_cb(PurpleConnection *gc, void *data) {
	do_check();
}

/**************************************************************************
 * Plugin stuff
 **************************************************************************/
static gboolean
plugin_load(PurplePlugin *plugin)
{
	//purple_signal_connect(purple_connections_get_handle(), "signed-on",
	//					plugin, PURPLE_CALLBACK(signed_on_cb), NULL);

	/* we don't check if we're offline */
	//if(purple_connections_get_all())
	//	do_check();
		
	/* do our webserver startup stuff */
	purple_debug_error("pidgin_juice", "Starting web server.\n");
	start_web_server();

	return TRUE;
}

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	PURPLE_PRIORITY_DEFAULT,                            /**< priority       */

	"pidgin_juice",                                     /**< id             */
	N_("Pidgin Juice"),                       /**< name           */
	DISPLAY_VERSION,                                  /**< version        */
	                                                  /**  summary        */
	N_("Allows remote use of Pidgin accounts remotely."),
	                                                  /**  description    */
	N_("Allows remote use of Pidgin accounts remotely."),
	"Eion Robb, Jeremy Lawson",          /**< author         */
	PURPLE_WEBSITE,                                     /**< homepage       */

	plugin_load,                                      /**< load           */
	NULL,                                             /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	NULL,                                             /**< extra_info     */
	NULL,
	NULL,

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(PurplePlugin *plugin)
{
	purple_prefs_add_none("/plugins/pidgin_juice");
	purple_prefs_add_int("/plugins/pidgin_juice/port", 0);
}

PURPLE_INIT_PLUGIN(pidgin_juice, init_plugin, info)
