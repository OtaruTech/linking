#ifndef __LINKING_TYPES_H__
#define __LINKING_TYPES_H__

#include <stdint.h>

namespace linking {

#define LK_NAMESPACE_BEGIN      namespace linking {
#define LK_NAMESPACE_END        }

typedef int32_t     Result;
static const Result Ok              = 0;
static const Result EFailed         = 1;
static const Result EUnsupported    = 2;
static const Result EInvalidState   = 3;
static const Result EInvalidArg     = 4;
static const Result ENullPointer    = 5;
static const Result ETimeout        = 6;
static const Result EBusy           = 7;
static const Result ENoMemory       = 8;

} // namespace linking

#endif
