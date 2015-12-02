/* Automatically generated from README */

#define README \
	"Gophernicus Server\n" \
	"Copyright (c) 2009-2010 Kim Holviala <kim@holviala.com>\n" \
	"\n" \
	"Gophernicus Server is a modern full-featured (and hopefully) secure\n" \
	"gopher daemon for inetd. It is licensed under the BSD license.\n" \
	"\n" \
	"Command line options:\n" \
	"    -h hostname   Change server hostname             [$HOSTNAME]\n" \
	"    -p port       Change server port                 [70]\n" \
	"    -r root       Change gopher root                 [/var/gopher]\n" \
	"    -t type       Change default filetype            [0]\n" \
	"    -w width      Change default page width          [70]\n" \
	"    -g mapfile    Change gophermap file              [gophermap]\n" \
	"    -c cgifilter  Change CGI script filter           [/cgi-bin/]\n" \
	"    -u userdir    Change users gopherspace           [public_gopher]\n" \
	"    -o charset    Change default output charset      [US-ASCII]\n" \
	"\n" \
	"    -s seconds    Session timeout in seconds         [1800]\n" \
	"    -i hits       Maximum hits until throttling      [4096]\n" \
	"    -k kbytes     Maximum transfer until throttling  [4194304]\n" \
	"\n" \
	"    -e ext:type   Map file extension to gopher filetype\n" \
	"    -f ext:bin    Filter files with extension through the binary\n" \
	"\n" \
	"    -nv           Disable virtual hosting\n" \
	"    -nl           Disable parent directory links\n" \
	"    -nf           Disable menu footer\n" \
	"    -nd           Disable date and filesize in menus\n" \
	"    -nc           Disable file content detection\n" \
	"    -no           Disable charset conversion for output\n" \
	"    -ns           Disable logging to syslog\n" \
	"\n" \
	"    -d            Debug to syslog (not for production use)\n" \
	"    -l            Display the BSD license\n" \
	"    -?            Display this help\n" \
	"\n" \
	""
