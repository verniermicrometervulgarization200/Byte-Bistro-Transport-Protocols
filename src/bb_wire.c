/// ========================================================================================================================================
// Module       : src/bb_wire.c
// Description  : Pack/parse helpers for Byte-Bistro on-wire frames (header+payload, checksum)
// Topics       : Architectural Framing, Checksums, Binary Serialization, Validation
// Project      : Byte Bistro (Copyright 2025)
// Purpose      : Implements construction of frames for transmission and strict validation on receive,
//                including magic check, length bounds, and CRC/Fletcher integrity.
// Author       : Rizky Johan Saputra (Independent Project)
// Note         : This implementation currently assumes little-endian host packing. For big-endian targets,
//                add explicit host<->LE conversions in both pack and parse paths.
/// ========================================================================================================================================

// ======================================================================================================
// SPECIFICATIONS
// ======================================================================================================
// - Frame Layout (little-endian, packed):
//   - magic    : 0xB17E (BB_MAGIC) to identify Byte-Bistro frames
//   - flags    : Bitfield for BB_FLAG_ACK, BB_FLAG_DATA, and BB_FLAG_FIN (extendable)
//   - hdrlen   : Number of bytes AFTER this field up to (but not including) payload (Currently set as 10)
//   - seq/ack  : 32-bit sequence and cumulative acknowledgment numbers (mod 2^32)
//   - len      : Payload length in bytes (0..65535)
//   - crc32c   : Integrity over header+payload with the crc field zeroed during computation
// - Checksums:
//   - Prefer CRC32C HW when available; otherwise fallback to Fletcher-32.
//   - Coverage: the entire header (with crc32c field set to 0) + payload.
// - Safety & Portability:
//   - Header is #pragma pack(push,1) from memcpy to/from wire (Avoid alignment issues).
//   - Endianness: fields are written in host endianness as packed bytes. Current targets are little-endian.
//                 If big-endian support is needed later, add explicit host<->LE conversions in pack/parse.
// - API Contracts:
//   - bb_wire_pack()   : Returns total bytes written (header+payload) or 0 on insufficient capacity.
//   - bb_wire_parse()  : R eturns true on valid header+checksum and sets *payload_out to the payload start.
// - Caller Responsibilities:
//   - Provide a buffer large enough for header + payload in pack(). Treat returned length as authoritative.
//   - In parse(), pass the exact received span. The function validates magic, crc and available bytes.
//   - Do not mutate the returned bb_hdr_t after parse without re-computing checksum.

// ======================================================================================================
// SETUP (ADJUST IF NECESSARY)
// ======================================================================================================
#include "bb_wire.h"
#include <string.h>

// ======================================================================================================
// IMPLEMENTATIONS
// ======================================================================================================
// Serialize the header and payload into buf and compute its checksum (crc32c if HW, else Fletcher-32)
size_t bb_wire_pack(uint8_t *buf, size_t cap, uint8_t flags, uint32_t seq, uint32_t ack,
                    const uint8_t* payload, uint16_t len){
    if (cap < sizeof(bb_hdr_t) + len) return 0;

    bb_hdr_t h = {0};
    h.magic  = BB_MAGIC;
    h.flags  = flags;
    h.hdrlen = 10;         // Set the bytes (seq..crc)
    h.seq    = seq;
    h.ack    = ack;
    h.len    = len;
    h.crc32c = 0;          // Zero during computation

    memcpy(buf, &h, sizeof(h));
    if (len) memcpy(buf + sizeof(h), payload, len);
    uint32_t crc = bb_crc32c_hw_available()
        ? bb_crc32c_hw(buf, sizeof(h) + len)
        : bb_fletcher32(buf, sizeof(h) + len);
    ((bb_hdr_t*)buf)->crc32c = crc;
    return sizeof(h) + len;
}

// Parse header from buf to validate magic and checksum. (Expose payload pointer)
bool bb_wire_parse(const uint8_t* buf, size_t n, bb_hdr_t* out, const uint8_t** payload_out){
    if (n < sizeof(bb_hdr_t)) return false;
    memcpy(out, buf, sizeof(bb_hdr_t));
    if (out->magic != BB_MAGIC) return false;

    // Temporarily zero the CRC field and compute over header+payload span.
    uint32_t crc = ((const bb_hdr_t*)buf)->crc32c;
    ((bb_hdr_t*)buf)->crc32c = 0;
    uint32_t calc = bb_crc32c_hw_available()
        ? bb_crc32c_hw(buf, n)
        : bb_fletcher32(buf, n);
    ((bb_hdr_t*)buf)->crc32c = crc;
    if (crc != calc) return false;
    if (n < sizeof(bb_hdr_t) + out->len) return false;
    *payload_out = buf + sizeof(bb_hdr_t);
    return true;
}

/// ========================================================================================================================================
// END (ADD IMPLEMENTATIONS IF NECESSARY)
/// ========================================================================================================================================