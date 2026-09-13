/****************************************************************************
* Platform Abstraction Layer
* Daryl Borth 2026
* Logger.h
*
* Multi-backend debug logging. Platform-agnostic - this header never
* includes a platform SDK header. Each platform's Platform::init() wires
* up whichever LoggingDriver implementations it supports and hands them
* to a single Logger instance.
*
* Zero-overhead when disabled: build with -DLOGGING_ENABLED=0 (or define
* it before this header is first included) and every LOG_*() call site
* compiles to nothing - no string literal in .rodata, no branch, no call.
***************************************************************************/
#pragma once

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

//! Master compile-time switch. Override via a build flag
//! (CXXFLAGS += -DLOGGING_ENABLED=0) for release builds that must not
//! carry any logging code or string literals at all. Defaults off.
#ifndef LOGGING_ENABLED
#define LOGGING_ENABLED 0
#endif

//! Plain-int mirror of LogLevel's Debug/Info/Warning/Error ordering, for
//! C call sites that can't spell `LogLevel::Debug`. Keep in sync with
//! `enum class LogLevel` below - the static_assert block in the C++
//! section will fail to compile if they ever drift apart.
#define LOG_LEVEL_DEBUG   0
#define LOG_LEVEL_INFO    1
#define LOG_LEVEL_WARNING 2
#define LOG_LEVEL_ERROR   3

#ifdef __cplusplus

#include "Mutex.h"

//!Severity of a single log call. `None` is only meaningful as a
//!LogConfig::level value ("log nothing"), never passed to a LOG_*() call.
enum class LogLevel : uint8_t
{
	Debug,
	Info,
	Warning,
	Error,
	None
};

static_assert(static_cast<uint8_t>(LogLevel::Debug)   == LOG_LEVEL_DEBUG,   "LOG_LEVEL_DEBUG out of sync with LogLevel");
static_assert(static_cast<uint8_t>(LogLevel::Info)    == LOG_LEVEL_INFO,    "LOG_LEVEL_INFO out of sync with LogLevel");
static_assert(static_cast<uint8_t>(LogLevel::Warning) == LOG_LEVEL_WARNING, "LOG_LEVEL_WARNING out of sync with LogLevel");
static_assert(static_cast<uint8_t>(LogLevel::Error)   == LOG_LEVEL_ERROR,   "LOG_LEVEL_ERROR out of sync with LogLevel");

//!Which single backend is active when LogConfig::mode isn't Multi. Kept
//!separate from LogBackendId below so the common case - "just point me at
//!one backend" - doesn't require touching a bitmask.
enum class LogMode : uint8_t
{
	OSReport, //!< built-in platform logging. Useful for emulators.
	UDP,
	SerialGecko,
	File,
	Multi //!< fan out to every backend set in LogConfig::multiBackendMask
};

//!Bitmask identifying an individual backend. Plain (not enum class) so
//!config code can OR them together; extend by appending a new bit, never
//!by renumbering existing ones (multiBackendMask may be persisted/passed
//!around by a caller).
enum LogBackendId : uint32_t
{
	LOGGER_NONE     = 0,
	LOGGER_OSREPORT = 1u << 0,
	LOGGER_UDP      = 1u << 1,
	LOGGER_SERIAL   = 1u << 2,
	LOGGER_FILE     = 1u << 3,
};

//!How LoggerFile flushes writes to storage.
enum class LogFlushPolicy : uint8_t
{
	Immediate,    //!< fflush() after every write - safest against a crash/power loss, slowest
	EveryNWrites, //!< fflush() every LogConfig::flushEveryNWrites writes
	Never         //!< rely on the C library / fclose() at shutdown - fastest, least crash-safe
};

//!Full runtime configuration for the Logger. Nothing backend-specific is
//!ever hardcoded in a backend implementation - it all comes from here,
//!supplied by app code (or left at these defaults, which are deliberately
//!safe/inert: OSReport only, nothing that touches hardware or network).
struct LogConfig
{
	LogMode mode = LogMode::File;
	LogLevel level = LogLevel::Info;

	//! Only consulted when mode == LogMode::Multi. OR LOGGER_* flags
	//! together, e.g. LOGGER_UDP | LOGGER_FILE.
	uint32_t multiBackendMask = LOGGER_OSREPORT;

	//! Regardless of mode/multiBackendMask, also mirror every line to the
	//! console OS default backend (OSReport/SYS_Report) if one is
	//! registered. This is what makes the framework "just work" under an
	//! emulator (Cemu/Dolphin capture stdout) with zero configuration.
	bool mirrorToOSReport = true;

	// ---- UDP ----
	const char * targetIp = "192.168.1.100";
	uint16_t targetPort = 4405;
	bool nonBlocking = true; //!< socket is always created non-blocking; kept for clarity/future use

	// ---- Serial / USB Gecko ----
	//! EXI channel (GC/Wii): 0 = memory card slot A, 1 = memory card slot
	//! B (where a USB Gecko is normally inserted). Ignored on Wii U.
	int geckoChannel = 1;

	//! Informational only on GC/Wii (EXI has a fixed protocol rate); used
	//! as the requested baud rate for a Wii U USB-to-TTL/USBSerialLogger
	//! backend, if one is registered.
	uint32_t serialBaudRate = 115200;

	// ---- SD / file ----
	char filePath[1024] = "sd:/debug.log";
	LogFlushPolicy flushPolicy = LogFlushPolicy::Immediate;
	uint32_t flushEveryNWrites = 16;

	// ---- Formatting ----
	bool includeLevelTag = true;         //!< prefix each line with "[DEBUG] "/"[INFO] "/etc.
	bool includeSequenceNumber = false;  //!< prefix each line with a monotonic call counter, useful for spotting dropped UDP packets
};

//!Abstract backend a Logger fans a formatted line out to. Every method
//!must be safe to call from any thread and must not block indefinitely -
//!Logger already serializes all calls to a given Logger instance under
//!its own mutex, so a backend does not need its own locking for write(),
//!but must still not do something like an unbounded blocking socket send.
class LoggingDriver
{
public:
	virtual ~LoggingDriver() = default;

	//!Opens whatever handle/socket/file this backend needs, using
	//!only fields from config (nothing hardcoded). Returning false
	//!leaves the backend registered but inert - Logger will simply
	//!skip it in dispatch() rather than treat it as fatal, so eg. a
	//!missing SD card or unattached USB Gecko degrades gracefully
	//!instead of taking the rest of logging down with it.
	virtual bool init(const LogConfig & config) = 0;

	//!Releases whatever init() acquired. Must be safe to call even
	//!if init() was never called or returned false.
	virtual void shutdown() = 0;

	//!Writes one already-formatted, newline-terminated line. `len`
	//!does not include the terminating NUL. Must not allocate.
	virtual void write(LogLevel level, const char * line, size_t len) = 0;

	//!Short identifier for diagnostics (eg. "UDP", "File").
	virtual const char * name() const = 0;
};

//!Central fan-out dispatcher. Concrete (not abstract) - there is exactly
//!one Logger, owned by Platform, and every platform builds the same kind
//!of Logger with a different set of backends registered into it.
class Logger
{
public:
	Logger();
	~Logger();

	Logger(const Logger &) = delete;
	Logger & operator=(const Logger &) = delete;

	//!Applies config, calling init()/shutdown() on registered
	//!backends as needed so only the ones actually selected by
	//!config are live. Safe to call again later (eg. a settings
	//!menu flipping LogMode at runtime) - it reconciles against
	//!whatever was previously active rather than assuming this is
	//!the first call.
	void init(const LogConfig & config);

	//!Shuts down every registered backend and forgets the config.
	//!Backends themselves remain registered (and owned) until this
	//!Logger is destroyed, so a later init() call can reactivate
	//!them without re-registering.
	void shutdown();

	//!Registers a backend under the given single-bit LogBackendId,
	//!for later selection via LogConfig::mode / multiBackendMask.
	//!Logger takes ownership (deletes it in ~Logger()). At most one
	//!backend may be registered per id; a duplicate registration
	//!deletes `backend` and returns false rather than leaking it or
	//!silently replacing the existing one.
	//!\return false if id is not a single bit, MAX_BACKENDS slots
	//!are already used, or id is already registered.
	bool registerBackend(LogBackendId id, LoggingDriver * backend);

	//!Runtime-adjusts the minimum severity without touching any
	//!other config field or re-touching backend init/shutdown.
	void setLevel(LogLevel level);
	LogLevel getLevel() const { return config.level; }

	//!Formats fmt/args into a fixed stack buffer and fans it out to
	//!every backend selected by the current config. No-ops (cheaply)
	//!if level is below the configured minimum or Logger hasn't been
	//!init()'d. Called by the LOG_*() macros via LogPrintf() below -
	//!application code should not normally call this directly.
	void log(LogLevel level, const char * fmt, va_list args);

	static const int MAX_BACKENDS = 8;

private:
	struct Slot
	{
		LogBackendId id = LOGGER_NONE;
		LoggingDriver * backend = nullptr;
		bool active = false; //!< true once init()'d against the current config and selected by it
	};

	//! Resolves config.mode (+ mirrorToOSReport) into the effective
	//! set of backend ids that should be active right now.
	uint32_t resolveActiveMask() const;

	Slot slots[MAX_BACKENDS];
	int slotCount = 0;
	LogConfig config;
	Mutex lock;
	uint32_t sequence = 0;
	bool initialized = false;
};

#endif // __cplusplus

/****************************************************************************
* C bridge
*
* LogPrintf() (below) is a C++ function taking a `LogLevel` argument, so
* its mangled name and argument type aren't callable from C.
***************************************************************************/
#ifdef __cplusplus
extern "C" {
#endif

//!Formats fmt/... and dispatches to platform->getLogger(), if any. Safe
//!to call before a Logger exists or from a background thread - both are
//!no-ops/thread-safe respectively. This is what LOG_*() below expands to
//!from C++; call it directly only if you need a level chosen at runtime.
#ifdef __cplusplus
void LogPrintf(LogLevel level, const char * fmt, ...);
#endif

//!C-callable equivalent of LogPrintf(), for .c translation units. `level`
//!is one of LOG_LEVEL_DEBUG/INFO/WARNING/ERROR. This is what LOG_*()
//!below expands to from C.
void Log_Printf(int level, const char * fmt, ...);

#ifdef __cplusplus
} // extern "C"
#endif

#if LOGGING_ENABLED
  #ifdef __cplusplus
    #define LOG_DEBUG(fmt, ...) LogPrintf(LogLevel::Debug,   fmt, ##__VA_ARGS__)
    #define LOG_INFO(fmt, ...)  LogPrintf(LogLevel::Info,    fmt, ##__VA_ARGS__)
    #define LOG_WARN(fmt, ...)  LogPrintf(LogLevel::Warning, fmt, ##__VA_ARGS__)
    #define LOG_ERROR(fmt, ...) LogPrintf(LogLevel::Error,   fmt, ##__VA_ARGS__)
  #else
    #define LOG_DEBUG(fmt, ...) Log_Printf(LOG_LEVEL_DEBUG,   fmt, ##__VA_ARGS__)
    #define LOG_INFO(fmt, ...)  Log_Printf(LOG_LEVEL_INFO,    fmt, ##__VA_ARGS__)
    #define LOG_WARN(fmt, ...)  Log_Printf(LOG_LEVEL_WARNING, fmt, ##__VA_ARGS__)
    #define LOG_ERROR(fmt, ...) Log_Printf(LOG_LEVEL_ERROR,   fmt, ##__VA_ARGS__)
  #endif
#else
//! (void)0, not (void)fmt: the point is that fmt/__VA_ARGS__ are
//! never referenced, so the compiler never has a reason to place the
//! format string literal in .rodata and never evaluates any argument
//! expression (including ones with side effects - by design, exactly
//! like assert() under NDEBUG, so don't rely on LOG_*() arguments
//! running when LOGGING_ENABLED is 0).
#define LOG_DEBUG(fmt, ...) ((void)0)
#define LOG_INFO(fmt, ...)  ((void)0)
#define LOG_WARN(fmt, ...)  ((void)0)
#define LOG_ERROR(fmt, ...) ((void)0)
#endif

//! Back-compat alias for the common case - equivalent to LOG_INFO.
#define LOG(fmt, ...) LOG_INFO(fmt, ##__VA_ARGS__)
