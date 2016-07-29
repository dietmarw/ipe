Ipe 7.2.5
July, 2016

                    Ipe extensible drawing editor
                    =============================

Introduction
============

This is the extensible drawing editor Ipe.  Ipe allows you to create
figures in PDF format, for inclusion into LaTeX (or plain TeX)
documents as well as stand-alone PDF documents, for instance to print
transparencies or for on-line presentations.

See the manual for an introduction. It is available in both HTML and
PDF format, either in the "build/doc" subdirectory of the source
distribution, or by selecting Help -> Ipe manual from the Ipe user
interface. 


The separate file "install.txt" explains how to build and install Ipe
on Unix.  Ipe is also available for Windows in the form of a zip
archive. On Windows, all you need to do is unzip the archive, and you
are ready to run.  (If you do want to compile on Windows yourself,
it's not difficult - based on the Unix instructions you should be able
to work it out.)

--------------------------------------------------------------------

    This file is part of the extensible drawing editor Ipe.
    Copyright (C) 1993-2016  Otfried Cheong

    Ipe is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    As a special exception, you have permission to link Ipe with the
    CGAL library and distribute executables, as long as you follow the
    requirements of the Gnu General Public License in regard to all of
    the software in the executable aside from CGAL.

    Ipe is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
    or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
    License for more details.

    You should have received a copy of the GNU General Public License
    along with Ipe; if not, you can find it at
    "http://www.gnu.org/copyleft/gpl.html", or write to the Free
    Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

--------------------------------------------------------------------

Ipe uses the Zlib library (www.gzip.org/zlib), the Freetype 2 library
(www.freetype.org), the Cairo library (www.cairographics.org), as well
as some code from Xpdf (www.foolabs.com/xpdf).  Ipe contains the Lua
5.2 or 5.3 interpreter (www.lua.org), and - except on Windows - uses
the Qt toolkit for its graphical user interface (www.qt.io).  Ipe
relies on Pdflatex for rendering text.

--------------------------------------------------------------------

Before reporting a bug, check that you have the latest version, and
check that it is not yet mentioned in the FAQ on the Ipe webpage.

You can report bugs on the bug tracking system at
"http://ipe.otfried.org/bugzilla.html".  Check the existing
reports to see whether your bug has already been reported.  Please do
not send bug reports directly to me (the first thing I would do with
the report is to enter it into the bug tracking system).

Suggestions for features, or random comments on Ipe can be sent to the
Ipe discussion mailing list at <ipe-discuss@lists.science.uu.nl>.  If
you have problems installing or using Ipe and cannot find a guru among
your real-life friends, the Ipe discussion mailing list would also be
the best place to ask.

You can send suggestions or comments directly to me by Email, but you
should then not expect a reply.  I cannot dedicate much time to Ipe,
and the little time I have I prefer to put into development.  I'm much
more likely to get involved in a discussion of desirable features on
the mailing list, where anyone interested can participate, rather than
by direct Email.

If you write interesting ipelets that might be useful to others,
please put a link or copy (as you prefer) on the Ipe wiki.  Feel free
to advertise them on the Ipe discussion list!

	Otfried Cheong
	School of Computing
	KAIST
	Daejeon, South Korea
	Email: otfried@ipe.airpost.net
	Ipe webpage: http://ipe.otfried.org

--------------------------------------------------------------------
