   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 7.00  07/25/24            */
   /*                                                     */
   /*         DEFTEMPLATE BSAVE/BLOAD HEADER FILE         */
   /*******************************************************/

/*************************************************************/
/* Purpose:                                                  */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Added support for templates maintaining their  */
/*            own list of facts.                             */
/*                                                           */
/*      6.30: Changed integer type/precision.                */
/*                                                           */
/*            Support for deftemplate slot facets.           */
/*                                                           */
/*      6.40: Removed LOCALE definition.                     */
/*                                                           */
/*            Pragma once and other inclusion changes.       */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*      7.00: Data driven backward chaining.                 */
/*                                                           */
/*            Deftemplate inheritance.                       */
/*                                                           */
/*            Support for non-reactive fact patterns.        */
/*                                                           */
/*            Support for certainty factors.                 */
/*                                                           */
/*            Support for named facts.                       */
/*                                                           */
/*************************************************************/

#ifndef _H_tmpltbin

#pragma once

#define _H_tmpltbin

#if (! RUN_TIME)

struct bsaveTemplateSlot
  {
   unsigned long slotName;
   unsigned int multislot : 1;
   unsigned int noDefault : 1;
   unsigned int defaultPresent : 1;
   unsigned int defaultDynamic : 1;
   unsigned int reactive : 1;
   unsigned long constraints;
   unsigned long defaultList;
   unsigned long facetList;
   unsigned long next;
  };

struct bsaveDeftemplate;
struct bsaveDeftemplateModule;

#include "cstrcbin.h"

struct bsaveDeftemplate
  {
   struct bsaveConstructHeader header;
   unsigned long parent;
   unsigned long child;
   unsigned long sibling;
   unsigned long slotList;
   unsigned int implied : 1;
   unsigned int cfd : 1;
   unsigned int named : 1;
   unsigned short numberOfSlots;
   unsigned long patternNetwork;
   unsigned long goalNetwork;
  };

#include "modulbin.h"

struct bsaveDeftemplateModule
  {
   struct bsaveDefmoduleItemHeader header;
  };

#define TMPLTBIN_DATA 61

#include "tmpltdef.h"

struct deftemplateBinaryData
  {
   Deftemplate *DeftemplateArray;
   unsigned long NumberOfDeftemplates;
   unsigned long NumberOfTemplateSlots;
   unsigned long NumberOfTemplateModules;
   struct templateSlot *SlotArray;
   struct deftemplateModule *ModuleArray;
  };

#define DeftemplateBinaryData(theEnv) ((struct deftemplateBinaryData *) GetEnvironmentData(theEnv,TMPLTBIN_DATA))
#define DeftemplatePointer(i) ((Deftemplate *) (&DeftemplateBinaryData(theEnv)->DeftemplateArray[i]))

#define BsaveDeftemplateIndex(ptr) ((ptr == NULL) ? ULONG_MAX : ((Deftemplate *) ptr)->header.bsaveID)
#define BloadDeftemplatePointer(i) ((Deftemplate *) ((i == ULONG_MAX) ? NULL : &DeftemplateBinaryData(theEnv)->DeftemplateArray[i]))


#ifndef _H_tmpltdef
#include "tmpltdef.h"
#endif

   void                           DeftemplateBinarySetup(Environment *);
   void                          *BloadDeftemplateModuleReference(Environment *,unsigned long);

#endif /* (! RUN_TIME) */

#endif /* _H_tmpltbin */



