#include "object.hpp"

using namespace lucus;

object::~object()
{
    assert(_refCount == 0);
}

void object::addRef()
{
    _refCount.fetch_add(1, std::memory_order_relaxed);
}

void object::releaseRef()
{
    u32 refCount = _refCount.fetch_sub(1, std::memory_order_acq_rel);
    assert(refCount > 0);
    if (refCount == 1) {
        delete this;
    }
}