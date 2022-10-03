/** $Id: VPoxSFInternal.h $ */
/** @file
 * VPoxSF - OS/2 Shared Folder IFS, Internal Header.
 */

/*
 * Copyright (c) 2007 knut st. osmundsen <bird-src-spam@anduin.net>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef GA_INCLUDED_SRC_os2_VPoxSF_VPoxSFInternal_h
#define GA_INCLUDED_SRC_os2_VPoxSF_VPoxSFInternal_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif


#define INCL_BASE
#define INCL_ERROR
#define INCL_LONGLONG
#define OS2EMX_PLAIN_CHAR
#include <os2ddk/bsekee.h>
#include <os2ddk/devhlp.h>
#include <os2ddk/unikern.h>
#include <os2ddk/fsd.h>
#undef RT_MAX

#include <iprt/types.h>
#include <iprt/assert.h>
#include <iprt/list.h>
#include <VPox/VPoxGuest.h>
#include <VPox/VPoxGuestLibSharedFoldersInline.h>


/** Allocation header used by RTMemAlloc.
 * This should be subtracted from round numbers. */
#define ALLOC_HDR_SIZE  (0x10 + 4)


/**
 * A shared folder
 */
typedef struct VPOXSFFOLDER
{
    /** For the shared folder list. */
    RTLISTNODE          ListEntry;
    /** Magic number (VPOXSFFOLDER_MAGIC). */
    uint32_t            u32Magic;
    /** Number of active references to this folder. */
    uint32_t volatile   cRefs;
    /** Number of open files referencing this folder.   */
    uint32_t volatile   cOpenFiles;
    /** Number of open searches referencing this folder.   */
    uint32_t volatile   cOpenSearches;
    /** Number of drives this is attached to. */
    uint8_t volatile    cDrives;

    /** The host folder handle. */
    SHFLROOT            idHostRoot;

    /** OS/2 volume handle. */
    USHORT              hVpb;

    /** The length of the name and tag, including zero terminators and such. */
    uint16_t            cbNameAndTag;
    /** The length of the folder name. */
    uint8_t             cchName;
    /** The shared folder name.  If there is a tag it follows as a second string. */
    RT_FLEXIBLE_ARRAY_EXTENSION
    char                szName[RT_FLEXIBLE_ARRAY];
} VPOXSFFOLDER;
/** Pointer to a shared folder. */
typedef VPOXSFFOLDER *PVPOXSFFOLDER;
/** Magic value for VPOXSFVP (Neal Town Stephenson). */
#define VPOXSFFOLDER_MAGIC      UINT32_C(0x19591031)

/** The shared mutex protecting folders list, drives and the connection. */
extern MutexLock_t      g_MtxFolders;
/** List of active folder (PVPOXSFFOLDER). */
extern RTLISTANCHOR     g_FolderHead;


/**
 * VPoxSF Volume Parameter Structure.
 *
 * @remarks Overlays the 36 byte VPFSD structure (fsd.h).
 * @note    No self pointer as the kernel may reallocate these.
 */
typedef struct VPOXSFVP
{
    /** Magic value (VPOXSFVP_MAGIC). */
    uint32_t         u32Magic;
    /** The folder. */
    PVPOXSFFOLDER    pFolder;
} VPOXSFVP;
AssertCompile(sizeof(VPOXSFVP) <= sizeof(VPFSD));
/** Pointer to a VPOXSFVP struct. */
typedef VPOXSFVP *PVPOXSFVP;
/** Magic value for VPOXSFVP (Laurence van Cott Niven). */
#define VPOXSFVP_MAGIC          UINT32_C(0x19380430)


/**
 * VPoxSF Current Directory Structure.
 *
 * @remark  Overlays the 8 byte CDFSD structure (fsd.h).
 */
typedef struct VPOXSFCD
{
    uint32_t u32Dummy;
} VPOXSFCD;
AssertCompile(sizeof(VPOXSFCD) <= sizeof(CDFSD));
/** Pointer to a VPOXSFCD struct. */
typedef VPOXSFCD *PVPOXSFCD;


/**
 * VPoxSF System File Structure.
 *
 * @remark  Overlays the 30 byte SFFSD structure (fsd.h).
 */
typedef struct VPOXSFSYFI
{
    /** Magic value (VPOXSFSYFI_MAGIC). */
    uint32_t            u32Magic;
    /** Self pointer for quick 16:16 to flat translation. */
    struct VPOXSFSYFI  *pSelf;
    /** The host file handle. */
    SHFLHANDLE          hHostFile;
    /** The shared folder (referenced). */
    PVPOXSFFOLDER       pFolder;
} VPOXSFSYFI;
AssertCompile(sizeof(VPOXSFSYFI) <= sizeof(SFFSD));
/** Pointer to a VPOXSFSYFI struct. */
typedef VPOXSFSYFI *PVPOXSFSYFI;
/** Magic value for VPOXSFSYFI (Jon Ellis Meacham). */
#define VPOXSFSYFI_MAGIC         UINT32_C(0x19690520)


/**
 * More file search data (on physical heap).
 */
typedef struct VPOXSFFSBUF /**< @todo rename as is no longer buffer. */
{
    /** The request (must be first). */
    VPOXSFLISTDIRREQ    Req;
    /** A magic number (VPOXSFFSBUF_MAGIC). */
    uint32_t            u32Magic;
    /** The filter string (full path), NULL if all files are request. */
    PSHFLSTRING         pFilter;
    /** Size of the buffer for directory entries. */
    uint32_t            cbBuf;
    /** Buffer for directory entries on the physical heap. */
    PSHFLDIRINFO        pBuf;
    /** Must have attributes (shifted down DOS attributes).  */
    uint8_t             fMustHaveAttribs;
    /** Non-matching attributes (shifted down DOS attributes).  */
    uint8_t             fExcludedAttribs;
    /** Set if FF_ATTR_LONG_FILENAME. */
    bool                fLongFilenames : 1;
    uint8_t             bPadding1;
    /** The local time offset to use for this search. */
    int16_t             cMinLocalTimeDelta;
    uint8_t             abPadding2[2];
    /** Number of valid bytes in the buffer. */
    uint32_t            cbValid;
    /** Number of entries left in the buffer.   */
    uint32_t            cEntriesLeft;
    /** The next entry. */
    PSHFLDIRINFO        pEntry;
    //uint32_t            uPadding3;
    /** Staging area for staging a full FILEFINDBUF4L (+ 32 safe bytes). */
    uint8_t             abStaging[RT_ALIGN_32(sizeof(FILEFINDBUF4L) + 32, 8)];
} VPOXSFFSBUF;
/** Pointer to a file search buffer. */
typedef VPOXSFFSBUF *PVPOXSFFSBUF;
/** Magic number for VPOXSFFSBUF (Robert Anson Heinlein). */
#define VPOXSFFSBUF_MAGIC       UINT32_C(0x19070707)


/**
 * VPoxSF File Search Structure.
 *
 * @remark  Overlays the 24 byte FSFSD structure (fsd.h).
 * @note    No self pointer as the kernel may reallocate these.
 */
typedef struct VPOXSFFS
{
    /** Magic value (VPOXSFFS_MAGIC). */
    uint32_t            u32Magic;
    /** The last file position position. */
    uint32_t            offLastFile;
    /** The host directory handle. */
    SHFLHANDLE          hHostDir;
    /** The shared folder (referenced). */
    PVPOXSFFOLDER       pFolder;
    /** Search data buffer. */
    PVPOXSFFSBUF        pBuf;
} VPOXSFFS;
AssertCompile(sizeof(VPOXSFFS) <= sizeof(FSFSD));
/** Pointer to a VPOXSFFS struct. */
typedef VPOXSFFS *PVPOXSFFS;
/** Magic number for VPOXSFFS (Isaak Azimov). */
#define VPOXSFFS_MAGIC          UINT32_C(0x19200102)


extern VBGLSFCLIENT g_SfClient;
extern uint32_t g_fHostFeatures;

void        vpoxSfOs2InitFileBuffers(void);
PSHFLSTRING vpoxSfOs2StrAlloc(size_t cwcLength);
PSHFLSTRING vpoxSfOs2StrDup(PCSHFLSTRING pSrc);
void        vpoxSfOs2StrFree(PSHFLSTRING pStr);

APIRET      vpoxSfOs2ResolvePath(const char *pszPath, PVPOXSFCD pCdFsd, LONG offCurDirEnd,
                                 PVPOXSFFOLDER *ppFolder, PSHFLSTRING *ppStrFolderPath);
APIRET      vpoxSfOs2ResolvePathEx(const char *pszPath, PVPOXSFCD pCdFsd, LONG offCurDirEnd, uint32_t offStrInBuf,
                                   PVPOXSFFOLDER *ppFolder, void **ppvBuf);
void        vpoxSfOs2ReleasePathAndFolder(PSHFLSTRING pStrPath, PVPOXSFFOLDER pFolder);
void        vpoxSfOs2ReleaseFolder(PVPOXSFFOLDER pFolder);
APIRET      vpoxSfOs2ConvertStatusToOs2(int vrc, APIRET rcDefault);
int16_t     vpoxSfOs2GetLocalTimeDelta(void);
void        vpoxSfOs2DateTimeFromTimeSpec(FDATE *pDosDate, FTIME *pDosTime, RTTIMESPEC SrcTimeSpec, int16_t cMinLocalTimeDelta);
PRTTIMESPEC vpoxSfOs2DateTimeToTimeSpec(FDATE DosDate, FTIME DosTime, int16_t cMinLocalTimeDelta, PRTTIMESPEC pDstTimeSpec);
APIRET      vpoxSfOs2FileStatusFromObjInfo(PBYTE pbDst, ULONG cbDst, ULONG uLevel, SHFLFSOBJINFO const *pSrc);
APIRET      vpoxSfOs2SetInfoCommonWorker(PVPOXSFFOLDER pFolder, SHFLHANDLE hHostFile, ULONG fAttribs,
                                         PFILESTATUS pTimestamps, PSHFLFSOBJINFO pObjInfoBuf, uint32_t offObjInfoInAlloc);

APIRET      vpoxSfOs2CheckEaOpForCreation(EAOP const *pEaOp);
APIRET      vpoxSfOs2MakeEmptyEaList(PEAOP pEaOp, ULONG uLevel);
APIRET      vpoxSfOs2MakeEmptyEaListEx(PEAOP pEaOp, ULONG uLevel, ULONG cbFullEasLeft, uint32_t *pcbWritten, ULONG *poffError);

DECLASM(PVPOXSFVP)  Fsh32GetVolParams(USHORT hVbp, PVPFSI *ppVpFsi /*optional*/);
DECLASM(APIRET)     SafeKernStrToUcs(PUconvObj, UniChar *, char *, LONG, LONG);
DECLASM(APIRET)     SafeKernStrFromUcs(PUconvObj, char *, UniChar *, LONG, LONG);


#endif /* !GA_INCLUDED_SRC_os2_VPoxSF_VPoxSFInternal_h */

