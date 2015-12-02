/*
 * Gophernicus Server - Copyright (c) 2009-2010 Kim Holviala <kim@holviala.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "gophernicus.h"


/*
 * Send a binary file to the client
 */
void send_binary_file(state *st)
{
	/* Faster sendfile() version */
#ifdef HAVE_SENDFILE
	int fd;
	off_t offset = 0;

	if (st->debug) syslog(LOG_INFO, "outputting binary file \"%s\"", st->req_realpath);

	if ((fd = open(st->req_realpath, O_RDONLY)) == ERROR) return;
	sendfile(1, fd, &offset, st->req_filesize);
	close(fd);

	/* More compatible POSIX fread()/fwrite() version */
#else
	FILE *fp;
	char buf[BUFSIZE];
	int bytes;

	if (st->debug) syslog(LOG_INFO, "outputting binary file \"%s\"", st->req_realpath);

	if ((fp = fopen(st->req_realpath , "r")) == NULL) return;
	while ((bytes = fread(buf, 1, sizeof(buf), fp)) > 0)
		fwrite(buf, bytes, 1, stdout);
	fclose(fp);
#endif
}


/*
 * Send a text file to the client
 */
void send_text_file(state *st)
{
	FILE *fp;
	char in[BUFSIZE];
	char out[BUFSIZE];

	if (st->debug) syslog(LOG_INFO, "outputting text file \"%s\"", st->req_realpath);
	if ((fp = fopen(st->req_realpath , "r")) == NULL) return;

	/* Loop through the file line by line */
	while (fgets(in, sizeof(in), fp) != NULL) {

		/* Covert to output charset & print */
		if (st->opt_iconv) strniconv(st->out_charset, out, in, sizeof(out));
		else sstrlcpy(out, in);

		chomp(out);

#ifdef ENABLE_STRICT_RFC1436
		if (strcmp(out, ".") == MATCH) printf(".." CRLF);
		else printf("%s" CRLF , out);
#else
		printf("%s" CRLF , out);
#endif
	}

#ifdef ENABLE_STRICT_RFC1436
	printf("." CRLF);
#endif
	fclose(fp);
}


/*
 * Print hURL redirect page
 */
void url_redirect(state *st)
{
	char *c;

	/* Log the redirect */
	if (st->opt_syslog) {
		syslog(LOG_INFO, "request for \"hURL:%s\" from %s",
			st->req_selector, st->req_remote_addr);
	}

	/* Basic security checking */
	if (sstrncmp(st->req_selector, "http://") != MATCH &&
	    sstrncmp(st->req_selector, "ftp://") != MATCH &&
	    sstrncmp(st->req_selector, "mailto:") != MATCH) die(st, ERR_ACCESS);

	if ((c = strchr(st->req_selector, '"'))) *c = '\0';
	if ((c = strchr(st->req_selector, '?'))) *c = '\0';

	/* Output HTML */
	printf("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2 Final//EN\">\n"
		"<HTML>\n<HEAD>\n"
		"  <META HTTP-EQUIV=\"Refresh\" content=\"1;URL=%1$s\">\n"
		"  <META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html;charset=iso-8859-1\">\n"
		"  <TITLE>URL Redirect page</TITLE>\n"
		"</HEAD>\n<BODY>\n"
		"<STRONG>Redirecting to <A HREF=\"%1$s\">%1$s</A></STRONG>\n"
		"<PRE>\n", st->req_selector);
	footer(st);
	printf("</PRE>\n</BODY>\n</HTML>\n");
}


/*
 * Handle /server-status
 */
#ifdef HAVE_SHMEM
void server_status(state *st, shm_state *shm, int shmid)
{
	struct shmid_ds shm_ds;
	time_t now;
	time_t uptime;
	int sessions;
	int i;

	/* Log the request */
	if (st->opt_syslog) {
		syslog(LOG_INFO, "request for \"/server-status\" from %s",
			st->req_remote_addr);
	}

	/* Update counters */
	shm->hits++;
	shm->kbytes += 1;

	/* Get server uptime */
	now = time(NULL);
	uptime = (now - shm->start_time) + 1;

	/* Get shared memory info */
	shmctl(shmid, IPC_STAT, &shm_ds);

	/* Print statistics */
	printf("Total Accesses: %li" CRLF
		"Total kBytes: %li" CRLF
		"Uptime: %i" CRLF
		"ReqPerSec: %.3f" CRLF
		"BytesPerSec: %li" CRLF
		"BytesPerReq: %li" CRLF
		"BusyServers: %i" CRLF
		"IdleServers: 0" CRLF
		"CPULoad: %.2f" CRLF,
			shm->hits,
			shm->kbytes,
			(int) uptime,
			(float) shm->hits / (float) uptime,
			shm->kbytes * 1024 / (int) uptime,
			shm->kbytes * 1024 / (shm->hits + 1),
			(int) shm_ds.shm_nattch,
			loadavg());

	/* Print active sessions */
	sessions = 0;

	for (i = 0; i < SHM_SESSIONS; i++) {
		if ((now - shm->session[i].req_atime) < st->session_timeout) {
			sessions++;

			printf("Session: %-4i %-40s %-4li %-7li gopher://%s:%i/%c%s" CRLF,
				(int) (now - shm->session[i].req_atime),
				shm->session[i].req_remote_addr,
				shm->session[i].hits,
				shm->session[i].kbytes,
				shm->session[i].server_host,
				shm->session[i].server_port,
				shm->session[i].req_filetype,
				shm->session[i].req_selector);
		}
	}

	printf("Total Sessions: %i" CRLF, sessions);
	printf("Server: " SERVER_SOFTWARE CRLF, st->server_platform);
}
#endif


/*
 * Setup environment variables as per the CGI spec
 */
void setenv_cgi(state *st, char *script)
{
	char buf[BUFSIZE];

	/* Security */
	setenv("PATH", SAFE_PATH, 1);

	/* Set up the environment as per CGI spec */
	setenv("GATEWAY_INTERFACE", "CGI/1.1", 1);
	setenv("CONTENT_LENGTH", "0", 1);
	setenv("QUERY_STRING", st->req_query_string, 1);
	snprintf(buf, sizeof(buf), SERVER_SOFTWARE, st->server_platform);
	setenv("SERVER_SOFTWARE", buf, 1);
	setenv("SERVER_PROTOCOL", "RFC1436", 1);
	setenv("SERVER_NAME", st->server_host, 1);
	snprintf(buf, sizeof(buf), "%i", st->server_port);
	setenv("SERVER_PORT", buf, 1);
	setenv("REQUEST_METHOD", "GET", 1);
	setenv("DOCUMENT_ROOT", st->server_root, 1);
	setenv("SCRIPT_NAME", st->req_selector, 1);
	setenv("SCRIPT_FILENAME", script, 1);
	setenv("REMOTE_ADDR", st->req_remote_addr, 1);
	setenv("HTTP_REFERER", st->req_referrer, 1);
	setenv("HTTP_ACCEPT_CHARSET", st->out_charset, 1);

	/* Gophernicus extras */
	snprintf(buf, sizeof(buf), "%c", st->req_filetype);
	setenv("GOPHER_FILETYPE", buf, 1);
	setenv("GOPHER_CHARSET", st->out_charset, 1);
	setenv("GOPHER_REFERER", st->req_referrer, 1);
	snprintf(buf, sizeof(buf), "%i", st->out_width);
	setenv("COLUMNS", buf, 1);

	/* Bucktooth extras */
	if (*st->req_query_string) {
		snprintf(buf, sizeof(buf), "%s?%s",
			st->req_selector, st->req_query_string);
		setenv("SELECTOR", buf, 1);
	}
	else setenv("SELECTOR", st->req_selector, 1);

	setenv("SERVER_HOST", st->server_host, 1);
	setenv("REQUEST", st->req_selector, 1);
	setenv("SEARCHREQUEST", st->req_query_string, 1);
}


/*
 * Execute a CGI script
 */
void run_cgi(state *st, char *script, char *arg)
{
	/* Setup environment & execute the binary */
	if (st->debug) syslog(LOG_INFO, "executing script \"%s\"", script);

	setenv_cgi(st, script);
	execl(script, script, arg, NULL);

	/* Didn't work - die */
	die(st, ERR_EXE);
}


/*
 * Handle file selectors
 */
void gopher_file(state *st)
{
	struct stat file;
	char buf[BUFSIZE];
	char *c;

	/* Refuse to serve out gophermaps */
	if ((c = strrchr(st->req_realpath, '/'))) c++;
	else c = st->req_realpath;
	if (strcmp(c, st->map_file) == MATCH) die(st, ERR_ACCESS);

	/* Check for & run CGI scripts and query scripts */
	if (strstr(st->req_realpath, st->cgi_file) || st->req_filetype == TYPE_QUERY)
		run_cgi(st, st->req_realpath, NULL);

	/* Check for a file suffix filter */
	if (*st->filter_dir && (c = strrchr(st->req_realpath, '.'))) {
		snprintf(buf, sizeof(buf), "%s/%s", st->filter_dir, c + 1);

		/* Filter file through the script */
		if (stat(buf, &file) == OK && (file.st_mode & S_IXOTH))
			run_cgi(st, buf, st->req_realpath);
	}

	/* Check for a filetype filter */
	if (*st->filter_dir) {
		snprintf(buf, sizeof(buf), "%s/%c", st->filter_dir, st->req_filetype);

		/* Filter file through the script */
		if (stat(buf, &file) == OK && (file.st_mode & S_IXOTH))
			run_cgi(st, buf, st->req_realpath);
	}

	/* Output regular files */
	if (st->req_filetype == TYPE_TEXT) send_text_file(st);
	else send_binary_file(st);
}


