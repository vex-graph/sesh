#ifndef SESH_SESSION_TEXT_H
#define SESH_SESSION_TEXT_H

#include "vex.h"
#include <stdarg.h>
#include <stdio.h>
/* Internal cold formatter: callers supply only metadata, never credentials.
 * Error leaves a terminated prefix when capacity is nonzero. No heap ownership.
 */
static inline bool SeshText_format(utf8 *dest, bytes cap, bool *outTruncated, string format, ...) {
    if (outTruncated != nullptr)
        *outTruncated = false;
    if (dest == nullptr || cap == 0) {
        if (outTruncated != nullptr)
            *outTruncated = true;
        THROW("session text: missing destination");
        return false;
    }
    if (format == nullptr) {
        THROW("session text: missing format");
        return false;
    }
    va_list args;
    va_start(args, format);
    i32 count = vsnprintf(dest, cap, format, args);
    va_end(args);
    if (count < 0 || (bytes) count >= cap) {
        if (outTruncated != nullptr)
            *outTruncated = true;
        THROW("session text: truncated");
        return false;
    }
    return true;
}
#define SESH_VALUE_STRING(Class, Expression) \
    bool Class##_toString(const Class *self, utf8 *dest, bytes cap, bool *outTruncated) { \
        if (self == nullptr) \
            return SeshText_format(dest, cap, outTruncated, "nullptr"); \
        return Expression; \
    }
#endif
