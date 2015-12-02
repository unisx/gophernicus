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
 * Return OS name & platform
 */
char *platform()
{
#ifdef HAVE_UNAME
	struct utsname name;
	static char buf[BUFSIZE];
	char *c;

	/* Fetch system name */
	uname(&name);

	/* We're only interested in marjor.minor */
	if ((c = strchr(name.release, '.')) != NULL)
		if ((c = strchr(c + 1, '.')) != NULL) *c = '\0';

	/* We don't want -VERSION */
	if ((c = strchr(name.release, '-')) != NULL) *c = '\0';

	/* Create a nicely formatted platform string */
	snprintf(buf, sizeof(buf), "%s %s %s",
		name.sysname,
		name.release,
		name.machine);

	return buf;

#else
	/* Fallback reply */
	return "Unknown computer-like system";
#endif

}


/*
 * Return current CPU load
 */
float loadavg(void)
{
#ifdef __linux
	/* Linux version */
        FILE *fp;
        char buf[BUFSIZE];

	/* Read one line from /proc/loadavg */
	buf[0] = '\0';
        if ((fp = fopen("/proc/loadavg" , "r")) == NULL) return 0;
	fgets(buf, sizeof(buf) - 1, fp);
        fclose(fp);

	/* Return CPU load */
	return (float) atof(buf);

#else
	/* Fallback reply */
	return 0;
#endif
}


