/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2014-2020 Jean-Pierre Charras, jp.charras at wanadoo.fr
 * Copyright (C) 2008 Wayne Stambaugh <stambaughw@gmail.com>
 * Copyright (C) 1992-2020 KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include <eda_base_frame.h>
#include <project.h>
#include <common.h>
#include <reporter.h>
#include <macros.h>
#include <mutex>
#include <wx/process.h>
#include <wx/config.h>
#include <wx/utils.h>
#include <wx/stdpaths.h>
#include <wx/url.h>
#include <wx/wx.h>


int ProcessExecute( const wxString& aCommandLine, int aFlags, wxProcess *callback )
{
    return (int) wxExecute( aCommandLine, aFlags, callback );
}


enum Bracket
{
    Bracket_None,
    Bracket_Normal  = ')',
    Bracket_Curly   = '}',
#ifdef  __WINDOWS__
    Bracket_Windows = '%',    // yeah, Windows people are a bit strange ;-)
#endif
    Bracket_Max
};


wxString ExpandTextVars( const wxString& aSource,
                         const std::function<bool( wxString* )>* aLocalResolver,
                         const PROJECT* aProject,
                         const std::function<bool( wxString* )>* aFallbackResolver )
{
    wxString newbuf;
    size_t   sourceLen = aSource.length();

    newbuf.Alloc( sourceLen );  // best guess (improves performance)

    for( size_t i = 0; i < sourceLen; ++i )
    {
        if( aSource[i] == '$' && i + 1 < sourceLen && aSource[i+1] == '{' )
        {
            wxString token;

            for( i = i + 2; i < sourceLen; ++i )
            {
                if( aSource[i] == '}' )
                    break;
                else
                    token.append( aSource[i] );
            }

            if( token.IsEmpty() )
                continue;

            if( aLocalResolver && (*aLocalResolver)( &token ) )
            {
                newbuf.append( token );
            }
            else if( aProject && aProject->TextVarResolver( &token ) )
            {
                newbuf.append( token );
            }
            else if( aFallbackResolver && (*aFallbackResolver)( &token ) )
            {
                newbuf.append( token );
            }
            else
            {
                // Token not resolved: leave the reference unchanged
                newbuf.append( "${" + token + "}" );
            }
        }
        else
        {
            newbuf.append( aSource[i] );
        }
    }

    return newbuf;
}


//
// Stolen from wxExpandEnvVars and then heavily optimized
//
wxString KIwxExpandEnvVars( const wxString& str, const PROJECT* aProject )
{
    size_t strlen = str.length();

    wxString strResult;
    strResult.Alloc( strlen );  // best guess (improves performance)

    for( size_t n = 0; n < strlen; n++ )
    {
        wxUniChar str_n = str[n];

        switch( str_n.GetValue() )
        {
#ifdef __WINDOWS__
        case wxT( '%' ):
#endif // __WINDOWS__
        case wxT( '$' ):
        {
            Bracket bracket;
#ifdef __WINDOWS__
            if( str_n == wxT( '%' ) )
              bracket = Bracket_Windows;
            else
#endif // __WINDOWS__
            if( n == strlen - 1 )
            {
                bracket = Bracket_None;
            }
            else
            {
                switch( str[n + 1].GetValue() )
                {
                case wxT( '(' ):
                    bracket = Bracket_Normal;
                    str_n = str[++n];                   // skip the bracket
                    break;

                case wxT( '{' ):
                    bracket = Bracket_Curly;
                    str_n = str[++n];                   // skip the bracket
                    break;

                default:
                    bracket = Bracket_None;
                }
            }

            size_t m = n + 1;
            wxUniChar str_m = str[m];

            while( m < strlen && ( wxIsalnum( str_m ) || str_m == wxT( '_' ) || str_m == wxT( ':' ) ) )
                str_m = str[++m];

            wxString strVarName( str.c_str() + n + 1, m - n - 1 );

            // NB: use wxGetEnv instead of wxGetenv as otherwise variables
            //     set through wxSetEnv may not be read correctly!
            bool expanded = false;
            wxString tmp = strVarName;

            if( aProject && aProject->TextVarResolver( &tmp ) )
            {
                strResult += tmp;
                expanded = true;
            }
            else if( wxGetEnv( strVarName, &tmp ) )
            {
                strResult += tmp;
                expanded = true;
            }
            else
            {
                // variable doesn't exist => don't change anything
#ifdef  __WINDOWS__
                if ( bracket != Bracket_Windows )
#endif
                if ( bracket != Bracket_None )
                    strResult << str[n - 1];

                strResult << str_n << strVarName;
            }

            // check the closing bracket
            if( bracket != Bracket_None )
            {
                if( m == strlen || str_m != (wxChar)bracket )
                {
                    // under MSW it's common to have '%' characters in the registry
                    // and it's annoying to have warnings about them each time, so
                    // ignore them silently if they are not used for env vars
                    //
                    // under Unix, OTOH, this warning could be useful for the user to
                    // understand why isn't the variable expanded as intended
#ifndef __WINDOWS__
                    wxLogWarning( _( "Environment variables expansion failed: missing '%c' "
                                     "at position %u in '%s'." ),
                                  (char)bracket, (unsigned int) (m + 1), str.c_str() );
#endif // __WINDOWS__
                }
                else
                {
                    // skip closing bracket unless the variables wasn't expanded
                    if( !expanded )
                        strResult << (wxChar)bracket;

                    m++;
                }
            }

            n = m - 1;  // skip variable name
            str_n = str[n];
        }
            break;

        case wxT( '\\' ):
            // backslash can be used to suppress special meaning of % and $
            if( n != strlen - 1 && (str[n + 1] == wxT( '%' ) || str[n + 1] == wxT( '$' )) )
            {
                str_n = str[++n];
                strResult += str_n;

                break;
            }
            KI_FALLTHROUGH;

        default:
            strResult += str_n;
        }
    }

#ifndef __WINDOWS__
    if( strResult.StartsWith( "~" ) )
        strResult.Replace( "~", wxGetHomeDir(), false );
#endif // __WINDOWS__

    return strResult;
}


const wxString ExpandEnvVarSubstitutions( const wxString& aString, PROJECT* aProject )
{
    // wxGetenv( wchar_t* ) is not re-entrant on linux.
    // Put a lock on multithreaded use of wxGetenv( wchar_t* ), called from wxEpandEnvVars(),
    static std::mutex getenv_mutex;

    std::lock_guard<std::mutex> lock( getenv_mutex );

    // We reserve the right to do this another way, by providing our own member function.
    return KIwxExpandEnvVars( aString, aProject );
}


const wxString ResolveUriByEnvVars( const wxString& aUri, PROJECT* aProject )
{
    wxString uri = ExpandTextVars( aUri, nullptr, aProject );

    // URL-like URI: return as is.
    wxURL url( uri );

    if( url.GetError() == wxURL_NOERR )
        return uri;

    // Otherwise, the path points to a local file. Resolve environment variables if any.
    return ExpandEnvVarSubstitutions( aUri, aProject );
}


bool EnsureFileDirectoryExists( wxFileName*     aTargetFullFileName,
                                const wxString& aBaseFilename,
                                REPORTER*       aReporter )
{
    wxString msg;
    wxString baseFilePath = wxFileName( aBaseFilename ).GetPath();

    // make aTargetFullFileName path, which is relative to aBaseFilename path (if it is not
    // already an absolute path) absolute:
    if( !aTargetFullFileName->MakeAbsolute( baseFilePath ) )
    {
        if( aReporter )
        {
            msg.Printf( _( "Cannot make path \"%s\" absolute with respect to \"%s\"." ),
                        aTargetFullFileName->GetPath(),
                        baseFilePath );
            aReporter->Report( msg, RPT_SEVERITY_ERROR );
        }

        return false;
    }

    // Ensure the path of aTargetFullFileName exists, and create it if needed:
    wxString outputPath( aTargetFullFileName->GetPath() );

    if( !wxFileName::DirExists( outputPath ) )
    {
        // Make every directory provided when the provided path doesn't exist
        if( wxFileName::Mkdir( outputPath, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL ) )
        {
            if( aReporter )
            {
                msg.Printf( _( "Output directory \"%s\" created.\n" ), outputPath );
                aReporter->Report( msg, RPT_SEVERITY_INFO );
                return true;
            }
        }
        else
        {
            if( aReporter )
            {
                msg.Printf( _( "Cannot create output directory \"%s\".\n" ), outputPath );
                aReporter->Report( msg, RPT_SEVERITY_ERROR );
            }

            return false;
        }
    }

    return true;
}


#ifdef __WXMAC__
wxString GetOSXKicadUserDataDir()
{
    // According to wxWidgets documentation for GetUserDataDir:
    // Mac: ~/Library/Application Support/appname
    wxFileName udir( wxStandardPaths::Get().GetUserDataDir(), wxEmptyString );

    // Since appname is different if started via launcher or standalone binary
    // map all to "kicad" here
    udir.RemoveLastDir();
    udir.AppendDir( "kicad" );

    return udir.GetPath();
}


wxString GetOSXKicadMachineDataDir()
{
    return wxT( "/Library/Application Support/kicad" );
}


wxString GetOSXKicadDataDir()
{
    // According to wxWidgets documentation for GetDataDir:
    // Mac: appname.app/Contents/SharedSupport bundle subdirectory
    wxFileName ddir( wxStandardPaths::Get().GetDataDir(), wxEmptyString );

    // This must be mapped to main bundle for everything but kicad.app
    const wxArrayString dirs = ddir.GetDirs();
    if( dirs[dirs.GetCount() - 3] != wxT( "kicad.app" ) )
    {
        // Bundle structure resp. current path is
        //   kicad.app/Contents/Applications/<standalone>.app/Contents/SharedSupport
        // and will be mapped to
        //   kicad.app/Contents/SharedSupprt
        ddir.RemoveLastDir();
        ddir.RemoveLastDir();
        ddir.RemoveLastDir();
        ddir.RemoveLastDir();
        ddir.AppendDir( wxT( "SharedSupport" ) );
    }

    return ddir.GetPath();
}
#endif


// add this only if it is not in wxWidgets (for instance before 3.1.0)
#ifdef USE_KICAD_WXSTRING_HASH
size_t std::hash<wxString>::operator()( const wxString& s ) const
{
    return std::hash<std::wstring>{}( s.ToStdWstring() );
}
#endif

#ifdef USE_KICAD_WXPOINT_LESS_AND_HASH
size_t std::hash<wxPoint>::operator() ( const wxPoint& k ) const
{
    auto xhash = std::hash<int>()( k.x );

    // 0x9e3779b9 is 2^33 / ( 1 + sqrt(5) )
    // Adding this value ensures that consecutive bits of y will not be close to each other
    // decreasing the likelihood of hash collision in similar values of x and y
    return xhash ^ ( std::hash<int>()( k.y )  + 0x9e3779b9 + ( xhash << 6 ) + ( xhash >> 2 ) );
}

bool std::less<wxPoint>::operator()( const wxPoint& aA, const wxPoint& aB ) const
{
    if( aA.x == aB.x )
        return aA.y < aB.y;

    return aA.x < aB.x;
}
#endif


std::ostream& operator<<( std::ostream& out, const wxSize& size )
{
    out << " width=\"" << size.GetWidth() << "\" height=\"" << size.GetHeight() << "\"";
    return out;
}


std::ostream& operator<<( std::ostream& out, const wxPoint& pt )
{
    out << " x=\"" << pt.x << "\" y=\"" << pt.y << "\"";
    return out;
}


/**
 * Performance enhancements to file and directory operations.
 *
 * Note: while it's annoying to have to make copies of wxWidgets stuff and then
 * add platform-specific performance optimizations, the following routines offer
 * SIGNIFICANT performance benefits.
 */

/**
 * A copy of wxMatchWild(), which wxWidgets attributes to Douglas A. Lewis
 * <dalewis@cs.Buffalo.EDU> and ircII's reg.c.
 *
 * This version is modified to skip any encoding conversions (for performance).
 */
bool matchWild( const char* pat, const char* text, bool dot_special )
{
    if( !*text )
    {
        /* Match if both are empty. */
        return !*pat;
    }

    const char *m = pat,
    *n = text,
    *ma = NULL,
    *na = NULL;
    int just = 0,
    acount = 0,
    count = 0;

    if( dot_special && (*n == '.') )
    {
        /* Never match so that hidden Unix files
         * are never found. */
        return false;
    }

    for(;;)
    {
        if( *m == '*' )
        {
            ma = ++m;
            na = n;
            just = 1;
            acount = count;
        }
        else if( *m == '?' )
        {
            m++;

            if( !*n++ )
                return false;
        }
        else
        {
            if( *m == '\\' )
            {
                m++;

                /* Quoting "nothing" is a bad thing */
                if( !*m )
                    return false;
            }
            if( !*m )
            {
                /*
                * If we are out of both strings or we just
                * saw a wildcard, then we can say we have a
                * match
                */
                if( !*n )
                    return true;

                if( just )
                    return true;

                just = 0;
                goto not_matched;
            }

            /*
            * We could check for *n == NULL at this point, but
            * since it's more common to have a character there,
            * check to see if they match first (m and n) and
            * then if they don't match, THEN we can check for
            * the NULL of n
            */
            just = 0;

            if( *m == *n )
            {
                m++;
                count++;
                n++;
            }
            else
            {
                not_matched:

                /*
                 * If there are no more characters in the
                 * string, but we still need to find another
                 * character (*m != NULL), then it will be
                 * impossible to match it
                 */
                if( !*n )
                    return false;

                if( ma )
                {
                    m = ma;
                    n = ++na;
                    count = acount;
                }
                else
                    return false;
            }
        }
    }
}


/**
 * A copy of ConvertFileTimeToWx() because wxWidgets left it as a static function
 * private to src/common/filename.cpp.
 */
#if wxUSE_DATETIME && defined(__WIN32__) && !defined(__WXMICROWIN__)

// Convert between wxDateTime and FILETIME which is a 64-bit value representing
// the number of 100-nanosecond intervals since January 1, 1601 UTC.
//
// This is the offset between FILETIME epoch and the Unix/wxDateTime Epoch.
static wxInt64 EPOCH_OFFSET_IN_MSEC = wxLL(11644473600000);


static void ConvertFileTimeToWx( wxDateTime *dt, const FILETIME &ft )
{
    wxLongLong t( ft.dwHighDateTime, ft.dwLowDateTime );
    t /= 10000; // Convert hundreds of nanoseconds to milliseconds.
    t -= EPOCH_OFFSET_IN_MSEC;

    *dt = wxDateTime( t );
}

#endif // wxUSE_DATETIME && __WIN32__


/**
 * TimestampDir
 *
 * This routine offers SIGNIFICANT performance benefits over using wxWidgets to gather
 * timestamps from matching files in a directory.
 * @param aDirPath the directory to search
 * @param aFilespec a (wildcarded) file spec to match against
 * @return a hash of the last-mod-dates of all matching files in the directory
 */
long long TimestampDir( const wxString& aDirPath, const wxString& aFilespec )
{
    long long timestamp = 0;

#if defined( __WIN32__ )
    // Win32 version.
    // Save time by not searching for each path twice: once in wxDir.GetNext() and once in
    // wxFileName.GetModificationTime().  Also cuts out wxWidgets' string-matching and case
    // conversion by staying on the MSW side of things.
    std::wstring filespec( aDirPath.t_str() );
    filespec += '\\';
    filespec += aFilespec.t_str();

    WIN32_FIND_DATA findData;
    wxDateTime      lastModDate;

    HANDLE fileHandle = ::FindFirstFile( filespec.data(), &findData );

    if( fileHandle != INVALID_HANDLE_VALUE )
    {
        do
        {
            ConvertFileTimeToWx( &lastModDate, findData.ftLastWriteTime );
            timestamp += lastModDate.GetValue().GetValue();
        }
        while ( FindNextFile( fileHandle, &findData ) != 0 );
    }

    FindClose( fileHandle );
#else
    // POSIX version.
    // Save time by not converting between encodings -- do everything on the file-system side.
    std::string filespec( aFilespec.fn_str() );
    std::string dir_path( aDirPath.fn_str() );

    DIR* dir = opendir( dir_path.c_str() );

    if( dir )
    {
        for( dirent* dir_entry = readdir( dir ); dir_entry; dir_entry = readdir( dir ) )
        {
            if( !matchWild( filespec.c_str(), dir_entry->d_name, true ) )
                continue;

            std::string entry_path = dir_path + '/' + dir_entry->d_name;
            struct stat entry_stat;

            if( wxCRT_Lstat( entry_path.c_str(), &entry_stat ) == 0 )
            {
                // Timestamp the source file, not the symlink
                if( S_ISLNK( entry_stat.st_mode ) )    // wxFILE_EXISTS_SYMLINK
                {
                    char buffer[ PATH_MAX + 1 ];
                    ssize_t pathLen = readlink( entry_path.c_str(), buffer, PATH_MAX );

                    if( pathLen > 0 )
                    {
                        struct stat linked_stat;
                        buffer[ pathLen ] = '\0';
                        entry_path = dir_path + buffer;

                        if( wxCRT_Lstat( entry_path.c_str(), &linked_stat ) == 0 )
                        {
                            entry_stat = linked_stat;
                        }
                        else
                        {
                            // if we couldn't lstat the linked file we'll have to just use
                            // the symbolic link info
                        }
                    }
                }

                if( S_ISREG( entry_stat.st_mode ) )    // wxFileExists()
                    timestamp += entry_stat.st_mtime * 1000;
            }
            else
            {
                // if we couldn't lstat the file itself all we can do is use the name
                timestamp += (signed) std::hash<std::string>{}( std::string( dir_entry->d_name ) );
            }
        }

        closedir( dir );
    }
#endif

    return timestamp;
}
