#include "core/pdf/MuContext.h"

#include "core/util/Logging.h"

#include <array>
#include <mutex>
#include <utility>

namespace sweet::core {
namespace {

/// The locks MuPDF takes on our behalf. Never destroyed, for the same reason as
/// the base context: a thread releasing its clone as the process exits still
/// locks these, and static destruction order gives no say in who goes first.
std::array<std::mutex, FZ_LOCK_MAX> &lockPool()
{
    static auto *pool = new std::array<std::mutex, FZ_LOCK_MAX>;
    return *pool;
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

/// Runs only if something other than instance() ever owns a context, which today
/// nothing does. The base is not dropped even then: see instance().
MuContext::~MuContext() = default;

MuContext &MuContext::instance()
{
    // Deliberately never destroyed, so the base context lives for the whole
    // process. Every rendering thread holds a clone of it and releases that clone
    // from thread-local teardown, which is not ordered against static
    // destruction - on Windows least of all. Outliving them all means a clone is
    // always released against a live base, and freeing MuPDF's store moments
    // before the process exits buys nothing.
    static MuContext *ctx = new MuContext;
    return *ctx;
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
