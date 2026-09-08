#pragma once

namespace helen {
    /** Unique CPU-only identity for one cache attempt.
     * The operation owns this token strongly; a tracked record holds only a weak reference.
     * Expiration cancels failed work automatically. Reset/release invalidate the weak reference,
     * so an older attempt cannot publish into reset state or a reused COM address.
     */
    class D3d9ReplacementTicket final {};
}
