/****************************************************************************
 * Platform Abstraction Layer (OGC driver)
 * Daryl Borth 2026
 * OgcSmbDriver.cpp
 *
 * Registers a "smb:/" newlib devoptab backed by libsmb2's synchronous
 * client API (smb2_open/smb2_pread/smb2_opendir/...)
 *
 * All paths handed in by newlib arrive with leading 'smb:/'
 * so every callback strips it via RelativePath() below.
 ***************************************************************************/
#include <cerrno>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/iosupport.h>
#include <smb2/smb2.h>
#include <smb2/libsmb2.h>
#include <cstdio>
#include "OgcSmbDriver.h"
#include "../Logger.h"

#ifdef HW_DOL
#include "gamecube/GameCubeNetwork.h"
#else
#include "wii/WiiNetwork.h"
#endif

smb2_context * OgcSmbDriver::ctx = nullptr;

// Fixed device name (smb:/)
static const char * const smbDeviceName = "smb";

// Last failure's detail, in libsmb2's own words where we have a context to
// ask - a real protocol/auth error ("NT_STATUS_ACCESS_DENIED", "Failed to
// resolve hostname") tells you far more than the generic POSIX errno
static char lastError[256] = "";

static void CaptureSmb2Error(const char * context)
{
	const char * detail = OgcSmbDriver::getContext() ? smb2_get_error(OgcSmbDriver::getContext()) : nullptr;

	if(detail && detail[0])
		snprintf(lastError, sizeof(lastError), "%s: %s", context, detail);
	else
		snprintf(lastError, sizeof(lastError), "%s", context);
}

/****************************************************************************
 * Path / error helpers
 ***************************************************************************/
static const char * RelativePath(const char * path)
{
	// Devoptab callbacks get the ORIGINAL path exactly as the app passed
	// it to opendir()/open()/etc - including our own "smb:" device prefix
	// Strip the device prefix before passing to libsmb2
	const char * colon = strchr(path, ':');
	if(colon)
		path = colon + 1;

	if(path[0] == '/')
		path++;
	return path;
}

static int SetErrnoFromSmb2(int smb2Result)
{
	// libsmb2's sync calls return -errno directly on failure.
	errno = (smb2Result < 0) ? -smb2Result : EIO;
	return -1;
}

static void FillStatFromSmb2(struct stat * st, const smb2_stat_64 & src)
{
	memset(st, 0, sizeof(struct stat));
	st->st_size  = (off_t)src.smb2_size;
	st->st_mode  = (src.smb2_type == SMB2_TYPE_DIRECTORY) ? (S_IFDIR | 0777) : (S_IFREG | 0666);
	st->st_nlink = src.smb2_nlink ? src.smb2_nlink : 1;
	st->st_mtime = (time_t)src.smb2_mtime;
	st->st_atime = (time_t)src.smb2_atime;
	st->st_ctime = (time_t)src.smb2_ctime;
}

/****************************************************************************
 * File state - devoptab hands us a caller-sized blob per open fd; we use it
 * to track the smb2fh plus our own read/write cursor, since we drive the
 * connection with pread/pwrite (explicit offsets) rather than depending on
 * an internal libsmb2 seek position.
 ***************************************************************************/
struct SmbFileState
{
	smb2fh * fh;
	uint64_t offset;
};

static int smb_open_r(struct _reent *, void * fileStruct, const char * path, int flags, int)
{
	SmbFileState * state = (SmbFileState *)fileStruct;

	// libsmb2's smb2_open() only documents O_RDONLY/O_WRONLY/O_RDWR/O_SYNC/
	// O_CREAT/O_EXCL as supported - O_APPEND is handled ourselves below via
	// the tracked write cursor, so it's stripped before handing flags off
	// rather than relying on the server/library to honor it.
	bool append = (flags & O_APPEND) != 0;
	smb2fh * fh = smb2_open(OgcSmbDriver::getContext(), RelativePath(path), flags & ~O_APPEND);
	if(!fh)
	{
		CaptureSmb2Error("open");
		return SetErrnoFromSmb2(-EIO);
	}

	state->fh = fh;
	state->offset = 0;

	if(append)
	{
		smb2_stat_64 st;
		if(smb2_fstat(OgcSmbDriver::getContext(), fh, &st) == 0)
			state->offset = st.smb2_size;
	}

	return 0;
}

static int smb_close_r(struct _reent *, void * fileStruct)
{
	SmbFileState * state = (SmbFileState *)fileStruct;
	smb2_close(OgcSmbDriver::getContext(), state->fh);
	return 0;
}

static ssize_t smb_write_r(struct _reent *, void * fileStruct, const char * ptr, size_t len)
{
	SmbFileState * state = (SmbFileState *)fileStruct;
	int written = smb2_pwrite(OgcSmbDriver::getContext(), state->fh, (const uint8_t *)ptr, (uint32_t)len, state->offset);
	if(written < 0)
		return SetErrnoFromSmb2(written);

	state->offset += written;
	return written;
}

static ssize_t smb_read_r(struct _reent *, void * fileStruct, char * ptr, size_t len)
{
	SmbFileState * state = (SmbFileState *)fileStruct;
	int bytesRead = smb2_pread(OgcSmbDriver::getContext(), state->fh, (uint8_t *)ptr, (uint32_t)len, state->offset);
	if(bytesRead < 0)
		return SetErrnoFromSmb2(bytesRead);

	state->offset += bytesRead;
	return bytesRead;
}

static off_t smb_seek_r(struct _reent *, void * fileStruct, off_t pos, int dir)
{
	SmbFileState * state = (SmbFileState *)fileStruct;

	uint64_t base = 0;
	if(dir == SEEK_CUR)
		base = state->offset;
	else if(dir == SEEK_END)
	{
		smb2_stat_64 st;
		if(smb2_fstat(OgcSmbDriver::getContext(), state->fh, &st) < 0)
			return SetErrnoFromSmb2(-EIO);
		base = st.smb2_size;
	}
	// SEEK_SET: base stays 0

	state->offset = base + pos;
	return (off_t)state->offset;
}

static int smb_fstat_r(struct _reent *, void * fileStruct, struct stat * st)
{
	SmbFileState * state = (SmbFileState *)fileStruct;
	smb2_stat_64 s;
	int result = smb2_fstat(OgcSmbDriver::getContext(), state->fh, &s);
	if(result < 0)
	{
		CaptureSmb2Error("fstat");
		return SetErrnoFromSmb2(result);
	}

	FillStatFromSmb2(st, s);
	return 0;
}

static int smb_stat_r(struct _reent *, const char * path, struct stat * st)
{
	smb2_stat_64 s;
	int result = smb2_stat(OgcSmbDriver::getContext(), RelativePath(path), &s);
	if(result < 0)
	{
		CaptureSmb2Error("stat");
		return SetErrnoFromSmb2(result);
	}

	FillStatFromSmb2(st, s);
	return 0;
}

static int smb_unlink_r(struct _reent *, const char * path)
{
	int result = smb2_unlink(OgcSmbDriver::getContext(), RelativePath(path));
	return (result < 0) ? SetErrnoFromSmb2(result) : 0;
}

static int smb_mkdir_r(struct _reent *, const char * path, int)
{
	int result = smb2_mkdir(OgcSmbDriver::getContext(), RelativePath(path));
	return (result < 0) ? SetErrnoFromSmb2(result) : 0;
}

static int smb_rmdir_r(struct _reent *, const char * path)
{
	int result = smb2_rmdir(OgcSmbDriver::getContext(), RelativePath(path));
	return (result < 0) ? SetErrnoFromSmb2(result) : 0;
}

/****************************************************************************
 * Directory iteration
 ***************************************************************************/
struct SmbDirState
{
	smb2dir * dir;
};

static DIR_ITER * smb_diropen_r(struct _reent *, DIR_ITER * dirState, const char * path)
{
	const char * relPath = RelativePath(path);
	smb2dir * dir = smb2_opendir(OgcSmbDriver::getContext(), relPath);
	if(!dir)
	{
		char context[64];
		snprintf(context, sizeof(context), "opendir '%s'", relPath[0] ? relPath : "(root)");
		CaptureSmb2Error(context);
		SetErrnoFromSmb2(-ENOENT);
		return nullptr;
	}

	((SmbDirState *)dirState->dirStruct)->dir = dir;
	return dirState;
}

static int smb_dirreset_r(struct _reent *, DIR_ITER * dirState)
{
	smb2_rewinddir(OgcSmbDriver::getContext(), ((SmbDirState *)dirState->dirStruct)->dir);
	return 0;
}

static int smb_dirnext_r(struct _reent *, DIR_ITER * dirState, char * filename, struct stat * filestat)
{
	smb2dir * dir = ((SmbDirState *)dirState->dirStruct)->dir;
	smb2dirent * entry = smb2_readdir(OgcSmbDriver::getContext(), dir);
	if(!entry)
	{
		errno = ENOENT; // no more entries - the standard devoptab end-of-listing signal
		return -1;
	}

	strncpy(filename, entry->name, NAME_MAX);
	filename[NAME_MAX] = '\0';
	FillStatFromSmb2(filestat, entry->st);
	return 0;
}

static int smb_dirclose_r(struct _reent *, DIR_ITER * dirState)
{
	smb2_closedir(OgcSmbDriver::getContext(), ((SmbDirState *)dirState->dirStruct)->dir);
	return 0;
}

/****************************************************************************
 * Devoptab registration
 ***************************************************************************/
static devoptab_t BuildSmbDevoptab()
{
	devoptab_t dotab = {};

	dotab.name         = smbDeviceName;
	dotab.structSize   = sizeof(SmbFileState);
	dotab.open_r       = smb_open_r;
	dotab.close_r      = smb_close_r;
	dotab.write_r      = smb_write_r;
	dotab.read_r       = smb_read_r;
	dotab.seek_r       = smb_seek_r;
	dotab.fstat_r      = smb_fstat_r;
	dotab.stat_r       = smb_stat_r;
	dotab.unlink_r     = smb_unlink_r;
	dotab.mkdir_r      = smb_mkdir_r;
	dotab.rmdir_r      = smb_rmdir_r;
	dotab.dirStateSize = sizeof(SmbDirState);
	dotab.diropen_r    = smb_diropen_r;
	dotab.dirreset_r   = smb_dirreset_r;
	dotab.dirnext_r    = smb_dirnext_r;
	dotab.dirclose_r   = smb_dirclose_r;

	return dotab;
}

/****************************************************************************
 * OgcSmbDriver
 ***************************************************************************/
void OgcSmbDriver::init()
{
	// Bring-up happens lazily in ensureNetworkUp() on the first connect()
}

void OgcSmbDriver::shutdown()
{
	disconnect();

#ifdef HW_DOL
	GameCubeNetwork::shutdown();
#else
	WiiNetwork::shutdown();
#endif
}

bool OgcSmbDriver::isNetworkUp() const
{
#ifdef HW_DOL
	return GameCubeNetwork::isUp();
#else
	return WiiNetwork::isUp();
#endif
}

bool OgcSmbDriver::ensureNetworkUp()
{
#ifdef HW_DOL
	if(GameCubeNetwork::ensureUp()) return true;
#else
	if(WiiNetwork::ensureUp()) return true;
#endif
	snprintf(lastError, sizeof(lastError), "Network unavailable");
	return false;
}

SmbConnectResult OgcSmbDriver::connect(const SmbShareInfo & info)
{
	lastError[0] = '\0';

	if(info.host[0] == '\0' || info.share[0] == '\0')
		return SmbConnectResult::InvalidSettings;

	if(ctx && strcmp(current.host, info.host) == 0 && strcmp(current.share, info.share) == 0 && strcmp(current.user, info.user) == 0)
		return SmbConnectResult::Success; // already connected to this exact target

	disconnect(); // drop any existing connection to a *different* target first
	if(!ensureNetworkUp())
		return SmbConnectResult::NetworkUnavailable;

	ctx = smb2_init_context();
	if(!ctx)
	{
		snprintf(lastError, sizeof(lastError), "Connection failed");
		return SmbConnectResult::ConnectFailed;
	}

	smb2_set_security_mode(ctx, SMB2_NEGOTIATE_SIGNING_ENABLED);
	smb2_set_version(ctx, SMB2_VERSION_ANY2);
	smb2_set_timeout(ctx, 30);
	if(info.user[0] != '\0')
		smb2_set_user(ctx, info.user);
	smb2_set_password(ctx, info.password);

	// This is the single call most likely to be the hang: it does DNS
	// resolution, TCP connect, and the full SMB2 negotiate/session-setup/
	// tree-connect exchange synchronously, with no timeout of its own.
	// If a lockup reliably lands here, try connecting by bare IP (rules
	// out getaddrinfo()/DNS) and check whether libsmb2 was built assuming
	// standard poll()/select() semantics that libogc's socket layer
	// doesn't fully provide.
	int connectResult = smb2_connect_share(ctx, info.host, info.share, info.user[0] ? info.user : nullptr);
	if(connectResult != 0)
	{
		CaptureSmb2Error("connect_share");
		smb2_destroy_context(ctx);
		ctx = nullptr;
		return SmbConnectResult::ConnectFailed;
	}

	static devoptab_t smbDevoptab = BuildSmbDevoptab();
	int devnum = AddDevice(&smbDevoptab);
	if(devnum < 0)
	{
		// A real connection succeeded but the devoptab itself couldn't be registered
		snprintf(lastError, sizeof(lastError), "Add device failed");
		smb2_disconnect_share(ctx);
		smb2_destroy_context(ctx);
		ctx = nullptr;
		return SmbConnectResult::ConnectFailed;
	}
	devoptabAdded = true;

	current = info;
	return SmbConnectResult::Success;
}

void OgcSmbDriver::disconnect()
{
	if(devoptabAdded)
	{
		RemoveDevice("smb:");
		devoptabAdded = false;
	}

	if(ctx)
	{
		smb2_disconnect_share(ctx);
		smb2_destroy_context(ctx);
		ctx = nullptr;
	}

	memset(&current, 0, sizeof(current));
}

const char * OgcSmbDriver::connectResultMessage(SmbConnectResult result) const
{
	switch(result)
	{
		case SmbConnectResult::Success:            return "Connected.";
		case SmbConnectResult::InvalidSettings:    return "Network share host/name is blank.";
		case SmbConnectResult::NetworkUnavailable: return "Unable to initialize network!";
		case SmbConnectResult::ConnectFailed:      return "Failed to connect to network share.";
		default:                                   return "Unknown network share error.";
	}
}

const char * OgcSmbDriver::getLastError() const
{
	return lastError;
}
