/*
 * ioQuake3-U — Wii U native (wut / GX2) port
 * code/wiiu/wiiu_classic_extract.c -- CLASSIC-only self-extracting pak.
 * zpack-classic.pk3 rides baked into the .rpx as a byte array; unpacked to SD
 * on first boot (length+checksum gated) so nobody has to copy it there by hand.
 */
#include <stdio.h>

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "zpack_classic_embedded.h"

#define WIIU_CLASSIC_PAK_PATH "fs:/vol/external01/quake3/baseq3/zpack-classic.pk3"

void WiiU_ExtractBundledZpackClassic( void ) {
	FILE *f;
	unsigned int existingLen = 0;
	unsigned int existingCsum = 0;
	unsigned char chunk[4096];
	size_t n;

	f = fopen( WIIU_CLASSIC_PAK_PATH, "rb" );
	if ( f ) {
		while ( ( n = fread( chunk, 1, sizeof( chunk ), f ) ) > 0 ) {
			size_t i;
			for ( i = 0; i < n; i++ ) {
				existingCsum += chunk[i];
			}
			existingLen += (unsigned int)n;
		}
		fclose( f );

		if ( existingLen == zpack_classic_data_len && existingCsum == zpack_classic_data_csum ) {
			return; // already up to date
		}
	}

	f = fopen( WIIU_CLASSIC_PAK_PATH, "wb" );
	if ( !f ) {
		return;
	}
	fwrite( zpack_classic_data, 1, zpack_classic_data_len, f );
	fclose( f );
}
