#
# Copyright (C) 2009-2010 Till Harbaum <till@harbaum.org>.
#
# This file is part of zeemoted.
#
# zeemoted is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# zeemoted is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with zeemoted. If not, see <http://www.gnu.org/licenses/>.
#

VERSION=1.2

zeemoted: zeemoted.c zeemote.h
	gcc -O2 -Wall -DVERSION=\"$(VERSION)\" -o $@ $< -lbluetooth -lfakekey -lX11

install: zeemoted
	install -d $(DESTDIR)/usr/bin
	install zeemoted $(DESTDIR)/usr/bin

run: zeemoted
	./zeemoted -j 00:1C:4D:00:8F:08

clean:
	rm -f zeemoted *~ *.bak
