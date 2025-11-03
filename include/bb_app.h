/// ========================================================================================================================================
// Module       : include/bb_app.h
// Description  : Application-level ASCII wire protocol serializer & parser for Byte-Bistro
// Topics       : Computer Networks, Data Communications, System Programming, Embedded Systems, Concurrency, Computer Architecture
// Project      : Byte Bistro (Copyright 2025)
// Purpose      : Converts structured order+reply objects into deterministic ASCII-wire payloads carried inside the binary network frame.
//                This isolates semantic application encoding from transport reliability logic (GBN/SR), making the protocol modular,
//                testable, versionable, and analyzable for latency and correctness verification.
// Author       : Rizky Johan Saputra (Independent Project)
// Note         : This layer is RFC-like stable. If protocol evolves, changes should occur here — NOT in GBN/SR/transport modules.
//                This aligns with strict separation of concerns (layered network stack design).
/// ========================================================================================================================================

// ======================================================================================================
// SPECIFICATIONS
// ======================================================================================================
// - This module implements the "Burger Order Protocol" (BOP) at the application layer.
// - Two canonical text formats exist and must be respected across the cross-version compatibilities:
//        - ORDER <id> <itemlist>\n
//        - REPLY <id> <latency_ms> <itemlist>\n
// - All encoding/decoding must remain deterministic with the conservation of whitespaces.
// - Failure state must be safely returned (-1) as this module is directly tested in fuzz / fault injection.
// - The transport layer must treat output of this module as opaque bytes (no rewriting).
// - This module creates the following processing chain of semantic → wire → semantic mapping only.

// ======================================================================================================
// SETUP (ADJUST IF NECESSARY)
// ======================================================================================================
#ifndef BB_APP_H
#define BB_APP_H
#include "bb_common.h"
#include "bb_proto.h"

// ======================================================================================================
// IMPLEMENTATIONS
// ======================================================================================================
// Transport the configurations (id and item list)
typedef struct { uint64_t id; char itemlist[256]; } bb_order_t;

// Declare the encoding by assigning the order line into the out buffer
int bb_app_encode_order(const bb_order_t* o, uint8_t* out, size_t cap);

// Declare the decoding by parsing the order line from in[0..n)
int bb_app_decode_order(const uint8_t* in, size_t n, bb_order_t* out);

// Declare the encoding by assigning the reply line into the out buffer
int bb_app_encode_reply(uint64_t id, const char* items, uint32_t latency_ms, uint8_t* out, size_t cap);

// Declare the parser of the reply line by extracting its id, latency_ms and items into the specified outputs
int bb_app_parse_reply(const uint8_t* in, size_t n, uint64_t* id, uint32_t* latency_ms, char* items, size_t items_cap);

// Declare an additional parser of the order line but only return the id and items (No struct)
int bb_app_parse_order(const uint8_t* in, size_t n, uint64_t* oid_out, char* items_out, size_t items_cap);
#endif

/// ========================================================================================================================================
// END (ADD IMPLEMENTATIONS IF NECESSARY)
/// ========================================================================================================================================