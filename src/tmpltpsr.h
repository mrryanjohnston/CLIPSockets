   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 7.00  08/08/24            */
   /*                                                     */
   /*            DEFTEMPLATE PARSER HEADER FILE           */
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
/*      6.23: Added support for templates maintaining their  */
/*            own list of facts.                             */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW and       */
/*            MAC_MCW).                                      */
/*                                                           */
/*            GetConstructNameAndComment API change.         */
/*                                                           */
/*            Support for deftemplate slot facets.           */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*      6.40: Removed LOCALE definition.                     */
/*                                                           */
/*            Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*      7.00: Support for certainty factors.                 */
/*                                                           */
/*            Support for named facts.                       */
/*                                                           */
/*************************************************************/

#ifndef _H_tmpltpsr

#pragma once

#define _H_tmpltpsr

#include "symbol.h"
#include "tmpltdef.h"

   bool                           ParseDeftemplate(Environment *,const char *);
   void                           InstallDeftemplate(Environment *,Deftemplate *);

#if CERTAINTY_FACTORS
#define TEMPLATE_CFD_STRING "CFD"
#define TEMPLATE_CF_STRING "CF"
#endif

#define TEMPLATE_ND_STRING "ND"
#define TEMPLATE_NAME_STRING "name"

#endif /* _H_tmpltpsr */



