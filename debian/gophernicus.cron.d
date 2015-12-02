# /etc/cron.d/gophernicus: crontab entries for the gophernicus package

PATH=/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin
MAILTO=root

@reboot     root   if [ -x /usr/lib/gophernicus/system-info.sh ]; then /usr/lib/gophernicus/system-info.sh; fi
2 2 * * *   root   if [ -x /usr/lib/gophernicus/system-info.sh ]; then /usr/lib/gophernicus/system-info.sh; fi

# EOF
