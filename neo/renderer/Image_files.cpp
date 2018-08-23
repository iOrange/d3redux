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

#include "../idlib/precompiled.h"
#pragma hdrstop

#include "tr_local.h"

/*

This file only has a single entry point:

void R_LoadImage( const char *name, byte **pic, int *width, int *height, bool makePowerOf2 );

*/

#define STB_IMAGE_IMPLEMENTATION

#define STBI_NO_PSD
#define STBI_NO_GIF
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_PNM

#define STBI_NO_STDIO

#include "stb/stb_image.h"


/*
================
R_WriteTGA
================
*/
void R_WriteTGA( const char *filename, const byte *data, int width, int height, bool flipVertical ) {
    byte	*buffer;
    int		i;
    int		bufferSize = width*height*4 + 18;
    int     imgStart = 18;

    buffer = (byte *)Mem_Alloc( bufferSize );
    memset( buffer, 0, 18 );
    buffer[2] = 2;		// uncompressed type
    buffer[12] = width&255;
    buffer[13] = width>>8;
    buffer[14] = height&255;
    buffer[15] = height>>8;
    buffer[16] = 32;	// pixel size
    if ( !flipVertical ) {
        buffer[17] = (1<<5);	// flip bit, for normal top to bottom raster order
    }

    // swap rgb to bgr
    for ( i=imgStart ; i<bufferSize ; i+=4 ) {
        buffer[i] = data[i-imgStart+2];		// blue
        buffer[i+1] = data[i-imgStart+1];		// green
        buffer[i+2] = data[i-imgStart+0];		// red
        buffer[i+3] = data[i-imgStart+3];		// alpha
    }

    fileSystem->WriteFile( filename, buffer, bufferSize );

    Mem_Free (buffer);
}


/*
=============
LoadSTB - loads any image format supported by STB_Image lib
=============
*/
static void LoadSTB( const char *filename, unsigned char **pic, int *width, int *height, ID_TIME_T *timestamp ) {
    byte	*fbuffer, *imgMem;
    int     fileLength, components;

    if ( pic ) {
        *pic = NULL;    // until proven otherwise
    }

    {
        idFile *f;

        f = fileSystem->OpenFileRead( filename );
        if ( !f ) {
            return;
        }
        fileLength = f->Length();
        if ( timestamp ) {
            *timestamp = f->Timestamp();
        }
        if ( !pic ) {
            fileSystem->CloseFile( f );
            return;	// just getting timestamp
        }
        fbuffer = (byte *)Mem_ClearedAlloc(fileLength);
        f->Read(fbuffer, fileLength);
        fileSystem->CloseFile( f );
    }

    imgMem = stbi_load_from_memory(fbuffer, fileLength, width, height, &components, STBI_rgb_alpha);

    Mem_Free(fbuffer);

    if (imgMem) {
        *pic = (byte *)R_StaticAlloc(*width * *height * 4);
        memcpy(*pic, imgMem, *width * *height * 4);

        stbi_image_free(imgMem);
    }

    /* And we're done! */
}

//===================================================================

/*
=================
R_LoadImage

Loads any of the supported image types into a cannonical
32 bit format.

Automatically attempts to load .jpg files if .tga files fail to load.

*pic will be NULL if the load failed.

Anything that is going to make this into a texture would use
makePowerOf2 = true, but something loading an image as a lookup
table of some sort would leave it in identity form.

It is important to do this at image load time instead of texture load
time for bump maps.

Timestamp may be NULL if the value is going to be ignored

If pic is NULL, the image won't actually be loaded, it will just find the
timestamp.
=================
*/
void R_LoadImage( const char *cname, byte **pic, int *width, int *height, ID_TIME_T *timestamp, bool makePowerOf2 ) {
    idStr name = cname;

    if ( pic ) {
        *pic = NULL;
    }
    if ( timestamp ) {
        *timestamp = 0xFFFFFFFF;
    }
    if ( width ) {
        *width = 0;
    }
    if ( height ) {
        *height = 0;
    }

    name.DefaultFileExtension( ".tga" );

    if (name.Length()<5) {
        return;
    }

    name.ToLower();
    idStr ext;
    name.ExtractFileExtension( ext );

    //#NOTE_SK: I'm using one generic function to load supported image formats using stb_image lib
    LoadSTB(name.c_str(), pic, width, height, timestamp);

    if ( ( width && *width < 1 ) || ( height && *height < 1 ) ) {
        if ( pic && *pic ) {
            R_StaticFree( *pic );
            *pic = 0;
        }
    }

    //
    // convert to exact power of 2 sizes
    //
    if ( pic && *pic && makePowerOf2 ) {
        int		w, h;
        int		scaled_width, scaled_height;
        byte	*resampledBuffer;

        w = *width;
        h = *height;

        for (scaled_width = 1 ; scaled_width < w ; scaled_width<<=1)
            ;
        for (scaled_height = 1 ; scaled_height < h ; scaled_height<<=1)
            ;

        if ( scaled_width != w || scaled_height != h ) {
            if ( globalImages->image_roundDown.GetBool() && scaled_width > w ) {
                scaled_width >>= 1;
            }
            if ( globalImages->image_roundDown.GetBool() && scaled_height > h ) {
                scaled_height >>= 1;
            }

            resampledBuffer = R_ResampleTexture( *pic, w, h, scaled_width, scaled_height );
            R_StaticFree( *pic );
            *pic = resampledBuffer;
            *width = scaled_width;
            *height = scaled_height;
        }
    }
}


/*
=======================
R_LoadCubeImages

Loads six files with proper extensions
=======================
*/
bool R_LoadCubeImages( const char *imgName, cubeFiles_t extensions, byte *pics[6], int *outSize, ID_TIME_T *timestamp ) {
    int		i, j;
    char	*cameraSides[6] =  { "_forward.tga", "_back.tga", "_left.tga", "_right.tga", 
        "_up.tga", "_down.tga" };
    char	*axisSides[6] =  { "_px.tga", "_nx.tga", "_py.tga", "_ny.tga", 
        "_pz.tga", "_nz.tga" };
    char	**sides;
    char	fullName[MAX_IMAGE_NAME];
    int		width, height, size = 0;

    if ( extensions == CF_CAMERA ) {
        sides = cameraSides;
    } else {
        sides = axisSides;
    }

    // FIXME: precompressed cube map files
    if ( pics ) {
        memset( pics, 0, 6*sizeof(pics[0]) );
    }
    if ( timestamp ) {
        *timestamp = 0;
    }

    for ( i = 0 ; i < 6 ; i++ ) {
        idStr::snPrintf( fullName, sizeof( fullName ), "%s%s", imgName, sides[i] );

        ID_TIME_T thisTime;
        if ( !pics ) {
            // just checking timestamps
            R_LoadImageProgram( fullName, NULL, &width, &height, &thisTime );
        } else {
            R_LoadImageProgram( fullName, &pics[i], &width, &height, &thisTime );
        }
        if ( thisTime == FILE_NOT_FOUND_TIMESTAMP ) {
            break;
        }
        if ( i == 0 ) {
            size = width;
        }
        if ( width != size || height != size ) {
            common->Warning( "Mismatched sizes on cube map '%s'", imgName );
            break;
        }
        if ( timestamp ) {
            if ( thisTime > *timestamp ) {
                *timestamp = thisTime;
            }
        }
        if ( pics && extensions == CF_CAMERA ) {
            // convert from "camera" images to native cube map images
            switch( i ) {
            case 0:	// forward
                R_RotatePic( pics[i], width);
                break;
            case 1:	// back
                R_RotatePic( pics[i], width);
                R_HorizontalFlip( pics[i], width, height );
                R_VerticalFlip( pics[i], width, height );
                break;
            case 2:	// left
                R_VerticalFlip( pics[i], width, height );
                break;
            case 3:	// right
                R_HorizontalFlip( pics[i], width, height );
                break;
            case 4:	// up
                R_RotatePic( pics[i], width);
                break;
            case 5: // down
                R_RotatePic( pics[i], width);
                break;
            }
        }
    }

    if ( i != 6 ) {
        // we had an error, so free everything
        if ( pics ) {
            for ( j = 0 ; j < i ; j++ ) {
                R_StaticFree( pics[j] );
            }
        }

        if ( timestamp ) {
            *timestamp = 0;
        }
        return false;
    }

    if ( outSize ) {
        *outSize = size;
    }
    return true;
}
