#pragma once

#include <mutex>
#include <string>
#include <vector>

#include <windows.h>

namespace helen
{
    class FileWriteRoutingService;

    /**
     * @brief Retains routing serialization while HelenHook writes originals and synchronizes session overlays.
     *
     * A transaction has no global bypass mode. It either synchronizes verified originals, is explicitly canceled without
     * a write, or fails closed on destruction so stale session data cannot be presented as a successful save.
     */
    class FileWriteRoutingTransaction
    {
    public:
        /**
         * @brief Latches affected routes failed when a transaction is abandoned before settlement.
         */
        ~FileWriteRoutingTransaction();

        FileWriteRoutingTransaction(const FileWriteRoutingTransaction&) = delete;
        FileWriteRoutingTransaction& operator=(const FileWriteRoutingTransaction&) = delete;
        FileWriteRoutingTransaction(FileWriteRoutingTransaction&&) = delete;
        FileWriteRoutingTransaction& operator=(FileWriteRoutingTransaction&&) = delete;

        /**
         * @brief Copies verified current originals to overlays and releases the retained routing lock.
         * @param error Receives ERROR_SUCCESS or the synchronization failure.
         * @return True when all affected overlays were synchronized.
         */
        bool Synchronize(DWORD& error);

        /**
         * @brief Settles a transaction without changing originals or overlays and releases its routing lock.
         */
        void CancelWithoutWrite();

    private:
        friend class FileWriteRoutingService;

        /**
         * @brief Constructs a transaction from the service's retained serialization lock.
         * @param service Routing service that owns the affected route state.
         * @param route_keys Canonical routes named by the trusted save.
         * @param lock Retained service mutex lock.
         */
        FileWriteRoutingTransaction(
            FileWriteRoutingService& service,
            std::vector<std::wstring> route_keys,
            std::unique_lock<std::mutex>&& lock);

        /** @brief Service whose route state is retained for the transaction lifetime. */
        FileWriteRoutingService* service_;

        /** @brief Canonical route keys affected by this trusted save. */
        std::vector<std::wstring> route_keys_;

        /** @brief Mutex lock retained from acquisition through original writing and synchronization. */
        std::unique_lock<std::mutex> lock_;

        /** @brief True after synchronization or explicit no-write cancellation. */
        bool settled_ = false;
    };
}
