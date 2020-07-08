//
// $Id: search.cpp,v 1.29 2006/01/10 22:03:58 shodan Exp $
//

//
// Copyright (c) 2001-2005, Andrew Aksyonoff. All rights reserved.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License. You should have
// received a copy of the GPL license along with this program; if you
// did not, you can find it at http://www.gnu.org/
//

#include "sphinx.h"
#include "sphinxutils.h"
#include <time.h>


#define CONF_CHECK(_hash,_key,_msg,_add) \
	if (!( _hash.Exists ( _key ) )) \
	{ \
		fprintf ( stdout, "ERROR: key '%s' not found " _msg, _key, _add ); \
		continue; \
	}


int main ( int argc, char ** argv )
{
	fprintf ( stdout, SPHINX_BANNER );
	if ( argc<=1 )
	{
		fprintf ( stdout,
			"Usage: search [OPTIONS] <word1 [word2 [word3 [...]]]>\n"
			"\n"
			"Options are:\n"
			"-c, --config <file>\tread configuration from specified file\n"
			"\t\t\t(default is sphinx.conf)\n"
			"-a, --any\t\tmatch document if any query word is matched\n"
			"\t\t\t(default is to match all)\n"
			"-g, -group <id>\t\tlimit matching documents to this group\n"
			"\t\t\t(default is not to limit by group)\n"
			"-s, --start <offset>\tstart matches list output from this offset\n"
			"\t\t\t(default is 0)\n"
			"-l, --limit <count>\tlimit matches list output size\n"
			"\t\t\t(default is 20)\n"
			"-q, --noinfo\t\tdo not output document info from SQL database\n"
			"\t\t\t(default is to output)\n"
			"--sort=date\t\tsort by date\n"
			"--rsort=date\t\treverse sort by date\n"
		);
		exit ( 0 );
	}

	///////////////////////////////////////////
	// get query and other commandline options
	///////////////////////////////////////////

	CSphQuery tQuery;
	char sQuery [ 1024 ];
	sQuery[0] = '\0';

	const char * sConfName = "sphinx.conf";
	CSphVector<DWORD> dGroups;
	bool bNoInfo = false;
	int iStart = 0;
	int iLimit = 20;

	#define OPT(_a1,_a2)	else if ( !strcmp(argv[i],_a1) || !strcmp(argv[i],_a2) )
	#define OPT1(_a1)		else if ( !strcmp(argv[i],_a1) )

	int i;
	for ( i=1; i<argc; i++ )
	{
		if ( argv[i][0]=='-' )
		{
			// this is an option
			if ( i==0 );
			OPT ( "-a", "--any" )		tQuery.m_eMode = SPH_MATCH_ANY;
			OPT ( "-p", "--phrase" )	tQuery.m_eMode = SPH_MATCH_PHRASE;
			OPT ( "-q", "--noinfo" )	bNoInfo = true;
			OPT1 ( "--sort=date" )		tQuery.m_eSort = SPH_SORT_DATE_DESC;
			OPT1 ( "--rsort=date" )		tQuery.m_eSort = SPH_SORT_DATE_ASC;
			OPT1 ( "--sort=ts" )		tQuery.m_eSort = SPH_SORT_TIME_SEGMENTS;
			else if ( (i+1)<argc )
			{
				if ( i==0 );
				OPT ( "-g", "--group")		{ if ( atoi ( argv[++i] ) ) dGroups.Add ( atoi ( argv[i] ) ); }
				OPT ( "-s", "--start" )		iStart = atoi ( argv[++i] );
				OPT ( "-l", "--limit" )		iLimit = atoi ( argv[++i] );
				OPT ( "-c", "--config" )	sConfName = argv[++i];
				else break; // unknown option
			}
			else break; // unknown option

		} else if ( strlen(sQuery) + strlen(argv[i]) + 1 < sizeof(sQuery) )
		{
			// this is a search term
			strcat ( sQuery, argv[i] );
			strcat ( sQuery, " " );
		}
	}
	iStart = Max ( iStart, 0 );
	iLimit = Min ( Max ( iLimit, 0 ), 1000 );

	if ( i!=argc )
	{
		fprintf ( stdout, "ERROR: malformed or unknown option near '%s'.\n", argv[i] );
		return 1;
	}

	#undef OPT

	/////////////
	// configure
	/////////////

	// load config
	CSphConfigParser cp;
	if ( !cp.Parse ( sConfName ) )
		sphDie ( "FATAL: failed to parse config file '%s'.\n", sConfName );

	CSphConfig & hConf = cp.m_tConf;
	if ( !hConf.Exists ( "index" ) )
		sphDie ( "FATAL: no indexes found in config file '%s'.\n", sConfName );

	/////////////////////
	// search each index
	/////////////////////

	hConf["index"].IterateStart ();
	while ( hConf["index"].IterateNext () )
	{
		const CSphConfigSection & hIndex = hConf["index"].IterateGet ();
		const char * sIndexName = hConf["index"].IterateGetKey().cstr();

		if ( !hIndex.Exists ( "path" ) )
			sphDie ( "FATAL: key 'path' not found in index '%s'.\n", sIndexName );

		// configure charset_type
		tQuery.m_pTokenizer = NULL;
		bool bUseUTF8 = false;
		if ( hIndex.Exists ( "charset_type" ) )
		{
			if ( hIndex["charset_type"]=="sbcs" )
				tQuery.m_pTokenizer = sphCreateSBCSTokenizer ();
			else if ( hIndex["charset_type"]=="utf-8" )
			{
				tQuery.m_pTokenizer = sphCreateUTF8Tokenizer ();
				bUseUTF8 = true;
			}
			else
				sphDie ( "FATAL: unknown charset type '%s' in index '%s'.\n", hIndex["charset_type"].cstr(), sIndexName );
		} else
		{
			tQuery.m_pTokenizer = sphCreateSBCSTokenizer ();
		}

		// get morphology type
		DWORD iMorph = SPH_MORPH_NONE;
		if ( hIndex.Exists ( "morphology" ) )
		{
			int iRuMorph = ( bUseUTF8 ? SPH_MORPH_STEM_RU_UTF8 : SPH_MORPH_STEM_RU_CP1251 );
			if ( hIndex["morphology"]=="stem_en" )			iMorph = SPH_MORPH_STEM_EN;
			else if ( hIndex["morphology"]=="stem_ru" )		iMorph = iRuMorph;
			else if ( hIndex["morphology"]=="stem_enru" )	iMorph = SPH_MORPH_STEM_EN | iRuMorph;
			else
				fprintf ( stdout, "WARNING: unknown morphology type '%s' ignored.\n", hIndex["morphology"].cstr() );
		}

		// do we want to show document info from database?
		#if USE_MYSQL
		MYSQL tSqlDriver;
		const char * sQueryInfo = NULL;

		while ( !bNoInfo )
		{
			if ( !hIndex.Exists ( "source" )
				|| !hConf.Exists ( "source" )
				|| !hConf["source"].Exists ( hIndex["source"] ) )
			{
				break;
			}
			const CSphConfigSection & hSource = hConf["source"][ hIndex["source"] ];

			if ( !hSource.Exists ( "sql_host" )
				|| !hSource.Exists ( "sql_user" )
				|| !hSource.Exists ( "sql_db" )
				|| !hSource.Exists ( "sql_pass" ) )
			{
				break;
			}

			if ( !hSource.Exists ( "sql_query_info" ) )
				break;
			sQueryInfo = hIndex["sql_query_info"].cstr();
			if ( !strstr ( sQueryInfo, "$id" ) )
				sphDie ( "FATAL: 'sql_query_info' value must contain '$id'." );

			int iPort = 3306;
			if ( hSource.Exists ( "sql_port" ) && hSource["sql_port"].intval() )
				iPort = hSource["sql_port"].intval();

			mysql_init ( &tSqlDriver );
			if ( !mysql_real_connect ( &tSqlDriver,
				hSource["sql_host"].cstr(),
				hSource["sql_user"].cstr(),
				hSource["sql_pass"].cstr(),
				hSource["sql_db"].cstr(),
				iPort,
				hSource.Exists ( "sql_sock" ) ? hSource["sql_sock"].cstr() : NULL,
				0 ) )
			{
				sphDie ( "FATAL: failed to connect to MySQL (error='%s').",
					mysql_error ( &tSqlDriver ) );
			}

			// all good
			break;
		}
		#endif

		// configure stopwords
		CSphDict * pDict = new CSphDict_CRC32 ( iMorph );
		pDict->LoadStopwords ( hIndex.Exists ( "stopwords" ) ? hIndex["stopwords"].cstr() : NULL,
			tQuery.m_pTokenizer );

		// configure charset_table
		assert ( tQuery.m_pTokenizer );
		if ( hIndex.Exists ( "charset_table" ) )
			if ( !tQuery.m_pTokenizer->SetCaseFolding ( hIndex["charset_table"].cstr() ) )
				sphDie ( "FATAL: failed to parse 'charset_table' in index '%s'.\n", sIndexName );

		//////////
		// search
		//////////

		tQuery.m_sQuery = sQuery;
		if ( dGroups.GetLength() )
		{
			tQuery.m_pGroups = new DWORD [ dGroups.GetLength() ];
			tQuery.m_iGroups = dGroups.GetLength();
			memcpy ( tQuery.m_pGroups, &dGroups[0], sizeof(DWORD)*dGroups.GetLength() );
		}

		CSphIndex * pIndex = sphCreateIndexPhrase ( hIndex["path"].cstr() );
		CSphQueryResult * pResult = pIndex->Query ( pDict, &tQuery );

		SafeDelete ( pIndex );
		SafeDelete ( pDict );
		SafeDelete ( tQuery.m_pTokenizer );

		/////////
		// print
		/////////

		if ( !pResult )
		{
			fprintf ( stdout, "query '%s': search error: can not open index.\n", sQuery );
			return 1;
		}

		fprintf ( stdout, "query '%s': returned %d matches of %d total in %.2f sec\n",
			sQuery, pResult->m_dMatches.GetLength(), pResult->m_iTotalMatches, pResult->m_fQueryTime );

		if ( pResult->m_dMatches.GetLength() )
		{
			fprintf ( stdout, "\ndisplaying matches:\n" );

			int iMaxIndex = Min ( iStart+iLimit, pResult->m_dMatches.GetLength() );
			for ( int i=iStart; i<iMaxIndex; i++ )
			{
				CSphMatch & tMatch = pResult->m_dMatches[i];
				time_t tStamp = tMatch.m_iTimestamp; // for 64-bit
				fprintf ( stdout, "%d. group=%d, document=%d, weight=%d, time=%s",
					1+i,
					tMatch.m_iGroupID,
					tMatch.m_iDocID,
					tMatch.m_iWeight,
					ctime ( &tStamp ) );

				#if USE_MYSQL
				if ( sQueryInfo )
				{
					char * sQuery = sphStrMacro ( sQueryInfo, "$id", tMatch.m_iDocID );
					const char * sError = NULL;

					#define LOC_MYSQL_ERROR(_arg) { sError = _arg; break; }
					for ( ;; )
					{
						if ( mysql_query ( &tSqlDriver, sQuery ) )
							LOC_MYSQL_ERROR ( "mysql_query" );

						MYSQL_RES * pSqlResult = mysql_use_result ( &tSqlDriver );
						if ( !pSqlResult )
							LOC_MYSQL_ERROR ( "mysql_use_result" );

						MYSQL_ROW tRow = mysql_fetch_row ( pSqlResult );
						if ( !tRow )
							LOC_MYSQL_ERROR ( "mysql_fetch_row" );

						fprintf ( stdout, "\n" );
						for ( int iField=0; iField<(int)pSqlResult->field_count; iField++ )
							fprintf ( stdout, "%s=%s\n", pSqlResult->fields[iField].name, tRow[iField] );
						fprintf ( stdout, "\n" );

						mysql_free_result ( pSqlResult );
						break;
					}

					if ( sError )
						sphDie ( "FATAL: sql_query_info: %s: %s.\n", sError, mysql_error ( &tSqlDriver ) );

					delete [] sQuery;
				}
				#endif
			}

			fprintf ( stdout, "\nwords:\n" );
			for ( int i=0; i<pResult->m_iNumWords; i++ )
			{
				fprintf ( stdout, "%d. '%s': %d documents, %d hits\n",
					1+i,
					pResult->m_tWordStats[i].m_sWord,
					pResult->m_tWordStats[i].m_iDocs,
					pResult->m_tWordStats[i].m_iHits );
			}
		}
	}
}

//
// $Id: search.cpp,v 1.29 2006/01/10 22:03:58 shodan Exp $
//
