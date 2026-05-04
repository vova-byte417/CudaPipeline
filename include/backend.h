#pragma once
#include "runtime/batch.h"

class Request;

class Backend
{
public:

    virtual ~Backend() {}

    virtual bool initialize() = 0;

    virtual bool submit(Request& req) = 0;

    virtual bool submit_batch(Batch& batch) = 0;

    virtual void shutdown() = 0;
};