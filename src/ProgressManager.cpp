#include <ProgressManager.hpp>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>


namespace {

constexpr size_t kMaxDisplayWidth = 80;
constexpr size_t kBarWidth = 20;
constexpr size_t kBannerWidth = 40;
constexpr std::chrono::milliseconds kRefreshInterval{200};

enum class WorkerState : int {
    Working,
    Done,
    Stopped,
    Failed,
};

std::string format_count(size_t value) {
    std::string digits = std::to_string(value);
    std::string result;
    result.reserve(digits.size() + digits.size() / 3);

    int position = 0;
    for (auto it = digits.rbegin(); it != digits.rend(); ++it) {
        if (position != 0 && position % 3 == 0) {
            result.push_back(',');
        }
        result.push_back(*it);
        ++position;
    }

    std::reverse(result.begin(), result.end());
    return result;
}

std::string percent_text(double value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(1) << value;
    return out.str();
}

std::string percent_field(double value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(1) << std::setw(5) << value;
    return out.str();
}

std::string format_duration(std::chrono::steady_clock::duration duration) {
    const auto total_seconds =
        std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    const auto hours = total_seconds / 3600;
    const auto minutes = (total_seconds % 3600) / 60;
    const auto seconds = total_seconds % 60;

    std::ostringstream out;
    out << std::setfill('0') << std::setw(2) << hours << ':'
        << std::setw(2) << minutes << ':'
        << std::setw(2) << seconds;
    return out.str();
}

std::string make_bar(double percent) {
    const double clamped = std::clamp(percent, 0.0, 100.0);
    size_t filled = static_cast<size_t>(clamped / 100.0 * kBarWidth + 0.5);
    filled = std::min(filled, kBarWidth);

    return std::string(filled, '#') + std::string(kBarWidth - filled, '.');
}

std::string fit_line(std::string line, size_t width) {
    if (line.size() > width) {
        line.resize(width);
    }
    else if (line.size() < width) {
        line.append(width - line.size(), ' ');
    }
    return line;
}

std::wstring to_wide(const std::string& text) {
    // Every character emitted by this file is ASCII.
    return std::wstring(text.begin(), text.end());
}

const char* outcome_text(ProgressManager::Outcome outcome) {
    switch (outcome) {
        case ProgressManager::Outcome::Completed: return "Complete";
        case ProgressManager::Outcome::Cancelled: return "Cancelled";
        case ProgressManager::Outcome::Failed:    return "Failed";
    }
    return "Unknown";
}

const char* worker_suffix(WorkerState state) {
    switch (state) {
        case WorkerState::Done:    return " - Done";
        case WorkerState::Stopped: return " - Stopped";
        case WorkerState::Failed:  return " - Failed";
        case WorkerState::Working: return "";
    }
    return "";
}

std::string centered_title(size_t width) {
    const std::string title = "RAY TRACER - RENDERING";
    if (title.size() >= width) {
        return title.substr(0, width);
    }

    const size_t left = (width - title.size()) / 2;
    return std::string(left, ' ') + title;
}

}  // namespace


struct ProgressManager::Impl {
    struct Worker {
        size_t total{0};
        std::atomic<size_t> completed{0};
        std::atomic<WorkerState> state{WorkerState::Working};
    };

    Impl(size_t imageWidth, size_t imageHeight, int samples, int threads)
        : width(imageWidth)
        , height(imageHeight)
        , samplesPerPixel(samples)
        , workerCount(std::max(threads, 0))
        , workers(static_cast<size_t>(std::max(threads, 0)))
    {}

    ~Impl() {
        run_stop(Outcome::Cancelled);
        restore_console();
    }

    void start();
    void run_stop(Outcome outcome) noexcept;

    void initialize_console();
    void restore_console() noexcept;
    void draw_interactive(bool final_frame, Outcome outcome);
    void draw_plain_progress();
    void print_plain_header();
    void print_final_summary(Outcome outcome);
    void display_loop() noexcept;

    std::vector<std::string> build_frame(
        size_t displayWidth,
        bool finalFrame,
        Outcome outcome) const;

    size_t total_rows() const;
    size_t completed_rows() const;
    double overall_percent() const;
    int progress_bucket() const;
    bool all_rows_complete() const;
    std::string summary_text(Outcome outcome) const;

    size_t width;
    size_t height;
    int samplesPerPixel;
    int workerCount;
    std::vector<Worker> workers;

    HANDLE console{INVALID_HANDLE_VALUE};
    bool interactive{false};
    bool console_initialized{false};
    DWORD original_mode{0};
    bool have_original_mode{false};
    CONSOLE_CURSOR_INFO original_cursor{};
    bool have_original_cursor{false};

    COORD anchor{0, 0};
    bool anchor_valid{false};
    int last_milestone{-1};

    bool started{false};
    bool finished{false};
    bool stopping{false};
    std::mutex stop_mutex;
    std::condition_variable stop_wake;
    std::thread display_thread;

    std::chrono::steady_clock::time_point render_start{};
    std::chrono::steady_clock::duration elapsed{};
};


void ProgressManager::Impl::start() {
    if (started) {
        return;
    }

    render_start = std::chrono::steady_clock::now();
    initialize_console();

    try {
        if (interactive) {
            draw_interactive(false, Outcome::Completed);
        }
        else {
            print_plain_header();
        }
    }
    catch (...) {
        restore_console();
        throw;
    }

    started = true;

    try {
        display_thread = std::thread([this]() { display_loop(); });
    }
    catch (...) {
        restore_console();
        started = false;
        throw;
    }
}


void ProgressManager::Impl::run_stop(Outcome outcome) noexcept {
    if (!started || finished) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(stop_mutex);
        stopping = true;
    }
    stop_wake.notify_all();

    if (display_thread.joinable()) {
        display_thread.join();
    }

    elapsed = std::chrono::steady_clock::now() - render_start;

    if (interactive) {
        try {
            draw_interactive(true, outcome);
        }
        catch (...) {
            // The final summary below is still useful even if a redraw fails.
        }
    }

    try {
        print_final_summary(outcome);
    }
    catch (...) {
        // Console output must never escape shutdown.
    }

    restore_console();
    started = false;
    finished = true;
}


void ProgressManager::Impl::display_loop() noexcept {
    std::unique_lock<std::mutex> lock(stop_mutex);

    while (!stop_wake.wait_for(lock, kRefreshInterval, [this]() { return stopping; })) {
        lock.unlock();

        try {
            if (interactive) {
                draw_interactive(false, Outcome::Completed);
            }
            else {
                draw_plain_progress();
            }
        }
        catch (...) {
            // A failed redraw must not terminate the rendering threads.
        }

        lock.lock();
    }
}


void ProgressManager::Impl::initialize_console() {
    console = GetStdHandle(STD_OUTPUT_HANDLE);

    DWORD mode = 0;
    CONSOLE_SCREEN_BUFFER_INFO info{};

    if (console == nullptr || console == INVALID_HANDLE_VALUE ||
        !GetConsoleMode(console, &mode) ||
        !GetConsoleScreenBufferInfo(console, &info)) {
        interactive = false;
        return;
    }

    interactive = true;
    console_initialized = true;
    original_mode = mode;
    have_original_mode = true;

    // Padded lines must never wrap and push the table out of alignment.
    SetConsoleMode(console, mode & ~ENABLE_WRAP_AT_EOL_OUTPUT);

    CONSOLE_CURSOR_INFO cursor{};
    if (GetConsoleCursorInfo(console, &cursor)) {
        original_cursor = cursor;
        have_original_cursor = true;

        CONSOLE_CURSOR_INFO hidden = cursor;
        hidden.bVisible = FALSE;
        SetConsoleCursorInfo(console, &hidden);
    }

    // Start on a clean line rather than overwriting a shell prompt.
    if (info.dwCursorPosition.X != 0) {
        DWORD written = 0;
        WriteConsoleW(console, L"\r\n", 2, &written, nullptr);
        GetConsoleScreenBufferInfo(console, &info);
    }
}


void ProgressManager::Impl::restore_console() noexcept {
    if (!console_initialized) {
        return;
    }

    if (console != nullptr && console != INVALID_HANDLE_VALUE) {
        if (have_original_mode) {
            SetConsoleMode(console, original_mode);
        }
        if (have_original_cursor) {
            SetConsoleCursorInfo(console, &original_cursor);
        }
    }

    console_initialized = false;
    interactive = false;
    anchor_valid = false;
}


std::vector<std::string> ProgressManager::Impl::build_frame(
    size_t displayWidth,
    bool finalFrame,
    Outcome outcome
) const {
    std::vector<std::string> lines;
    lines.reserve(static_cast<size_t>(workerCount) + 12);

    const size_t bannerWidth = std::min(kBannerWidth, displayWidth);
    const std::string border(bannerWidth, '=');

    lines.push_back(border);
    lines.push_back(centered_title(bannerWidth));
    lines.push_back(border);
    lines.push_back("");

    {
        std::ostringstream out;
        out << "Resolution: " << width << 'x' << height;
        lines.push_back(out.str());
    }
    lines.push_back("Samples per pixel: " + std::to_string(samplesPerPixel));
    lines.push_back("Worker threads: " + std::to_string(workerCount));
    lines.push_back("");

    for (int i = 0; i < workerCount; ++i) {
        const Worker& worker = workers[static_cast<size_t>(i)];
        const size_t done = std::min(
            worker.completed.load(std::memory_order_relaxed), worker.total);
        const double percent = worker.total == 0
            ? 100.0
            : 100.0 * static_cast<double>(done) / static_cast<double>(worker.total);

        std::ostringstream out;
        out << "Thread " << std::setw(2) << std::setfill('0') << i
            << std::setfill(' ') << " [" << make_bar(percent) << "] "
            << percent_field(percent) << "% | "
            << std::setw(9) << format_count(done) << " / "
            << std::setw(9) << format_count(worker.total)
            << worker_suffix(worker.state.load(std::memory_order_relaxed));
        lines.push_back(out.str());
    }

    lines.push_back("");

    {
        const size_t done = completed_rows();
        const size_t total = total_rows();
        const double percent = overall_percent();

        std::ostringstream out;
        out << "Overall: [" << make_bar(percent) << "] "
            << percent_field(percent) << "% | "
            << std::setw(9) << format_count(done) << " / "
            << std::setw(9) << format_count(total) << " rows";
        lines.push_back(out.str());
    }

    {
        const auto shownElapsed = finalFrame
            ? elapsed
            : std::chrono::steady_clock::now() - render_start;
        lines.push_back("Elapsed: " + format_duration(shownElapsed));
    }

    std::string status;
    if (finalFrame) {
        status = outcome_text(outcome);
    }
    else if (all_rows_complete()) {
        status = "Finishing...";
    }
    else {
        status = "Rendering...";
    }
    lines.push_back("Status: " + status);

    return lines;
}


void ProgressManager::Impl::draw_interactive(bool finalFrame, Outcome outcome) {
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (console == INVALID_HANDLE_VALUE || !GetConsoleScreenBufferInfo(console, &info)) {
        return;
    }

    const size_t availableWidth = info.dwSize.X > 1
        ? static_cast<size_t>(info.dwSize.X - 1)
        : size_t{1};
    const size_t displayWidth = std::max<size_t>(1, std::min(kMaxDisplayWidth, availableWidth));

    const std::vector<std::string> lines = build_frame(displayWidth, finalFrame, outcome);
    if (lines.empty()) {
        return;
    }

    size_t visible_lines = lines.size();

    if (!anchor_valid) {
        // The initial frame is written at the current cursor. The console may
        // scroll while it is written, so the anchor is derived from the final
        // cursor position rather than the starting one.
    }
    else {
        if (anchor.Y >= info.dwSize.Y) {
            return;
        }

        visible_lines = std::min(
            visible_lines, static_cast<size_t>(info.dwSize.Y - anchor.Y));

        const COORD target{anchor.X, anchor.Y};
        if (!SetConsoleCursorPosition(console, target)) {
            return;
        }
    }

    std::wstring block;
    for (size_t i = 0; i < visible_lines; ++i) {
        if (i != 0) {
            block += L"\r\n";
        }
        block += to_wide(fit_line(lines[i], displayWidth));
    }

    DWORD written = 0;
    if (!WriteConsoleW(
            console,
            block.c_str(),
            static_cast<DWORD>(block.size()),
            &written,
            nullptr)) {
        return;
    }

    if (!anchor_valid) {
        CONSOLE_SCREEN_BUFFER_INFO after{};
        if (GetConsoleScreenBufferInfo(console, &after)) {
            const LONG anchorY =
                after.dwCursorPosition.Y - static_cast<LONG>(visible_lines) + 1;
            anchor.X = 0;
            anchor.Y = static_cast<SHORT>(std::max<LONG>(0, anchorY));
            anchor_valid = true;
        }
    }
}


void ProgressManager::Impl::print_plain_header() {
    const std::string border(kBannerWidth, '=');

    std::cout << border << '\n';
    std::cout << centered_title(kBannerWidth) << '\n';
    std::cout << border << "\n\n";
    std::cout << "Resolution: " << width << 'x' << height << '\n';
    std::cout << "Samples per pixel: " << samplesPerPixel << '\n';
    std::cout << "Worker threads: " << workerCount << "\n\n";

    std::cout << "Progress: " << percent_field(overall_percent()) << "% ("
              << format_count(completed_rows()) << " / "
              << format_count(total_rows()) << " rows)\n" << std::flush;

    last_milestone = progress_bucket();
}


void ProgressManager::Impl::draw_plain_progress() {
    const int bucket = progress_bucket();
    if (bucket <= last_milestone) {
        return;
    }

    last_milestone = bucket;

    std::cout << "Progress: " << percent_field(overall_percent()) << "% ("
              << format_count(completed_rows()) << " / "
              << format_count(total_rows()) << " rows)\n" << std::flush;
}


void ProgressManager::Impl::print_final_summary(Outcome outcome) {
    const std::string summary = summary_text(outcome);

    if (interactive && console != INVALID_HANDLE_VALUE) {
        std::wstring output = L"\r\n";
        output += to_wide(summary);
        output += L"\r\n";

        DWORD written = 0;
        WriteConsoleW(
            console,
            output.c_str(),
            static_cast<DWORD>(output.size()),
            &written,
            nullptr);
    }
    else {
        std::cout << summary << std::endl;
    }
}


size_t ProgressManager::Impl::total_rows() const {
    size_t total = 0;
    for (const Worker& worker : workers) {
        total += worker.total;
    }
    return total;
}


size_t ProgressManager::Impl::completed_rows() const {
    size_t total = 0;
    for (const Worker& worker : workers) {
        total += std::min(
            worker.completed.load(std::memory_order_relaxed), worker.total);
    }
    return total;
}


double ProgressManager::Impl::overall_percent() const {
    const size_t total = total_rows();
    if (total == 0) {
        return 100.0;
    }

    return 100.0 * static_cast<double>(completed_rows()) / static_cast<double>(total);
}


int ProgressManager::Impl::progress_bucket() const {
    const double percent = overall_percent();
    return std::min(10, static_cast<int>(percent / 10.0));
}


bool ProgressManager::Impl::all_rows_complete() const {
    return completed_rows() == total_rows();
}


std::string ProgressManager::Impl::summary_text(Outcome outcome) const {
    std::ostringstream out;
    out << "Rendering ";

    switch (outcome) {
        case Outcome::Completed: out << "complete in "; break;
        case Outcome::Cancelled: out << "cancelled after "; break;
        case Outcome::Failed:    out << "failed after "; break;
    }

    out << format_duration(elapsed)
        << " (" << percent_text(overall_percent()) << "%, "
        << format_count(completed_rows()) << " / "
        << format_count(total_rows()) << " rows).";
    return out.str();
}


ProgressManager::ProgressManager(
    size_t imageWidth,
    size_t imageHeight,
    int samplesPerPixel,
    int workerCount
) : m_impl(std::make_unique<Impl>(
        imageWidth, imageHeight, samplesPerPixel, workerCount))
{}


ProgressManager::~ProgressManager() = default;


void ProgressManager::set_worker_total(int worker, size_t totalRows) {
    if (worker < 0 || worker >= m_impl->workerCount) {
        return;
    }

    m_impl->workers[static_cast<size_t>(worker)].total = totalRows;
}


void ProgressManager::add_completed_rows(int worker, size_t rows) {
    if (worker < 0 || worker >= m_impl->workerCount) {
        return;
    }

    m_impl->workers[static_cast<size_t>(worker)]
        .completed.fetch_add(rows, std::memory_order_relaxed);
}


void ProgressManager::finish_worker(int worker) {
    if (worker < 0 || worker >= m_impl->workerCount) {
        return;
    }

    m_impl->workers[static_cast<size_t>(worker)]
        .state.store(WorkerState::Done, std::memory_order_relaxed);
}


void ProgressManager::stop_worker(int worker) {
    if (worker < 0 || worker >= m_impl->workerCount) {
        return;
    }

    m_impl->workers[static_cast<size_t>(worker)]
        .state.store(WorkerState::Stopped, std::memory_order_relaxed);
}


void ProgressManager::fail_worker(int worker) {
    if (worker < 0 || worker >= m_impl->workerCount) {
        return;
    }

    m_impl->workers[static_cast<size_t>(worker)]
        .state.store(WorkerState::Failed, std::memory_order_relaxed);
}


void ProgressManager::start() {
    m_impl->start();
}


void ProgressManager::stop(Outcome outcome) noexcept {
    m_impl->run_stop(outcome);
}
