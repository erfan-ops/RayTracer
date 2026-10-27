#pragma once

#include <memory>


class Renderer;

/**
 * @file PreviewWindow.hpp
 * @brief Native Win32 window that displays a Renderer's image as it renders.
 *
 * The window is a presentation layer only. It keeps its own DIB-format copy of
 * the image and pulls a scanline from the renderer once that row has been
 * marked complete, so it never reads pixels a worker thread is still writing
 * and never blocks the renderer with a lock.
 *
 * Rendering itself is not owned by this class; the caller starts the render
 * worker and runs the message loop with run() until the user closes the
 * window. All Win32 details live in the implementation file.
 */
class PreviewWindow {
public:
    /**
     * @brief Creates and shows the window for @p renderer.
     *
     * The initial client area matches the render resolution, scaled down if
     * needed so the window fits on the work area.
     *
     * @throws std::system_error if the window class or window cannot be
     *         created.
     */
    explicit PreviewWindow(const Renderer& renderer);
    ~PreviewWindow();

    // The window owns a HWND and GDI objects, so it is neither copyable nor
    // movable. The renderer it displays must outlive it.
    PreviewWindow(const PreviewWindow&) = delete;
    PreviewWindow(PreviewWindow&&) = delete;
    PreviewWindow& operator=(const PreviewWindow&) = delete;
    PreviewWindow& operator=(PreviewWindow&&) = delete;

    /// Runs the Win32 message loop until the window is closed.
    void run();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
