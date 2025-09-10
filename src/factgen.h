   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 7.00  11/27/24            */
   /*                                                     */
   /*        FACT RETE FUNCTION GENERATION HEADER FILE    */
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
/*      6.30: Support for performance optimizations.         */
/*                                                           */
/*            Increased maximum values for pattern/slot      */
/*            indices.                                       */
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
/*      7.00: Support for ?var:slot references to facts in   */
/*            methods and rule actions.                      */
/*                                                           */
/*************************************************************/

#ifndef _H_factgen

#pragma once

#define _H_factgen

#include "reorder.h"

/**********************************************************/
/* factGetVarPN1Call: This structure is used to store the */
/*   arguments to the most general extraction routine for */
/*   retrieving a variable from the fact pattern network. */
/**********************************************************/
struct factGetVarPN1Call
  {
   unsigned int factAddress : 1;
   unsigned int allFields : 1;
   unsigned short whichField;
   unsigned short whichSlot;
  };

/***********************************************************/
/* factGetVarPN2Call: This structure is used to store the  */
/*   arguments to the most specific extraction routine for */
/*   retrieving a variable from the fact pattern network.  */
/*   It is used for retrieving the single value stored in  */
/*   a single field slot (the slot index can be used to    */
/*   directly to retrieve the value from the fact array).  */
/***********************************************************/
struct factGetVarPN2Call
  {
   unsigned short whichSlot;
  };

/**********************************************************/
/* factGetVarPN3Call:  */
/**********************************************************/
struct factGetVarPN3Call
  {
   unsigned int fromBeginning : 1;
   unsigned int fromEnd : 1;
   unsigned short beginOffset;
   unsigned short endOffset;
   unsigned short whichSlot;
  };

/**************************************************************/
/* factConstantPN1Call: Used for testing for a constant value */
/*   in the fact pattern network. Compare the value of a      */
/*   single field slot to a constant.                         */
/**************************************************************/
struct factConstantPN1Call
  {
   unsigned int testForEquality : 1;
   unsigned short whichSlot;
  };

/******************************************************************/
/* factConstantPN2Call: Used for testing for a constant value in  */
/*   the fact pattern network. Compare the value of a multifield  */
/*   slot to a constant (where the value retrieved for comparison */
/*   from the slot contains no multifields before or only one     */
/*   multifield before and none after).                           */
/******************************************************************/
struct factConstantPN2Call
  {
   unsigned int testForEquality : 1;
   unsigned int fromBeginning : 1;
   unsigned short offset;
   unsigned short whichSlot;
  };

/**********************************************************/
/* factGetVarJN1Call: This structure is used to store the */
/*   arguments to the most general extraction routine for */
/*   retrieving a fact variable from the join network.    */
/**********************************************************/
struct factGetVarJN1Call
  {
   unsigned int factAddress : 1;
   unsigned int allFields : 1;
   unsigned int lhs : 1;
   unsigned int rhs : 1;
   unsigned short whichPattern;
   unsigned short whichSlot;
   unsigned short whichField;
  };

/**********************************************************/
/* factGetVarJN2Call:  */
/**********************************************************/
struct factGetVarJN2Call
  {
   unsigned int lhs : 1;
   unsigned int rhs : 1;
   unsigned short whichPattern;
   unsigned short whichSlot;
  };

/**********************************************************/
/* factGetVarJN3Call:  */
/**********************************************************/
struct factGetVarJN3Call
  {
   unsigned int fromBeginning : 1;
   unsigned int fromEnd : 1;
   unsigned int lhs : 1;
   unsigned int rhs : 1;
   unsigned short beginOffset;
   unsigned short endOffset;
   unsigned short whichPattern;
   unsigned short whichSlot;
  };

/**********************************************************/
/* factCompVarsPN1Call:  */
/**********************************************************/
struct factCompVarsPN1Call
  {
   unsigned int pass : 1;
   unsigned int fail : 1;
   unsigned short field1;
   unsigned short field2;
  };

/**********************************************************/
/* factCompVarsJN1Call:  */
/**********************************************************/
struct factCompVarsJN1Call
  {
   unsigned int pass : 1;
   unsigned int fail : 1;
   unsigned int p1lhs: 1;
   unsigned int p1rhs: 1;
   unsigned int p2lhs: 1;
   unsigned int p2rhs: 1;
   unsigned short pattern1;
   unsigned short pattern2;
   unsigned short slot1;
   unsigned short slot2;
  };

/**********************************************************/
/* factCompVarsJN2Call:  */
/**********************************************************/
struct factCompVarsJN2Call
  {
   unsigned int pass : 1;
   unsigned int fail : 1;
   unsigned int p1lhs: 1;
   unsigned int p1rhs: 1;
   unsigned int p2lhs: 1;
   unsigned int p2rhs: 1;
   unsigned int fromBeginning1 : 1;
   unsigned int fromBeginning2 : 1;
   unsigned short offset1;
   unsigned short offset2;
   unsigned short pattern1;
   unsigned short pattern2;
   unsigned short slot1;
   unsigned short slot2;
  };

/**********************************************************/
/* factCheckLengthPNCall: This structure is used to store */
/*   the  arguments to the routine for determining if the */
/*   length of a multifield slot is equal or greater than */
/*   a specified value.                                   */
/**********************************************************/

struct factCheckLengthPNCall
  {
    unsigned int exactly : 1;
    unsigned short minLength;
    unsigned short whichSlot;
  };

/****************************************/
/* GLOBAL EXTERNAL FUNCTION DEFINITIONS */
/****************************************/

   void                       InitializeFactReteFunctions(Environment *);
   struct expr               *FactPNVariableComparison(Environment *,struct lhsParseNode *,
                                                              struct lhsParseNode *);
   struct expr               *FactJNVariableComparison(Environment *,struct lhsParseNode *,
                                                              struct lhsParseNode *,bool);
   void                       FactReplaceGetvar(Environment *,struct expr *,struct lhsParseNode *,int);
   void                       FactReplaceGetfield(Environment *,struct expr *,struct lhsParseNode *);
   struct expr               *FactGenPNConstant(Environment *,struct lhsParseNode *);
   struct expr               *FactGenGetfield(Environment *,struct lhsParseNode *);
   struct expr               *FactGenGetvar(Environment *,struct lhsParseNode *,int);
   struct expr               *FactGenCheckLength(Environment *,struct lhsParseNode *);
   struct expr               *FactGenCheckZeroLength(Environment *,unsigned short);
   int                        FactSlotReferenceVar(Environment *,Expression *,void *);
   int                        RuleFactSlotReferenceVar(Environment *,Expression *,struct lhsParseNode *);
   int                        MethodFactSlotReferenceVar(Environment *,Expression *,Expression *);
   CLIPSLexeme               *FindTemplateForFactAddress(CLIPSLexeme *,struct lhsParseNode *);

#endif /* _H_factgen */
