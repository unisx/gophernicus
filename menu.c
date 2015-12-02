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
 * Callback for sorting directories folders first
 */
int foldersort(const struct dirent **a, const struct dirent **b)
{
	/* Sort directories first */
	if ((*a)->d_type == DT_DIR && (*b)->d_type != DT_DIR) return -1;
	if ((*a)->d_type != DT_DIR && (*b)->d_type == DT_DIR) return 1;

	/* Alphabetic sort for the rest */
	return strcmp((*a)->d_name, (*b)->d_name);
}


/*
 * Print a list of users with ~/public_gopher
 */
#ifdef HAVE_PASSWD
void userlist(state *st)
{
	struct passwd *pwd;
	struct stat dir;
	char buf[BUFSIZE];
	struct tm *ltime;
	char timestr[20];
	int width;

	/* Width of filenames for fancy listing */
	width = st->out_width - DATE_WIDTH - 15;

	/* Loop through all users */
	setpwent();
	while ((pwd = getpwent()) != NULL) {

		/* Skip too small uids */
		if (pwd->pw_uid < PASSWD_MIN_UID) continue;

		/* Look for a world-readable user-owned ~/public_gopher */
		snprintf(buf, sizeof(buf), "%s/%s", pwd->pw_dir, st->user_dir);
		if (stat(buf, &dir) == ERROR) continue;
		if ((dir.st_mode & S_IROTH) == 0) continue;
		if (dir.st_uid != pwd->pw_uid) continue;

		/* Found one */
		snprintf(buf, sizeof(buf), USERDIR_FORMAT);

		if (st->opt_date) {
			ltime = localtime(&dir.st_mtime);
			strftime(timestr, sizeof(timestr), DATE_FORMAT, ltime);

			printf("1%-*.*s   %s        -  \t/~%s/\t%s\t%i" CRLF,
				width, width, buf, timestr, pwd->pw_name,
				st->server_host, st->server_port);
		}
		else {
			printf("1%.*s\t/~%s/\t%s\t%i" CRLF, st->out_width, buf,
				pwd->pw_name, st->server_host_default, st->server_port);
		}
	}

	endpwent();
}
#endif

/*
 * Print a list of available virtual hosts
 */
void vhostlist(state *st)
{
	struct dirent **dir;
	struct stat file;
	struct tm *ltime;
	char timestr[20];
	char buf[BUFSIZE];
	int width;
	int num;
	int i;

	/* Scan the root dir for vhost dirs */
	num = scandir(st->server_root, &dir, NULL, alphasort);
	if (num < 0) die(st, ERR_NOTFOUND);

	/* Width of filenames for fancy listing */
	width = st->out_width - DATE_WIDTH - 15;

	/* Loop through the directory entries */
	for (i = 0; i < num; i++) {

		/* Get full path+name */
		snprintf(buf, sizeof(buf), "%s/%s", st->server_root, dir[i]->d_name);

		/* Skip dotfiles */
		if (dir[i]->d_name[0] == '.') goto next;

		/* Skip non-world readable vhosts */
		if (stat(buf, &file) == ERROR) goto next;
		if ((file.st_mode & S_IROTH) == 0) goto next;

		/* We're only interested in directories */
		if ((file.st_mode & S_IFMT) != S_IFDIR) goto next;

		/* Generate display string for vhost */
		snprintf(buf, sizeof(buf), VHOST_FORMAT, dir[i]->d_name);

		/* Fancy listing */
		if (st->opt_date) {
			ltime = localtime(&file.st_mtime);
			strftime(timestr, sizeof(timestr), DATE_FORMAT, ltime);

			printf("1%-*.*s   %s        -  \t/;%s\t%s\t%i" CRLF,
				width, width, buf, timestr, dir[i]->d_name, 
				dir[i]->d_name, st->server_port);
		}

		/* Teh boring version */
		else {
			printf("1%.*s\t/;%s\t%s\t%i" CRLF, st->out_width, buf,
				dir[i]->d_name, dir[i]->d_name, st->server_port);
		}

		/* Free allocated directory entry */
		next:
		free(dir[i]);
	}
}


/*
 * Return gopher filetype for a file
 */
char gopher_filetype(state *st, char *file)
{
	FILE *fp;
	static char *filetypes[] = { FILETYPES };
	char suffixdot[20];
	char line[BUFSIZE];
	char *suffix;
	int i;

	/* If it ends with an slash it's a dir */
	if ((i = strlen(file)) == 0) return st->default_filetype;
	if (file[i - 1] == '/') return '1';

	/* Get file suffix */
	if ((suffix = strrchr(file, '.'))) {

		/* Add a dot to suffix for easier searching (.txt -> .txt.) */
		snprintf(suffixdot, sizeof(suffixdot), "%s.", suffix);

		/* Loop through the filetype array looking for the suffix */
		for (i = 0; filetypes[i]; i += 2)
			if (strstr(filetypes[i + 1], suffixdot)) return *filetypes[i];
	}

	/* Are we allowed to look inside files? */
	if (!st->opt_magic) return st->default_filetype;

	/* Read data from the file */
	if ((fp = fopen(file , "r")) == NULL) return st->default_filetype;
	i = fread(line, 1, sizeof(line), fp);
	fclose(fp);

	/* Binary or text? */
	if (memchr(line, '\0', i)) return TYPE_BINARY;
	return st->default_filetype;
}


/*
 * Handle gophermaps
 */
int gophermap(state *st, char *file)
{
	FILE *fp;
	char line[BUFSIZE];
	char *selector;
	char *name;
	char *host;
	char *c;
	char type;
	int port;

	/* Try to open the file */
	if (st->debug) syslog(LOG_INFO, "handling gophermap \"%s\"", file);
	if ((fp = fopen(file , "r")) == NULL) return 0;

	/* Read lines one by one */
	while (fgets(line, sizeof(line) - 1, fp)) {

		/* Parse type & name */
		chomp(line);
		type = line[0];
		name = line + 1;

		/* Ignore #comments */
		if (type == '#') continue;

		/* Stop handling gophermap? */
		if (type == '*') return 0;
		if (type == '.') return 1;

		/* Print a list of users with public_gopher */
		if (type == '~') {
#ifdef HAVE_PASSWD
			userlist(st);
#endif
			continue;
		}

		/* Print a list of available virtual hosts */
		if (type == '%') {
			if (st->opt_vhost) vhostlist(st);
			continue;
		}

		/* Hide files in menus */
		if (type == '-') {
			if (st->hidden_count < MAX_HIDDEN)
				sstrlcpy(st->hidden[st->hidden_count++], name);
			continue;
		}

		/* Print out non-resources as info text */
		if (!strchr(line, '\t')) {
			info(line);
			continue;
		}

		/* Parse selector */
		selector = EMPTY;
		if ((c = strchr(name, '\t'))) {
			*c = '\0';
			selector = c + 1;
		}

		/* Parse host */
		host = st->server_host;
		if ((c = strchr(selector, '\t'))) {
			*c = '\0';
			host = c + 1;
		}

		/* Parse port */
		port = st->server_port;
		if ((c = strchr(host, '\t'))) {
			*c = '\0'; 
			port = atoi(c + 1);
		}

		/* Handle absolute and hURL gopher resources */
		if (selector[0] == '/' || strncmp(selector, "URL:", 4) == FOUND) {
			printf("%c%s\t%s\t%s\t%i" CRLF, type, name,
				selector, host, port);
		}

		/* Handle relative resources */
		else {
			printf("%c%s\t%s%s\t%s\t%i" CRLF, type, name,
				st->req_selector, selector, host, port);
		}
	}

	/* Clean up & return */
	fclose(fp);
	return 1;
}


/*
 * Handle gopher menus
 */
void gopher_menu(state *st)
{
	struct dirent **dir;
	struct stat file;
	struct tm *ltime;
	char buf[BUFSIZE];
	char pathname[BUFSIZE];
	char displayname[BUFSIZE];
	char encodedname[BUFSIZE];
	char timestr[20];
	char sizestr[20];
	char *parent;
	char type;
	int width;
	int num;
	int i;
	int n;

	/* Check for a gophermap */
	snprintf(pathname, sizeof(pathname), "%s/%s", st->req_realpath, st->map_file);
	if (stat(pathname, &file) == OK) {

		/* Executable map? */
		if ((file.st_mode & S_IXOTH)) runcgi(st, pathname);

		/* Handle non-exec maps */
		if (gophermap(st, pathname)) {
			footer(st);
			return;
		}
	}

	/* Scan the directory */
#ifdef HAVE_BROKEN_SCANDIR
	num = scandir(st->req_realpath, &dir, NULL, (int (*)(const void *, const void *)) foldersort);
#else
	num = scandir(st->req_realpath, &dir, NULL, foldersort);
#endif
	if (num < 0) die(st, ERR_NOTFOUND);

	/* Create link to parent directory */
	if (st->opt_parent) {
		sstrlcpy(buf, st->req_selector);
		parent = dirname(buf);

		/* Root has no parent */
		if (strcmp(st->req_selector, ROOT) != FOUND) {

			/* Prevent double-slash */
			if (strcmp(parent, ROOT) == FOUND) parent++;

			/* Print link */
			printf("1%-*s\t%s/\t%s\t%i" CRLF,
				st->opt_date ? (st->out_width - 1) : (int) strlen(PARENT),
				PARENT, parent, st->server_host, st->server_port);
		}
	}

	/* Width of filenames for fancy listing */
	width = st->out_width - DATE_WIDTH - 15;

	/* Loop through the directory entries */
	for (i = 0; i < num; i++) {

		/* Get full path+name */
		snprintf(pathname, sizeof(pathname), "%s/%s",
			st->req_realpath, dir[i]->d_name);

		/* Skip dotfiles and gophermaps */
		if (dir[i]->d_name[0] == '.') goto next;
		if (strcmp(dir[i]->d_name, st->map_file) == FOUND) goto next;

		/* Skip files marked for hiding */
		for (n = 0; n < st->hidden_count; n++)
			if (strcmp(dir[i]->d_name, st->hidden[n]) == FOUND) goto next;

		/* Convert filename to 7bit ASCII & encoded versions */
		strntoascii(displayname, dir[i]->d_name, sizeof(displayname));
		strnencode(encodedname, dir[i]->d_name, sizeof(encodedname));

		/* Skip non-world readable files */
		if (stat(pathname, &file) == ERROR) goto next;
		if ((file.st_mode & S_IROTH) == 0) goto next;

		/* Handle inline .gophermap */
		if (strstr(displayname, st->map_file) > displayname) {
			gophermap(st, pathname);
			goto next;
		}

		/* Handle directories */
		if ((file.st_mode & S_IFMT) == S_IFDIR) {

			/* Dir listing with dates */
			if (st->opt_date) {
				ltime = localtime(&file.st_mtime);
				strftime(timestr, sizeof(timestr), DATE_FORMAT, ltime);

				printf("1%-*.*s   %s        -  \t%s%s/\t%s\t%i" CRLF,
					width, width, displayname, timestr, st->req_selector,
					encodedname, st->server_host, st->server_port);
			}

			/* Regular dir listing */
			else {
				printf("1%.*s\t%s%s/\t%s\t%i" CRLF, st->out_width,
					displayname, st->req_selector, encodedname,
					st->server_host, st->server_port);
			}

			goto next;
		}

		/* Get file type */
		type = gopher_filetype(st, pathname);

		/* File listing with dates & sizes */
		if (st->opt_date) {
			ltime = localtime(&file.st_mtime);
			strftime(timestr, sizeof(timestr), DATE_FORMAT, ltime);
			strfsize(sizestr, file.st_size, sizeof(sizestr));

			printf("%c%-*.*s   %s %s\t%s%s\t%s\t%i" CRLF, type,
				width, width, displayname, timestr, sizestr,
				st->req_selector, encodedname, st->server_host, st->server_port);
		}

		/* Regular file listing */
		else {
			printf("%c%.*s\t%s%s\t%s\t%i" CRLF, type, st->out_width,
				displayname, st->req_selector, encodedname,
				st->server_host, st->server_port);
		}

		/* Free allocated directory entry */
		next:
		free(dir[i]);
	}
	free(dir);

	/* Print footer */
	footer(st);
}
