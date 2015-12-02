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
 * Dump a file to standard output
 */
void dump_file(char *file, off_t size)
{
	/* Faster sendfile() version */
#ifdef HAVE_SENDFILE
	int fd;
	off_t offset = 0;

	if ((fd = open(file, O_RDONLY)) == ERROR) return;
	sendfile(1, fd, &offset, size);
	close(fd);

	/* More compatible POSIX fread()/fwrite() version */
#else
	FILE *fp;
	char buf[BUFSIZE];
	int bytes;

	if ((fp = fopen(file , "r")) == NULL) return;
	while ((bytes = fread(buf, 1, sizeof(buf), fp)) > 0)
		fwrite(buf, bytes, 1, stdout);
	fclose(fp);
#endif
}


/*
 * Print hURL redirect page
 */
void url_redirect(state *st, char *url)
{
	/* Log the redirect */
	if (st->opt_syslog) {
		syslog(LOG_INFO, "request for \"hURL:%s\" from %s",
			url, st->req_remote_addr);
	}

	/* Output HTML */
	printf("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2 Final//EN\">\n"
		"<HTML>\n<HEAD>\n"
		"  <META HTTP-EQUIV=\"Refresh\" content=\"1;URL=%1$s\">\n"
		"  <META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html;charset=iso-8859-1\">\n"
		"  <TITLE>URL Redirect page</TITLE>\n"
		"</HEAD>\n<BODY>\n"
		"<P ALIGN=center>\n"
		"  <STRONG>Redirecting to: <A HREF=\"%1$s\">%1$s</A></STRONG>\n"
		"</P>\n"
		"</BODY>\n</HTML>\n", url);
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
		"CPULoad: %.2f" CRLF
		"Server: %s" CRLF
		"Platform: %s" CRLF,
			shm->hits,
			shm->kbytes,
			(int) uptime,
			(float) shm->hits / (float) uptime,
			shm->kbytes * 1024 / (int) uptime,
			shm->kbytes * 1024 / (shm->hits + 1),
			(int) shm_ds.shm_nattch,
			loadavg(),
			SERVER_SOFTWARE, platform());

	/* Print active sessions */
	sessions = 0;

	for (i = 0; i < SHM_SESSIONS; i++) {
		if ((now - shm->session[i].req_atime) < st->session_timeout) {
			sessions++;

			printf("Session: %-4i %-40s %-4li %-7li gopher://%s:%i/%c%s\n",
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

	printf("Total Sessions: %i\n", sessions);
}
#endif


/*
 * Execute a CGI script
 */
void runcgi(state *st, char *file)
{
	char buf[BUFSIZE];

	if (st->debug) syslog(LOG_INFO, "executing file \"%s\"", file);

	/* Security */
	setenv("PATH", SAFE_PATH, 1);

	/* Set up the environment as per CGI spec */
	setenv("GATEWAY_INTERFACE", "CGI/1.1", 1);
	setenv("CONTENT_LENGTH", "0", 1);
	setenv("QUERY_STRING", st->req_query_string, 1);
	setenv("SERVER_SOFTWARE", SERVER_SOFTWARE, 1);
	setenv("SERVER_PROTOCOL", "GOPHER/0", 1);
	setenv("SERVER_NAME", st->server_host, 1);
	snprintf(buf, sizeof(buf), "%i", st->server_port);
	setenv("SERVER_PORT", buf, 1);
	setenv("REQUEST_METHOD", "GET", 1);
	setenv("DOCUMENT_ROOT", st->server_root, 1);
	setenv("SCRIPT_NAME", st->req_selector, 1);
	setenv("SCRIPT_FILENAME", file, 1);
	setenv("REMOTE_ADDR", st->req_remote_addr, 1);
	snprintf(buf, sizeof(buf), "gopher://%s:%i/%c%s",
		st->server_host,
		st->server_port,
		st->req_filetype,
		st->req_referrer);
	setenv("HTTP_REFERER", buf, 1);

	/* gopherd extras */
	setenv("SERVER_PLATFORM", platform(), 1);
	setenv("REQUEST_PROTOCOL", st->req_protocol, 1);
	snprintf(buf, sizeof(buf), "%c", st->req_filetype);
	setenv("GOPHER_FILETYPE", buf, 1);
	setenv("GOPHER_REFERER", st->req_referrer, 1);

	/* bucktooth extras */
	if (strlen(st->req_query_string) > 0) {
		snprintf(buf, sizeof(buf), "%s?%s",
			st->req_selector, st->req_query_string);
		setenv("SELECTOR", buf, 1);
	}
	else {
		setenv("SELECTOR", st->req_selector, 1);
	}
	setenv("SERVER_HOST", st->server_host, 1);
	setenv("REQUEST", st->req_selector, 1);
	setenv("SEARCHREQUEST", st->req_query_string, 1);

	/* Try to execute the binary */
	execl(file, file, (char *) NULL);

	/* Didn't work - die */
	die(st, ERR_EXE);
}


/*
 * Handle file selectors
 */
void gopher_file(state *st)
{
	/* Check for & run CGI scripts */
	if (strstr(st->req_realpath, st->cgi_file))
		runcgi(st, st->req_realpath);

	/* Handle regular files */
	if (st->debug) syslog(LOG_INFO, "outputting file \"%s\"", st->req_realpath);
	dump_file(st->req_realpath, st->req_filesize);
}

