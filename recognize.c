/* recognition code for SID objects */

#define SYSTEM_PRIVATE

#include <proto/intuition.h>
#include <proto/multimedia.h>
#include <clib/alib_protos.h>

#include "class_version.h"


ULONG Recognize(struct DtCodeContext *dcc, ULONG recog_type);


const struct TagItem ClassTags[] = {
	{MMA_RecognizeCode, (ULONG)Recognize},
	{MMA_MediaType, MMT_AUDIO},
	{MMA_ClassType, MMCLASS_DEMUXER},
	{TAG_END, 0}
};


const struct TagItem* ClassAttributes(void)
{
	return ClassTags;
}


BOOL strequ(UBYTE *str1, UBYTE *str2, ULONG count)
{
	while (count--) if (*str1++ != *str2++) return FALSE;
	return TRUE;
}

ULONG Recognize(struct DtCodeContext *dcc, ULONG recog_type)
{
	struct Library *IntuitionBase = dcc->dcc_IntuitionBase;
	struct Library *MultimediaBase = dcc->dcc_MultimediaBase;
	LONG probability = 0;
	ULONG sid_signature[2];

	if (DoMethod(dcc->dcc_Source, MMM_Peek, dcc->dcc_Port, (ULONG)sid_signature, 8) == 8)
	{
		if ((sid_signature[0] == MAKE_ID('P','S','I','D')) ||
		    (sid_signature[0] == MAKE_ID('R','S','I','D')))
		{
			UWORD version = (sid_signature[1] >> 16) & 0xFFFF;
			
			probability += 9500;
	
			MLOG(LOG_INFO, "Detected SID signature.");

			if (version >= 1 && version <= 4)
			{
				probability += 500;
				MLOGV(LOG_INFO, "SID version %d.", version);
			}
		}
	}
	else MLOG(LOG_ERRORS, "Unexpected end of data.");

	DoMethod(dcc->dcc_Source, MMM_Restore);
	return probability;
}
