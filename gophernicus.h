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

#ifndef _GOPHERNICUS_H
#define _GOPHERNICUS_H


/*
 * Platform detection & features
 */

/* Defaults should fit standard POSIX systems */
#define HAVE_IPv4		/* IPv4 should work anywhere */
#define HAVE_IPv6		/* Requires modern POSIX */
#define HAVE_PASSWD		/* For systems with passwd-like userdb */
#define PASSWD_MIN_UID 100	/* Minimum allowed UID for ~userdirs */
#define HAVE_LOCALES		/* setlocale() and friends */
#define HAVE_SHMEM		/* Shared memory support */
#define HAVE_UNAME		/* uname() */
#undef  HAVE_STRLCPY		/* strlcpy() from OpenBSD */
#undef  HAVE_SENDFILE		/* sendfile() in Linux & others */


/* Linux */
#ifdef __linux
#undef PASSWD_MIN_UID
#define PASSWD_MIN_UID 500
#define HAVE_SENDFILE

#include <features.h>
#if ! __GLIBC_PREREQ(2,8)
#define HAVE_BROKEN_SCANDIR	/* scandir() weirdness */
#endif

#endif


/*
 * Include headers
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <libgen.h>
#include <time.h>
#include <syslog.h>
#include <errno.h>
#include <pwd.h>
#include <limits.h>

#ifdef HAVE_SENDFILE
#include <sys/sendfile.h>
#include <fcntl.h>
#endif

#ifdef HAVE_LOCALES
#include <locale.h>
#endif

#ifdef HAVE_SHMEM
#include <sys/ipc.h>
#include <sys/shm.h>
#else
#define shm_state void
#endif

#if defined(HAVE_IPv4) || defined(HAVE_IPv6)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#ifdef HAVE_UNAME
#include <sys/utsname.h>
#endif


/*
 * Compile-time configuration
 */

/* Common stuff */
#define CRLF		"\r\n"
#define EMPTY		""
#define PARENT		".."
#define ROOT		"/"

#define FALSE		0
#define TRUE		1

#define OK		0
#define ERROR		-1

#define FOUND		0

/* Protocol names */
#define PROTO_GOPHER		"GOPHER/0"
#define PROTO_GOPHERPLUS	"GOPHER/+"

/* Gopher filetypes */
#define TYPE_TEXT	'0'
#define TYPE_MENU	'1'
#define TYPE_BINARY	'9'
#define TYPE_GIF	'g'
#define TYPE_IMAGE	'I'

/* Defaults for settings */
#define DEFAULT_ROOT	"/var/gopher"
#define DEFAULT_HOST	"localhost"
#define DEFAULT_PORT	70
#define DEFAULT_TYPE	TYPE_TEXT
#define DEFAULT_MAP	"gophermap"
#define DEFAULT_CGI	"/cgi-bin/"
#define DEFAULT_USERDIR	"public_gopher"
#define DEFAULT_ADDR	"unknown"
#define DEFAULT_WIDTH	70
#define DEFAULT_CTYPE	"US-ASCII"
#define MIN_WIDTH	40
#define MAX_WIDTH	200

/* Session defaults */
#define DEFAULT_SESSION_TIMEOUT		1800
#define DEFAULT_SESSION_MAX_KBYTES	1048576
#define DEFAULT_SESSION_MAX_HITS	1024

/* Dummy values for gopher protocol */
#define DUMMY_SELECTOR	"null"
#define DUMMY_HOST	"invalid.host\t0"

/* Safe $PATH for exec() */
#define SAFE_PATH	"/usr/bin:/bin"

/* Special requests */
#define SERVER_STATUS	"/server-status"

/* Error messages */
#define ERR_ACCESS	"Access denied!"
#define ERR_NOTFOUND	"File or directory not found!"
#define ERR_ROOT	"Refusing to run as root!"
#define ERR_NOSEL	"No selector!"
#define ERR_EXE		"Couldn't execute file!"

/* Buffers */
#define BUFSIZE		1024	/* Default size for all strings */
#define MAX_HIDDEN	32	/* Maximum number of hidden files */

/* String formats */
#define SERVER_SOFTWARE	"Gophernicus/" VERSION
#define FOOTER_FORMAT	"Gophered by " SERVER_SOFTWARE " on %s"
#define DATE_FORMAT	"%Y-%b-%d %H:%M"	/* See man 3 strftime */
#define DATE_WIDTH	17
#define DATE_LOCALE 	"POSIX"
#define USERDIR_FORMAT	"~%s", pwd->pw_name	/* See man 3 getpwent */
#define VHOST_FORMAT	"gopher://%s/"

/* File suffix to gopher filetype mappings */
#define FILETYPES \
	"0", ".txt.sh.c.cpp.h.log.conf.", \
	"1", ".map.menu.", \
	"5", ".gz.tgz.tar.zip.bz2.rar.", \
	"7", ".q.qry.", \
	"9", ".iso.so.o.xls.doc.ppt.ttf.bin.", \
	"c", ".ics.ical.", \
	"g", ".gif.", \
	"h", ".html.htm.xhtml.css.swf.rdf.rss.xml.", \
	"I", ".jpg.jpeg.png.bmp.svg.tif.tiff.ico.xbm.xpm.pcx.", \
	"M", ".mbox.", \
	"p", ".pdf.ps.", \
	"s", ".mp3.wav.mid.wma.flac.ogg.aiff.aac.", \
	"v", ".avi.mp4.mpg.mov.qt.asf.mpv.", \
	NULL, NULL

/* Latin-1 to US-ASCII look-alike conversion table */
#define LATIN1_START 128
#define LATIN1_END 255

#define LATIN1_ASCII \
	"E?,f..++^%S<??Z?" \
	"?''\"\"*--~?s>??zY" \
	" !c_*Y|$\"C?<?-R-" \
	"??23'u?*,1?>????" \
	"AAAAAAACEEEEIIII" \
	"DNOOOOO*OUUUUYTB" \
	"aaaaaaaceeeeiiii" \
	"dnooooo/ouuuuyty"


/* Struct for keeping the current options & state */
typedef struct {

	/* Request */
	char req_protocol[16];
	char req_selector[BUFSIZE];
	char req_realpath[BUFSIZE];
	char req_query_string[BUFSIZE];
	char req_referrer[BUFSIZE];
	char req_remote_addr[64];
	/* char req_remote_host[256]; */
	char req_filetype;
	off_t req_filesize;

	/* Output */
	int  out_width;
	char out_ctype[16];

	/* Settings */
	char server_root[BUFSIZE];
	char server_host_default[256];
	char server_host[256];
	int  server_port;

	char default_filetype;
	char map_file[64];
	char cgi_file[64];
	char user_dir[64];

	char hidden[MAX_HIDDEN][NAME_MAX];
	int hidden_count;

	/* Session */
	int session_timeout;
	int session_max_kbytes;
	int session_max_hits;

	/* Feature options */
	char opt_parent;
	char opt_footer;
	char opt_date;
	char opt_syslog;
	char opt_magic;
	char opt_vhost;
	char debug;
} state;

/* Shared memory for session & accounting data */
#ifdef HAVE_SHMEM

#define SHM_KEY		0xbeeb0003	/* Unique identifier + struct version */
#define SHM_MODE	0600		/* Access mode for the shared memory */
#define SHM_SESSIONS	256		/* Max amount of user sessions to track */

typedef struct {
	long hits;
	long kbytes;

	time_t req_atime;
	char req_selector[128];
	char req_remote_addr[64];
	char req_filetype;

	char server_host[64];
	int  server_port;

	int  out_width;
	char out_ctype[16];

	char opt_parent;
	char opt_footer;
	char opt_date;
} shm_session;

typedef struct {
	time_t start_time;
	long hits;
	long kbytes;
	shm_session session[SHM_SESSIONS];
} shm_state;

#endif


/*
 * Useful macros
 */
#define info(str) printf("i%s\t%s\t%s" CRLF, str, DUMMY_SELECTOR, DUMMY_HOST);
#define strclear(str) str[0] = '\0';
#define sstrlcpy(dest, src) strlcpy(dest, src, sizeof(dest))


/*
 * Include generated headers
 */
#include "functions.h"
#include "readme.h"
#include "license.h"
#include "error_gif.h"

#endif

