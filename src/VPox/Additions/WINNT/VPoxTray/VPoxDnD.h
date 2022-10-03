/* $Id: VPoxDnD.h $ */
/** @file
 * VPoxDnD.h - Windows-specific bits of the drag'n drop service.
 */

/*
 * Copyright (C) 2013-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef GA_INCLUDED_SRC_WINNT_VPoxTray_VPoxDnD_h
#define GA_INCLUDED_SRC_WINNT_VPoxTray_VPoxDnD_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/critsect.h>

#include <iprt/cpp/mtlist.h>
#include <iprt/cpp/ministring.h>

class VPoxDnDWnd;

/**
 * Class for implementing IDataObject for VPoxTray's DnD support.
 */
class VPoxDnDDataObject : public IDataObject
{
public:

    enum Status
    {
        Status_Uninitialized = 0,
        Status_Initialized,
        Status_Dropping,
        Status_Dropped,
        Status_Aborted,
        Status_32Bit_Hack = 0x7fffffff
    };

public:

    VPoxDnDDataObject(LPFORMATETC pFormatEtc = NULL, LPSTGMEDIUM pStgMed = NULL, ULONG cFormats = 0);
    virtual ~VPoxDnDDataObject(void);

public: /* IUnknown methods. */

    STDMETHOD(QueryInterface)(REFIID iid, void ** ppvObject);
    STDMETHOD_(ULONG, AddRef)(void);
    STDMETHOD_(ULONG, Release)(void);

public: /* IDataObject methods. */

    STDMETHOD(GetData)(LPFORMATETC pFormatEtc, LPSTGMEDIUM pMedium);
    STDMETHOD(GetDataHere)(LPFORMATETC pFormatEtc, LPSTGMEDIUM pMedium);
    STDMETHOD(QueryGetData)(LPFORMATETC pFormatEtc);
    STDMETHOD(GetCanonicalFormatEtc)(LPFORMATETC pFormatEct,  LPFORMATETC pFormatEtcOut);
    STDMETHOD(SetData)(LPFORMATETC pFormatEtc, LPSTGMEDIUM pMedium, BOOL fRelease);
    STDMETHOD(EnumFormatEtc)(DWORD dwDirection, IEnumFORMATETC **ppEnumFormatEtc);
    STDMETHOD(DAdvise)(LPFORMATETC pFormatEtc, DWORD advf, IAdviseSink *pAdvSink, DWORD *pdwConnection);
    STDMETHOD(DUnadvise)(DWORD dwConnection);
    STDMETHOD(EnumDAdvise)(IEnumSTATDATA **ppEnumAdvise);

public:

    static const char* ClipboardFormatToString(CLIPFORMAT fmt);

    int Abort(void);
    void SetStatus(Status status);
    int Signal(const RTCString &strFormat, const void *pvData, size_t cbData);

protected:

    bool LookupFormatEtc(LPFORMATETC pFormatEtc, ULONG *puIndex);
    void RegisterFormat(LPFORMATETC pFormatEtc, CLIPFORMAT clipFormat, TYMED tyMed = TYMED_HGLOBAL,
                        LONG lindex = -1, DWORD dwAspect = DVASPECT_CONTENT, DVTARGETDEVICE *pTargetDevice = NULL);

    /** Current drag and drop status. */
    Status      m_enmStatus;
    /** Internal reference count of this object. */
    LONG        m_cRefs;
    /** Number of native formats registered. This can be a different number than supplied with m_lstFormats. */
    ULONG       m_cFormats;
    /** Array of registered FORMATETC structs. Matches m_cFormats. */
    LPFORMATETC m_paFormatEtc;
    /** Array of registered STGMEDIUM structs. Matches m_cFormats. */
    LPSTGMEDIUM m_paStgMedium;
    /** Event semaphore used for waiting on status changes. */
    RTSEMEVENT  m_EvtDropped;
    /** Format of currently retrieved data. */
    RTCString   m_strFormat;
    /** The retrieved data as a raw buffer. */
    void       *m_pvData;
    /** Raw buffer size (in bytes). */
    size_t      m_cbData;
};

/**
 * Class for implementing IDropSource for VPoxTray's DnD support.
 */
class VPoxDnDDropSource : public IDropSource
{
public:

    VPoxDnDDropSource(VPoxDnDWnd *pThis);
    virtual ~VPoxDnDDropSource(void);

public:

    VPOXDNDACTION GetCurrentAction(void) { return m_enmActionCurrent; }

public: /* IUnknown methods. */

    STDMETHOD(QueryInterface)(REFIID iid, void ** ppvObject);
    STDMETHOD_(ULONG, AddRef)(void);
    STDMETHOD_(ULONG, Release)(void);

public: /* IDropSource methods. */

    STDMETHOD(QueryContinueDrag)(BOOL fEscapePressed, DWORD dwKeyState);
    STDMETHOD(GiveFeedback)(DWORD dwEffect);

protected:

    /** Reference count of this object. */
    LONG                  m_cRefs;
    /** Pointer to parent proxy window. */
    VPoxDnDWnd           *m_pWndParent;
    /** Current drag effect. */
    DWORD                 m_dwCurEffect;
    /** Current action to perform on the host. */
    VPOXDNDACTION         m_enmActionCurrent;
};

/**
 * Class for implementing IDropTarget for VPoxTray's DnD support.
 */
class VPoxDnDDropTarget : public IDropTarget
{
public:

    VPoxDnDDropTarget(VPoxDnDWnd *pThis);
    virtual ~VPoxDnDDropTarget(void);

public: /* IUnknown methods. */

    STDMETHOD(QueryInterface)(REFIID iid, void ** ppvObject);
    STDMETHOD_(ULONG, AddRef)(void);
    STDMETHOD_(ULONG, Release)(void);

public: /* IDropTarget methods. */

    STDMETHOD(DragEnter)(IDataObject *pDataObject, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect);
    STDMETHOD(DragOver)(DWORD grfKeyState, POINTL pt, DWORD *pdwEffect);
    STDMETHOD(DragLeave)(void);
    STDMETHOD(Drop)(IDataObject *pDataObject, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect);

protected:

    static void DumpFormats(IDataObject *pDataObject);
    static DWORD GetDropEffect(DWORD grfKeyState, DWORD dwAllowedEffects);
    void reset(void);

public:

    /** Returns the data as mutable raw. Use with caution! */
    void *DataMutableRaw(void) const { return m_pvData; }

    /** Returns the data size (in bytes). */
    size_t DataSize(void) const { return m_cbData; }

    RTCString Formats(void) const;
    int WaitForDrop(RTMSINTERVAL msTimeout);

protected:

    /** Reference count of this object. */
    LONG                  m_cRefs;
    /** Pointer to parent proxy window. */
    VPoxDnDWnd           *m_pWndParent;
    /** Current drop effect. */
    DWORD                 m_dwCurEffect;
    /** Copy of the data object's current FORMATETC struct.
     *  Note: We don't keep the pointer of the DVTARGETDEVICE here! */
    FORMATETC             m_FormatEtc;
    /** Stringified data object's format currently in use.  */
    RTCString             m_strFormat;
    /** Pointer to actual format data. */
    void                 *m_pvData;
    /** Size (in bytes) of format data. */
    size_t                m_cbData;
    /** Event for waiting on the "drop" event. */
    RTSEMEVENT            m_EvtDrop;
    /** Result of the drop event. */
    int                   m_rcDropped;
};

/**
 * Class for implementing IEnumFORMATETC for VPoxTray's DnD support.
 */
class VPoxDnDEnumFormatEtc : public IEnumFORMATETC
{
public:

    VPoxDnDEnumFormatEtc(LPFORMATETC pFormatEtc, ULONG cFormats);
    virtual ~VPoxDnDEnumFormatEtc(void);

public:

    STDMETHOD(QueryInterface)(REFIID iid, void ** ppvObject);
    STDMETHOD_(ULONG, AddRef)(void);
    STDMETHOD_(ULONG, Release)(void);

    STDMETHOD(Next)(ULONG cFormats, LPFORMATETC pFormatEtc, ULONG *pcFetched);
    STDMETHOD(Skip)(ULONG cFormats);
    STDMETHOD(Reset)(void);
    STDMETHOD(Clone)(IEnumFORMATETC **ppEnumFormatEtc);

public:

    static void CopyFormat(LPFORMATETC pFormatDest, LPFORMATETC pFormatSource);
    static HRESULT CreateEnumFormatEtc(UINT cFormats, LPFORMATETC pFormatEtc, IEnumFORMATETC **ppEnumFormatEtc);

private:

    /** Reference count of this object. */
    LONG        m_cRefs;
    /** Current index for format iteration. */
    ULONG       m_uIdxCur;
    /** Number of format this object contains. */
    ULONG       m_cFormats;
    /** Array of FORMATETC formats this object contains. Matches m_cFormats. */
    LPFORMATETC m_paFormatEtc;
};

struct VPOXDNDCONTEXT;
class VPoxDnDWnd;

/**
 * A drag'n drop event from the host.
 */
typedef struct VPOXDNDEVENT
{
    /** The actual DnD HGCM event data. */
    PVBGLR3DNDEVENT pVbglR3Event;

} VPOXDNDEVENT, *PVPOXDNDEVENT;

/**
 * DnD context data.
 */
typedef struct VPOXDNDCONTEXT
{
    /** Pointer to the service environment. */
    const VPOXSERVICEENV      *pEnv;
    /** Started indicator. */
    bool                       fStarted;
    /** Shutdown indicator. */
    bool                       fShutdown;
    /** The registered window class. */
    ATOM                       wndClass;
    /** The DnD main event queue. */
    RTCMTList<VPOXDNDEVENT>    lstEvtQueue;
    /** Semaphore for waiting on main event queue
     *  events. */
    RTSEMEVENT                 hEvtQueueSem;
    /** List of drag'n drop proxy windows.
     *  Note: At the moment only one window is supported. */
    RTCMTList<VPoxDnDWnd*>     lstWnd;
    /** The DnD command context. */
    VBGLR3GUESTDNDCMDCTX       cmdCtx;

} VPOXDNDCONTEXT, *PVPOXDNDCONTEXT;

/**
 * Everything which is required to successfully start
 * a drag'n drop operation via DoDragDrop().
 */
typedef struct VPOXDNDSTARTUPINFO
{
    /** Our DnD data object, holding
     *  the raw DnD data. */
    VPoxDnDDataObject         *pDataObject;
    /** The drop source for sending the
     *  DnD request to a IDropTarget. */
    VPoxDnDDropSource         *pDropSource;
    /** The DnD effects which are wanted / allowed. */
    DWORD                      dwOKEffects;

} VPOXDNDSTARTUPINFO, *PVPOXDNDSTARTUPINFO;

/**
 * Class for handling a DnD proxy window.
 ** @todo Unify this and VPoxClient's DragInstance!
 */
class VPoxDnDWnd
{
    /**
     * Current state of a DnD proxy
     * window.
     */
    enum State
    {
        Uninitialized = 0,
        Initialized,
        Dragging,
        Dropped,
        Canceled
    };

    /**
     * Current operation mode of
     * a DnD proxy window.
     */
    enum Mode
    {
        /** Unknown mode. */
        Unknown = 0,
        /** Host to guest. */
        HG,
        /** Guest to host. */
        GH
    };

public:

    VPoxDnDWnd(void);
    virtual ~VPoxDnDWnd(void);

public:

    int Initialize(PVPOXDNDCONTEXT a_pCtx);
    void Destroy(void);

public:

    /** The window's thread for the native message pump and OLE context. */
    static DECLCALLBACK(int) Thread(RTTHREAD hThread, void *pvUser);

public:

    static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM lParam);
    /** The per-instance wndproc routine. */
    LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

public:

#ifdef VPOX_WITH_DRAG_AND_DROP_GH
    int RegisterAsDropTarget(void);
    int UnregisterAsDropTarget(void);
#endif

public:

    int OnCreate(void);
    void OnDestroy(void);

    int Abort(void);

    /* Host -> Guest */
    int OnHgEnter(const RTCList<RTCString> &formats, VPOXDNDACTIONLIST m_lstActionsAllowed);
    int OnHgMove(uint32_t u32xPos, uint32_t u32yPos, VPOXDNDACTION dndAction);
    int OnHgDrop(void);
    int OnHgLeave(void);
    int OnHgDataReceive(PVBGLR3GUESTDNDMETADATA pMeta);
    int OnHgCancel(void);

#ifdef VPOX_WITH_DRAG_AND_DROP_GH
    /* Guest -> Host */
    int OnGhIsDnDPending(void);
    int OnGhDrop(const RTCString &strFormat, VPOXDNDACTION dndActionDefault);
#endif

    void PostMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
    int ProcessEvent(PVPOXDNDEVENT pEvent);

    int Hide(void);
    void Reset(void);

protected:

    int checkForSessionChange(void);
    int makeFullscreen(void);
    int mouseMove(int x, int y, DWORD dwMouseInputFlags);
    int mouseRelease(void);
    int setMode(Mode enmMode);

public: /** @todo Make protected! */

    /** Pointer to DnD context. */
    PVPOXDNDCONTEXT            m_pCtx;
    /** The proxy window's main thread for processing
     *  window messages. */
    RTTHREAD                   m_hThread;
    /** Critical section to serialize access. */
    RTCRITSECT                 m_CritSect;
    /** Event semaphore to wait for new DnD events. */
    RTSEMEVENT                 m_EvtSem;
#ifdef RT_OS_WINDOWS
    /** The window's handle. */
    HWND                       m_hWnd;
    /** List of allowed MIME types this
     *  client can handle. Make this a per-instance
     *  property so that we can selectively allow/forbid
     *  certain types later on runtime. */
    RTCList<RTCString>         m_lstFmtSup;
    /** List of formats for the current
     *  drag'n drop operation. */
    RTCList<RTCString>         m_lstFmtActive;
    /** List of all current drag'n drop actions allowed. */
    VPOXDNDACTIONLIST          m_lstActionsAllowed;
    /** The startup information required
     *  for the actual DoDragDrop() call. */
    VPOXDNDSTARTUPINFO         m_startupInfo;
    /** Is the left mouse button being pressed
     *  currently while being in this window? */
    bool                       m_fMouseButtonDown;
# ifdef VPOX_WITH_DRAG_AND_DROP_GH
    /** Pointer to IDropTarget implementation for
     *  guest -> host support. */
    VPoxDnDDropTarget         *m_pDropTarget;
# endif /* VPOX_WITH_DRAG_AND_DROP_GH */
#else /* !RT_OS_WINDOWS */
    /** @todo Implement me. */
#endif /* !RT_OS_WINDOWS */

    /** The window's own DnD context. */
    VBGLR3GUESTDNDCMDCTX       m_cmdCtx;
    /** The current operation mode. */
    Mode                       m_enmMode;
    /** The current state. */
    State                      m_enmState;
    /** Format being requested. */
    RTCString                  m_strFmtReq;
};

#endif /* !GA_INCLUDED_SRC_WINNT_VPoxTray_VPoxDnD_h */

