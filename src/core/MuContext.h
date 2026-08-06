#pragma once

#include <QString>

#include <stdexcept>

#include <mupdf/fitz.h>

namespace sweet::core {

/// Raised when a MuPDF call fails. MuPDF signals errors through
/// setjmp/longjmp, so every fz_try block is kept tiny and trivially
/// destructible, and the failure is re-raised as this exception afterwards.
class MuError : public std::runtime_error
{
public:
    explicit MuError(QString message);
    const QString &details() const noexcept { return m_details; }

private:
    QString m_details;
};

/// Owns the process-wide base fz_context and hands out per-thread clones.
///
/// MuPDF allows one document to be used from several threads as long as each
/// thread owns a context cloned from the same base and a locking callback is
/// installed. Clones share the store and allocator, so objects created on one
/// thread stay valid on the others.
class MuContext
{
public:
    /// Context belonging to the calling thread. Created on first use and
    /// released automatically when the thread exits.
    static fz_context *forThread();

    /// Last error message recorded by MuPDF on this thread.
    static QString lastError(fz_context *ctx);

    /// Throws MuError with MuPDF's own message. Call right after fz_catch.
    [[noreturn]] static void raise(fz_context *ctx, const QString &what);

    MuContext(const MuContext &) = delete;
    MuContext &operator=(const MuContext &) = delete;

private:
    MuContext();
    ~MuContext();

    static MuContext &instance();

    fz_context *m_base = nullptr;
};

} // namespace sweet::core
