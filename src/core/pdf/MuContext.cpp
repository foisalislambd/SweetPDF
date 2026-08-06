#include "core/pdf/MuContext.h"

#include "core/util/Logging.h"

#include <array>
#include <mutex>
#include <utility>

namespace sweet::core {
namespace {

std::array<std::mutex, FZ_LOCK_MAX> &lockPool()
{
    static std::array<std::mutex, FZ_LOCK_MAX> pool;
    return pool;
}

void lockMutex(void *, int which)
{
    lockPool()[static_cast<std::size_t>(which)].lock();
}

void unlockMutex(void *, int which)
{
    lockPool()[static_cast<std::size_t>(which)].unlock();
}

fz_locks_context *sharedLocks()
{
    static fz_locks_context locks{ nullptr, lockMutex, unlockMutex };
    return &locks;
}

/// Drops the cloned context when its owning thread goes away.
struct ThreadContext
{
    fz_context *ctx = nullptr;

    ~ThreadContext()
    {
        if (ctx) {
            fz_drop_context(ctx);
            ctx = nullptr;
        }
    }
};

} // namespace

MuError::MuError(QString message)
    : std::runtime_error(message.toStdString())
    , m_details(std::move(message))
{
}

MuContext::MuContext()
{
    m_base = fz_new_context(nullptr, sharedLocks(), FZ_STORE_DEFAULT);
    if (!m_base)
        throw MuError(QStringLiteral("Could not create the MuPDF context"));

    fz_try(m_base)
        fz_register_document_handlers(m_base);
    fz_catch(m_base) {
        fz_drop_context(m_base);
        m_base = nullptr;
    }

    if (!m_base)
        throw MuError(QStringLiteral("Could not register MuPDF document handlers"));
}

MuContext::~MuContext()
{
    if (m_base) {
        fz_drop_context(m_base);
        m_base = nullptr;
    }
}

MuContext &MuContext::instance()
{
    static MuContext ctx;
    return ctx;
}

fz_context *MuContext::forThread()
{
    static thread_local ThreadContext local;
    if (!local.ctx) {
        local.ctx = fz_clone_context(instance().m_base);
        if (!local.ctx)
            throw MuError(QStringLiteral("Could not clone the MuPDF context for this thread"));
    }
    return local.ctx;
}

QString MuContext::lastError(fz_context *ctx)
{
    const char *message = ctx ? fz_caught_message(ctx) : nullptr;
    if (!message || !*message)
        return QStringLiteral("unknown MuPDF error");
    return QString::fromUtf8(message);
}

void MuContext::raise(fz_context *ctx, const QString &what)
{
    const QString detail = lastError(ctx);
    qCWarning(lcPdf) << what << "-" << detail;
    throw MuError(QStringLiteral("%1: %2").arg(what, detail));
}

} // namespace sweet::core
