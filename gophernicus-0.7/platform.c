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
 * Generate OS name, version & machine we're running on
 */
void platform(state *st)
{
#ifdef HAVE_UNAME
#ifdef _AIX
	FILE *fp;
#endif
	struct utsname name;
	char release[16];
	char machine[32];
	char *c;

	/* Fetch system name */
	uname(&name);

	/* AIX wants to do *everything* differently... */
#ifdef _AIX
	snprintf(release, sizeof(release), "%s.%s",
		name.version, name.release);

	if ((fp = popen("/usr/bin/uname -M", "r")) != NULL) {
		fgets(machine, sizeof(machine), fp);
		pclose(fp);

		chomp(machine);
		if ((c = strchr(machine, ','))) sstrlcpy(machine, c + 1);
	}
	else sstrlcpy(machine, name.machine);

	/* Normal unices do the right thing */
#else
	sstrlcpy(release, name.release);
	sstrlcpy(machine, name.machine);

	/* We're only interested in major.minor version */
	if ((c = strchr(release, '.'))) if ((c = strchr(c + 1, '.'))) *c = '\0';
	if ((c = strchr(release, '-'))) *c = '\0';
#endif

	/* Create a nicely formatted platform string */
	snprintf(st->server_platform, sizeof(st->server_platform), "%s %s %s",
		name.sysname,
		release,
		machine);

	/* Debug */
	if (st->debug) {
		syslog(LOG_INFO, "generated platform string \"%s\"",
			st->server_platform);
	}

#else
	/* Fallback reply */
	strlcpy(out, "Unknown computer-like system", outsize);
#endif

}


/*
 * Return current CPU load
 */
float loadavg(void)
{
        FILE *fp;
        char buf[BUFSIZE];

	/* Linux version */
#ifdef __linux
	buf[0] = '\0';
        if ((fp = fopen("/proc/loadavg" , "r")) == NULL) return 0;
	fgets(buf, sizeof(buf), fp);
        fclose(fp);

	return (float) atof(buf);

	/* Generic slow version - parse the output of uptime */
#else
	char *c;

	if ((fp = popen("/usr/bin/uptime", "r")) != NULL) {
		fgets(buf, sizeof(buf), fp);
		pclose(fp);

		if ((c = strstr(buf, "average: ")))
			return (float) atof(c + 10);
	}

	/* Fallback reply */
	return 0;
#endif
}


