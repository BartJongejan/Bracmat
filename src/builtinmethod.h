#ifndef BUILTINMETHOD_H
#define BUILTINMETHOD_H
#include "nodestruct.h"
#include "typedobjectnode.h"

method_pnt findBuiltInMethodByName(typedObjectnode* object, const char* name);
method_pnt findBuiltInMethod(typedObjectnode* object, psk methodName);

#endif
