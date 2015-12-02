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
 * Return bigger of two integers
 *
 * Hmmm... Why is this a function and not a #define? *wondering*
 */
inline int max(int a, int b)
{
	if (a > b) return a;
	return b;
}


/*
 * Print gopher menu line
 */
void info(state *st, char *str, char type)
{
	char buf[BUFSIZE];
	char selector[16];

	/* Convert string to output charset */
	if (st->opt_iconv) strniconv(st->out_charset, buf, str, sizeof(buf));
	else sstrlcpy(buf, str);

	/* Handle gopher++ titles */
	strclear(selector);
	if (type == TYPE_TITLE) {
		sstrlcpy(selector, "TITLE");
		type = TYPE_INFO;
	}

	/* Output info line */
	strcut(buf, st->out_width);
	printf("%c%s\t%s\t%s" CRLF,
		type, buf, selector, DUMMY_HOST);
}


/*
 * Print menu footer
 */
void footer(state *st)
{
	char buf[BUFSIZE];

	if (!st->opt_footer) return;

	/* Create horizontal line */
	strrepeat(buf, '_', st->out_width);

	/* Menu footer? */
	if (st->req_filetype == TYPE_MENU || st->req_filetype == TYPE_QUERY) {
		info(st, buf, TYPE_INFO);
		snprintf(buf, sizeof(buf), FOOTER_FORMAT, platform());
		info(st, buf, TYPE_INFO);
		printf("." CRLF);
	}

	/* Plain text footer */
	else {
		printf("%s" CRLF, buf);
		printf(FOOTER_FORMAT CRLF, platform());
	}
}


/*
 * Print error message & exit
 */
void die(state *st, char *message)
{
	static unsigned char error_gif[] = ERROR_GIF;

	/* Log the error */
	if (st->opt_syslog) {
		syslog(LOG_ERR, "error \"%s\" for request \"%s\" from %s",
			message, st->req_selector, st->req_remote_addr);
	}

	/* Handle menu errors */
	if (st->req_filetype == TYPE_MENU || st->req_filetype == TYPE_QUERY) {

		info(st, message, TYPE_TITLE);
		info(st, EMPTY, TYPE_INFO);
		info(st, message, TYPE_ERROR);

		footer(st);
	}

	/* Handle image errors */
	else if (st->req_filetype == TYPE_GIF || st->req_filetype == TYPE_IMAGE) {
		fwrite(error_gif, sizeof(error_gif), 1, stdout);
	}

	/* Use plain text error for other filetypes */
	else {
		printf("Error: %s" CRLF, message);
		footer(st);
	}

	/* Quit */
	exit(ERROR);
}


/*
 * Convert gopher selector to an absolute path
 */
void selector_to_path(state *st)
{
	DIR *dp;
	struct dirent *dir;
	struct stat file;
#ifdef HAVE_PASSWD
	struct passwd *pwd;
	char buf[BUFSIZE];
	char *path = EMPTY;
	char *c;

	/* Virtual userdir (~user -> /home/user/public_gopher)? */
	if (*(st->user_dir) && strncmp(st->req_selector, "/~", 2) == MATCH) {

		/* Parse userdir login name & path */;
		sstrlcpy(buf, st->req_selector + 2);
		if ((c = strchr(buf, '/'))) {
			*c = '\0';
			path = c + 1;
		}

		/* Check user validity */
		if ((pwd = getpwnam(buf)) == NULL) die(st, ERR_NOTFOUND);
		if (pwd->pw_uid < PASSWD_MIN_UID) die(st, ERR_NOTFOUND);

		/* Generate absolute path to users own gopher root */
		snprintf(st->req_realpath, sizeof(st->req_realpath),
			"%s/%s/%s", pwd->pw_dir, st->user_dir, path);

		/* Check ~public_gopher access rights */
                if (stat(st->req_realpath, &file) == ERROR) die(st, ERR_NOTFOUND);
                if ((file.st_mode & S_IROTH) == 0) die(st, ERR_ACCESS);
                if (file.st_uid != pwd->pw_uid) die(st, ERR_ACCESS);

		/* Userdirs always come from the default vhost */
		if (st->opt_vhost)
			sstrlcpy(st->server_host, st->server_host_default);
		return;
	}
#endif

	/* Handle virtual hosting */
	if (st->opt_vhost) {

		/* Try looking for the selector from the current vhost */
		snprintf(st->req_realpath, sizeof(st->req_realpath), "%s/%s%s",
			st->server_root, st->server_host, st->req_selector);
                if (stat(st->req_realpath, &file) == OK) return;

		/* Loop through all vhosts looking for the selector */
		if ((dp = opendir(st->server_root)) == NULL) die(st, ERR_NOTFOUND);
		while ((dir = readdir(dp)) != NULL) {

			/* Skip .hidden dirs and . & .. */
			if (dir->d_name[0] == '.') continue;

			/* Generate path to the found vhost */
			snprintf(st->req_realpath, sizeof(st->req_realpath), "%s/%s%s",
				st->server_root, dir->d_name, st->req_selector);

			/* Did we find the selector under this vhost? */
			if (stat(st->req_realpath, &file) == OK) {

				/* Virtual host found - update state & return */
				sstrlcpy(st->server_host, dir->d_name);
				return;
			}
		}
	}

	/* Handle normal selectors */
	snprintf(st->req_realpath, sizeof(st->req_realpath),
		"%s%s", st->server_root, st->req_selector);
}


/*
 * Get remote peer IP address
 */
char *get_peer_address(void)
{
#ifdef HAVE_IPv4
	struct sockaddr_in addr;
	socklen_t addrsize = sizeof(addr);
#endif
#ifdef HAVE_IPv6
	struct sockaddr_in6 addr6;
	socklen_t addr6size = sizeof(addr6);
	static char address[INET6_ADDRSTRLEN];
#endif
	char *c;

	/* Are we a CGI script? */
	if ((c = getenv("REMOTE_ADDR")) != NULL) return c;
	/* if ((c = getenv("REMOTE_HOST")) != NULL) return c; */

	/* Try IPv4 first */
#ifdef HAVE_IPv4
	if (getpeername(0, (struct sockaddr *) &addr, &addrsize) == OK) {
		c = inet_ntoa(addr.sin_addr);
		if (strlen(c) > 0 && *c != '0') return c;
	}
#endif

	/* IPv4 didn't work - try IPv6 */
#ifdef HAVE_IPv6
	if (getpeername(0, (struct sockaddr *) &addr6, &addr6size) == OK) {
		if (inet_ntop(AF_INET6, &addr6.sin6_addr, address, sizeof(address)) != NULL)
			return address;
	}
#endif

	/* Nothing works... I'm out of ideas */
	return DEFAULT_ADDR;
}


/*
 * Read & parse gopher++ headers
 */
#ifdef ENABLE_GOPHERPLUSPLUS
void get_plusplus_headers(state *st)
{
	char buf[BUFSIZE];
	char *c;
	int i;

	/* Loop through the client headers */
	while (fgets(buf, sizeof(buf) - 1, stdin) != NULL) {

		/* Empty line marks the end of headers */
		chomp(buf);
		if (!*buf) return;

		/* Debug output */
		if (st->debug) syslog(LOG_INFO, "got gopher++ header \"%s\"", buf);

		/* Parse header */
		if ((c = strheader(buf, "User-Agent"))) { sstrlcpy(st->req_user_agent, c); continue; }
		if ((c = strheader(buf, "Referer"))) { sstrlcpy(st->req_referrer, c); continue; }
		if ((c = strheader(buf, "Accept-Charset"))) { sstrlcpy(st->out_charset, c); continue; }
		if ((c = strheader(buf, "Charset"))) { sstrlcpy(st->out_charset, c); continue; }
		if ((c = strheader(buf, "Filetype"))) { st->req_filetype = *c; continue; }

		if ((c = strheader(buf, "Columns"))) {
			i = atoi(c);
			if (i > MAX_WIDTH) i = MAX_WIDTH;
			if (i < MIN_WIDTH) i = MIN_WIDTH;
			if (i < MIN_WIDTH + DATE_WIDTH) st->opt_date = FALSE;
			st->out_width = i;

			continue;
		}

		if ((c = strheader(buf, "Host"))) {
			sstrlcpy(st->server_host, c);

			if ((c = strchr(st->server_host, ':'))) {
				*c++ = '\0';
				st->server_port = atoi(c);
			}
			continue;
		}
	}
}
#endif


/*
 * Initialize state struct to default/empty values
 */
void init_state(state *st)
{
        /* Request */
	sstrlcpy(st->req_protocol, PROTO_GOPHER0);
	strclear(st->req_selector);
	strclear(st->req_realpath);
	strclear(st->req_query_string);
	strclear(st->req_referrer);
	sstrlcpy(st->req_remote_addr, DEFAULT_ADDR);
	/* strclear(st->req_remote_host); */
	strclear(st->req_user_agent);
        st->req_filetype = DEFAULT_TYPE;
	st->req_filesize = 0;

        /* Output */
        st->out_width = DEFAULT_WIDTH;
        sstrlcpy(st->out_charset, DEFAULT_CHARSET);

        /* Settings */
        sstrlcpy(st->server_root, DEFAULT_ROOT);
        sstrlcpy(st->server_host_default, DEFAULT_HOST);
        sstrlcpy(st->server_host, DEFAULT_HOST);
        st->server_port = DEFAULT_PORT;

        st->default_filetype = DEFAULT_TYPE;
        sstrlcpy(st->map_file, DEFAULT_MAP);
        sstrlcpy(st->cgi_file, DEFAULT_CGI);
        sstrlcpy(st->user_dir, DEFAULT_USERDIR);

	st->hidden_count = 0;

	/* Session */
        st->session_timeout = DEFAULT_SESSION_TIMEOUT;
        st->session_max_kbytes = DEFAULT_SESSION_MAX_KBYTES;
        st->session_max_hits = DEFAULT_SESSION_MAX_HITS;

        /* Feature options */
	st->opt_vhost = TRUE;
        st->opt_parent = TRUE;
        st->opt_footer = TRUE;
        st->opt_date = TRUE;
        st->opt_syslog = TRUE;
        st->opt_magic = TRUE;
        st->opt_iconv = TRUE;
        st->debug = FALSE;
}


/*
 * Main
 */
int main(int argc, char *argv[])
{
	struct stat file;
	state st;
	char buf[BUFSIZE];
	char *dest;
	char *c;
	char *z;
	int i;
#ifdef HAVE_SHMEM
	struct shmid_ds shm_ds;
	shm_state *shm;
	int shmid;
#endif

	/* Initialize state */
#ifdef HAVE_LOCALES
	setlocale(LC_TIME, DATE_LOCALE);
#endif
	init_state(&st);

	/* Try to get shared memory */
#ifdef HAVE_SHMEM
	if ((shmid = shmget(SHM_KEY, sizeof(shm_state), IPC_CREAT | SHM_MODE)) == ERROR) {

		/* Getting memory failed -> delete the old allocation */
		shmctl(shmid, IPC_RMID, &shm_ds);
		shm = NULL;
	}
	else {
		/* Map shared memory */
		if ((shm = (shm_state *) shmat(shmid, (void *) 0, 0)) == (void *) ERROR)
			shm = NULL;

		/* Initialize mapped shared memory */
		if (shm && shm->start_time == 0) shm->start_time = time(NULL);
	}
#endif

	/* Refuse to run as root */
	if (getuid() == 0) die(&st, ERR_ROOT);

	/* Get server hostname */
	if ((c = getenv("HOSTNAME")) != NULL) sstrlcpy(st.server_host, c);
	else if ((gethostname(buf, sizeof(buf))) != ERROR) sstrlcpy(st.server_host, buf);

	/* Get remote peer IP */
	sstrlcpy(st.req_remote_addr, get_peer_address());

	/* Handle command line arguments */
	while ((i = getopt(argc, argv, "h:p:r:t:w:g:c:u:o:s:i:k:n:dl?-")) != ERROR) {
		switch(i) {
			case 'h': sstrlcpy(st.server_host, optarg); break;
			case 'p': st.server_port = atoi(optarg); break;
			case 'r': sstrlcpy(st.server_root, optarg); break;
			case 't': st.default_filetype = *optarg; break;
			case 'w': st.out_width = atoi(optarg); break;
			case 'g': sstrlcpy(st.map_file, optarg); break;
			case 'c': sstrlcpy(st.cgi_file, optarg); break;
			case 'u': sstrlcpy(st.user_dir, optarg);  break;
			case 'o': sstrlcpy(st.out_charset, optarg);  break;

			case 's': st.session_timeout = atoi(optarg); break;
			case 'i': st.session_max_kbytes = abs(atoi(optarg)); break;
			case 'k': st.session_max_hits = abs(atoi(optarg)); break;

			case 'n':
				if (*optarg == 'v') { st.opt_vhost = FALSE; break; }
				if (*optarg == 'l') { st.opt_parent = FALSE; break; }
				if (*optarg == 'f') { st.opt_footer = FALSE; break; }
				if (*optarg == 'd') { st.opt_date = FALSE; break; }
				if (*optarg == 'c') { st.opt_magic = FALSE; break; }
				if (*optarg == 'o') { st.opt_iconv = FALSE; break; }
				if (*optarg == 's') { st.opt_syslog = FALSE; break; }
				break;

			case 'd': st.debug = TRUE; break;
			case 'l': printf(LICENSE);; return OK;
			default : printf(README); return OK;
		}
	}

	/* Sanitize options */
	if (st.out_width > MAX_WIDTH) st.out_width = MAX_WIDTH;
	if (st.out_width < MIN_WIDTH) st.out_width = MIN_WIDTH;
	if (st.out_width  < MIN_WIDTH + DATE_WIDTH) st.opt_date = FALSE;
	if (!st.opt_syslog) st.debug = FALSE;

	/* Primary vhost directory must exist or we disable vhosting */
	if (st.opt_vhost) {
		snprintf(buf, sizeof(buf), "%s/%s", st.server_root, st.server_host);
                if (stat(buf, &file) == ERROR) {
			st.opt_vhost = FALSE;
			if (st.debug)
				syslog(LOG_INFO, "disabling vhosting: %s must exist", buf);
		}
	}

	/* Open syslog() */
	if (st.opt_syslog) openlog(BINARY, LOG_PID, LOG_DAEMON);

	/* Read selector, remove CRLF & encodings */
	buf[0] = '/';
	if (fgets(buf + 1, sizeof(buf) - 2, stdin) == NULL) die(&st, ERR_NOSEL);

	chomp(buf);
	strndecode(buf, buf, sizeof(buf));	/* Do decoding in-place :) */

	if (st.debug) syslog(LOG_INFO, "client sent us \"%s\"", buf + 1);

	/* Handle hURL: redirect page */
	if (strncmp(buf + 1, "URL:", 4) == MATCH) {
		url_redirect(&st, buf + 5);
		return OK;
	}

	/* Handle /server-status */
#ifdef HAVE_SHMEM
	if (strncmp(buf + 1, SERVER_STATUS, sizeof(SERVER_STATUS) - 1) == MATCH) {
		if (shm) server_status(&st, shm, shmid);
		return OK;
	}

	/* We'll handle HTTP requests for /server-status too */
	if (strncmp(buf + 1, "GET " SERVER_STATUS, sizeof("GET " SERVER_STATUS) - 1) == MATCH) {
		printf("HTTP/1.0 200 OK" CRLF);
		printf("Content-Type: text/plain" CRLF);
		printf("Server: " SERVER_SOFTWARE CRLF CRLF, platform());

		if (shm) server_status(&st, shm, shmid);
		return OK;
	}
#endif

	/* Redirect HTTP requests to gopher */
	if (strncmp(buf + 1, "GET ", 4) == MATCH) {
		if (st.debug) syslog(LOG_INFO, "got http request, redirecting to gopher");

		printf("HTTP/1.0 301 Moved Permanently" CRLF);
		printf("Location: gopher://%s:%i/" CRLF, st.server_host, st.server_port);
		printf("Server: " SERVER_SOFTWARE CRLF CRLF, platform());
		return OK;
	}

	/* Save default server_host & fetch session data (including new server_host) */
	sstrlcpy(st.server_host_default, st.server_host);
#ifdef HAVE_SHMEM
	if (shm) get_shm_session(&st, shm);
#endif

	/* Loop through the selector, fix it & separate query_string */
	dest = st.req_selector;

	for (c = buf; *c;) {

		/* Deny dotfile selectors (also blocks /../ hacks) */
		if (*c == '/' && *(c + 1) == '.') die(&st, ERR_ACCESS);

		/* Skip duplicate slashes */
		while (*c == '/' && *(c + 1) == '/') c++;

		/* Start of a type 7 query string? */
		if (*c == '\t') {
			sstrlcpy(st.req_query_string, c + 1);

			/* Query ends on first tab */
			if ((z = strchr(st.req_query_string, '\t'))) {
				*z++ = '\0';

				/* Detect gopher protocol version */
				if (strcmp(z, "+") == MATCH)
					sstrlcpy(st.req_protocol, PROTO_GOPHERPLUS);

				else if (strcmp(z, PROTO_GOPHERPLUSPLUS) == MATCH)
					sstrlcpy(st.req_protocol, PROTO_GOPHERPLUSPLUS);
			}

			break;
		}

		/* Start of virtual host hint? */
		if (*c == ';') {
			if (st.opt_vhost) {
				sstrlcpy(st.server_host, c + 1);
				if ((z = strchr(st.server_host, '\t'))) *z = '\0';
			}

			/* Skip vhost on selector */
			while (*c && *c != '\t') c++;
			continue;
		}

		/* Copy valid char */
		*dest++ = *c++;
	}
	*dest = '\0';

	/* Separate HTTP-style query string */
	if ((z = strchr(st.req_selector, '?'))) {
		*z++ = '\0';
		sstrlcpy(st.req_query_string, z);
	}

	/* Guess request filetype so we can die() with style... */
	i = st.opt_magic;
	st.opt_magic = FALSE;	/* Don't stat() now */
	st.req_filetype = gopher_filetype(&st, st.req_selector);
	st.opt_magic = i;

	/* Handle gopher++ extra headers */
#ifdef ENABLE_GOPHERPLUSPLUS
	if (strcmp(st.req_protocol, PROTO_GOPHERPLUSPLUS) == MATCH)
		get_plusplus_headers(&st);
#endif

	/* Convert seletor to path & stat() */
	selector_to_path(&st);
	if (st.debug) syslog(LOG_INFO, "path to resource is \"%s\"", st.req_realpath);
	if (stat(st.req_realpath, &file) == ERROR) die(&st, ERR_NOTFOUND);
	st.req_filesize = file.st_size;

	/* Everyone must have read access but no write access */
	if ((file.st_mode & S_IROTH) == 0) die(&st, ERR_ACCESS);
	if ((file.st_mode & S_IWOTH) != 0) die(&st, ERR_ACCESS);

	/* If stat said it was a dir then it's a menu */
	if ((file.st_mode & S_IFMT) == S_IFDIR && st.req_filetype != TYPE_MENU)
		st.req_filetype = TYPE_MENU;

	/* Menu selectors must end with a slash */
	if (st.req_filetype == TYPE_MENU && strlast(st.req_selector) != '/')
		sstrlcat(st.req_selector, "/");

	/* Keep count of hits and data transfer */
#ifdef HAVE_SHMEM
	if (shm) {
		shm->hits++;
		shm->kbytes += st.req_filesize / 1024;

		/* Update user session */
		update_shm_session(&st, shm);
	}
#endif

	/* Log the request */
	if (!*st.req_user_agent) sstrlcpy(st.req_user_agent, st.req_protocol);
	if (st.opt_syslog) {
		syslog(LOG_INFO, "request for \"gopher://%s:%i/%c%s\" from %s using \"%s\"",
			st.server_host,
			st.server_port,
			st.req_filetype,
			st.req_selector,
			st.req_remote_addr,
			st.req_user_agent);
	}

	/* Check file type & act accordingly */
	switch (file.st_mode & S_IFMT) {
		case S_IFDIR:
			gopher_menu(&st);
			break;

		case S_IFREG:
			gopher_file(&st);
			break;

		default:
			die(&st, ERR_ACCESS);
	}

	/* Clean exit */
	return OK;
}

