#include "calculation.h"
#include "hashtypes.h"
#include "encoding.h"
#include "memory.h"
#include "copy.h"
#include "wipecopy.h"
#include "globals.h"
#include "input.h"
#include "eval.h"
#include <string.h>
#include <assert.h>

enum { Push, ResolveAndPush };

typedef void (*funct)();

struct forthvariable;

typedef union forthvalue /* a number. either integer or 'real' */
	{
	double floating;
	LONG integer;
	} forthvalue;

typedef union stackvalue
	{
	forthvalue val;
	forthvalue* valp; /*pointer to value held by a variable*/
	} stackvalue;

typedef struct forthvariable
	{
	forthvalue u;
	char* name;
	struct forthvariable* next;
	} forthvariable;

typedef struct forthword
	{
	tFlags v;
	union
		{
		double floating; LONG integer; funct funcp; forthvalue* valp;
		} u;
	} forthword;

typedef struct forthMemory
	{
	forthword* word;
	stackvalue stack[64];
	stackvalue* sp;
	forthvariable* var;
	} forthMemory;

static forthvalue* getVariablePointer(forthMemory* This, char* name)
	{
	forthvariable* varp = This->var;
	forthvariable* curvarp = varp;
	while (curvarp != 0 && strcmp(curvarp->name, name))
		{
		curvarp = curvarp->next;
		}
	if (curvarp == 0)
		{
		curvarp = varp;
		varp = (forthvariable*)bmalloc(__LINE__, sizeof(forthvariable));
		varp->name = bmalloc(__LINE__,strlen(name)+1);
		strcpy(varp->name, name);
		varp->next = curvarp;
		curvarp = varp;
		}
	return &(curvarp->u);
	}

static Boolean compile(struct typedObjectnode* This, ppsk arg)
	{
	psk Arg = (*arg)->RIGHT;
	if (!is_op(Arg))
		{
		psk ret = 0;/* findhash(HASH(This), Arg);*/
		if (ret)
			{
			wipe(*arg);
			*arg = same_as_w(ret);
			return TRUE;
			}
		}
	return FALSE;
	}

static Boolean calculate(struct typedObjectnode* This, ppsk arg)
	{
	psk Arg = (*arg)->RIGHT;
	if (!is_op(Arg))
		{
		psk ret = 0;
		if (ret)
			{
			wipe(*arg);
			*arg = same_as_w(ret);
			return TRUE;
			}
		}
	return FALSE;
	}

static int polish1(psk code)
	{
	int C;
	int R;
	switch (Op(code))
		{
		case PLUS:
		case TIMES:
		case EXP:
		case LOG:
		case AND:
		case OR:
		case MATCH:
			{
			R = polish1(code->LEFT);
			if (R == -1)
				return -1;
			C = polish1(code->RIGHT);
			if (C == -1)
				return -1;
			return 1 + R + C;
			}
		case FUN:
		case FUU:
			if (is_op(code->LEFT))
				return -1;
			C = polish1(code->RIGHT);
			if (C == -1)
				return -1;
			return 1 + C;
		default:
			if (is_op(code))
				{
				R = polish1(code->LEFT);
				if (R == -1)
					return -1;
				C = polish1(code->RIGHT);
				if (C == -1)
					return -1;
				return 1 + R + C;
				}
			else
				{
				if (code->v.fl & QDOUBLE || INTEGER(code))
					return 1;
				else
					return -1;
				}
		}
	}

void sinus(void)
	{

	}

void cosinus(void)
	{

	}

forthvalue cpop(forthMemory* This)
	{
	assert(This->sp > This->stack);
	--(This->sp);
    return (This->sp)->val;
	}

forthvalue* pcpop(forthMemory* This)
	{
	assert(This->sp > This->stack);
	--(This->sp);
	return (This->sp)->valp;
	}

void cpush(forthMemory* This, forthvalue val)
	{
	assert(This->sp < &(This->stack[0]) + sizeof(This->stack) / sizeof(This->stack[0]));
	(This->sp)->val = val;
	++(This->sp);
	}

void pcpush(forthMemory* This, forthvalue* valp)
	{
	assert(This->sp < &(This->stack[0]) + sizeof(This->stack) / sizeof(This->stack[0]));
	(This->sp)->valp = valp;
	++(This->sp);
	}

static forthword* polish2(forthMemory* This, psk code, forthword* word)
	{
	switch (Op(code))
		{
		case PLUS:
		case TIMES:
		case EXP:
		case LOG:
		case AND:
		case OR:
		case MATCH:
			word = polish2(This,code->LEFT, word);
			word = polish2(This,code->RIGHT, word);
			word->v.fl = Op(code);
			return ++word;
/*		case MATCH:
			{
			forthvalue* valp = pcpop(This);
			forthvalue val = cpop(This);
			*valp = val;
			cpush(This, val);
			}
			*/
		case FUN:
		case FUU:
			{
			word = polish2(This,code->RIGHT, word);
			word->v.fl = IS_OPERATOR;
			if (!strcmp("sin", &code->LEFT->u.sobj))
				word->u.funcp = sinus;
			if (!strcmp("cos", &code->LEFT->u.sobj))
				word->u.funcp = cosinus;
			}
		default:
			if (is_op(code))
				{
				word = polish2(This,code->LEFT, word);
				word = polish2(This,code->RIGHT, word);
				word->v.fl = Op(code);
				return ++word;
				}
			else
				{
				/*enum {Push,ResolveAndPush};*/
				if (INTEGER(code))
					{
					word->u.integer = (int)STRTOL(&(code->u.sobj), 0, 10);
					if (HAS_MINUS_SIGN(code))
						{
						word->u.integer = -(word->u.integer);
						}
					word->v.fl = Push;
					/*When executing, push number onto the data stack*/
					}
				else if (code->v.fl & QDOUBLE)
					{
					word->u.floating = strtod(&(code->u.sobj), 0);
					word->v.fl = Push;
					/*When executing, push number onto the data stack*/
					}
				else
					{
					/*variable*/
					word->u.valp = getVariablePointer(This, &(code->u.sobj));
					word->v.fl = (code->v.fl & INDIRECT) ? ResolveAndPush : Push;
					/* When executing, 'ValuePointer' means that the value pointer must be pushed onto the data stack. Following MATCH will pop it.
					* 'Resolve' means that the value pointed at must be pushed onto the data stack.
					*/
					}
				++word;
				return word;
				};
		}
	}

static Boolean calculationnew(struct typedObjectnode* This, ppsk arg)
	{
	psk code = *arg;
	forthword* lastdata;
	forthMemory* forthstuff;
	int length;
	if (is_object(code))
		code = code->RIGHT;
	length = polish1(code);
	if (length < 0)
		return FALSE;
	This->voiddata = bmalloc(__LINE__, sizeof(forthMemory));
	forthstuff = (forthMemory*)(This->voiddata);
	forthstuff->var = 0;
	forthstuff->word = bmalloc(__LINE__, length * sizeof(forthword) + 1);
	forthstuff->sp = forthstuff->stack;
	lastdata = polish2((forthMemory*)(This->voiddata),code, (forthword*)(This->voiddata));
	lastdata->v.fl = 0;
	return TRUE;
	}

static Boolean calculationdie(struct typedObjectnode* This, ppsk arg)
	{
	forthvariable* curvarp = ((forthMemory*)(This->voiddata))->var;
	while (curvarp)
		{
		forthvariable* nextvarp = curvarp->next;
		bfree(curvarp->name);
		bfree(curvarp);
		curvarp = nextvarp;
		}
	bfree(((forthMemory*)(This->voiddata))->word);
	bfree(This->voiddata);
	return TRUE;
	}

method calculation[] = {
	{"compile",compile},
	{"calculate",calculate},
	{"New",calculationnew},
	{"Die",calculationdie},
	{NULL,NULL} };
/*
Standard methods are 'New' and 'Die'.
A user defined 'die' can be added after creation of the object and will be invoked just before 'Die'.

Example:

new$hash:?h;

	 (=(Insert=.out$Insert & lst$its & lst$Its & (Its..insert)$!arg)
	  (die = .out$"Oh dear")
	):(=?(h.));

	(h..Insert)$(X.x);

	:?h;



	(=(Insert=.out$Insert & lst$its & lst$Its & (Its..insert)$!arg)
	  (die = .out$"The end.")
	):(=?(new$hash:?k));

	(k..Insert)$(Y.y);

	:?k;

A little problem is that in the last example, the '?' ends up as a flag on the '=' node.

Bart 20010222

*/

