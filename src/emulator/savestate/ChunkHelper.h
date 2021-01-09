#ifndef _CHUNK_HELPER_H_
#define _CHUNK_HELPER_H_

#include "Chunk.h"
#include "EmCommon.h"

template <typename T>
class SaveChunkHelper {
   public:
    SaveChunkHelper(T& t);
    inline SaveChunkHelper<T>& Do8(uint8 value);
    inline SaveChunkHelper<T>& Do8(uint8 v1, uint8 v2, uint8 v3, uint8 v4);
    inline SaveChunkHelper<T>& Do16(uint16 value);
    inline SaveChunkHelper<T>& Do16(uint16 v1, uint16 v2);
    inline SaveChunkHelper<T>& Do32(uint32 value);
    inline SaveChunkHelper<T>& Do64(uint64 value);
    inline SaveChunkHelper<T>& DoBool(bool value);
    inline SaveChunkHelper<T>& DoDouble(double value);
    inline SaveChunkHelper<T>& DoBuffer(void* buffer, size_t size);

   private:
    T& t;
};

template <typename T>
class LoadChunkHelper {
   public:
    LoadChunkHelper(T& t);

    LoadChunkHelper& Do8(uint8& value);
    LoadChunkHelper& Do8(uint8& v1, uint8& v2, uint8& v3, uint8& v4);
    LoadChunkHelper& Do16(uint16& value);
    LoadChunkHelper& Do16(uint16& v1, uint16& v2);
    LoadChunkHelper& Do32(uint32& value);
    LoadChunkHelper& Do64(uint64& value);
    LoadChunkHelper& Do8(int8& value);
    LoadChunkHelper& Do8(int8& v1, int8& v2, int8& v3, int8& v4);
    LoadChunkHelper& Do16(int16& value);
    LoadChunkHelper& Do16(int16& v1, int16& v2);
    LoadChunkHelper& Do32(int32& value);
    LoadChunkHelper& Do64(int64& value);
    LoadChunkHelper& DoBool(bool& value);
    LoadChunkHelper& DoDouble(double& value);
    LoadChunkHelper& DoBuffer(void* buffer, size_t size);

   private:
    T& t;
};

///////////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION
///////////////////////////////////////////////////////////////////////////////

template <typename T>
SaveChunkHelper<T>::SaveChunkHelper(T& t) : t(t) {}

template <typename T>
SaveChunkHelper<T>& SaveChunkHelper<T>::Do8(uint8 value) {
    t.Put8(value);

    return *this;
}

template <typename T>
SaveChunkHelper<T>& SaveChunkHelper<T>::Do8(uint8 v1, uint8 v2, uint8 v3, uint8 v4) {
    t.Put32(v1 | (v2 << 8) | (v3 << 16) | (v4 << 24));

    return *this;
}

template <typename T>
SaveChunkHelper<T>& SaveChunkHelper<T>::Do16(uint16 value) {
    t.Put16(value);

    return *this;
}

template <typename T>
SaveChunkHelper<T>& SaveChunkHelper<T>::Do16(uint16 v1, uint16 v2) {
    t.Put32(v1 | (v2 << 16));

    return *this;
}

template <typename T>
SaveChunkHelper<T>& SaveChunkHelper<T>::Do32(uint32 value) {
    t.Put32(value);

    return *this;
}

template <typename T>
SaveChunkHelper<T>& SaveChunkHelper<T>::Do64(uint64 value) {
    t.Put64(value);

    return *this;
}

template <typename T>
SaveChunkHelper<T>& SaveChunkHelper<T>::DoBool(bool value) {
    t.PutBool(value);

    return *this;
}

template <typename T>
SaveChunkHelper<T>& SaveChunkHelper<T>::DoDouble(double value) {
    t.PutDouble(value);

    return *this;
}

template <typename T>
SaveChunkHelper<T>& SaveChunkHelper<T>::DoBuffer(void* buffer, size_t size) {
    t.PutBuffer(buffer, size);

    return *this;
}

template <typename T>
LoadChunkHelper<T>::LoadChunkHelper(T& t) : t(t) {}

template <typename T>
LoadChunkHelper<T>& LoadChunkHelper<T>::Do8(uint8& value) {
    value = t.Get8();

    return *this;
}

template <typename T>
LoadChunkHelper<T>& LoadChunkHelper<T>::Do8(uint8& v1, uint8& v2, uint8& v3, uint8& v4) {
    uint32 v = t.Get32();

    v1 = v & 0xff;
    v2 = (v >> 8) & 0xff;
    v3 = (v >> 16) & 0xff;
    v4 = (v >> 24) & 0xff;

    return *this;
}

template <typename T>
LoadChunkHelper<T>& LoadChunkHelper<T>::Do16(uint16& value) {
    value = t.Get16();

    return *this;
}

template <typename T>
LoadChunkHelper<T>& LoadChunkHelper<T>::Do16(uint16& v1, uint16& v2) {
    uint32 v = t.Get32();

    v1 = v & 0xffff;
    v2 = (v >> 16) & 0xffff;

    return *this;
}

template <typename T>
LoadChunkHelper<T>& LoadChunkHelper<T>::Do32(uint32& value) {
    value = t.Get32();

    return *this;
}

template <typename T>
LoadChunkHelper<T>& LoadChunkHelper<T>::Do64(uint64& value) {
    value = t.Get64();

    return *this;
}

template <typename T>
LoadChunkHelper<T>& LoadChunkHelper<T>::Do8(int8& value) {
    value = t.Get8();

    return *this;
}

template <typename T>
LoadChunkHelper<T>& LoadChunkHelper<T>::Do8(int8& v1, int8& v2, int8& v3, int8& v4) {
    uint32 v = t.Get32();

    v1 = v & 0xff;
    v2 = (v >> 8) & 0xff;
    v3 = (v >> 16) & 0xff;
    v4 = (v >> 24) & 0xff;

    return *this;
}

template <typename T>
LoadChunkHelper<T>& LoadChunkHelper<T>::Do16(int16& value) {
    value = t.Get16();

    return *this;
}

template <typename T>
LoadChunkHelper<T>& LoadChunkHelper<T>::Do16(int16& v1, int16& v2) {
    uint32 v = t.Get32();

    v1 = v & 0xffff;
    v2 = (v >> 16) & 0xffff;

    return *this;
}

template <typename T>
LoadChunkHelper<T>& LoadChunkHelper<T>::Do32(int32& value) {
    value = t.Get32();

    return *this;
}

template <typename T>
LoadChunkHelper<T>& LoadChunkHelper<T>::Do64(int64& value) {
    value = t.Get64();

    return *this;
}

template <typename T>
LoadChunkHelper<T>& LoadChunkHelper<T>::DoBool(bool& value) {
    value = t.GetBool();

    return *this;
}

template <typename T>
LoadChunkHelper<T>& LoadChunkHelper<T>::DoDouble(double& value) {
    value = t.GetDouble();

    return *this;
}

template <typename T>
LoadChunkHelper<T>& LoadChunkHelper<T>::DoBuffer(void* buffer, size_t size) {
    t.GetBuffer(buffer, size);

    return *this;
}

#endif  // _CHUNK_HELPER_H_
