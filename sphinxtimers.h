//
// $Id: sphinxtimers.h,v 1.4 2005/04/17 20:49:59 shodan Exp $
//

//
// Copyright (c) 2001-2005, Andrew Aksyonoff. All rights reserved.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License. You should have
// received a copy of the GPL license along with this program; if you
// did not, you can find it at http://www.gnu.org/
//

DECLARE_TIMER ( collect_hits )
DECLARE_TIMER ( sort_hits )
DECLARE_TIMER ( write_hits )
DECLARE_TIMER ( invert_hits )
DECLARE_TIMER ( read_hits )

DECLARE_TIMER ( src_document )
DECLARE_TIMER ( src_mysql )
DECLARE_TIMER ( src_xmlpipe )

DECLARE_TIMER ( query_init )
DECLARE_TIMER ( query_load_dir )
DECLARE_TIMER ( query_load_words )
DECLARE_TIMER ( query_match )
DECLARE_TIMER ( query_sort )

//
// $Id: sphinxtimers.h,v 1.4 2005/04/17 20:49:59 shodan Exp $
//

