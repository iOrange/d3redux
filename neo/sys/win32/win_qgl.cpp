/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company. 

This file is part of the Doom 3 GPL Source Code (?Doom 3 Source Code?).  

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/
/*
** QGL_WIN.C
**
** This file implements the operating system binding of GL to QGL function
** pointers.  When doing a port of Doom you must implement the following
** two functions:
**
** QGL_Init() - loads libraries, assigns function pointers, etc.
** QGL_Shutdown() - unloads libraries, NULLs function pointers
*/
#include "../../idlib/precompiled.h"
#pragma hdrstop

#include <float.h>
#include "win_local.h"
#include "../../renderer/tr_local.h"


//#include "gl_logfuncs.cpp"

/*
** QGL_Shutdown
**
** Unloads the specified DLL then nulls out all the proc pointers.  This
** is only called during a hard shutdown of the OGL subsystem (e.g. vid_restart).
*/
void QGL_Shutdown( void )
{
	common->Printf( "...shutting down QGL\n" );

	if ( win32.hinstOpenGL )
	{
		common->Printf( "...unloading OpenGL DLL\n" );
		FreeLibrary( win32.hinstOpenGL );
	}

	win32.hinstOpenGL = NULL;
}

#define GR_NUM_BOARDS 0x0f


#pragma warning (disable : 4113 4133 4047 )
#define GPA( a ) GetProcAddress( win32.hinstOpenGL, a )

/*
** QGL_Init
**
** This is responsible for binding our qgl function pointers to 
** the appropriate GL stuff.  In Windows this means doing a 
** LoadLibrary and a bunch of calls to GetProcAddress.  On other
** operating systems we need to do the right thing, whatever that
** might be.
*/
bool QGL_Init( const char *dllname )
{
	assert( win32.hinstOpenGL == 0 );

	common->Printf( "...initializing QGL\n" );

	common->Printf( "...calling LoadLibrary( '%s' ): ", dllname );

	if ( ( win32.hinstOpenGL = LoadLibrary( dllname ) ) == 0 )
	{
		common->Printf( "failed\n" );
		return false;
	}
	common->Printf( "succeeded\n" );

	return true;
}


/*
==================
GLimp_EnableLogging

==================
*/
void GLimp_EnableLogging( bool enable ) {
	static bool		isEnabled;
	static int		initialFrames;
	static char		ospath[ MAX_OSPATH ];

	// return if we're already active
	if ( isEnabled && enable ) {
		// decrement log counter and stop if it has reached 0
		r_logFile.SetInteger( r_logFile.GetInteger() - 1 );
		if ( r_logFile.GetInteger() ) {
			return;
		}
		common->Printf( "closing logfile '%s' after %i frames.\n", ospath, initialFrames );
		enable = false;

		fclose( tr.logFile );
		tr.logFile = NULL;

	}

	// return if we're already disabled
	if ( !enable && !isEnabled ) {
		return;
	}

	isEnabled = enable;

    if (enable) {
        if (!tr.logFile) {
            struct tm		*newtime;
            ID_TIME_T			aclock;
            idStr			qpath;
            int				i;
            const char		*path;

            initialFrames = r_logFile.GetInteger();

            // scan for an unused filename
            for (i = 0; i < 9999; i++) {
                sprintf(qpath, "renderlog_%i.txt", i);
                if (fileSystem->ReadFile(qpath, NULL, NULL) == -1) {
                    break;		// use this name
                }
            }

            path = fileSystem->RelativePathToOSPath(qpath, "fs_savepath");
            idStr::Copynz(ospath, path, sizeof(ospath));
            tr.logFile = fopen(ospath, "wt");

            // write the time out to the top of the file
            time(&aclock);
            newtime = localtime(&aclock);
            fprintf(tr.logFile, "// %s", asctime(newtime));
            fprintf(tr.logFile, "// %s\n\n", cvarSystem->GetCVarString("si_version"));
        }
    }
}
