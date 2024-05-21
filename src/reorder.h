   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 7.00  01/23/24            */
   /*                                                     */
   /*                 REORDER HEADER FILE                 */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides routines necessary for converting the   */
/*   the LHS of a rule into an appropriate form suitable for */
/*   the KB Rete topology. This includes transforming the    */
/*   LHS so there is at most one "or" CE (and this is the    */
/*   first CE of the LHS if it is used), adding initial      */
/*   patterns to the LHS (if no LHS is specified or a "test" */
/*   or "not" CE is the first pattern within an "and" CE),   */
/*   removing redundant CEs, and determining appropriate     */
/*   information on nesting for implementing joins from the  */
/*   right.                                                  */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.30: Support for join network changes.              */
/*                                                           */
/*            Changes to the algorithm for processing        */
/*            not/and CE groups.                             */
/*                                                           */
/*            Additional optimizations for combining         */
/*            conditional elements.                          */
/*                                                           */
/*            Added support for hashed alpha memories.       */
/*                                                           */
/*      6.31: Removed the marked flag used for not/and       */
/*            unification.                                   */
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
/*      7.00: Support for data driven backward chaining.     */
/*                                                           */
/*************************************************************/

#ifndef _H_reorder

#pragma once

#define _H_reorder

struct lhsParseNode;

#include "expressn.h"
#include "pattern.h"
#include "ruledef.h"

typedef enum
  {
   PATTERN_CE_NODE = 2049,
   AND_CE_NODE,
   OR_CE_NODE,
   NOT_CE_NODE,
   TEST_CE_NODE,
   NAND_CE_NODE,
   EXISTS_CE_NODE,
   FORALL_CE_NODE,
   SF_WILDCARD_NODE,
   MF_WILDCARD_NODE,
   SF_VARIABLE_NODE,
   MF_VARIABLE_NODE,
   GBL_VARIABLE_NODE,
   PREDICATE_CONSTRAINT_NODE,
   RETURN_VALUE_CONSTRAINT_NODE,
   FCALL_NODE,
   GCALL_NODE,
   PCALL_NODE,
   INTEGER_NODE,
   FLOAT_NODE,
   SYMBOL_NODE,
   STRING_NODE,
   INSTANCE_NAME_NODE,
   FACT_STORE_MULTIFIELD_NODE,
   DEFTEMPLATE_PTR_NODE,
   DEFCLASS_PTR_NODE,
   UNKNOWN_NODE
  } ParseNodeType;

#define UNSPECIFIED_SLOT USHRT_MAX
#define NO_INDEX USHRT_MAX

/***********************************************************************/
/* lhsParseNode structure: Stores information about the intermediate   */
/*   parsed representation of the lhs of a rule.                       */
/***********************************************************************/
struct lhsParseNode
  {
   ParseNodeType pnType;
   union
     {
      void *value;
      CLIPSLexeme *lexemeValue;
      struct functionDefinition *functionValue;
     };
   unsigned int negated : 1;
   unsigned int exists : 1;
   unsigned int existsNand : 1;
   unsigned int logical : 1;
   unsigned int multifieldSlot : 1;
   unsigned int bindingVariable : 1;
   unsigned int derivedConstraints : 1;
   unsigned int userCE : 1;
   unsigned int whichCE : 7; // TBD increase?
   unsigned int withinMultifieldSlot : 1;
   unsigned int goalCE : 1;
   unsigned int explicitCE : 1;
   unsigned short multiFieldsBefore;
   unsigned short multiFieldsAfter;
   unsigned short singleFieldsBefore;
   unsigned short singleFieldsAfter;
   struct constraintRecord *constraints;
   struct lhsParseNode *referringNode;
   struct patternParser *patternType;
   short pattern;
   unsigned short index;
   CLIPSLexeme *slot;
   unsigned short slotNumber;
   int beginNandDepth;
   int endNandDepth;
   unsigned short joinDepth;
   struct expr *networkTest;
   struct expr *externalNetworkTest;
   struct expr *secondaryNetworkTest;
   struct expr *externalLeftHash;
   struct expr *externalRightHash;
   struct expr *constantSelector;
   struct expr *constantValue;
   struct expr *leftHash;
   struct expr *rightHash;
   struct expr *betaHash;
   struct lhsParseNode *expression;
   struct lhsParseNode *secondaryExpression;
   void *userData;
   struct lhsParseNode *right;
   struct lhsParseNode *bottom;
  };

   struct lhsParseNode           *ReorderPatterns(Environment *,struct lhsParseNode *,bool *);
   struct lhsParseNode           *CopyLHSParseNodes(Environment *,struct lhsParseNode *);
   void                           CopyLHSParseNode(Environment *,struct lhsParseNode *,struct lhsParseNode *,bool);
   struct lhsParseNode           *GetLHSParseNode(Environment *);
   void                           ReturnLHSParseNodes(Environment *,struct lhsParseNode *);
   struct lhsParseNode           *ExpressionToLHSParseNodes(Environment *,struct expr *);
   struct expr                   *LHSParseNodesToExpression(Environment *,struct lhsParseNode *);
   void                           AddInitialPatterns(Environment *,struct lhsParseNode *);
   bool                           IsExistsSubjoin(struct lhsParseNode *,int);
   struct lhsParseNode           *CombineLHSParseNodes(Environment *,struct lhsParseNode *,struct lhsParseNode *);
   bool                           ConstantNode(struct lhsParseNode *);
   unsigned short                 NodeTypeToType(struct lhsParseNode *);
   ParseNodeType                  TypeToNodeType(unsigned short);

#endif /* _H_reorder */





