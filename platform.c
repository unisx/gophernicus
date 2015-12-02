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
#ifdef __OSX_CARBON__
#include "/Developer/Headers/FlatCarbon/Gestalt.h"
#endif


/*
 * Get OS name, version & architecture we're running on
 */
void platform(state *st)
{
#ifdef HAVE_UNAME
#if defined(_AIX) || defined(__linux)
	FILE *fp;
#endif
#ifdef __linux
	char buf[BUFSIZE];
#endif
#ifdef __OSX_CARBON__
	SInt32 major;
	SInt32 minor;
	SInt32 hardware;
#endif
	struct utsname name;
	char sysname[32];
	char release[32];
	char machine[32];
	char *c;

	/* Fetch system information */
	uname(&name);

	strclear(sysname);
	strclear(release);
	strclear(machine);

	/* AIX-specific */
#ifdef _AIX

	/* Fix uname() results */
	sstrlcpy(machine, "powerpc");
	snprintf(release, sizeof(release), "%s.%s",
		name.version, name.release);

	/* Get hardware name using shell uname */
	if (!*st->server_description &&
	    (fp = popen("/usr/bin/uname -M", "r"))) {

		fgets(st->server_description,
			sizeof(st->server_description), fp);
		pclose(fp);

		strreplace(st->server_description, ',', ' ');
		chomp(st->server_description);
	}
#endif

	/* Mac OS X Carbonized version (32bit only?) */
#ifdef __OSX_CARBON__

	/* Hardcode correct OS name (instead of "Darwin") */
	sstrlcpy(sysname, "MacOSX");

	/* Get offical OS X major.minor version */
	if (Gestalt(gestaltSystemVersionMajor, &major) != OK) major = 0;
	if (Gestalt(gestaltSystemVersionMinor, &minor) != OK) minor = 0;

	snprintf(release, sizeof(release), "%i.%i",
		(int) major, (int) minor);

	/* Get hardware name */
	if (!*st->server_description &&
	    Gestalt(gestaltUserVisibleMachineName, &hardware) == OK) {

		/* Clones are illegal now... */
		sstrlcpy(st->server_description, "Apple ");
		sstrlcat(st->server_description, (char *) hardware + 1);

		/* Remove hardware revision */
		for (c = st->server_description; *c; c++)
			if (*c >= '0' && *c <= '9') { *c = '\0'; break; }
	}
#endif

	/* Linux uname() just says Linux/2.6 - let's dig deeper... */
#ifdef __linux

	/* Most Linux ARM boards have hardware name in /proc/cpuinfo */
	if (!*st->server_description && (fp = fopen("/proc/cpuinfo" , "r"))) {

		while (fgets(buf, sizeof(buf), fp)) {
			if ((c = strkey(buf, "Hardware"))) {
				sstrlcpy(st->server_description, c);
				chomp(st->server_description);
				break;
			}
		}
		fclose(fp);
	}

	/* Identify Linux distribution using lsb_release */
	if (!*sysname && (fp = popen("/usr/bin/lsb_release -i -s", "r"))) {
		fgets(sysname, sizeof(sysname), fp);
		chomp(sysname);
		pclose(fp);
	}

	if (!*release && (fp = popen("/usr/bin/lsb_release -r -s", "r"))) {
		fgets(release, sizeof(release), fp);
		chomp(release);
		pclose(fp);
	}
#endif

	/* Fill in the blanks using uname() data */
	if (!*sysname) sstrlcpy(sysname, name.sysname);
	if (!*release) sstrlcpy(release, name.release);
	if (!*machine) sstrlcpy(machine, name.machine);

	/* We're only interested in major.minor version */
	if ((c = strchr(release, '.'))) if ((c = strchr(c + 1, '.'))) *c = '\0';
	if ((c = strchr(release, '-'))) *c = '\0';
	if ((c = strchr(release, '/'))) *c = '\0';

	/* Create a nicely formatted platform string */
	snprintf(st->server_platform, sizeof(st->server_platform), "%s/%s %s",
		sysname,
		release,
		machine);

	/* Debug */
	if (st->debug) {
		syslog(LOG_INFO, "generated platform string \"%s\"",
			st->server_platform);
	}

#else
	/* Fallback reply */
	sstrlcpy(st->server_platform, "Unknown computer-like system");
#endif
}


/*
 * Return current CPU load
 */
float loadavg(void)
{
	FILE *fp;
	char buf[BUFSIZE];

	/* Faster Linux version */
#ifdef __linux
	buf[0] = '\0';
	if ((fp = fopen("/proc/loadavg" , "r")) == NULL) return 0;
	fgets(buf, sizeof(buf), fp);
	fclose(fp);

	return (float) atof(buf);

	/* Generic slow version - parse the output of uptime */
#else
#ifdef HAVE_POPEN
	char *c;

	if ((fp = popen("/usr/bin/uptime", "r"))) {
		fgets(buf, sizeof(buf), fp);
		pclose(fp);

		if ((c = strstr(buf, "average: ")) || (c = strstr(buf, "averages: ")))
			return (float) atof(c + 10);
	}
#endif

	/* Fallback reply */
	return 0;
#endif
}


