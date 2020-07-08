//
// $Id: sphinx.h,v 1.54 2006/01/10 22:03:58 shodan Exp $
//

//
// Copyright (c) 2001-2005, Andrew Aksyonoff. All rights reserved.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License. You should have
// received a copy of the GPL license along with this program; if you
// did not, you can find it at http://www.gnu.org/
//

#ifndef _sphinx_
#define _sphinx_

/////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
	#define USE_MYSQL		1	/// whether to compile with MySQL support
	#define USE_WINDOWS		1	/// whether to compile for Windows
#else
	#define USE_MYSQL		1	/// whether to compile with MySQL support
	#define USE_WINDOWS		0	/// whether to compile for Windows
#endif

/////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


#if USE_MYSQL
#if USE_WINDOWS
	#include <winsock2.h>
	#include <mysql.h>
#else
	#include <mysql/mysql.h>
#endif // USE_WINDOWS
#endif // USE_MYSQL


#if USE_WINDOWS
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>

	#define strcasecmp			strcmpi
	#define strncasecmp			_strnicmp
	#define snprintf			_snprintf

	typedef __int64				SphOffset_t;
	#define STDOUT_FILENO		fileno(stdout)
#else
	typedef unsigned int		DWORD;
	typedef unsigned short		WORD;
	typedef unsigned char		BYTE;
	typedef off_t				SphOffset_t;
#endif

#include "sphinxstd.h"

/////////////////////////////////////////////////////////////////////////////

#define SPHINX_VERSION			"0.9.5-dev"
#define SPHINX_BANNER			"Sphinx " SPHINX_VERSION "\nCopyright (c) 2001-2005, Andrew Aksyonoff\n\n"
#define SPHINX_SEARCHD_PROTO	3

#define SPH_MAX_QUERY_WORDS		10
#define SPH_MAX_WORD_LEN		64
#define SPH_MAX_FILENAME_LEN	512
#define SPH_MAX_FIELD_COUNT		32

#define SPH_CLOG_BITS_DIR		10
#define SPH_CLOG_BITS_PAGE		22
#define SPH_CLOG_DIR_PAGES		(1 << SPH_CLOG_BITS_DIR)

#define SPH_CACHE_WRITE			1048576

/////////////////////////////////////////////////////////////////////////////

void	sphDie ( char *message, ... );
void *	sphMalloc ( size_t size );
void *	sphRealloc ( void *ptr, size_t size );
void 	sphFree ( void * ptr );
char *	sphDup ( const char *s );

/// time, in seconds
float			sphLongTimer ();

/// Sphinx CRC32 implementation
DWORD			sphCRC32 ( const BYTE * pString );

/// replaces all occurences of sMacro in sTemplate with textual representation of iValue
char *			sphStrMacro ( const char * sTemplate, const char * sMacro, int iValue );

/// new that never returns NULL (it crashes instead)
void *			operator new ( size_t iSize );

/// new that never returns NULL (it crashes instead)
void *			operator new [] ( size_t iSize );

/////////////////////////////////////////////////////////////////////////////
// TOKENIZERS
/////////////////////////////////////////////////////////////////////////////

/// generic tokenizer
class ISphTokenizer
{
public:
	/// virtualizing dtor
	virtual				~ISphTokenizer () {}

	/// pass next buffer
	virtual void		SetBuffer ( BYTE * sBuffer, int iLength, bool bLast ) = 0;

	/// get next token
	virtual BYTE *		GetToken () = 0;

	/// set new translation table
	/// returns true on success, false on failure
	virtual bool		SetCaseFolding ( const char * sConfig ) = 0;
};

/// create SBCS tokenizer
ISphTokenizer *			sphCreateSBCSTokenizer ();

/// create UTF-8 tokenizer
ISphTokenizer *			sphCreateUTF8Tokenizer ();

/////////////////////////////////////////////////////////////////////////////
// DICTIONARIES
/////////////////////////////////////////////////////////////////////////////

/// morphology flags
enum ESphMorphology
{
	SPH_MORPH_NONE				= 0,
	SPH_MORPH_STEM_EN			= (1<<1),
	SPH_MORPH_STEM_RU_CP1251	= (1<<2),
	SPH_MORPH_STEM_RU_UTF8		= (1<<3),
};


/// abstract word dictionary interface
struct CSphDict
{
	/// virtualizing dtor
	virtual				~CSphDict () {}

	/// get word ID by word
	virtual DWORD		GetWordID ( BYTE * pWord ) = 0;

	/// load stopwords from given files
	virtual void		LoadStopwords ( const char * sFiles, ISphTokenizer * pTokenizer ) = 0;
};


/// CRC32 dictionary
struct CSphDict_CRC32 : CSphDict
{
	/// ctor
	/// iMorph is a combination of ESphMorphology flags
						CSphDict_CRC32 ( DWORD iMorph );

	/// virtualizing dtor
	virtual				~CSphDict_CRC32 () {}

	/// get word ID by word
	/// does requested morphology and returns CRC32
	virtual DWORD		GetWordID ( BYTE * pWord );

	/// load stopwords from given files
	virtual void		LoadStopwords ( const char * sFiles, ISphTokenizer * pTokenizer );

protected:
	DWORD				m_iMorph;		///< morphology flags
	int					m_iStopwords;	///< stopwords count
	DWORD *				m_pStopwords;	///< stopwords ID list
};

/////////////////////////////////////////////////////////////////////////////
// DATASOURCES
/////////////////////////////////////////////////////////////////////////////

/// hit info
struct CSphWordHit
{
	DWORD	m_iDocID;		///< document ID
	DWORD	m_iWordID;		///< word ID in current dictionary
	DWORD	m_iWordPos;		///< word position in current document
};


/// document info
struct CSphDocInfo
{
	DWORD	m_iDocID;		///< document ID
	DWORD	m_iGroupID;		///< documents group ID
	DWORD	m_iTimestamp;	///< document timestamp
};


/// source statistics
struct CSphSourceStats
{
	/// how much documents
	int				m_iTotalDocuments;

	/// how much bytes
	SphOffset_t		m_iTotalBytes;


	/// ctor
	CSphSourceStats () :
		m_iTotalDocuments ( 0 ),
		m_iTotalBytes ( 0 )
	{};
};


/// generic data source
class CSphHTMLStripper;
class CSphSource
{
public:
	CSphVector<CSphWordHit>				m_dHits;	///< current document split into words
	CSphDocInfo							m_tDocInfo;


public:
	/// ctor
										CSphSource ();

	/// dtor
	virtual								~CSphSource ();

	/// set dictionary
	void								SetDict ( CSphDict * dict );

	/// set HTML stripping mode
	/// sExtractAttrs defines what attributes to store
	/// sExtractAttrs format is "img=alt; a=alt,title"
	/// sExtractAttrs can be empty, this means that all the HTML will be stripped
	/// sExtractAttrs can be NULL, this means that no HTML stripping will be performed
	/// returns NULL on success
	/// returns error position on sConfig parsing failure
	const char *						SetStripHTML ( const char * sExtractAttrs );

	/// set tokenizer
	void								SetTokenizer ( ISphTokenizer * pTokenizer );

	/// get stats
	virtual const CSphSourceStats *		GetStats ();

public:
	/// document getter
	/// to be implemented by descendants
	virtual int							Next () = 0;

	/// field count getter
	/// to be implemented by descendants
	/// MUST be called AFTER the indexing is over
	/// because at indexing stage, we might not be sure of the exact count
	virtual int							GetFieldCount () = 0;

	/// post-index callback
	/// gets called when the indexing is succesfully (!) over
	virtual void						PostIndex () {}

protected:
	ISphTokenizer *						m_pTokenizer;	///< my tokenizer
	CSphDict *							m_pDict;		///< my dict
	CSphSourceStats						m_iStats;		///< my stats
	bool								m_bStripHTML;	///< whether to strip HTML
	CSphHTMLStripper *					m_pStripper;	///< my HTML stripper
};


/// generic document source
/// provides multi-field support and generic tokenizer
struct CSphSource_Document : CSphSource
{
	/// ctor
							CSphSource_Document () : m_bCallWordCallback ( false ) {}

	/// my generic tokenizer
	virtual int				Next ();

	/// this is what we can call for my descendants
	virtual void			WordCallback ( char * word ) { word = word; }

	/// field data getter
	/// to be implemented by descendants
	virtual BYTE **			NextDocument () = 0;

	/// field count getter
	virtual int				GetFieldCount ();

protected:
	/// my field count
	/// MUST be filled by NextDocument ()
	int						m_iFieldCount;

	/// whether to call the callback
	bool					m_bCallWordCallback;
};


/// generic text source
/// one-field plain-text documents
struct CSphSource_Text : CSphSource_Document
{
					CSphSource_Text () { m_iFieldCount = 1; }
	BYTE **			NextDocument ();
	virtual BYTE *	NextText () = 0;
};


#if USE_MYSQL
/// MySQL source params
struct CSphSourceParams_MySQL
{
	// query params
	const char *	m_sQuery;
	const char *	m_sQueryPre;
	const char *	m_sQueryPost;
	const char *	m_sQueryRange;
	const char *	m_sQueryPostIndex;
	const char *	m_sGroupColumn;
	const char *	m_sDateColumn;
	int				m_iRangeStep;

	// connection params
	const char *	m_sHost;
	const char *	m_sUser;
	const char *	m_sPass;
	const char *	m_sDB;
	int				m_iPort;
	const char *	m_sUsock;

	/// ctor which sets defaults
					CSphSourceParams_MySQL ();
};


/// MySQL source implementation
/// multi-field plain-text documents fetched from given query
struct CSphSource_MySQL : CSphSource_Document
{
						CSphSource_MySQL ();
	virtual				~CSphSource_MySQL ();

	bool				Init ( CSphSourceParams_MySQL * pParams );
	virtual BYTE **		NextDocument ();
	virtual void		PostIndex ();

protected:
	MYSQL_RES *			m_pSqlResult;
	MYSQL_ROW			m_tSqlRow;
	MYSQL				m_tSqlDriver;
	char *				m_sSqlDSN;

	char *				m_sQuery;		///< main fetch query
	char *				m_sQueryPost;	///< post-fetch query
	int					m_iGroupColumn;	///< group_id column number
	int					m_iDateColumn;	///< date column number

	BYTE *				m_dFields [ SPH_MAX_FIELD_COUNT ];
	int					m_dRemapFields [ SPH_MAX_FIELD_COUNT ];

	int					m_iRangeStep;	///< ID range step, -1 if not using ranges
	int					m_iMinID;		///< grand min ID
	int					m_iMaxID;		///< grand max ID
	int					m_iCurrentID;	///< current min ID

	DWORD				m_iMaxFetchedID;///< max actually fetched ID

	bool						m_bSqlConnected;
	CSphSourceParams_MySQL *	m_pParams;

	static const int			MACRO_COUNT = 2;
	static const char * const	MACRO_VALUES [ MACRO_COUNT ];

protected:
	bool				RunQueryStep ();
	int					GetColumnIndex ( const char * sColumn );
};
#endif


/// XML pipe source implementation
class CSphSource_XMLPipe : public CSphSource
{
public:
	/// ctor
					CSphSource_XMLPipe ();

	/// dtor
					~CSphSource_XMLPipe ();

	/// initializer
	bool			Init ( const char * sCommand );

	/// hit chunk getter
	virtual int		Next ();

	/// field count getter
	virtual int		GetFieldCount ();

private:
	enum Tag_e
	{
		TAG_DOCUMENT = 0,
		TAG_ID,
		TAG_GROUP,
		TAG_TITLE,
		TAG_BODY
	};

private:
	/// are we scanning body or expecting document?
	bool			m_bBody;

	/// what's our current tag
	Tag_e			m_eTag;

	/// tag name
	const char *	m_pTag;

	/// tag name length
	int				m_iTagLength;

	/// incoming stream
	FILE *			m_pPipe;

	/// buffer
	BYTE			m_sBuffer [ 4096 ];

	/// current buffer pos
	BYTE *			m_pBuffer;

	/// buffered end pos
	BYTE *			m_pBufferEnd;

private:
	/// current word position
	int				m_iWordPos;

private:
	/// set current tag
	void			SetTag ( const char * sTag );

	/// read in some more data
	/// moves everything from current ptr (m_pBuffer) to the beginng
	/// reads in as much data as possible to the end
	/// returns false on EOF
	bool			UpdateBuffer ();

	/// skips whitespace
	/// does buffer updates
	/// returns false on EOF
	bool			SkipWhitespace ();

	/// check if what's at current pos is either opening/closing current tag (m_pTag)
	/// return false on failure
	bool			CheckTag ( bool bOpen, bool bStrict=true );

	/// skips whitespace and opening/closing current tag (m_pTag)
	/// returns false on failure
	bool			SkipTag ( bool bOpen, bool bWarnOnEOF=true, bool bStrict=true );

	/// scan for tag with integer value
	bool			ScanInt ( const char * sTag, DWORD * pRes, bool bStrict=true );

	/// scan for tag with string value
	bool			ScanStr ( const char * sTag, char * pRes, int iMaxLength );
};

/////////////////////////////////////////////////////////////////////////////
// SEARCH QUERIES
/////////////////////////////////////////////////////////////////////////////

/// search query match
struct CSphMatch : public CSphDocInfo
{
	int			m_iWeight;
};


/// search query sorting orders
enum ESphSortOrder
{
	SPH_SORT_RELEVANCE = 0,		///< sort by document relevance desc, then by date
	SPH_SORT_DATE_DESC,			///< sort by document date desc, then by relevance desc
	SPH_SORT_DATE_ASC,			///< sort by document date asc, then by relevance desc
	SPH_SORT_TIME_SEGMENTS,		///< sort by time segments (hour/day/week/etc) desc, then by relevance desc

	SPH_SORT_TOTAL
};


/// search query matching mode
enum ESphMatchMode
{
	SPH_MATCH_ALL = 0,			///< match all query words
	SPH_MATCH_ANY,				///< match any query word
	SPH_MATCH_PHRASE,			///< match this exact phrase

	SPH_MATCH_TOTAL
};


/// search query
class CSphQuery
{
public:
	const char *	m_sQuery;		///< query string. MUST be not NULL
	int *			m_pWeights;		///< user-supplied per-field weights. may be NULL. default is NULL. NOT OWNED, WILL NOT BE FREED in dtor.
	int				m_iWeights;		///< number of user-supplied weights. missing fields will be assigned weight 1. default is 0
	ESphMatchMode	m_eMode;		///< match mode. default is "match all"
	DWORD *			m_pGroups;		///< groups to match. default is NULL, which means "match all". OWNED, WILL BE FREED in dtor.
	int				m_iGroups;		///< count of groups to match
	ESphSortOrder	m_eSort;		///< sorting order
	ISphTokenizer *	m_pTokenizer;	///< tokenizer to use. NOT OWNED.

	DWORD			m_iMinID;		///< min ID to match, 0 by default
	DWORD			m_iMaxID;		///< max ID to match, UINT_MAX by default
	DWORD			m_iMinTS;		///< min timestamp to match, 0 by default
	DWORD			m_iMaxTS;		///< max timestamp to match, UINT_MAX by default

public:
					CSphQuery ();	///< ctor, fills defaults
					~CSphQuery ();	///< dtor, safely frees owned fields
};


/// search query result
class CSphQueryResult
{
public:
	static const int		MAX_MATCHES = 1000;

	struct WordStat_t
	{
		char *				m_sWord;	///< i-th search term (normalized word form)
		int					m_iDocs;	///< document count for this term
		int					m_iHits;	///< hit count for this term
	}						m_tWordStats [ SPH_MAX_QUERY_WORDS ];

	int						m_iNumWords;		///< query word count
	float					m_fQueryTime;		///< query time, seconds
	CSphVector<CSphMatch>	m_dMatches;			///< top matching documents, no more than MAX_MATCHES
	int						m_iTotalMatches;	///< total matches count

public:
							CSphQueryResult ();		///< ctor
	virtual					~CSphQueryResult ();	///< dtor, which releases all owned stuff
};

/////////////////////////////////////////////////////////////////////////////
// FULLTEXT INDICES
/////////////////////////////////////////////////////////////////////////////

/// progress info
struct CSphIndexProgress
{
	enum Phase_e
	{
		PHASE_COLLECT,				///< document collection phase
		PHASE_COLLECT_END,			///< document collection phase end
		PHASE_SORT,					///< final sorting phase
		PHASE_SORT_END				///< final sorting phase end
	};

	Phase_e			m_ePhase;		///< current indexing phase

	int				m_iDocuments;	///< PHASE_COLLECT: documents collected so far
	SphOffset_t		m_iBytes;		///< PHASE_COLLECT: bytes collected so far

	SphOffset_t		m_iHits;		///< PHASE_SORT: hits sorted so far
	SphOffset_t		m_iHitsTotal;	///< PHASE_SORT: hits total
};


/// match queue interface
typedef ISphQueue<CSphMatch, CSphQueryResult::MAX_MATCHES>	ISphMatchQueue;


/// generic fulltext index interface
class CSphIndex
{
public:
	typedef void ProgressCallback_t ( const CSphIndexProgress * pStat );

public:
								CSphIndex() : m_pProgress ( NULL ) {}
	virtual						~CSphIndex () {}
	virtual	void				SetProgressCallback ( ProgressCallback_t * pfnProgress ) { m_pProgress = pfnProgress; }

public:
	virtual int					Build ( CSphDict * dict, CSphSource * source, int iMemoryLimit ) = 0;
	virtual CSphQueryResult *	Query ( CSphDict * dict, CSphQuery * pQuery ) = 0;
	virtual bool				QueryEx ( CSphDict * dict, CSphQuery * pQuery, CSphQueryResult * pResult, ISphMatchQueue * pTop ) = 0;

protected:
	ProgressCallback_t *		m_pProgress;
};

/////////////////////////////////////////////////////////////////////////////

/// create phrase fulltext index implemntation
CSphIndex *			sphCreateIndexPhrase ( const char * sFilename );

/// tell libsphinx to be quiet or not (logs and loglevels to come later)
void				sphSetQuiet ( bool bQuiet );

/// create proper queue for given query
ISphMatchQueue *	sphCreateQueue ( CSphQuery * pQuery );

/// convert queue to linear matches array
void				sphFlattenQueue ( ISphMatchQueue * pQueue, CSphQueryResult * pResult );

#endif // _sphinx_

//
// $Id: sphinx.h,v 1.54 2006/01/10 22:03:58 shodan Exp $
//
