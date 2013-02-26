## Makefile.am -- Process this file with automake to produce Makefile.in
SUBDIRS = src

sysconfdir = /etc/itvencoder
sysconf_DATA = conf/itvencoder.conf.sample

channelsconfdir = $(sysconfdir)/channels.d
channelsconf_DATA = conf/channels.d/channel-1.conf.sample conf/channels.d/channel-2.conf.sample conf/channels.d/channel-3.conf.sample

logrotateconfdir = /etc/logrotate.d
logrotateconf_DATA = conf/itvencoder

supervisordconfdir =/etc
supervisordconf_DATA = conf/supervisord.conf

EXTRA_DIST = conf src/*.h src/libjansson.a
