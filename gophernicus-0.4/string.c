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
 * Return last character of a string
 */
char strlast(char *str)
{
	size_t len;

	if ((len = strlen(str) - 1) >= 0) return str[len];
	else return 0;
}


/*
 * Remove CRLF from a string
 */
void chomp(char *str)
{
	char *c;

	if ((c = strrchr(str, '\n'))) *c = '\0';
	if ((c = strrchr(str, '\r'))) *c = '\0';
}


/*
 * Convert UTF-8/Latin-1 string to 7bit ASCII
 */
void strntoascii(char *out, char *in, size_t outsize)
{
	char latin2ascii[] = LATIN1_ASCII;
	unsigned long c;
	size_t len;
	int i;

	/* Loop through the input string */
	len = strlen(in);
	while (--outsize && len > 0) {

		/* Get one input char */
		c = (unsigned char) *in++;
		len--;

		/* 7-bit chars are the same in all three charsets */
		if (!(c & 0x80)) {
			*out++ = c;
			continue;
		}

		/* We'll assume Latin-1 which requres 0 extra bytes */
		i = 0;

		/* UTF-8? */
		if ((*in & 0xc0) == 0x80) {

			/* Four-byte UTF-8? */
			if ((c & 0xf8) == 0xf0 && len >= 3) {
				c = c & 0x07;
				i = 3;
			}

			/* Three-byte UTF-8? */
			else if ((c & 0xf0) == 0xe0 && len >= 2) {
				c = c & 0x0f;
				i = 2;
			}

			/* Two-byte UTF-8? */
			else if ((c & 0xe0) == 0xc0 && len >= 1) {
				c = c & 0x1f;
				i = 1;
			}

			/* Parse rest of the UTF-8 bytes */
			while (i--) {
				c <<= 6;
				c += *in++ & 0x3f;
				len--;
			}
		}

		/* Convert Latin-1 (and UTF-8) to ASCII */
		if (c >= LATIN1_START && c <= LATIN1_END)
			c = latin2ascii[c - LATIN1_START];
		else
			c = '?';

		/* Output converted char */
		*out++ = (unsigned char) c;

	}

	/* Zero-terminate output */
	*out = '\0';
}


/*
 * Encode string with #OCT encoding
 */
void strnencode(char *out, const char *in, size_t outsize)
{
	unsigned char c;

	/* Loop through the input string */
	while (--outsize) {

		/* End of source? */
		if (!(c = *in++)) break;

		/* Need to encode the char? */
		if (c < '+' || c > '~') {

			/* Can we fit the encoded version into outbuffer? */
			if (outsize < 5) break;

			/* Output encoded char */
			sprintf(out, "#%.3o", c);
			out += 4;
		}

		/* Copy regular chars */
		else *out++ = c;
	}

	/* Zero-terminate output */
	*out = '\0';
}


/*
 * Decode both %HEX and #OCT encodings
 */
void strndecode(char *out, char *in, size_t outsize)
{
	unsigned char c;
	unsigned int i;

	/* Loop through the input string */
	while (--outsize) {

		/* End of source? */
		if (!(c = *in++)) break;

		/* Parse %hex encoding */
		if (c == '%' && strlen(in) >= 2) {
			sscanf(in, "%2x", &i);
			*out++ = i;
			in += 2;
			continue;
		}

		/* Parse #octal encoding */
		if (c == '#' && strlen(in) >= 3) {
			sscanf(in, "%3o", &i);
			*out++ = i;
			in += 3;
			continue;
		}

		/* Copy non-encoded chars */
		*out++ = c;
	}

	/* Zero-terminate output */
	*out = '\0';
}


/*
 * Format number to human-readable filesize with unit
 */
void strfsize(char *out, off_t size, size_t outsize)
{
	static char *unit[] = { "KB", "MB", "GB", "TB", "PB", NULL };
	int u;
	float s;

	/* Start with kilobytes */
	s = ((float) size) / 1024;
	u = 0;

	/* Loop through the units until the size is small enough */
	while (s >= 1000 && unit[(u + 1)]) {
		s = s / 1024;
		u++;
	}

	/* Format size */
	snprintf(out, outsize, "%7.1f %s", s, unit[u]);
}


#ifndef HAVE_STRLCPY
/*
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <string.h>

/*
 * Copy src to string dst of size siz.  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz == 0).
 * Returns strlen(src); if retval >= siz, truncation occurred.
 */
size_t strlcpy(char *dst, const char *src, size_t siz)
{
	char *d = dst;
	const char *s = src;
	size_t n = siz;

	/* Copy as many bytes as will fit */
	if (n != 0) {
		while (--n != 0) {
			if ((*d++ = *s++) == '\0')
				break;
		}
	}

	/* Not enough room in dst, add NUL and traverse rest of src */
	if (n == 0) {
		if (siz != 0)
			*d = '\0';		/* NUL-terminate dst */
		while (*s++)
			;
	}

	return(s - src - 1);	/* count does not include NUL */
}

/*
 * Appends src to string dst of size siz (unlike strncat, siz is the
 * full size of dst, not space left).  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz <= strlen(dst)).
 * Returns strlen(src) + MIN(siz, strlen(initial dst)).
 * If retval >= siz, truncation occurred.
 */
size_t strlcat(char *dst, const char *src, size_t siz)
{
	char *d = dst;
	const char *s = src;
	size_t n = siz;
	size_t dlen;

	/* Find the end of dst and adjust bytes left but don't go past end */
	while (n-- != 0 && *d != '\0')
		d++;
	dlen = d - dst;
	n = siz - dlen;

	if (n == 0)
		return(dlen + strlen(s));
	while (*s != '\0') {
		if (n != 1) {
			*d++ = *s;
			n--;
		}
		s++;
	}
	*d = '\0';

	return(dlen + (s - src));	/* count does not include NUL */
}

#endif
