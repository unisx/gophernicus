/*
 * Gophernicus - Copyright (c) 2009-2010 Kim Holviala <kim@holviala.com>
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
	if (st->opt_iconv) sstrniconv(st->out_charset, buf, str);
	else sstrlcpy(buf, str);

	/* Handle gopher title resources */
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
 * Print footer
 */
void footer(state *st)
{
	char line[BUFSIZE];
	char buf[BUFSIZE];
	char msg[BUFSIZE];

	if (!st->opt_footer) {
#ifndef ENABLE_STRICT_RFC1436
		if (st->req_filetype == TYPE_MENU || st->req_filetype == TYPE_QUERY)
#endif
			printf("." CRLF);
		return;
	}

	/* Create horizontal line */
	strrepeat(line, '_', st->out_width);

	/* Create right-aligned footer message */
	snprintf(buf, sizeof(buf), FOOTER_FORMAT, st->server_platform);
	snprintf(msg, sizeof(msg), "%*s", st->out_width - 1, buf);

	/* Menu footer? */
	if (st->req_filetype == TYPE_MENU || st->req_filetype == TYPE_QUERY) {
		info(st, line, TYPE_INFO);
		info(st, msg, TYPE_INFO);
		printf("." CRLF);
	}

	/* Plain text footer */
	else {
		printf("%s" CRLF, line);
		printf("%s" CRLF, msg);
#ifdef ENABLE_STRICT_RFC1436
		printf("." CRLF);
#endif
	}
}


/*
 * Print error message & exit
 */
void die(state *st, char *message, char *description)
{
	int en = errno;
	static const char error_gif[] = ERROR_GIF;

	/* Handle NULL description */
	if (description == NULL) description = strerror(en);

	/* Log the error */
	if (st->opt_syslog) {
		syslog(LOG_ERR, "error \"%s\" for request \"%s\" from %s",
			description, st->req_selector, st->req_remote_addr);
	}
	log_combined(st, HTTP_404);

	/* Handle menu errors */
	if (st->req_filetype == TYPE_MENU || st->req_filetype == TYPE_QUERY) {
#ifdef ENABLE_STRICT_RFC1436
		printf("3" ERROR_PREFIX "%s\tTITLE\t" DUMMY_HOST CRLF, message);
#endif
		printf("i" ERROR_PREFIX "%s\tTITLE\t" DUMMY_HOST CRLF, message);
		footer(st);
	}

	/* Handle image errors */
	else if (st->req_filetype == TYPE_GIF || st->req_filetype == TYPE_IMAGE) {
		fwrite(error_gif, sizeof(error_gif), 1, stdout);
	}

	/* Handle HTML errors */
	else if (st->req_filetype == TYPE_HTML) {
		printf("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2 Final//EN\">\n"
			"<HTML>\n<HEAD>\n"
			"  <META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html;charset=iso-8859-1\">\n"
			"  <TITLE>" ERROR_PREFIX "%1$s</TITLE>\n"
			"</HEAD>\n<BODY>\n"
			"<STRONG>" ERROR_PREFIX "%1$s</STRONG>\n"
			"<PRE>\n", message);
		footer(st);
		printf("</PRE>\n</BODY>\n</HTML>\n");
	}

	/* Use plain text error for other filetypes */
	else {
		printf(ERROR_PREFIX "%s" CRLF, message);
		footer(st);
	}

	/* Quit */
	exit(EXIT_FAILURE);
}


/*
 * Apache-compatible combined logging
 */
void log_combined(state *st, int status)
{
	FILE *fp;
	struct tm *ltime;
	char timestr[64];
	time_t now;

	/* Try to open the logfile for appending */
	if (!*st->log_file) return;
	if ((fp = fopen(st->log_file , "a")) == NULL) return;

	/* Format time */
	now = time(NULL);
	ltime = localtime(&now);
	strftime(timestr, sizeof(timestr), HTTP_DATE, ltime);

	/* Generate log entry */
	fprintf(fp, "%s %s:%i - [%s] \"GET %c%s HTTP/1.0\" %i %li \"%s\" \"" HTTP_USERAGENT "\"\n",
		st->req_remote_addr, 
		st->server_host,
		st->server_port,
		timestr,
		st->req_filetype,
		st->req_selector,
		status,
		(long) st->req_filesize,
		st->req_referrer);
	fclose(fp);
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
	if (*(st->user_dir) && sstrncmp(st->req_selector, "/~") == MATCH) {

		/* Parse userdir login name & path */;
		sstrlcpy(buf, st->req_selector + 2);
		if ((c = strchr(buf, '/'))) {
			*c = '\0';
			path = c + 1;
		}

		/* Check user validity */
		if ((pwd = getpwnam(buf)) == NULL)
			die(st, ERR_NOTFOUND, "User not found");
		if (pwd->pw_uid < PASSWD_MIN_UID)
			die(st, ERR_NOTFOUND, "User found but UID too low");

		/* Generate absolute path to users own gopher root */
		snprintf(st->req_realpath, sizeof(st->req_realpath),
			"%s/%s/%s", pwd->pw_dir, st->user_dir, path);

		/* Check ~public_gopher access rights */
		if (stat(st->req_realpath, &file) == ERROR)
			die(st, ERR_NOTFOUND, NULL);
		if ((file.st_mode & S_IROTH) == 0)
			die(st, ERR_ACCESS, "~/public_gopher not world-readable");
		if (file.st_uid != pwd->pw_uid)
			die(st, ERR_ACCESS, "~/ and ~/public_gopher owned by different users");

		/* Userdirs always come from the default vhost */
		if (st->opt_vhost)
			sstrlcpy(st->server_host, st->server_host_default);
		return;
	}
#endif

	/* Virtual hosting */
	if (st->opt_vhost) {

		/* Try looking for the selector from the current vhost */
		snprintf(st->req_realpath, sizeof(st->req_realpath), "%s/%s%s",
			st->server_root, st->server_host, st->req_selector);
		if (stat(st->req_realpath, &file) == OK) return;

		/* Loop through all vhosts looking for the selector */
		if ((dp = opendir(st->server_root)) == NULL) die(st, ERR_NOTFOUND, NULL);
		while ((dir = readdir(dp))) {

			/* Skip .hidden dirs and . & .. */
			if (dir->d_name[0] == '.') continue;

			/* Special case - skip lost+found (don't ask) */
			if (sstrncmp(dir->d_name, "lost+found") == MATCH) continue;

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
		closedir(dp);
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
	if ((c = getenv("REMOTE_ADDR"))) return c;
	/* if ((c = getenv("REMOTE_HOST"))) return c; */

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
		if (inet_ntop(AF_INET6, &addr6.sin6_addr, address, sizeof(address))) {

			/* Strip ::ffff: IPv4-in-IPv6 prefix */
			if (sstrncmp(address, "::ffff:") == MATCH) return (address + 7);
			else return address;
		}
	}
#endif

	/* Nothing works... I'm out of ideas */
	return DEFAULT_ADDR;
}


/*
 * Initialize state struct to default/empty values
 */
void init_state(state *st)
{
	static const char *filetypes[] = { FILETYPES };
	int i;

	/* Request */
	strclear(st->req_selector);
	strclear(st->req_realpath);
	strclear(st->req_query_string);
	strclear(st->req_referrer);
	sstrlcpy(st->req_remote_addr, DEFAULT_ADDR);
	/* strclear(st->req_remote_host); */
	st->req_filetype = DEFAULT_TYPE;
	st->req_filesize = 0;

	/* Output */
	st->out_width = DEFAULT_WIDTH;
	st->out_charset = DEFAULT_CHARSET;

	/* Settings */
	strclear(st->server_platform);
	sstrlcpy(st->server_root, DEFAULT_ROOT);
	sstrlcpy(st->server_host_default, DEFAULT_HOST);
	sstrlcpy(st->server_host, DEFAULT_HOST);
	st->server_port = DEFAULT_PORT;

	st->default_filetype = DEFAULT_TYPE;
	sstrlcpy(st->map_file, DEFAULT_MAP);
	sstrlcpy(st->tag_file, DEFAULT_TAG);
	sstrlcpy(st->cgi_file, DEFAULT_CGI);
	sstrlcpy(st->user_dir, DEFAULT_USERDIR);
	strclear(st->log_file);

	st->hidden_count = 0;
	st->filetype_count = 0;
	strclear(st->filter_dir);

	strclear(st->server_description);
	sstrlcpy(st->gopher_proxy, DEFAULT_PROXY);

	/* Session */
	st->session_timeout = DEFAULT_SESSION_TIMEOUT;
	st->session_max_kbytes = DEFAULT_SESSION_MAX_KBYTES;
	st->session_max_hits = DEFAULT_SESSION_MAX_HITS;

	/* Feature options */
	st->opt_vhost = TRUE;
	st->opt_parent = TRUE;
	st->opt_header = TRUE;
	st->opt_footer = TRUE;
	st->opt_date = TRUE;
	st->opt_syslog = TRUE;
	st->opt_magic = TRUE;
	st->opt_iconv = TRUE;
	st->opt_query = TRUE;
	st->opt_caps = TRUE;
	st->opt_shm = TRUE;
	st->debug = FALSE;

	/* Load default suffix -> filetype mappings */
	for (i = 0; filetypes[i]; i += 2) {
		if (st->filetype_count < MAX_FILETYPES) {
			sstrlcpy(st->filetype[st->filetype_count].suffix, filetypes[i]);
			st->filetype[st->filetype_count].type = *filetypes[i + 1];
			st->filetype_count++;
		}
	}
}


/*
 * Add one suffix->filetype mapping to the filetypes array
 */
void add_ftype_mapping(state *st, char *suffix)
{
	char *type;
	int i;

	/* Let's not do anything stupid */
	if (!*suffix) return;
	if (!(type = strchr(suffix, '='))) return;

	/* Extract type from the suffix:X string */
	*type++ = '\0';
	if (!*type) return;

	/* Loop through the filetype array */
	for (i = 0; i < st->filetype_count; i++) {

		/* Old entry found? */
		if (strcasecmp(st->filetype[i].suffix, suffix) == MATCH) {
			st->filetype[i].type = *type;
			return;
		}
	}

	/* No old entry found - add new entry */
	if (i < MAX_FILETYPES) {
		sstrlcpy(st->filetype[i].suffix, suffix);
		st->filetype[i].type = *type;
		st->filetype_count++;
	}
}


/*
 * Parse command-line arguments
 */
void parse_args(state *st, int argc, char *argv[])
{
	FILE *fp;
	static const char readme[] = README;
	static const char license[] = LICENSE;
	struct stat file;
	char buf[BUFSIZE];
	int opt;

	/* Parse args */
	while ((opt = getopt(argc, argv, "h:p:r:t:g:a:c:u:m:l:w:o:s:i:k:f:e:D:L:P:n:db?-")) != ERROR) {
		switch(opt) {
			case 'h': sstrlcpy(st->server_host, optarg); break;
			case 'p': st->server_port = atoi(optarg); break;
			case 'r': sstrlcpy(st->server_root, optarg); break;
			case 't': st->default_filetype = *optarg; break;
			case 'g': sstrlcpy(st->map_file, optarg); break;
			case 'a': sstrlcpy(st->map_file, optarg); break;
			case 'c': sstrlcpy(st->cgi_file, optarg); break;
			case 'u': sstrlcpy(st->user_dir, optarg);  break;
			case 'm': /* obsolete, replaced by -l */
			case 'l': sstrlcpy(st->log_file, optarg);  break;

			case 'w': st->out_width = atoi(optarg); break;
			case 'o':
				if (sstrncasecmp(optarg, "UTF-8") == MATCH) st->out_charset = UTF_8;
				if (sstrncasecmp(optarg, "ISO-8859-1") == MATCH) st->out_charset = ISO_8859_1;
				break;

			case 's': st->session_timeout = atoi(optarg); break;
			case 'i': st->session_max_kbytes = abs(atoi(optarg)); break;
			case 'k': st->session_max_hits = abs(atoi(optarg)); break;

			case 'f': sstrlcpy(st->filter_dir, optarg); break;
			case 'e': add_ftype_mapping(st, optarg); break;

			case 'D': sstrlcpy(st->server_description, optarg); break;
			case 'P': sstrlcpy(st->gopher_proxy, optarg); break;

			case 'n':
				if (*optarg == 'v') { st->opt_vhost = FALSE; break; }
				if (*optarg == 'l') { st->opt_parent = FALSE; break; }
				if (*optarg == 'h') { st->opt_header = FALSE; break; }
				if (*optarg == 'f') { st->opt_footer = FALSE; break; }
				if (*optarg == 'd') { st->opt_date = FALSE; break; }
				if (*optarg == 'c') { st->opt_magic = FALSE; break; }
				if (*optarg == 'o') { st->opt_iconv = FALSE; break; }
				if (*optarg == 'q') { st->opt_query = FALSE; break; }
				if (*optarg == 's') { st->opt_syslog = FALSE; break; }
				if (*optarg == 'a') { st->opt_caps = FALSE; break; }
				if (*optarg == 'm') { st->opt_shm = FALSE; break; }
				break;

			case 'd': st->debug = TRUE; break;
			case 'b': printf(license); exit(EXIT_SUCCESS);
			default : printf(readme); exit(EXIT_SUCCESS);
		}
	}

	/* Sanitize options */
	if (st->out_width > MAX_WIDTH) st->out_width = MAX_WIDTH;
	if (st->out_width < MIN_WIDTH) st->out_width = MIN_WIDTH;
	if (st->out_width  < MIN_WIDTH + DATE_WIDTH) st->opt_date = FALSE;
	if (!st->opt_syslog) st->debug = FALSE;

	/* Primary vhost directory must exist or we disable vhosting */
	if (st->opt_vhost) {
		snprintf(buf, sizeof(buf), "%s/%s", st->server_root, st->server_host);
		if (stat(buf, &file) == ERROR) {
			st->opt_vhost = FALSE;
			if (st->debug)
				syslog(LOG_INFO, "disabling vhosting: %s must exist", buf);
		}
	}

	/* If -D arg looks like a file load the file contents */
	if (*st->server_description == '/') {

		if ((fp = fopen(st->server_description , "r"))) {
			fgets(st->server_description, sizeof(st->server_description), fp);
			chomp(st->server_description);
			fclose(fp);
		}
		else strclear(st->server_description);
	}
}


/*
 * Main
 */
int main(int argc, char *argv[])
{
	struct stat file;
	state st;
	char self[64];
	char buf[BUFSIZE];
	char *dest;
	char *c;
	int i;
#ifdef HAVE_SHMEM
	struct shmid_ds shm_ds;
	shm_state *shm;
	int shmid;
#endif

	/* Get the name of this binary */
	if ((c = strrchr(argv[0], '/'))) sstrlcpy(self, c + 1);
	else sstrlcpy(self, argv[0]);

	/* Initialize state */
#ifdef HAVE_LOCALES
	setlocale(LC_TIME, DATE_LOCALE);
#endif
	init_state(&st);

	/* Refuse to run as root */
	if (getuid() == 0)
		die(&st, ERR_ACCESS, "Refusing to run as root");

	/* Get server hostname */
	if ((c = getenv("HOSTNAME"))) sstrlcpy(st.server_host, c);
	else if ((gethostname(buf, sizeof(buf))) != ERROR) sstrlcpy(st.server_host, buf);

	/* Get remote peer IP */
	sstrlcpy(st.req_remote_addr, get_peer_address());

	/* Handle command line arguments */
	parse_args(&st, argc, argv);

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
		if (shm && shm->start_time == 0) {
			shm->start_time = time(NULL);

			/* Keep server platform & description in shm */
			platform(&st);
			sstrlcpy(shm->server_platform, st.server_platform);
			sstrlcpy(shm->server_description, st.server_description);
		}
	}

	/* For debugging shared memory issues */
	if (!st.opt_shm) shm = NULL;
#endif

	/* Get server platform and description */
#ifdef HAVE_SHMEM
	if (shm) {
		sstrlcpy(st.server_platform, shm->server_platform);

		if (!*st.server_description)
			sstrlcpy(st.server_description, shm->server_description);
	}
	else
#endif
		platform(&st);

	/* Open syslog() */
	if (st.opt_syslog) openlog(self, LOG_PID, LOG_DAEMON);

	/* Read selector, remove CRLF & encodings */
	buf[0] = '/';
	if (fgets(buf + 1, sizeof(buf) - 2, stdin) == NULL) buf[1] = '\0';

	chomp(buf);
	strndecode(buf, buf, sizeof(buf));	/* Do decoding in-place :) */

	if (st.debug) syslog(LOG_INFO, "client sent us \"%s\"", buf + 1);

	/* Handle hURL: redirect page */
	if (sstrncmp(buf + 1, "URL:") == MATCH) {
		st.req_filetype = TYPE_HTML;
		sstrlcpy(st.req_selector, buf + 1);
		url_redirect(&st);
		return OK;
	}

	/* Handle HTTP /server-status requests */
#ifdef HAVE_SHMEM
	if (sstrncmp(buf + 1, "GET " SERVER_STATUS) == MATCH) {
		printf("HTTP/1.0 200 OK" CRLF);
		printf("Content-Type: text/plain" CRLF);
		printf("Server: " SERVER_SOFTWARE_FULL CRLF CRLF, st.server_platform);

		/* Make logging accurate */
		sstrlcpy(st.req_selector, SERVER_STATUS);

		if (shm) server_status(&st, shm, shmid);
		return OK;
	}
#endif

	/* Redirect HTTP requests to a gopher proxy */
	if (sstrncmp(buf + 1, "GET ") == MATCH) {
		if (st.debug) syslog(LOG_INFO, "got http request, redirecting to proxy");

		printf("HTTP/1.0 301 Moved Permanently" CRLF);
		printf("Location: %s%s:%i/" CRLF,
			st.gopher_proxy,
			st.server_host,
			st.server_port);
		printf("Server: " SERVER_SOFTWARE_FULL CRLF CRLF, st.server_platform);
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

		/* Skip duplicate slashes and /./ */
		while (*c == '/' && *(c + 1) == '/') c++;
		if (*c == '/' && *(c + 1) == '.' && *(c + 2) == '/') c += 2;

		/* Start of a query string (either type 7 or HTTP-style)? */
		if (*c == '\t' || (st.opt_query && *c == '?')) {
			sstrlcpy(st.req_query_string, c + 1);
			if ((c = strchr(st.req_query_string, '\t'))) *c = '\0';
			break;
		}

		/* Start of virtual host hint? */
		if (*c == ';') {
			if (st.opt_vhost) sstrlcpy(st.server_host, c + 1);

			/* Skip vhost on selector */
			while (*c && *c != '\t') c++;
			continue;
		}

		/* Copy valid char */
		*dest++ = *c++;
	}
	*dest = '\0';

	/* Deny requests for Slashdot and /../ hackers */
	if (strstr(st.req_selector, "/."))
		die(&st, ERR_ACCESS, "Refusing to serve out dotfiles");

	/* Handle /server-status requests */
#ifdef HAVE_SHMEM
	if (sstrncmp(st.req_selector, SERVER_STATUS) == MATCH) {
		if (shm) server_status(&st, shm, shmid);
		return OK;
	}
#endif

	/* Remove possible extra cruft from server_host */
	if ((c = strchr(st.server_host, '\t'))) *c = '\0';

	/* Guess request filetype so we can die() with style... */
	i = st.opt_magic;
	st.opt_magic = FALSE;	/* Don't stat() now */
	st.req_filetype = gopher_filetype(&st, st.req_selector);
	st.opt_magic = i;

	/* Convert seletor to path & stat() */
	selector_to_path(&st);
	if (st.debug) syslog(LOG_INFO, "path to resource is \"%s\"", st.req_realpath);

	if (stat(st.req_realpath, &file) == ERROR) {

		/* Handle virtual /caps.txt requests */
		if (st.opt_caps && sstrncmp(st.req_selector, CAPS_TXT) == MATCH) {
			caps_txt(&st, shm);
			return OK;
		}

		/* Requested file not found - die() */
		die(&st, ERR_NOTFOUND, NULL);
	}

	/* Fetch request filesize from stat() */
	st.req_filesize = file.st_size;

	/* Everyone must have read access but no write access */
	if ((file.st_mode & S_IROTH) == 0)
		die(&st, ERR_ACCESS, "File or directory not world-readable");
	if ((file.st_mode & S_IWOTH) != 0)
		die(&st, ERR_ACCESS, "File or directory world-writeable");

	/* If stat said it was a dir then it's a menu */
	if ((file.st_mode & S_IFMT) == S_IFDIR) st.req_filetype = TYPE_MENU;

	/* Not a dir - let's guess the filetype again... */
	else if ((file.st_mode & S_IFMT) == S_IFREG)
		st.req_filetype = gopher_filetype(&st, st.req_realpath);

	/* Menu selectors must end with a slash */
	if (st.req_filetype == TYPE_MENU && strlast(st.req_selector) != '/')
		sstrlcat(st.req_selector, "/");

	/* Change directory to wherever the resource was */
	sstrlcpy(buf, st.req_realpath);

	if ((file.st_mode & S_IFMT) != S_IFDIR) c = dirname(buf);
	else c = buf;

	if (chdir(c) == ERROR) die(&st, ERR_ACCESS, NULL);

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
	if (st.opt_syslog) {
		syslog(LOG_INFO, "request for \"gopher://%s:%i/%c%s\" from %s",
			st.server_host,
			st.server_port,
			st.req_filetype,
			st.req_selector,
			st.req_remote_addr);
	}

	/* Check file type & act accordingly */
	switch (file.st_mode & S_IFMT) {
		case S_IFDIR:
			log_combined(&st, HTTP_OK);
			gopher_menu(&st);
			break;

		case S_IFREG:
			log_combined(&st, HTTP_OK);
			gopher_file(&st);
			break;

		default:
			die(&st, ERR_ACCESS, "Refusing to serve out special files");
	}

	/* Clean exit */
	return OK;
}

