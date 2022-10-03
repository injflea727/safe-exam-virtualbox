/* $Id: vmstarter.mm $ */
/** @file
 * VPox Qt GUI -  Helper application for starting vpox the right way when the user double clicks on a file type association.
 */

/*
 * Copyright (C) 2010-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#import <Cocoa/Cocoa.h>
#include <iprt/cdefs.h>

@interface AppDelegate: NSObject
{
NSString *m_strVPoxPath;
}
@end

@implementation AppDelegate
-(id) init
{
    self = [super init];
    if (self)
    {
        /* Get the path of VPox by looking where our bundle is located. */
        m_strVPoxPath = [[[[NSBundle mainBundle] bundlePath]
                          stringByAppendingPathComponent:@"/../../../../VirtualPox.app"]
                         stringByStandardizingPath];
        /* We kill ourself after 1 seconds */
        [NSTimer scheduledTimerWithTimeInterval:1.0
            target:NSApp
            selector:@selector(terminate:)
            userInfo:nil
            repeats:NO];
    }

    return self;
}

- (void)application:(NSApplication *)sender openFiles:(NSArray *)filenames
{
    RT_NOREF(sender);

    BOOL fResult = FALSE;
    NSWorkspace *pWS = [NSWorkspace sharedWorkspace];
    /* We need to check if vpox is running already. If so we sent an open
       event. If not we start a new process with the file as parameter. */
    NSArray *pApps = [pWS runningApplications];
    bool fVPoxRuns = false;
    for (NSRunningApplication *pApp in pApps)
    {
        if ([pApp.bundleIdentifier isEqualToString:@"org.virtualpox.app.VirtualPox"])
        {
            fVPoxRuns = true;
            break;
        }
    }
    if (fVPoxRuns)
    {
        /* Send the open event.
         * Todo: check for an method which take a list of files. */
        for (NSString *filename in filenames)
            fResult = [pWS openFile:filename withApplication:m_strVPoxPath andDeactivate:TRUE];
    }
    else
    {
        /* Fire up a new instance of VPox. We prefer LSOpenApplication over
           NSTask, cause it makes sure that VPox will become the front most
           process after starting up. */
/** @todo should replace all this with -[NSWorkspace
 *  launchApplicationAtURL:options:configuration:error:] because LSOpenApplication is deprecated in
 * 10.10 while, FSPathMakeRef is deprecated since 10.8. */
        /* The state horror show starts right here: */
        OSStatus err = noErr;
        Boolean fDir;
        void *asyncLaunchRefCon = NULL;
        FSRef fileRef;
        CFStringRef file = NULL;
        CFArrayRef args = NULL;
        void **list = (void**)malloc(sizeof(void*) * [filenames count]);
        for (size_t i = 0; i < [filenames count]; ++i)
            list[i] = [filenames objectAtIndex:i];
        do
        {
            NSString *strVPoxExe = [m_strVPoxPath stringByAppendingPathComponent:@"Contents/MacOS/VirtualPox"];
            err = FSPathMakeRef((const UInt8*)[strVPoxExe UTF8String], &fileRef, &fDir);
            if (err != noErr)
                break;
            args = CFArrayCreate(NULL, (const void **)list, [filenames count], &kCFTypeArrayCallBacks);
            if (args == NULL)
                break;
            LSApplicationParameters par = { 0, 0, &fileRef, asyncLaunchRefCon, 0, args, 0 };
            err = LSOpenApplication(&par, NULL);
            if (err != noErr)
                break;
            fResult = TRUE;
        }while(0);
        if (list)  /* Why bother checking, because you've crashed already if it's NULL! */
            free(list);
        if (file)
            CFRelease(file);
        if (args)
            CFRelease(args);
    }
}
@end

int main()
{
    /* Global auto release pool. */
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    /* Create our own delegate for the application. */
    AppDelegate *pAppDelegate = [[AppDelegate alloc] init];
    [[NSApplication sharedApplication] setDelegate: (id<NSApplicationDelegate>)pAppDelegate]; /** @todo check out ugly cast */
    pAppDelegate = nil;
    /* Start the event loop. */
    [NSApp run];
    /* Cleanup */
    [pool release];
    return 0;
}

