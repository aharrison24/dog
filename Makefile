#  6/21/2000
#  Version 1.7
#
# Tested on these platforms:
#  JUnix 1.0, 1.1
#  Debian 2.0, 2.1, 2.2
#  Slackware 3.6, 4.0, 7.0
#  RedHat 5.2, 6.0, 6.1, 6.2
#  SuSE 6.4
#  FreeBSD 3.2, 3.4, 4.0
#  NetBSD 1.4.1
#  OpenBSD 2.5, 2.6
#  BSDi 4.0.1, 4.1
#  Solaris 7.0
#  SCO UnixWare 7.0
#
#  Please email any platforms you have success with to
#  dogboy@photodex.com!

INSTALL = /usr/bin/install -c
OBJS = dog.o getopt.o getopt1.o
CFLAGS = -O3 -Wall

prefix = debian/dog/usr
bindir = ${prefix}/bin
mandir = ${prefix}/share/man

%.o: %.c
	gcc ${CFLAGS} -c $< -o $@

dog:	${OBJS}
	gcc ${CFLAGS} -o dog ${OBJS}

install:	dog
	$(INSTALL) -m 644 dog.1 ${mandir}/man1
	$(INSTALL) -m 755 dog ${bindir}

clean:
	rm -f dog *.o *~
