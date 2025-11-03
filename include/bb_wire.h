/// ========================================================================================================================================
// Module       : include/bb_wire.h
// Description  : On-wire frame format, flags, and pack/parse API for Byte-Bistro
// Topics       : Architectural Framing, Binary Protocols, Endianness, Checksums
// Project      : Byte Bistro (Copyright 2025)
// Purpose      : Defines the exact binary layout of Byte-Bistro frames and exposes helpers to pack headers/payloads
//                and to parse/validate them on receive (including checksum verification).
// Author       : Rizky Johan Saputra (Independent Project)
// Note         : The header is byte-packed and copied verbatim to/from the wire. Keep this the single source of truth
//                for the frame schema (acts as the project’s mini-RFC). Changing this layout is a protocol versioning event.
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
#ifndef BB_WIRE_H
#define BB_WIRE_H
#include "bb_common.h"
#include "bb_checksum.h"

#define BB_MAGIC 0xB17E
#define BB_FLAG_ACK 0x01
#define BB_FLAG_DATA 0x02
#define BB_FLAG_FIN 0x04

// ======================================================================================================
// IMPLEMENTATIONS
// ======================================================================================================
// Transport the wiring configurations
#pragma pack(push,1)
typedef struct {
uint16_t magic;
uint8_t flags;
uint8_t hdrlen;
uint32_t seq;
uint32_t ack;
uint16_t len;
uint32_t crc32c;
} bb_hdr_t;
#pragma pack(pop)

// Build a wire frame into buf with its header and payload
size_t bb_wire_pack(uint8_t *buf, size_t cap, uint8_t flags, uint32_t seq, uint32_t ack, const uint8_t* payload, uint16_t len);

// Parse and validate the wire frame from the retrieved buf[0..n) state
bool bb_wire_parse(const uint8_t* buf, size_t n, bb_hdr_t* out, const uint8_t** payload_out);
#endif

/// ========================================================================================================================================
// END (ADD IMPLEMENTATIONS IF NECESSARY)
/// ========================================================================================================================================