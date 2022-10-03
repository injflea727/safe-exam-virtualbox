/* $Id: AudioHlp.h $ */
/** @file
 * Audio helper routines.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VPOX_INCLUDED_SRC_Audio_AudioHlp_h
#define VPOX_INCLUDED_SRC_Audio_AudioHlp_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <limits.h>

#include <iprt/circbuf.h>
#include <iprt/critsect.h>
#include <iprt/file.h>
#include <iprt/path.h>

#include <VPox/vmm/pdmdev.h>
#include <VPox/vmm/pdm.h>
#include <VPox/vmm/pdmaudioifs.h>

/** @name Audio calculation helper methods.
 * @{ */
uint32_t AudioHlpCalcBitrate(uint8_t cBits, uint32_t uHz, uint8_t cChannels);
/** @} */

/** @name Audio PCM properties helper methods.
 * @{ */
bool     AudioHlpPcmPropsAreValid(PCPDMAUDIOPCMPROPS pProps);
/** @}  */

/** @name Audio configuration helper methods.
 * @{ */
bool    AudioHlpStreamCfgIsValid(PCPDMAUDIOSTREAMCFG pCfg);
/** @}  */


/** @name AUDIOHLPFILE_FLAGS_XXX
 * @{ */
/** No flags defined. */
#define AUDIOHLPFILE_FLAGS_NONE             UINT32_C(0)
/** Keep the audio file even if it contains no audio data. */
#define AUDIOHLPFILE_FLAGS_KEEP_IF_EMPTY    RT_BIT_32(0)
/** Audio file flag validation mask. */
#define AUDIOHLPFILE_FLAGS_VALID_MASK       UINT32_C(0x1)
/** @} */

/** Audio file default open flags. */
#define AUDIOHLPFILE_DEFAULT_OPEN_FLAGS (RTFILE_O_OPEN_CREATE | RTFILE_O_APPEND | RTFILE_O_WRITE | RTFILE_O_DENY_WRITE)

/**
 * Audio file types.
 */
typedef enum AUDIOHLPFILETYPE
{
    /** The customary invalid zero value. */
    AUDIOHLPFILETYPE_INVALID = 0,
    /** Raw (PCM) file. */
    AUDIOHLPFILETYPE_RAW,
    /** Wave (.WAV) file. */
    AUDIOHLPFILETYPE_WAV,
    /** Hack to blow the type up to 32-bit. */
    AUDIOHLPFILETYPE_32BIT_HACK = 0x7fffffff
} AUDIOHLPFILETYPE;

/** @name AUDIOHLPFILENAME_FLAGS_XXX
 * @{ */
/** No flags defined. */
#define AUDIOHLPFILENAME_FLAGS_NONE         UINT32_C(0)
/** Adds an ISO timestamp to the file name. */
#define AUDIOHLPFILENAME_FLAGS_TS           RT_BIT_32(0)
/** Valid flag mask. */
#define AUDIOHLPFILENAME_FLAGS_VALID_MASK   AUDIOHLPFILENAME_FLAGS_TS
/** @} */

/**
 * Audio file handle.
 */
typedef struct AUDIOHLPFILE
{
    /** Type of the audio file. */
    AUDIOHLPFILETYPE    enmType;
    /** Audio file flags, AUDIOHLPFILE_FLAGS_XXX. */
    uint32_t            fFlags;
    /** Amount of wave data written. */
    uint64_t            cbWaveData;
    /** Actual file handle. */
    RTFILE              hFile;
    /** File name and path. */
    RT_FLEXIBLE_ARRAY_EXTENSION
    char                szName[RT_FLEXIBLE_ARRAY];
} AUDIOHLPFILE;
/** Pointer to an audio file handle. */
typedef AUDIOHLPFILE *PAUDIOHLPFILE;

/** @name Audio file methods.
 * @{ */
int     AudioHlpFileCreateAndOpen(PAUDIOHLPFILE *ppFile, const char *pszDir, const char *pszName,
                                  uint32_t iInstance, PCPDMAUDIOPCMPROPS pProps);
int     AudioHlpFileCreateAndOpenEx(PAUDIOHLPFILE *ppFile, AUDIOHLPFILETYPE enmType, const char *pszDir,
                                    uint32_t iInstance, uint32_t fFilename, uint32_t fCreate,
                                    PCPDMAUDIOPCMPROPS pProps, uint64_t fOpen, const char *pszName, ...);
int     AudioHlpFileCreateF(PAUDIOHLPFILE *ppFile, uint32_t fFlags, AUDIOHLPFILETYPE enmType,
                            const char *pszPath, uint32_t fFilename, uint32_t uInstance, const char *pszFileFmt, ...);

void    AudioHlpFileDestroy(PAUDIOHLPFILE pFile);
int     AudioHlpFileOpen(PAUDIOHLPFILE pFile, uint64_t fOpen, PCPDMAUDIOPCMPROPS pProps);
int     AudioHlpFileClose(PAUDIOHLPFILE pFile);
int     AudioHlpFileDelete(PAUDIOHLPFILE pFile);
bool    AudioHlpFileIsOpen(PAUDIOHLPFILE pFile);
int     AudioHlpFileWrite(PAUDIOHLPFILE pFile, const void *pvBuf, size_t cbBuf);
/** @}  */

#endif /* !VPOX_INCLUDED_SRC_Audio_AudioHlp_h */

