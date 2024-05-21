   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 7.00  12/24/23            */
   /*                                                     */
   /*                 DEVELOPER HEADER FILE               */
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
/*      6.24: Converted INSTANCE_PATTERN_MATCHING to         */
/*            DEFRULE_CONSTRUCT.                             */
/*                                                           */
/*      6.30: Added support for hashed alpha memories.       */
/*                                                           */
/*            Changed garbage collection algorithm.          */
/*            Functions enable-gc-heuristics and             */
/*            disable-gc-heuristics are no longer supported. */
/*                                                           */
/*            Changed integer type/precision.                */
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
/*            UDF redesign.                                  */
/*                                                           */
/*      6.50: Support for data driven backward chaining.     */
/*                                                           */
/*************************************************************/

#ifndef _H_developr

#pragma once

#define _H_developr

   void                           DeveloperCommands(Environment *);
   void                           PrimitiveTablesInfoCommand(Environment *,UDFContext *,UDFValue *);
   void                           PrimitiveTablesUsageCommand(Environment *,UDFContext *,UDFValue *);

#if DEFRULE_CONSTRUCT && DEFTEMPLATE_CONSTRUCT
   void                           ShowFactPatternNetworkCommand(Environment *,UDFContext *,UDFValue *);
   void                           ShowGoalPatternNetworkCommand(Environment *,UDFContext *,UDFValue *);
   void                           ValidateFactIntegrityCommand(Environment *,UDFContext *,UDFValue *);
   void                           FactStopNodeMatchesCommand(Environment *,UDFContext *,UDFValue *);
#endif
#if DEFRULE_CONSTRUCT && OBJECT_SYSTEM
   void                           PrintObjectPatternNetworkCommand(Environment *,UDFContext *,UDFValue *);
#endif
#if OBJECT_SYSTEM
   void                           InstanceTableUsageCommand(Environment *,UDFContext *,UDFValue *);
#endif
#if DEFRULE_CONSTRUCT
   void                           ValidateBetaMemoriesCommand(Environment *,UDFContext *,UDFValue *);
#endif

#endif /* _H_developr */


