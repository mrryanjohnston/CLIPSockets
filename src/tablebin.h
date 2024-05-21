   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 7.00  02/06/24            */
   /*                                                     */
   /*           DEFTABLE BSAVE/BLOAD HEADER FILE          */
   /*******************************************************/

/*************************************************************/
/* Purpose:                                                  */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      7.00: Deftable construct added.                      */
/*                                                           */
/*************************************************************/

#ifndef _H_dftblbin

#pragma once

#define _H_tablebin

#if (! RUN_TIME)

#include "constrct.h"
#include "cstrcbin.h"
#include "modulbin.h"
#include "tabledef.h"

struct bsaveDeftable
  {
   struct bsaveConstructHeader header;
   unsigned long columns;
   unsigned long rows;
   unsigned long columnCount;
   unsigned long rowCount;
   unsigned long columnHashTableSize;
   unsigned long rowHashTableSize;
  };

struct bsaveDeftableModule
  {
   struct bsaveDefmoduleItemHeader header;
  };

#define DFTBLBIN_DATA 66

struct deftableBinaryData
  {
   Deftable *DeftableArray;
   unsigned long NumberOfDeftables;
   struct deftableModule *ModuleArray;
   unsigned long NumberOfDeftableModules;
  };

#define DeftableBinaryData(theEnv) ((struct deftableBinaryData *) GetEnvironmentData(theEnv,DFTBLBIN_DATA))
#define DeftablePointer(i) ((Deftable *) (&DeftableBinaryData(theEnv)->DeftableArray[i]))

   void                           DeftableBinarySetup(Environment *);
   void                          *BloadDeftableModuleReference(Environment *,unsigned long);

#endif /* (! RUN_TIME) */

#endif /* _H_tablebin */
