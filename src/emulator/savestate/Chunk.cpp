#include "Chunk.h"

#include <cstring>

#include "Byteswapping.h"
#include "Logging.h"

#if (EM_HOST_BYTE_ORDER == EM_BIG_ENDIAN)
    #define SWAP_IF_REQUIRED(x) Byteswap(x);
#else
    #define SWAP_IF_REQUIRED(x) ;
#endif

Chunk::Chunk(size_t size, void* buffer) : chunkSize(size), buffer(buffer), next(buffer) {}

void Chunk::Reset() {
    next = buffer;
    error = false;
}

bool Chunk::AssertOkForSize(size_t size) {
    if (error) return false;

    error = chunkSize - (static_cast<uint8*>(next) - static_cast<uint8*>(buffer)) < size;

    return !error;
}

void Chunk::Put8(uint8 value) { Put32(value); }

void Chunk::Put16(uint16 value) { Put32(value); }

void Chunk::Put32(uint32 value) {
    if (!AssertOkForSize(4)) return;

    SWAP_IF_REQUIRED(value);

    *static_cast<uint32*>(next) = value;

    next = static_cast<uint8*>(next) + 4;
}

void Chunk::Put64(uint64 value) {
    Put32(value & 0xffffffff);
    Put32((value >> 32) & 0xffffffff);
}

void Chunk::PutBool(bool value) { Put32(static_cast<uint32>(value)); }

void Chunk::PutDouble(double value) {
    union {
        uint64 ival;
        double dval;
    } loc;

    loc.dval = value;
    Put64(loc.ival);
}

void Chunk::PutBuffer(void* buffer, size_t size) {
    size_t paddedSize = (size & 0x03) ? ((size & ~0x03) + 4) : size;

    if (!AssertOkForSize(paddedSize)) return;

    std::memcpy(next, buffer, size);

    next = static_cast<uint8*>(next) + paddedSize;
}

void Chunk::PutString(const string& str, size_t maxLength) {
    if (str.size() > maxLength) {
        logging::printf("string %s exceeds length", str.c_str());
        error = true;

        return;
    }

    char buffer[maxLength + 1];
    memset(buffer, 0, maxLength + 1);
    strcpy(buffer, str.c_str());

    PutBuffer(buffer, maxLength + 1);
}

uint8 Chunk::Get8() { return Get32() & 0xff; }

uint16 Chunk::Get16() { return Get32() & 0xffff; }

uint32 Chunk::Get32() {
    if (!AssertOkForSize(4)) return 0;

    uint32 value = *static_cast<uint32*>(next);
    SWAP_IF_REQUIRED(value);

    next = static_cast<uint8*>(next) + 4;

    return value;
}

uint64 Chunk::Get64() { return Get32() | (static_cast<uint64>(Get32()) << 32); }

bool Chunk::GetBool() { return Get32(); }

void Chunk::GetBuffer(void* buffer, size_t size) {
    size_t paddedSize = (size & 0x03) ? ((size & ~0x03) + 4) : size;

    if (!AssertOkForSize(paddedSize)) return;

    std::memcpy(buffer, next, size);

    next = static_cast<uint8*>(next) + paddedSize;
}

double Chunk::GetDouble() {
    union {
        uint64 ival;
        double dval;
    } loc;

    loc.ival = Get64();

    return loc.dval;
}

string Chunk::GetString(size_t maxLength) {
    char buffer[maxLength + 1];
    GetBuffer(buffer, maxLength + 1);

    return buffer;
}

bool Chunk::HasError() const { return error; }