/*
 * code/wiiu/wiiu_account.cpp -- Mii nickname lookup for default player name.
 * nn::act is the only account API wut exposes, and there's no "OS username" concept
 * here -- the Mii nickname is the closest thing. Read-only, unlike nn::swkbd, so it's
 * actually safe to call during boot.
 */
#include <string.h>

#include <nn/act/client_cpp.h>

#include "wiiu_account.h"

extern "C" int WiiU_GetAccountName(char *out, size_t outSize)
{
	if (!out || outSize == 0) {
		return 0;
	}
	out[0] = '\0';

	if (nn::act::Initialize().IsFailure()) {
		return 0;
	}

	int ok = 0;
	int16_t wname[nn::act::MiiNameSize];
	memset(wname, 0, sizeof(wname));

	if (nn::act::GetMiiName(wname).IsSuccess()) {
		/* Mii names love decorative glyphs Quake's font can't render -- strip non-ASCII outright. */
		size_t o = 0;
		for (size_t i = 0; i < nn::act::MiiNameSize - 1 && wname[i] != 0 && o < outSize - 1; i++) {
			uint16_t c = (uint16_t)wname[i];
			if (c >= 0x20 && c < 0x7F) {
				out[o++] = (char)c;
			}
		}
		while (o > 0 && out[o - 1] == ' ') {
			o--;
		}
		out[o] = '\0';
		ok = (o > 0) ? 1 : 0;
	}

	nn::act::Finalize();
	return ok;
}
