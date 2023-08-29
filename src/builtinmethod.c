#include "builtinmethod.h"
#include "nodedefs.h"
#include <string.h>

method_pnt findBuiltInMethodByName(typedObjectnode* object, const char* name)
    {
    method* methods = object->vtab;
    if(methods)
        {
        for(; methods->name && strcmp(methods->name, name); ++methods)
            ;
        return methods->func;
        }
    return NULL;
    }

method_pnt findBuiltInMethod(typedObjectnode* object, psk methodName)
    {
    if(!is_op(methodName))
        {
        return findBuiltInMethodByName(object, (const char*)POBJ(methodName));
        }
    return NULL;
    }
