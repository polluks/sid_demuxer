/// autodoc

/****** sid.demuxer/background ********************************************
*
* DESCRIPTION
*   The class is a Reggae demultiplexer for C64 SID music files. The class
*   performs following tasks:
*   - SID signature verification (PSID/RSID).
*   - Parsing SID header fields (version, songs, addresses, metadata).
*   - Extracting song information (name, author, released).
*   The class does not process the actual 6502 machine code data - it needs
*   a SID emulator/player for actual playback.
*
*   Object of sid.demuxer class has two ports: port 0 is an input and
*   accepts MMF_STREAM format, port 1 is an output and produces
*   MMF_AUDIO_SID format.
*
* NEW ATTRIBUTES
*   MMA_Audio_Channels         (V1)     [..G.Q], ULONG
*   MMA_Audio_BitsPerSample    (V1)     [..G.Q], ULONG
*   MMA_DataFormat             (V1)     [..G.Q], STRPTR
*   MMA_MetaData               (V1)     [..G.Q], struct MetaItem *
*   MMA_Audio_SongCount        (V1)     [..G.Q], ULONG
*   MMA_Audio_StartSong        (V1)     [..G.Q], ULONG
*   MMA_Audio_SIDVersion       (V1)     [..G.Q], ULONG
*   MMA_Audio_Name             (V1)     [..G.Q], STRPTR
*   MMA_Audio_Author           (V1)     [..G.Q], STRPTR
*   MMA_Audio_Released         (V1)     [..G.Q], STRPTR
*
* NEW METHODS
*   MMM_Pull(port, buffer, length)  (V1)
*
*   1.0  (25.05.2026)
*   - Initial revision.
*
*****************************************************************************
*
*/

/****** sid.demuxer/MMA_DataFormat **************************************
*
* NAME
*   MMA_DataFormat (V50) [..G.Q], STRPTR
*
* FUNCTION
*   Returns read-only textual NULL-terminated description of decoded format.
*   This class returns "SID" string.
*
*****************************************************************************
*
*/

/****** sid.demuxer/MMA_Audio_SongCount ********************************
*
* NAME
*   MMA_Audio_SongCount (V1) [..G.Q], ULONG
*
* FUNCTION
*   Returns number of songs contained in the SID file (1-256).
*
*****************************************************************************
*
*/

/****** sid.demuxer/MMA_Audio_StartSong ********************************
*
* NAME
*   MMA_Audio_StartSong (V1) [..G.Q], ULONG
*
* FUNCTION
*   Returns the default starting song number (1-based).
*
*****************************************************************************
*
*/

/****** sid.demuxer/MMA_Audio_SIDVersion *******************************
*
* NAME
*   MMA_Audio_SIDVersion (V1) [..G.Q], ULONG
*
* FUNCTION
*   Returns the SID file format version (1, 2, 3, or 4).
*
*****************************************************************************
*
*/

/****** sid.demuxer/MMA_Audio_Name *************************************
*
* NAME
*   MMA_Audio_Name (V1) [..G.Q], STRPTR
*
* FUNCTION
*   Returns the song/module name from the SID header.
*
*****************************************************************************
*
*/

/****** sid.demuxer/MMA_Audio_Author ***********************************
*
* NAME
*   MMA_Audio_Author (V1) [..G.Q], STRPTR
*
* FUNCTION
*   Returns the author name from the SID header.
*
*****************************************************************************
*
*/

/****** sid.demuxer/MMA_Audio_Released *********************************
*
* NAME
*   MMA_Audio_Released (V1) [..G.Q], STRPTR
*
* FUNCTION
*   Returns copyright/release information from the SID header.
*
*****************************************************************************
*
*/

/****** sid.demuxer/MMM_Pull ********************************************
*
* NAME
*   MMM_Pull -- Writes encoded data to specified buffer. (V50)
*
* SYNOPSIS
*   ULONG DoMethod(Object *obj, MMM_Pull, ULONG port, APTR buffer, ULONG
*     length);
*
* FUNCTION
*   This class does not process audio data, it only strips the SID header
*   and provides metadata attributes. Pull requests forward the raw C64
*   binary data portion to the connected stream.
*
* INPUTS
*   obj - object to perform method on.
*   port - number of port, must be output (1).
*   buffer - destination buffer for binary data.
*   length - amount of data to be pulled in bytes.
*
* RESULT
*   Number of bytes pulled, secondary error information via MMA_ErrorCode.
*
* SEE ALSO
*   multimedia.class/MMA_ErrorCode
*
*****************************************************************************
*
*/

///
/// includes

#define __NOLIBBASE__
#define SYSTEM_PRIVATE

#include <string.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/multimedia.h>
#include <proto/query.h>
#include <emul/emulregs.h>
#include <exec/resident.h>
#include <exec/libraries.h>
#include <clib/alib_protos.h>
#include <clib/debug_protos.h>
#include <classes/multimedia/multimedia.h>
#include <classes/multimedia/audio.h>
#include <classes/multimedia/metadata.h>
#include <hardware/byteswap.h>

///
/// basic defs and structures

#define SUPERCLASS "multimedia.class"

#include "class_version.h"

struct Library *SysBase, *IntuitionBase, *UtilityBase, *MultimediaBase;

struct ClassBase
{
	struct Library          LibNode;
	Class                  *LibClass;
	APTR                    Seglist;
	struct SignalSemaphore  BaseLock;
	BOOL                    InitFlag;
	const struct TagItem   *Attributes;
};

#define SID_HEADER_V1_SIZE  0x76
#define SID_HEADER_V2_SIZE  0x7C
#define SID_META_STR_LEN    32

#define SID_CHIP_6581 "6581"
#define SID_CHIP_8580 "8580"

struct SIDHeader_v1
{
	UBYTE  magicID[4];
	UWORD  version;
	UWORD  dataOffset;
	UWORD  loadAddress;
	UWORD  initAddress;
	UWORD  playAddress;
	UWORD  songs;
	UWORD  startSong;
	ULONG  speed;
	UBYTE  name[SID_META_STR_LEN];
	UBYTE  author[SID_META_STR_LEN];
	UBYTE  released[SID_META_STR_LEN];
};

struct SIDHeader_v2
{
	struct SIDHeader_v1 v1;
	UWORD  flags;
	UBYTE  startPage;
	UBYTE  pageLength;
	UBYTE  secondSIDAddress;
	UBYTE  thirdSIDAddress;
};

struct ObjData
{
	UWORD  sid_version;
	UWORD  songs;
	UWORD  start_song;
	UWORD  load_address;
	UWORD  init_address;
	UWORD  play_address;
	ULONG  speed;
	UWORD  flags;
	UBYTE  name[SID_META_STR_LEN + 1];
	UBYTE  author[SID_META_STR_LEN + 1];
	UBYTE  released[SID_META_STR_LEN + 1];
	UQUAD  data_offset;
	UQUAD  data_length;
	struct MetaItem metadata[5];
	UBYTE  chip_version[32];
};

# pragma pack(1)
# pragma pack()

#ifndef MMF_AUDIO_SID
#define MMF_AUDIO_SID (MKBUNDLENUM('S','I','D',' ') | MKBUNDL(0x01, MKID('A','U','D','I')))
#endif

ULONG InputFormats[] = {MMF_STREAM, 0};
ULONG OutputFormats[] = {MMF_AUDIO_SID, MMF_STREAM, 0};

BOOL strequ(UBYTE *str1, UBYTE *str2, ULONG count)
{
	while (count--) if (*str1++ != *str2++) return FALSE;
	return TRUE;
}

inline ULONG xget(Object *obj, ULONG attr)
{
	ULONG val;

	DoMethod(obj, OM_GET, attr, (ULONG)&val);
	return val;
}

#define OBJNAME(obj) xget(obj, MMA_ObjectName)

static void copy_sid_string(UBYTE *dest, UBYTE *src)
{
	ULONG i;
	for (i = 0; i < SID_META_STR_LEN; i++)
	{
		dest[i] = src[i];
		if (src[i] == 0) break;
	}
	dest[i] = 0;
}

static void build_metadata(struct ObjData *d)
{
	d->metadata[0].mi_Id = MMETA_Title;
	d->metadata[0].mi_Length = strlen(d->name);
	d->metadata[0].mi_Data = (APTR)d->name;

	d->metadata[1].mi_Id = MMETA_Author;
	d->metadata[1].mi_Length = strlen(d->author);
	d->metadata[1].mi_Data = (APTR)d->author;

	d->metadata[2].mi_Id = MMETA_Album;
	d->metadata[2].mi_Length = strlen(d->released);
	d->metadata[2].mi_Data = (APTR)d->released;

	d->metadata[3].mi_Id = MMETA_Performer;
	d->metadata[3].mi_Length = strlen(d->chip_version);
	d->metadata[3].mi_Data = (APTR)d->chip_version;

	d->metadata[4].mi_Id = 0;
	d->metadata[4].mi_Length = 0;
	d->metadata[4].mi_Data = NULL;
}

static UWORD be2word(UBYTE *p)
{
	return (p[0] << 8) | p[1];
}

static ULONG be2long(UBYTE *p)
{
	return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

///
/// class macros

#define GET_BASE struct ClassBase *cb = (struct ClassBase*)cl->cl_UserData
#define GET_DATA struct ObjData *d = (struct ObjData*)INST_DATA(cl, obj)

///
/// protos

/* standards */

struct Library *LibInit(struct Library *unused, APTR seglist, struct Library *sysb);
struct ClassBase *lib_init(struct ClassBase *cb, APTR seglist, struct Library *SysBase);
APTR lib_expunge(struct ClassBase *cb);
struct Library *LibOpen(void);
ULONG LibClose(void);
APTR LibExpunge(void);
ULONG LibReserved(void);
Class *GetClass(void);
LONG ClassDispatcher(void);
LONG dummy_function(void);
Class *init_class(struct ClassBase *cb);
BOOL InitResources(struct ClassBase *cb);
void FreeResources(struct ClassBase *cb);
LONG New(Class *cl, Object *obj, struct opSet *msg);
LONG Dispose(Class *cl, Object *obj, Msg msg);
LONG Set(Class *cl, Object *obj, struct opSet *msg);
LONG Get(Class *cl, Object *obj, struct opGet *msg);

/* class specific */

LONG Pull(Class *cl, Object *obj, struct mmopData *msg);

///
/// dummy_function()

LONG dummy_function(void)
{
	return -1;
}

///
/// resident *

const char LibName[] = CLASSNAME;
char VTag[] = VERSTAG;

static const struct TagItem RTags[] =
{
	{QUERYINFOATTR_NAME, (ULONG)LibName},
	{QUERYINFOATTR_IDSTRING, (ULONG)&VTag[1]},
	{QUERYINFOATTR_DESCRIPTION, (ULONG)"SID music fileformat demuxer"},
	{QUERYINFOATTR_COPYRIGHT, (ULONG)"(c) 2026"},
	{QUERYINFOATTR_AUTHOR, (ULONG)"OpenCode"},
	{QUERYINFOATTR_DATE, (ULONG)DATE},
	{QUERYINFOATTR_VERSION, VERSION},
	{QUERYINFOATTR_REVISION, REVISION},
	{QUERYINFOATTR_SUBTYPE, QUERYSUBTYPE_LIBRARY},
	{QUERYINFOATTR_CLASS, QUERYCLASS_MULTIMEDIA},
	{QUERYINFOATTR_SUBCLASS, QUERYSUBCLASS_MULTIMEDIA_DEMUXER},
	{MMA_MediaType, MMT_AUDIO},
	{MMA_SupportedFormats, (ULONG)InputFormats},
	{TAG_END, 0}
};



struct Resident ROMTag =
{
	RTC_MATCHWORD,
	&ROMTag,
	&ROMTag + 1,
	RTF_EXTENDED | RTF_PPC,
	VERSION,
	NT_LIBRARY,
	0,
	(STRPTR)LibName,
	VSTRING,
	(APTR)LibInit,
	REVISION,
	(struct TagItem *)RTags
};

APTR JumpTable[] =
{
	(APTR)FUNCARRAY_32BIT_NATIVE,
	(APTR)LibOpen,
	(APTR)LibClose,
	(APTR)LibExpunge,
	(APTR)LibReserved,
	(APTR)GetClass,
	(APTR)0xFFFFFFFF
};

///
/// init_class()

static const struct EmulLibEntry ClassDispatcher_gate =
{
	TRAP_LIB,
	0,
	(void(*)(void))ClassDispatcher
};


Class *init_class(struct ClassBase *cb)
{
	Class *cl = NULL;

	if ((cl = MakeClass(LibName, SUPERCLASS, NULL, sizeof(struct ObjData), 0L)))
	{
		cl->cl_Dispatcher.h_Entry = (HOOKFUNC)&ClassDispatcher_gate;
		cl->cl_UserData = (ULONG)cb;
		AddClass(cl);
	}
	cb->LibClass = cl;

	return cl;
}

///
/// InitResources()

BOOL InitResources(struct ClassBase *cb)
{
	if (!(IntuitionBase = OpenLibrary("intuition.library", 50))) return FALSE;
	if (!(UtilityBase = OpenLibrary("utility.library", 50))) return FALSE;
	if (!(MultimediaBase = OpenLibrary("Multimedia/multimedia.class", 50))) return FALSE;
	if (!(init_class(cb))) return FALSE;
	return TRUE;
}

///
/// FreeResources()

void FreeResources(struct ClassBase *cb)
{
	cb = cb;

	if (MultimediaBase) CloseLibrary(MultimediaBase);
	if (UtilityBase) CloseLibrary(UtilityBase);
	if (IntuitionBase) CloseLibrary(IntuitionBase);

	return;
}

///
/// LibInit()

struct ClassBase *lib_init(struct ClassBase *cb, APTR seglist, struct Library *sysbase)
{
	InitSemaphore(&cb->BaseLock);
	cb->Seglist = seglist;
	cb->Attributes = 0;
	sysbase = sysbase;
	return cb;
}

struct Library *LibInit(struct Library *unused, APTR seglist, struct Library *sysbase)
{
	unused = unused;
	SysBase = sysbase;

	return (NewCreateLibraryTags(
		LIBTAG_FUNCTIONINIT, (ULONG)JumpTable,
		LIBTAG_LIBRARYINIT,  (ULONG)lib_init,
		LIBTAG_MACHINE,      MACHINE_PPC,
		LIBTAG_BASESIZE,     sizeof(struct ClassBase),
		LIBTAG_SEGLIST,      (ULONG)seglist,
		LIBTAG_TYPE,         NT_LIBRARY,
		LIBTAG_NAME,         (ULONG)ROMTag.rt_Name,
		LIBTAG_IDSTRING,     (ULONG)ROMTag.rt_IdString,
		LIBTAG_FLAGS,        LIBF_CHANGED | LIBF_SUMUSED,
		LIBTAG_VERSION,      VERSION,
		LIBTAG_REVISION,     REVISION,
		LIBTAG_PUBLIC,       TRUE,
	TAG_END));
}
///
/// LibOpen()

struct Library *LibOpen(void)
{
	struct ClassBase *cb = (struct ClassBase*)REG_A6;
	struct Library *lib = (struct Library*)cb;

	ObtainSemaphore(&cb->BaseLock);

	if (!cb->InitFlag)
	{
		if (InitResources(cb)) cb->InitFlag = TRUE;
		else
		{
			FreeResources(cb);
			lib = NULL;
		}
	}
  
	if (lib)
	{
		cb->LibNode.lib_Flags &= ~LIBF_DELEXP;
		cb->LibNode.lib_OpenCnt++;
	}

	ReleaseSemaphore(&cb->BaseLock);
	return lib;
}

///
/// LibClose()

ULONG LibClose(void)
{
	struct ClassBase *cb = (struct ClassBase*)REG_A6;
	ULONG ret = 0;

	ObtainSemaphore(&cb->BaseLock);
	if (--cb->LibNode.lib_OpenCnt == 0)
	{
		if (cb->LibNode.lib_Flags & LIBF_DELEXP) ret = (ULONG)lib_expunge(cb);
	}
	ReleaseSemaphore(&cb->BaseLock);

	return ret;
}

///
/// LibExpunge()

APTR LibExpunge(void)
{
	struct ClassBase *cb = (struct ClassBase*)REG_A6;

	return(lib_expunge(cb));
}


APTR lib_expunge(struct ClassBase *cb)
{
	APTR seglist = NULL;

	ObtainSemaphore(&cb->BaseLock);

	if (cb->LibNode.lib_OpenCnt == 0)
	{
		if (!cb->LibClass || FreeClass(cb->LibClass))
		{
			cb->LibClass = NULL;
			Forbid();
			Remove((struct Node*)cb);
			Permit();
			FreeResources(cb);
			seglist = cb->Seglist;
			FreeMem((UBYTE*)cb - cb->LibNode.lib_NegSize, cb->LibNode.lib_NegSize + cb->LibNode.lib_PosSize);
			cb = NULL;
		}
		if (cb && cb->LibClass) AddClass(cb->LibClass);
	}
	else cb->LibNode.lib_Flags |= LIBF_DELEXP;

	if (cb) ReleaseSemaphore(&cb->BaseLock);
	return seglist;
}

///
/// LibReserved()

ULONG LibReserved(void)
{
	return 0;
}

///
/// GetClass()

Class *GetClass(VOID)
{
	struct ClassBase *cb = (struct ClassBase*)REG_A6;

	return cb->LibClass;
}

///
//---------------------------------------------------------------------------------------------
/// GetHeader()

BOOL GetHeader(Class *cl, Object *obj)
{
	GET_DATA;
	BOOL result = FALSE;
	UBYTE hdr_buf[SID_HEADER_V2_SIZE];
	UQUAD stream_length;
	UQUAD pos;

	if (DoMethod(obj, MMM_Pull, 0, (ULONG)hdr_buf, 8) == 8)
	{
		stream_length = MediaGetPort64(obj, 0, MMA_StreamLength);

		if ((strequ(hdr_buf, (UBYTE*)"PSID", 4)) || 
		    (strequ(hdr_buf, (UBYTE*)"RSID", 4)))
		{
			d->sid_version = be2word(&hdr_buf[4]);

			if (d->sid_version < 1 || d->sid_version > 4)
			{
				seterr(MMERR_WRONG_DATA);
				MLOG(LOG_ERRORS, "Unknown SID version: %d.", d->sid_version);
				return FALSE;
			}

			d->data_offset = (d->sid_version == 1) ? SID_HEADER_V1_SIZE : SID_HEADER_V2_SIZE;

			pos = 0;
			DoMethod(obj, MMM_Seek, 0, MMM_SEEK_BYTES, &pos);

			if (d->sid_version == 1)
			{
				if (DoMethod(obj, MMM_Pull, 0, (ULONG)hdr_buf, SID_HEADER_V1_SIZE) != SID_HEADER_V1_SIZE)
				{
					seterr(MMERR_END_OF_DATA);
					MLOG(LOG_ERRORS, "Unexpected end of data while reading v1 header.");
					return FALSE;
				}
				d->flags = 0;
			}
			else
			{
				if (DoMethod(obj, MMM_Pull, 0, (ULONG)hdr_buf, SID_HEADER_V2_SIZE) != SID_HEADER_V2_SIZE)
				{
					seterr(MMERR_END_OF_DATA);
					MLOG(LOG_ERRORS, "Unexpected end of data while reading v2+ header.");
					return FALSE;
				}
				d->flags = be2word(&hdr_buf[0x76]);
			}

			d->data_offset = be2word(&hdr_buf[0x06]);
			d->load_address = be2word(&hdr_buf[0x08]);
			d->init_address = be2word(&hdr_buf[0x0A]);
			d->play_address = be2word(&hdr_buf[0x0C]);
			d->songs = be2word(&hdr_buf[0x0E]);
			d->start_song = be2word(&hdr_buf[0x10]);
			d->speed = be2long(&hdr_buf[0x12]);

			copy_sid_string(d->name, &hdr_buf[0x16]);
			copy_sid_string(d->author, &hdr_buf[0x36]);
			copy_sid_string(d->released, &hdr_buf[0x56]);

			{
				UWORD chip = (d->sid_version >= 2) ? ((d->flags >> 2) & 0x03) : 0;
				UWORD count = (d->sid_version >= 2) ? ((d->flags & 0x03) + 1) : 1;

				if (chip == 1) strcpy(d->chip_version, SID_CHIP_8580);
				else if (chip == 2) strcpy(d->chip_version, "6581+8580");
				else if (chip == 3) strcpy(d->chip_version, "unknown");
				else strcpy(d->chip_version, SID_CHIP_6581);

				if (count > 1)
					strcat(d->chip_version, count == 3 ? " \xD73 (stereo)" : " \xD72 (stereo)");
			}

			build_metadata(d);

			if (stream_length > d->data_offset)
				d->data_length = stream_length - d->data_offset;
			else
				d->data_length = 0;

			MLOGV(LOG_INFO, "SID v%d, %d songs, name: '%s', author: '%s'.",
				d->sid_version, d->songs, d->name, d->author);

			pos = d->data_offset;
			DoMethod(obj, MMM_Seek, 0, MMM_SEEK_BYTES, &pos);

			result = TRUE;
		}
		else
		{
			seterr(MMERR_WRONG_DATA);
			MLOG(LOG_ERRORS, "Not a SID file.");
		}
	}
	else
	{
		seterr(MMERR_END_OF_DATA);
		MLOG(LOG_ERRORS, "Unexpected end of data while reading SID signature.");
	}

	return result;
}

///
//---------------------------------------------------------------------------------------------
/// New()

LONG New(Class *cl, Object *obj, struct opSet *msg)
{
	LONG newobj = 0;

	if ((obj = (Object*)DoSuperMethodA(cl, obj, (Msg)msg)))
	{
		DoMethod(obj, MMM_LockObject);

		DoMethod(obj, MMM_AddPort, 0);
		DoMethod(obj, MMM_SetPort, 0, MMA_Port_Type, MDP_TYPE_INPUT);
		DoMethod(obj, MMM_SetPort, 0, MMA_Port_FormatsTable, (ULONG)InputFormats);
		DoMethod(obj, MMM_SetPort, 0, MMA_Port_Format, MMF_STREAM);

		DoMethod(obj, MMM_AddPort, 1);
		DoMethod(obj, MMM_SetPort, 1, MMA_Port_Type, MDP_TYPE_OUTPUT);
		DoMethod(obj, MMM_SetPort, 1, MMA_Port_FormatsTable, (ULONG)OutputFormats);
		DoMethod(obj, MMM_SetPort, 1, MMA_Port_Format, MMF_AUDIO_SID);

		newobj = (LONG)obj;

		DoMethod(obj, MMM_UnlockObject);
	}

	if (!newobj) CoerceMethod(cl, obj, OM_DISPOSE);
	else MLOGV(LOG_INFO, "Object \"%s\" created.", OBJNAME((Object*)newobj));

	return newobj;
}

///
/// Dispose()

LONG Dispose(Class *cl, Object *obj, Msg msg)
{
	DoMethod(obj, MMM_LockObject);
	MLOGV(LOG_INFO, "Object \"%s\" disposed.", OBJNAME(obj));
	DoMethod(obj, MMM_UnlockObject);
	return DoSuperMethodA(cl, obj, msg);
}

///
/// GetPort()

LONG GetPort(Class *cl, Object *obj, struct mmopGetPort *msg)
{
	switch (msg->Attribute)
	{
		case MMA_DataFormat:
		case MMA_ExtraData:
		case MMA_MediaType:
		case MMA_MetaData:
		return DoMethod(obj, OM_GET, msg->Attribute, (ULONG)msg->Storage);
	}
	return (DoSuperMethodA(cl, obj, (Msg)msg));
}

///
/// Get()

LONG Get(Class *cl, Object *obj, struct opGet *msg)
{
	GET_DATA;

	switch (msg->opg_AttrID)
	{
		case MMA_Audio_SongCount:
			*msg->opg_Storage = d->songs;
		return TRUE;

		case MMA_Audio_StartSong:
			*msg->opg_Storage = d->start_song;
		return TRUE;

		case MMA_Audio_SIDVersion:
			*msg->opg_Storage = d->sid_version;
		return TRUE;

		case MMA_Audio_BitsPerSample:
			*msg->opg_Storage = 8;
		return TRUE;

		case MMA_Audio_Channels:
			*msg->opg_Storage = 1;
		return TRUE;

		case MMA_DataFormat:
			*msg->opg_Storage = (LONG)"SID";
		return TRUE;

		case MMA_Audio_Name:
			*msg->opg_Storage = (LONG)d->name;
		return TRUE;

		case MMA_Audio_Author:
			*msg->opg_Storage = (LONG)d->author;
		return TRUE;

		case MMA_Audio_Released:
			*msg->opg_Storage = (LONG)d->released;
		return TRUE;

		case MMA_MetaData:
			*msg->opg_Storage = (LONG)&(d->metadata[0]);
		return TRUE;

		case MMA_MediaType:
			*msg->opg_Storage = MMT_AUDIO;
		return TRUE;

		default: return DoSuperMethodA(cl, obj, (Msg)msg);
	}
}

///
/// Pull()

LONG Pull(Class *cl, Object *obj, struct mmopData *msg)
{
	GET_DATA;
	ULONG bytes_pulled = 0;

	MLOGV(LOG_VERBOSE, "-> Pull on port %lu of '%s', %lu bytes to $%08lX.",
		msg->Port, OBJNAME(obj), msg->Length, msg->Buffer);

	DoMethod(obj, MMM_LockObject);
	seterr(0);

	if (msg->Buffer && msg->Length)
	{
		switch (msg->Port)
		{
			case 0:
				bytes_pulled = DoSuperMethodA(cl, obj, (Msg)msg);
			break;

			case 1:
				bytes_pulled = DoMethod(obj, MMM_Pull, 0, msg->Buffer, msg->Length);

				if (bytes_pulled < msg->Length)
				{
					seterr(MediaGetPortFwd(obj, 0, MMA_ErrorCode));
				}
			break;

			default:
				seterr(MMERR_WRONG_ARGUMENTS);
				MLOGV(LOG_ERRORS, "'%s' has no port %lu.", OBJNAME(obj), msg->Port);
			break;
		}
	}
	else
	{
		MLOG(LOG_ERRORS, "NULL buffer or zero length.");
		seterr(MMERR_WRONG_ARGUMENTS);
	}

	DoMethod(obj, MMM_UnlockObject);

	MLOGV(LOG_VERBOSE, "<- Pull on port %lu of '%s' finished, %lu bytes delivered.",
		msg->Port, OBJNAME(obj), bytes_pulled);

	return bytes_pulled;
}

///
/// Setup()

LONG Setup(Class *cl, Object *obj, struct mmopPort *msg)
{
	GET_DATA;
	LONG rv = 0;

	MLOGV(LOG_VERBOSE, "Started for port %lu.", msg->Port);

	if (msg->Port == 0)
	{
		rv = GetHeader(cl, obj);
	}
	else if (msg->Port == 1) rv = TRUE;
	else MLOGV(LOG_ERRORS, "Setup on non existing port %ld.", msg->Port);

	return rv;
}

///
/// dispatcher

LONG ClassDispatcher(void)
{
	Class *cl = (Class*)REG_A0;
	Object *obj = (Object*)REG_A2;
	Msg msg = (Msg)REG_A1;

	switch(msg->MethodID)
	{
		case OM_NEW:                return New(cl, obj, (struct opSet*)msg);
		case OM_DISPOSE:            return Dispose(cl, obj, msg);
		case OM_GET:                return Get(cl, obj, (struct opGet*)msg);
		case MMM_Pull:              return Pull(cl, obj, (struct mmopData*)msg);
		case MMM_Setup:             return Setup(cl, obj, (struct mmopPort*)msg);
		case MMM_GetPort:           return GetPort(cl, obj, (struct mmopGetPort*)msg);
		default:                    return DoSuperMethodA(cl, obj, msg);
	}
}

///
