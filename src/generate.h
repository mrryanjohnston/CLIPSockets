   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 7.00  08/24/24            */
   /*                                                     */
   /*                GENERATE HEADER FILE                 */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides routines for converting field           */
/*   constraints to expressions which can be used            */
/*   in the pattern and join networks.                       */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.30: Added support for hashed alpha memories.       */
/*                                                           */
/*            Added support for hashed comparisons to        */
/*            constants.                                     */
/*                                                           */
/*            Reimplemented algorithm for comparisons to     */
/*            variables contained within not/and CEs.        */
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
/*************************************************************/

#ifndef _H_generate

#pragma once

#define _H_generate

#include "analysis.h"
#include "expressn.h"
#include "reorder.h"

   void                           FieldConversion(Environment *,struct lhsParseNode *,struct lhsParseNode *,struct nandFrame *);
   struct expr                   *GetvarReplace(Environment *,struct lhsParseNode *,bool,bool,struct nandFrame *);
   void                           AddNandUnification(Environment *,struct lhsParseNode *,struct nandFrame *);

#endif /* _H_generate */



