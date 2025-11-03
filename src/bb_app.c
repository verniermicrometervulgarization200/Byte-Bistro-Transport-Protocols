/// ========================================================================================================================================
// Module       : src/bb_app.c
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
#include "bb_app.h"
#include <stdio.h>
#include <string.h>

// ======================================================================================================
// IMPLEMENTATIONS
// ======================================================================================================
// Encode the application based order structure into a canonical ASCII payload
int bb_app_encode_order(const bb_order_t* o, uint8_t* out, size_t cap) {
    if (!o || !out || cap == 0) return -1;
    // Keep it strictly "ORDER <id> <items>\n"
    int n = snprintf((char*)out, cap, "ORDER %llu %s\n",
                     (unsigned long long)o->id, o->itemlist);
    return (n > 0 && (size_t)n < cap) ? n : -1;
}

// Decode the incoming raw order text payload into a populated bb_order_t structure
int bb_app_decode_order(const uint8_t* in, size_t n, bb_order_t* out) {
    if (!in || !out || n == 0) return -1;

    // Declare a bounded and NULL terminated copy for the sscanf and strtok searches
    char buf[512];
    size_t m = (n < sizeof(buf) - 1) ? n : (sizeof(buf) - 1);
    memcpy(buf, in, m);
    buf[m] = '\0';

    // Consider the fixed tags
    const char* p = buf;
    if (strncmp(p, "ORDER ", 6) != 0) return -1;
    p += 6;

    // Parse the id (unsigned long long)
    unsigned long long id = 0ULL;
    int consumed = 0;
    if (sscanf(p, "%llu%n", &id, &consumed) != 1) return -1;
    p += consumed;

    // Consider a single space before every items
    if (*p != ' ') return -1;
    ++p;

    // Declare the rest of line (up to '\n') as items
    out->id = (uint64_t)id;
    // Trim the present trailing newlines
    size_t L = strcspn(p, "\n");
    if (L >= sizeof(out->itemlist)) L = sizeof(out->itemlist) - 1;
    memcpy(out->itemlist, p, L);
    out->itemlist[L] = '\0';
    return 0;
}

// Encode the reply wire-format message with its id, latency, and returned items list
int bb_app_encode_reply(uint64_t id, const char* items, uint32_t latency_ms,
                        uint8_t* out, size_t cap) {
    if (!out || !items || cap == 0) return -1;
    // Strict: "REPLY <id> <latency_ms> <items>\n"
    int n = snprintf((char*)out, cap, "REPLY %llu %u %s\n",
                     (unsigned long long)id, latency_ms, items);
    return (n > 0 && (size_t)n < cap) ? n : -1;
}

// Parse the reply payload into its extract id, measured server-side latency, and items field
int bb_app_parse_reply(const uint8_t* in, size_t n,
                       uint64_t* id, uint32_t* latency_ms,
                       char* items, size_t items_cap) {
    if (!in || !id || !latency_ms || !items || items_cap == 0) return -1;

    char buf[512];
    size_t m = (n < sizeof(buf) - 1) ? n : (sizeof(buf) - 1);
    memcpy(buf, in, m);
    buf[m] = '\0';

    if (strncmp(buf, "REPLY ", 6) != 0) return -1;
    const char* p = buf + 6;

    unsigned long long idu = 0ULL;
    unsigned lms = 0U;
    int c1 = 0, c2 = 0;

    // Parse "<id> <latency_ms> "
    if (sscanf(p, "%llu%n", &idu, &c1) != 1) return -1;
    p += c1;
    if (*p != ' ') return -1;
    ++p;
    if (sscanf(p, "%u%n", &lms, &c2) != 1) return -1;
    p += c2;
    if (*p != ' ') return -1;
    ++p;

    // Rest until the newline is <items>
    size_t L = strcspn(p, "\n");
    if (L >= items_cap) L = items_cap - 1;
    memcpy(items, p, L);
    items[L] = '\0';

    *id = (uint64_t)idu;
    *latency_ms = (uint32_t)lms;
    return 0;
}

// Decode the order when the caller only needs the id and items
int bb_app_parse_order(const uint8_t* in, size_t n, uint64_t* oid_out,
                       char* items_out, size_t items_cap)
{
    bb_order_t tmp;
    int rc = bb_app_decode_order(in, n, &tmp);
    if (rc < 0) return -1;

    if (oid_out)   *oid_out = tmp.id;
    if (items_out && items_cap > 0){
        strncpy(items_out, tmp.itemlist, items_cap);
        items_out[items_cap-1] = '\0';
    }
    return 0;
}

/// ========================================================================================================================================
// END (ADD IMPLEMENTATIONS IF NECESSARY)
/// ========================================================================================================================================