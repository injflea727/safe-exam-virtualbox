-- $Id: TestManagerVPoxPilot-1.pgsql $
--- @file
-- VPox Test Manager - Setup for the 1st VPox Pilot.
--

--
-- Copyright (C) 2012-2020 Oracle Corporation
--
-- This file is part of VirtualPox Open Source Edition (OSE), as
-- available from http://www.virtualpox.org. This file is free software;
-- you can redistribute it and/or modify it under the terms of the GNU
-- General Public License (GPL) as published by the Free Software
-- Foundation, in version 2 as it comes in the "COPYING" file of the
-- VirtualPox OSE distribution. VirtualPox OSE is distributed in the
-- hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
--
-- The contents of this file may alternatively be used under the terms
-- of the Common Development and Distribution License Version 1.0
-- (CDDL) only, as it comes in the "COPYING.CDDL" file of the
-- VirtualPox OSE distribution, in which case the provisions of the
-- CDDL are applicable instead of those of the GPL.
--
-- You may elect to license modified versions of this file under the
-- terms and conditions of either the GPL or the CDDL or both.
--



\set ON_ERROR_STOP 1
\connect testmanager;

BEGIN WORK;

--
-- The user we assign all the changes too.
--
INSERT INTO Users (sUsername, sEmail, sFullName, sLoginName)
    VALUES ('vpox-pilot-config', 'pilot1@example.org', 'VPox Pilot Configurator', 'vpox-pilot-config');
\set idUserQuery '(SELECT uid FROM Users WHERE sUsername = \'vpox-pilot-config\')'

--
-- Configure a scheduling group with build sources.
--
INSERT INTO BuildSources (uidAuthor, sName, sProduct, sBranch, asTypes, asOsArches)
    VALUES (:idUserQuery, 'VPox trunk builds', 'VirtualPox', 'trunk', ARRAY['release', 'strict'],  NULL);

INSERT INTO BuildSources (uidAuthor, sName, sProduct, sBranch, asTypes, asOsArches)
    VALUES (:idUserQuery, 'VPox TestSuite trunk builds', 'VPox TestSuite', 'trunk', ARRAY['release'], NULL);

INSERT INTO SchedGroups (sName, sDescription, fEnabled, idBuildSrc, idBuildSrcTestSuite)
    VALUES ('VirtualPox Trunk', NULL, TRUE,
            (SELECT idBuildSrc FROM BuildSources WHERE sName = 'VPox trunk builds'),
            (SELECT idBuildSrc FROM BuildSources WHERE sName = 'VPox TestSuite trunk builds') );
\set idSchedGroupQuery '(SELECT idSchedGroup FROM SchedGroups WHERE sName = \'VirtualPox Trunk\')'

--
-- Configure three test groups.
--
INSERT INTO TestGroups (uidAuthor, sName)
    VALUES (:idUserQuery, 'VPox smoketests');
\set idGrpSmokeQuery        '(SELECT idTestGroup FROM TestGroups WHERE sName = \'VPox smoketests\')'
INSERT INTO SchedGroupMembers (idSchedGroup, idTestGroup, uidAuthor, idTestGroupPreReq)
    VALUES (:idSchedGroupQuery, :idGrpSmokeQuery, :idUserQuery, NULL);

INSERT INTO TestGroups (uidAuthor, sName)
    VALUES (:idUserQuery, 'VPox general');
\set idGrpGeneralQuery      '(SELECT idTestGroup FROM TestGroups WHERE sName = \'VPox general\')'
INSERT INTO SchedGroupMembers (idSchedGroup, idTestGroup, uidAuthor, idTestGroupPreReq)
    VALUES (:idSchedGroupQuery, :idGrpGeneralQuery, :idUserQuery, :idGrpSmokeQuery);

INSERT INTO TestGroups (uidAuthor, sName)
    VALUES (:idUserQuery, 'VPox benchmarks');
\set idGrpBenchmarksQuery   '(SELECT idTestGroup FROM TestGroups WHERE sName = \'VPox benchmarks\')'
INSERT INTO SchedGroupMembers (idSchedGroup, idTestGroup, uidAuthor, idTestGroupPreReq)
    VALUES (:idSchedGroupQuery, :idGrpBenchmarksQuery, :idUserQuery, :idGrpGeneralQuery);


--
-- Testcases
--
INSERT INTO TestCases (uidAuthor, sName, fEnabled, cSecTimeout, sBaseCmd, sTestSuiteZips)
    VALUES (:idUserQuery, 'VPox install', TRUE, 600,
            'validationkit/testdriver/vpoxinstaller.py --vpox-build @BUILD_BINARIES@ @ACTION@ -- testdriver/base.py @ACTION@',
            '@VALIDATIONKIT_ZIP@');
INSERT INTO TestCaseArgs (idTestCase, uidAuthor, sArgs)
    VALUES ((SELECT idTestCase FROM TestCases WHERE sName = 'VPox install'), :idUserQuery, '');
INSERT INTO TestGroupMembers (idTestGroup, idTestCase, uidAuthor)
    VALUES (:idGrpSmokeQuery, (SELECT idTestCase FROM TestCases WHERE sName = 'VPox install'), :idUserQuery);

COMMIT WORK;

