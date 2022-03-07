#ifndef HASHTYPES_H
#define HASHTYPES_H
#include "typedobjectnode.h"
#include "nodestruct.h"
#include "platformdependentdefs.h"

typedef struct classdef
    {
    char* name;
    method* vtab;
    } classdef;

typedef struct pskRecord
    {
    psk entry;
    struct pskRecord* next;
    } pskRecord;

typedef int(*cmpfuncTp)(const char* s, const char* p);
typedef LONG(*hashfuncTp)(const char* s);

typedef struct Hash
    {
    pskRecord** hash_table;
    ULONG hash_size;
    ULONG elements;     /* elements >= record_count */
    ULONG record_count; /* record_count >= size - unoccupied */
    ULONG unoccupied;
    cmpfuncTp cmpfunc;
    hashfuncTp hashfunc;
    } Hash;

#endif