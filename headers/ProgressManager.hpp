#pragma once

#include <cstddef>
#include <memory>


/**
 * @file ProgressManager.hpp
 * @brief Centralised console progress display for the render workers.
 *
 * Worker threads only publish counters into this object; every console write
 * is made by a single display thread (or by the caller while that thread is
 * stopped). On an interactive Windows console the table is redrawn in place
 * with cursor positioning. When stdout is redirected, a plain-text,
 * milestone-based fallback is used instead.
 */
class ProgressManager {
public:
    /// Final state reported by stop().
    enum class Outcome {
        Completed,
        Cancelled,
        Failed,
    };

    /**
     * @brief Prepares a display for @p workerCount workers.
     *
     * No console output happens until start(). All worker workloads must be
     * registered with set_worker_total() before start() as well.
     */
    ProgressManager(
        size_t imageWidth,
        size_t imageHeight,
        int samplesPerPixel,
        int workerCount
    );
    ~ProgressManager();

    ProgressManager(const ProgressManager&) = delete;
    ProgressManager(ProgressManager&&) = delete;
    ProgressManager& operator=(const ProgressManager&) = delete;
    ProgressManager& operator=(ProgressManager&&) = delete;

    /// Sets the number of scanlines assigned to a worker. Call before start().
    void set_worker_total(int worker, size_t totalRows);

    /// Adds completed scanlines to a worker's counter. Called at row boundaries.
    void add_completed_rows(int worker, size_t rows = 1);

    /// Marks a worker as having finished all of its assigned rows.
    void finish_worker(int worker);

    /// Marks a worker that exited before completing its assigned rows.
    void stop_worker(int worker);

    /// Marks a worker that terminated because of an error.
    void fail_worker(int worker);

    /// Initialises the console and starts periodic redraws.
    void start();

    /// Stops redraws, joins the display thread, prints the final summary and
    /// restores the console. Safe to call more than once.
    void stop(Outcome outcome) noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
