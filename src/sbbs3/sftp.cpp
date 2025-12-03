#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <stdexcept>
#include <memory>
#include <vector>

#include "sbbs.h"
#include "xpprintf.h" // for asprintf() on Win32

constexpr uint32_t lib_flag {UINT32_C(1) << 31};
constexpr uint32_t lib_mask {~lib_flag};
constexpr uint32_t users_gid {UINT32_MAX};

#define SLASH_FILES "/files"
#define SLASH_VFILES "/vfiles"
#define SLASH_FLS "/fls"
#define SLASH_HOME "/home"
#define MAX_FILES_PER_READDIR 25

constexpr int32_t no_more_files = -3;
constexpr int32_t dot = -2;
constexpr int32_t dotdot = -1;

/*
 * This does all the work of mapping an sftp path received from the client
 * to an opendir handle.  It will also enforce permissions.
 */
typedef enum map_path_result {
	MAP_FAILED,
	MAP_BAD_PATH,
	MAP_PERMISSION_DENIED,
	MAP_ALLOC_FAILED,
	MAP_INVALID_ARGS,
	MAP_SMB_FAILED,
	// The rest should be in this order at the end
	MAP_SUCCESS,
	MAP_TO_FILE = MAP_SUCCESS,
	MAP_TO_DIR,
	MAP_TO_SYMLINK,
} map_path_result_t;

typedef enum map_path_mode {
	MAP_STAT,
	MAP_READ,
	MAP_WRITE,
	MAP_RDWR,
} map_path_mode_t;

struct pathmap {
	const char *sftp_patt;  // %s is replaced with user alias
	const char *real_patt;  // %s is replaced with datadir, %d with user number
	const char *link_patt;  // %s is replaced with datadir, %d with user number
	sftp_file_attr_t (*get_attrs)(sbbs_t *sbbs, const char *path);
	bool is_dynamic;
};

static sftp_file_attr_t rootdir_attrs(sbbs_t *sbbs, const char *path);
static sftp_file_attr_t homedir_attrs(sbbs_t *sbbs, const char *path);
static sftp_file_attr_t homefile_attrs(sbbs_t *sbbs, const char *path);
static sftp_file_attr_t sshkeys_attrs(sbbs_t *sbbs, const char *path);
static char *sftp_parse_crealpath(sbbs_t *sbbs, const char *filename);
static char *expand_slash(const char *orig);
static bool is_in_filebase(const char *path);
static enum sftp_dir_tree in_tree(const char *path);
static char * get_longname(sbbs_t *sbbs, const char *path, const char *link, sftp_file_attr_t attr);
static sftp_file_attr_t get_attrs(sbbs_t *sbbs, const char *path, char **link);

const char            files_path[] = SLASH_FILES;
constexpr size_t      files_path_len = (sizeof(files_path) - 1);
const char            vfiles_path[] = SLASH_VFILES;
constexpr size_t      vfiles_path_len = (sizeof(vfiles_path) - 1);
const char            fls_path[] = SLASH_FLS;
constexpr size_t      fls_path_len = (sizeof(fls_path) - 1);

static struct pathmap static_files[] = {
	// TODO: ftpalias.cfg
	// TODO: User to user file transfers
	// TODO: Upload to sysop
	{"/", nullptr, nullptr, rootdir_attrs},
	{SLASH_FILES "/", nullptr, nullptr, rootdir_attrs},
	//{SLASH_FLS "/", nullptr, nullptr, rootdir_attrs},
	{SLASH_HOME "/", nullptr, nullptr, homedir_attrs},
	{SLASH_HOME "/%s/", nullptr, nullptr, homedir_attrs},
	// TODO: Some way for a sysop/mod authour to map things in here
	{SLASH_HOME "/%s/.ssh/", nullptr, nullptr, homedir_attrs},
	{SLASH_HOME "/%s/.ssh/authorized_keys", "%suser/%04d.sshkeys", SLASH_HOME "/%s/sshkeys", sshkeys_attrs},
	{SLASH_HOME "/%s/sshkeys", "%suser/%04d.sshkeys", nullptr, homefile_attrs},
	{SLASH_HOME "/%s/plan", "%suser/%04d.plan", nullptr, homefile_attrs},
	{SLASH_HOME "/%s/signature", "%suser/%04d.sig", nullptr, homefile_attrs},
	{SLASH_HOME "/%s/smtptags", "%suser/%04d.smtptags", nullptr, homefile_attrs},
	//{SLASH_VFILES "/", nullptr, nullptr, rootdir_attrs},
};
constexpr size_t      static_files_sz = (sizeof(static_files) / sizeof(static_files[0]));

class path_map {
bool is_static_ {true};
enum sftp_dir_tree tree_ {SFTP_DTREE_FULL};
map_path_result_t result_ {MAP_FAILED};

// Too lazy to write a std::expected thing here.
int find_lib_sz_(const char *libnam, size_t lnsz)
{
	for (int l = 0; l < sbbs->cfg.total_libs; l++) {
		if (!user_can_access_lib(&sbbs->cfg, l, &sbbs->useron, &sbbs->client))
			continue;
		char *exp {};
		switch (tree_) {
			case SFTP_DTREE_FULL:
				exp = expand_slash(sbbs->cfg.lib[l]->lname);
				break;
			case SFTP_DTREE_SHORT:
				exp = expand_slash(sbbs->cfg.lib[l]->sname);
				break;
			case SFTP_DTREE_VIRTUAL:
				exp = expand_slash(sbbs->cfg.lib[l]->vdir);
				break;
		}
		if (exp == nullptr)
			return -1;
		if ((memcmp(libnam, exp, lnsz) == 0)
		    && (exp[lnsz] == 0)) {
			free(exp);
			return l;
		}
		free(exp);
	}
	return -1;
}

int find_dir_sz_(const char *dirnam, int lib, size_t dnsz)
{
	for (int d = 0; d < sbbs->cfg.total_dirs; d++) {
		if (sbbs->cfg.dir[d]->lib != lib)
			continue;
		if (!user_can_access_dir(&sbbs->cfg, d, &sbbs->useron, &sbbs->client))
			continue;
		char *exp {};
		switch (tree_) {
			case SFTP_DTREE_FULL:
				exp = expand_slash(sbbs->cfg.dir[d]->lname);
				break;
			case SFTP_DTREE_SHORT:
				exp = expand_slash(sbbs->cfg.dir[d]->sname);
				break;
			case SFTP_DTREE_VIRTUAL:
				exp = expand_slash(sbbs->cfg.dir[d]->vdir);
				break;
		}
		if (exp == nullptr)
			return -1;
		if ((memcmp(dirnam, exp, dnsz) == 0)
		    && (exp[dnsz] == 0)) {
			free(exp);
			return d;
		}
		free(exp);
	}
	return -1;
}

public:
const map_path_mode_t mode;
sbbs_t * const sbbs{};
char * local_path{};
char * sftp_path{};
char * sftp_link_target{};
union {
	struct {
		uint32_t offset;
		int32_t idx;
		int lib;
		int dir;
	} filebase {0, no_more_files, -1, -1};
	struct {
		struct pathmap *mapping;
	} rootdir;
} info;

path_map() = delete;
path_map(sbbs_t *sbbsptr, const char* path, map_path_mode_t mode) : mode(mode),  sbbs(sbbsptr)
{
	path_map(sbbs, reinterpret_cast<const uint8_t*>(path), mode);
}

path_map(sbbs_t *sbbsptr, const uint8_t* path, map_path_mode_t mode) : mode(mode), sbbs(sbbsptr)
{
	const char *c;
	const char *cpath = reinterpret_cast<const char *>(path);

	if (path == nullptr || sbbs == nullptr) {
		result_ = MAP_INVALID_ARGS;
		return;
	}

	this->sftp_path = sftp_parse_crealpath(sbbs, cpath);
	if (this->sftp_path == nullptr) {
		return;
	}
	if (is_in_filebase(this->sftp_path)) {
		tree_ = in_tree(this->sftp_path);
		// This is in the file base.
		if (mode == MAP_RDWR) {
			result_ = MAP_PERMISSION_DENIED;
			return;
		}
		this->is_static_ = false;
		this->info.filebase.dir = -1;
		this->info.filebase.lib = -1;
		this->info.filebase.idx = dot;
		char * lib;
		size_t sz{files_path_len};     // Initialize in case tree_ isn't valid (which should have trown an exception)
		switch (tree_) {
			case SFTP_DTREE_FULL:
				sz = files_path_len;
				break;
			case SFTP_DTREE_SHORT:
				sz = fls_path_len;
				break;
			case SFTP_DTREE_VIRTUAL:
				sz = vfiles_path_len;
				break;
		}
		if (this->sftp_path[sz] == 0 || this->sftp_path[sz + 1] == 0) {
			// Root...
			result_ = MAP_TO_DIR;
			return;
		}
		lib = &this->sftp_path[sz + 1];
		c = strchr(lib, '/');
		size_t libsz;
		if (c == nullptr) {
			libsz = strlen(lib);
		}
		else {
			libsz = c - (lib);
		}
		this->info.filebase.lib = find_lib_sz_(lib, libsz);
		if (this->info.filebase.lib == -1) {
			result_ = MAP_BAD_PATH;
			return;
		}
		if (c == nullptr || c[1] == 0) {
			result_ = MAP_TO_DIR;
			return;
		}
		// There's a dir name too...
		const char *dir = &lib[libsz + 1];
		c = strchr(dir, '/');
		size_t      dirsz;
		if (c == nullptr) {
			dirsz = strlen(dir);
		}
		else {
			dirsz = c - dir;
		}
		this->info.filebase.dir = find_dir_sz_(dir, this->info.filebase.lib, dirsz);
		if (this->info.filebase.dir == -1) {
			result_ = MAP_BAD_PATH;
			return;
		}
		if (c == nullptr || c[1] == 0) {
			result_ = MAP_TO_DIR;
			return;
		}
		// There's a filename too!  What fun!
		smb_t       smb{};
		smbfile_t   file{};
		result_ = MAP_TO_FILE;
		const char *fname = &dir[dirsz + 1];
		if (asprintf(&this->local_path, "%s/%s", sbbs->cfg.dir[this->info.filebase.dir]->path, fname) == -1) {
			result_ = MAP_ALLOC_FAILED;
			return;
		}
		if (smb_open_dir(&sbbs->cfg, &smb, this->info.filebase.dir) != SMB_SUCCESS) {
			result_ = MAP_SMB_FAILED;
			return;
		}
		if (smb_findfile(&smb, fname, &file) != SMB_SUCCESS) {
			/*
				 * If it doesn't exist, and we're trying to write,
				 * this is success
				 */
			if ((mode == MAP_READ) || (mode == MAP_STAT)) {
				result_ = MAP_BAD_PATH;
				return;
			}
			if (access(this->local_path, F_OK)) {
				// File already exists...
				result_ = MAP_PERMISSION_DENIED;
				return;
			}
			return;
		}
		this->info.filebase.idx = file.file_idx.idx.number;
		this->info.filebase.offset = file.file_idx.idx.offset;
		// TODO: Keep this around?
		smb_freefilemem(&file);
		smb_close(&smb);
		/* TODO: Sometimes some users can overwrite some files...
			 *       but for now, nobody can.
			 */
		if (mode == MAP_WRITE || mode == MAP_RDWR) {
			result_ = MAP_PERMISSION_DENIED;
			return;
		}
		if (mode == MAP_READ) {
			if (!user_can_download(&sbbs->cfg, this->info.filebase.dir, &sbbs->useron, &sbbs->client, nullptr)) {
				result_ = MAP_PERMISSION_DENIED;
				return;
			}
		}
		return;
	}
	else {
		// Static files
		unsigned sfidx;

		this->is_static_ = true;
		size_t   pathlen = strlen(this->sftp_path);
		for (sfidx = 0; sfidx < static_files_sz; sfidx++) {
			char tmpdir[MAX_PATH + 1];
			snprintf(tmpdir, sizeof(tmpdir), static_files[sfidx].sftp_patt, sbbs->useron.alias);
			if (strncmp(this->sftp_path, tmpdir, pathlen) == 0
			    && (tmpdir[pathlen] == 0
			        || (tmpdir[pathlen] == '/' && tmpdir[pathlen + 1] == 0))) {
				this->info.rootdir.mapping = &static_files[sfidx];
				if (static_files[sfidx].real_patt) {
					if (asprintf(&this->local_path, static_files[sfidx].real_patt, sbbs->cfg.data_dir, sbbs->useron.number) == -1) {
						result_ = MAP_ALLOC_FAILED;
						return;
					}
					result_ = MAP_TO_FILE;
				}
				else
					result_ = MAP_TO_DIR;
				if ((mode == MAP_READ || mode == MAP_STAT)
				    && (result_ == MAP_TO_FILE)) {
					if (access(this->local_path, R_OK)) {
						result_ = MAP_BAD_PATH;
						return;
					}
				}
				if (static_files[sfidx].link_patt) {
					if (asprintf(&this->sftp_link_target, static_files[sfidx].link_patt, sbbs->useron.alias) == -1) {
						result_ = MAP_ALLOC_FAILED;
						return;
					}
				}
				return;
			}
		}
		result_ = MAP_FAILED;
		return;
	}
}

bool cleanup()
{
	switch (result_) {
		case MAP_BAD_PATH:
			return sftps_send_error(sbbs->sftp_state, SSH_FX_NO_SUCH_FILE, "No such file");
		case MAP_PERMISSION_DENIED:
			return sftps_send_error(sbbs->sftp_state, SSH_FX_PERMISSION_DENIED, "No such file");
		default:
			if (result_ >= MAP_SUCCESS) {
				return sftps_send_error(sbbs->sftp_state, SSH_FX_PERMISSION_DENIED, "No such file");
			}
			return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Mapping failure");
	}
}

const map_path_result_t &result(void) const {
	return result_;
}

const bool &is_static(void) const {
	return is_static_;
}

const enum sftp_dir_tree &tree(void) const {
	return tree_;
}

const bool success(void) {
	return result_ >= MAP_SUCCESS;
}

~path_map() {
	free(local_path);
	free(sftp_path);
	free(sftp_link_target);
}
};


class file_names {
uint32_t entries_{0};
sbbs_t * const sbbs{};
std::vector<char *> fnames;
std::vector<char *> lnames;
std::vector<sftp_file_attr_t> attrs;
public:
uint32_t entries(void) { return entries_; }

bool add_name(char *fname, char *lname, sftp_file_attr_t attr) {
	unsigned added = 0;
	try {
		fnames.push_back(fname);
		added++;
		lnames.push_back(lname);
		added++;
		attrs.push_back(attr);
		added++;
		entries_ += 1;
	}
	catch (...) {
		if (added < 1)
			free(fname);
		if (added < 2)
			free(lname);
		if (added < 3)
			sftp_fattr_free(attr);
		return false;
	}
	return true;
}

bool
generic_dot_attr_entry(char *fname, sftp_file_attr_t attr, char **link, int32_t& idx)
{
	if (attr == nullptr) {
		free(fname);
		return false;
	}
	char *lname = get_longname(sbbs, fname, link ? *link : nullptr, attr);
	if (lname == nullptr) {
		free(fname);
		sftp_fattr_free(attr);
		return false;
	}
	bool ret = add_name(fname, lname, attr);
	if (ret)
		idx++;
	return ret;
}

bool
generic_dot_entry(char *fname, const char *path, int32_t& idx)
{
	char *           link = nullptr;
	sftp_file_attr_t attr = get_attrs(sbbs, path, &link);
	if (attr == nullptr) {
		free(link);
		free(fname);
		return false;
	}
	bool ret = generic_dot_attr_entry(fname, attr, &link, idx);
	free(link);
	return ret;
}

bool
generic_dot_realpath_entry(char *fname, const char *path, int32_t& idx)
{
	char *vpath = sftp_parse_crealpath(sbbs, path);
	if (vpath == nullptr) {
		free(fname);
		return false;
	}
	bool ret = generic_dot_entry(fname, vpath, idx);
	free(vpath);
	return ret;
}

bool send(void) {
	if (entries_ < 1)
		return false;
	return sftps_send_name(sbbs->sftp_state, entries_, &fnames[0], &lnames[0], &attrs[0]);
}

file_names() = delete;
file_names(sbbs_t *sbbsptr) : sbbs(sbbsptr) {}

~file_names(void) {
	for (auto&& name : fnames)
		free(name);
	for (auto&& name : lnames)
		free(name);
	for (auto&& attr : attrs)
		sftp_fattr_free(attr);
}
};

static std::string
sftp_attr_string(sftp_file_attr_t attr)
{
	uint64_t           u64;
	uint32_t           u32;
	sftp_str_t         str;
	std::ostringstream ret;
	std::string        rstr;

	try {
		if (sftp_fattr_get_size(attr, &u64))
			ret << "size=" << u64 << ", ";
		if (sftp_fattr_get_uid(attr, &u32))
			ret << "uid=" << u32 << ", ";
		if (sftp_fattr_get_gid(attr, &u32))
			ret << "gid=" << u32 << ", ";
		if (sftp_fattr_get_permissions(attr, &u32))
			ret << "perm=0" << std::oct << u32 << ", " << std::dec;
		/*
		 * We can't use std::put_time() because apparently std::gmtime_r() isn't
		 * available on Win32.
		 */
		if (sftp_fattr_get_atime(attr, &u32)) {
			struct tm t;
			time_t    tt = u32;
			if (gmtime_r(&tt, &t))
				ret << "atime=" << std::put_time(&t, "%c") << u32 << ", ";
			else
				ret << "atime=" << u32 << ", ";
		}
		if (sftp_fattr_get_mtime(attr, &u32)) {
			struct tm t;
			time_t    tt = u32;
			if (gmtime_r(&tt, &t))
				ret << "mtime=" << std::put_time(&t, "%c") << u32 << ", ";
			else
				ret << "mtime=" << u32 << ", ";
		}
		u32 = sftp_fattr_get_ext_count(attr);
		for (uint32_t idx = 0; idx < u32; idx++) {
			str = sftp_fattr_get_ext_type(attr, idx);
			if (str)
				ret << str->c_str << "=";
			else
				ret << "<null>=";
			free_sftp_str(str);
			str = sftp_fattr_get_ext_data(attr, idx);
			if (str)
				ret << str->c_str << ", ";
			else
				ret << "<null>, ";
			free_sftp_str(str);
		}
		std::string rstr = ret.str();
		if (rstr.length() > 2)
			rstr.erase(rstr.length() - 2);
	}
	catch (...) {
		rstr = "<error>"; // TODO: This could throw as well. :(
	}
	return rstr;
}

static bool
is_in_filebase(const char *path)
{
	if (memcmp(files_path, path, files_path_len) == 0) {
		if (path[files_path_len] == 0 || path[files_path_len] == '/')
			return true;
	}
	if (memcmp(fls_path, path, fls_path_len) == 0) {
		if (path[fls_path_len] == 0 || path[fls_path_len] == '/')
			return true;
	}
	if (memcmp(vfiles_path, path, vfiles_path_len) == 0) {
		if (path[vfiles_path_len] == 0 || path[vfiles_path_len] == '/')
			return true;
	}
	return false;
}

static enum sftp_dir_tree
in_tree(const char *path)
{
	if (memcmp(files_path, path, files_path_len) == 0) {
		if (path[files_path_len] == 0 || path[files_path_len] == '/')
			return SFTP_DTREE_FULL;
	}
	if (memcmp(fls_path, path, fls_path_len) == 0) {
		if (path[fls_path_len] == 0 || path[fls_path_len] == '/')
			return SFTP_DTREE_SHORT;
	}
	if (memcmp(vfiles_path, path, vfiles_path_len) == 0) {
		if (path[vfiles_path_len] == 0 || path[vfiles_path_len] == '/')
			return SFTP_DTREE_VIRTUAL;
	}
	throw std::invalid_argument( "Invalid path" );
}

/*
 * Replaces a slash with a dash.
 *
 * The SFTP protocol requires the use of Solidus as a path separator,
 * and dir and lib names can contain it.  Rather than a visually
 * different and one-way mapping as used in the FTP server, take
 * advantage of the fact that dir and lib names aren't unicode to have
 * a visially similar reversible mapping.
 *
 * Even in the future, should these support unicode, it will at least
 * still be visually more similar, even if it's not reversible.
 */
static char *
expand_slash(const char *orig)
{
	char *      p;
	const char *p2;
	char *      p3;
	unsigned    slashes = 0;

	for (p2 = orig; *p2; p2++) {
		if (*p2 == '/')
			slashes++;
	}
	p = static_cast<char *>(malloc(strlen(orig) + (slashes * 2) + 1));
	if (p == nullptr)
		return p;
	p3 = p;
	for (p2 = orig; *p2; p2++) {
		if (*p2 == '/')
			*p3++ = '-';
		else
			*p3++ = *p2;
	}
	*p3 = 0;
	return p;
}

static sftp_file_attr_t
dummy_attrs(void)
{
	return sftp_fattr_alloc();
}

static sftp_file_attr_t
homedir_attrs(sbbs_t *sbbs, const char *path)
{
	sftp_file_attr_t attr = sftp_fattr_alloc();

	if (attr == nullptr)
		return nullptr;
	sftp_fattr_set_permissions(attr, S_IFDIR | S_IRWXU | S_IRUSR | S_IWUSR | S_IXUSR);
	sftp_fattr_set_uid_gid(attr, sbbs->useron.number, users_gid);
	return attr;
}

static sftp_file_attr_t
rootdir_attrs(sbbs_t *sbbs, const char *path)
{
	sftp_file_attr_t attr = sftp_fattr_alloc();

	if (attr == nullptr)
		return nullptr;
	sftp_fattr_set_permissions(attr, S_IFDIR | S_IRWXU | S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
	sftp_fattr_set_uid_gid(attr, 1, users_gid);
	return attr;
}

static sftp_file_attr_t
homefile_attrs(sbbs_t *sbbs, const char *path)
{
	sftp_file_attr_t attr = sftp_fattr_alloc();

	if (attr == nullptr)
		return nullptr;
	sftp_fattr_set_permissions(attr, S_IFREG | S_IRWXU | S_IRUSR | S_IWUSR);
	sftp_fattr_set_uid_gid(attr, sbbs->useron.number, users_gid);
	sftp_fattr_set_size(attr, flength(path));
	uint32_t fd = static_cast<uint32_t>(fdate(path));
	sftp_fattr_set_times(attr, fd, fd);
	return attr;
}

static sftp_file_attr_t
sshkeys_attrs(sbbs_t *sbbs, const char *path)
{
	sftp_file_attr_t attr = sftp_fattr_alloc();

	if (attr == nullptr)
		return nullptr;
	sftp_fattr_set_permissions(attr, S_IFLNK | S_IRWXU | S_IRUSR | S_IWUSR);
	sftp_fattr_set_uid_gid(attr, sbbs->useron.number, users_gid);
	sftp_fattr_set_size(attr, flength(path));
	uint32_t fd = static_cast<uint32_t>(fdate(path));
	sftp_fattr_set_times(attr, fd, fd);
	return attr;
}

void
remove_trailing_slash(char *str)
{
	size_t end = strlen(str);

	if (end > 0)
		end--;
	while (str[end] == '/' && end > 0)
		str[end] = 0;
}

/*
 * This is mostly copied from the xpdev _fullpath() implementation
 */
static char *
sftp_resolve_path(char *target, const char *path, size_t size)
{
	char *out;
	char *p;

	if (target == NULL) {
		size = MAX_PATH + 1;
		if ((target = (char*)malloc(size)) == NULL) {
			return NULL;
		}
	}
	strncpy(target, path, size);
	target[size - 1] = 0;
	out = target;

	for (; *out; out++) {
		while (*out == '/') {
			if (*(out + 1) == '/')
				memmove(out, out + 1, strlen(out));
			else if (*(out + 1) == '.' && (*(out + 2) == '/' || *(out + 2) == 0))
				memmove(out, out + 2, strlen(out) - 1);
			else if (*(out + 1) == '.' && *(out + 2) == '.' && (*(out + 3) == '/' || *(out + 3) == 0))  {
				*out = 0;
				p = strrchr(target, '/');
				if (p != NULL) {
					memmove(p, out + 3, strlen(out + 3) + 1);
					out = p;
				}
			}
			else  {
				out++;
			}
		}
	}
	if (size > 1 && *target == 0) {
		target[0] = '/';
		target[1] = 0;
	}
	return target;
}

static char *
sftp_parse_crealpath(sbbs_t *sbbs, const char *filename)
{
	char *ret;
	char *tmp;

	if (sbbs->sftp_cwd == nullptr) {
		if (asprintf(&sbbs->sftp_cwd, SLASH_HOME "/%s", sbbs->useron.alias) == -1)
			sbbs->sftp_cwd = nullptr;
	}
	if (sbbs->sftp_cwd == nullptr)
		return nullptr;
	if (!isfullpath(filename)) {
		if (asprintf(&tmp, "%s/%s", sbbs->sftp_cwd, filename) == -1)
			return tmp;
		ret = sftp_resolve_path(nullptr, tmp, 0);
		free(tmp);
	}
	else {
		ret = sftp_resolve_path(nullptr, filename, 0);
	}
	if (ret == nullptr)
		return ret;
	if (ret[0] == 0) {
		free(ret);
		return nullptr;
	}
	remove_trailing_slash(ret);

	return ret;
}

static char *
sftp_parse_realpath(sbbs_t *sbbs, sftp_str_t filename)
{
	return sftp_parse_crealpath(sbbs, reinterpret_cast<char *>(filename->c_str));
}

static unsigned
parse_file_handle(sbbs_t *sbbs, sftp_filehandle_t handle)
{
	constexpr size_t nfdes = sizeof(sbbs->sftp_filedes) / sizeof(sbbs->sftp_filedes[0]);

	long             tmp = strtol(reinterpret_cast<char *>(handle->c_str), nullptr, 10);
	if (tmp <= 0)
		return UINT_MAX;
	if (tmp > UINT_MAX)
		return UINT_MAX;
	if (static_cast<size_t>(tmp) > nfdes)
		return UINT_MAX;
	if (sbbs->sftp_filedes[tmp - 1] == nullptr)
		return UINT_MAX;
	return tmp - 1;
}

static unsigned
parse_dir_handle(sbbs_t *sbbs, sftp_dirhandle_t handle)
{
	constexpr size_t nfdes = sizeof(sbbs->sftp_dirdes) / sizeof(sbbs->sftp_dirdes[0]);

	if (handle->len < 3)
		return UINT_MAX;
	if (memcmp(handle->c_str, "D:", 2) != 0)
		return UINT_MAX;
	long tmp = strtol(reinterpret_cast<char *>(handle->c_str + 2), nullptr, 10);
	if (tmp <= 0)
		return UINT_MAX;
	if (tmp > UINT_MAX)
		return UINT_MAX;
	if (static_cast<size_t>(tmp) > nfdes)
		return UINT_MAX;
	if (sbbs->sftp_dirdes[tmp - 1] == nullptr)
		return UINT_MAX;
	return tmp - 1;
}

/*
 * From FreeBSD ls
 */
void
format_time(sbbs_t *sbbs, time_t ftime, char *longstring, size_t sz)
{
	static time_t now;
	const char *  format;
	static int    d_first = (sbbs->cfg.sys_date_fmt == DDMMYY);

	now = time(NULL);

#define SIXMONTHS       ((365 / 2) * 86400)
	if (ftime + SIXMONTHS > now && ftime < now + SIXMONTHS)
		/* mmm dd hh:mm || dd mmm hh:mm */
		format = d_first ? "%e %b %R" : "%b %e %R";
	else
		/* mmm dd  yyyy || dd mmm  yyyy */
		format = d_first ? "%e %b  %Y" : "%b %e  %Y";
	strftime(longstring, sz, format, localtime(&ftime));
}

static void
uid_to_string(sbbs_t *sbbs, uint32_t uid, char *buf)
{
	if (uid == 0)
		strcpy(buf, "<nobody>");
	if (username(&sbbs->cfg, uid, buf) == nullptr || buf[0] == 0)
		strcpy(buf, "unknown");
}

static void
gid_to_string(sbbs_t *sbbs, uint32_t gid, char *buf)
{
	if (gid == users_gid)
		strcpy(buf, "users");
	else if (gid & lib_flag)
		strcpy(buf, sbbs->cfg.lib[gid & lib_mask]->vdir);
	else
		strcpy(buf, sbbs->cfg.dir[gid]->code);
}

static char *
get_longname(sbbs_t *sbbs, const char *path, const char *link, sftp_file_attr_t attr)
{
	char *      ret;
	const char *fname;
	uint32_t    perms;
	uint32_t    mtime;
	uint64_t    sz;
	char        pstr[11];
	char        szstr[21];
	char        datestr[20];
	char        owner[LEN_ALIAS + 1];
	char        group[LEN_EXTCODE + 1];

	memset(pstr, '-', sizeof(pstr) - 1);
	pstr[sizeof(pstr) - 1] = 0;
	if (sftp_fattr_get_permissions(attr, &perms)) {
		switch (perms & S_IFMT) {
			case S_IFSOCK:
				pstr[0] = 's';
				break;
			case S_IFLNK:
				pstr[0] = 'l';
				break;
			case S_IFREG:
				pstr[0] = '-';
				break;
			case S_IFBLK:
				pstr[0] = 'b';
				break;
			case S_IFDIR:
				pstr[0] = 'd';
				break;
			case S_IFCHR:
				pstr[0] = 'c';
				break;
			case S_IFIFO:
				pstr[0] = 'p';
				break;
		}
		if (perms & S_IRUSR)
			pstr[1] = 'r';
		if (perms & S_IWUSR)
			pstr[2] = 'w';
		if (((perms & S_IXUSR) == 0) && (perms & S_ISUID))
			pstr[3] = 'S';
		else if ((perms & S_IXUSR) && (perms & S_ISUID))
			pstr[3] = 's';
		else if (perms & S_IXUSR)
			pstr[3] = 'x';
		if (perms & S_IRGRP)
			pstr[4] = 'r';
		if (perms & S_IWGRP)
			pstr[5] = 'w';
		if (((perms & S_IXGRP) == 0) && (perms & S_ISGID))
			pstr[6] = 'S';
		else if ((perms & S_IXGRP) && (perms & S_ISGID))
			pstr[6] = 's';
		else if (perms & S_IXGRP)
			pstr[6] = 'x';
		if (perms & S_IROTH)
			pstr[7] = 'r';
		if (perms & S_IWOTH)
			pstr[8] = 'w';
		if (perms & S_IXOTH) {
			if (perms & S_ISVTX)
				pstr[9] = 't';
			else
				pstr[9] = 'x';
		}
		else if (perms & S_ISVTX)
			pstr[9] = 'T';
	}
	sz = 0;
	sftp_fattr_get_size(attr, &sz);
	snprintf(szstr, sizeof szstr, "%8" PRIu64, sz);
	mtime = 0;
	sftp_fattr_get_mtime(attr, &mtime);
	format_time(sbbs, mtime, datestr, sizeof(datestr));
	uint32_t uid{0};
	sftp_fattr_get_uid(attr, &uid);
	uid_to_string(sbbs, uid, owner);
	uid = 0;
	sftp_fattr_get_gid(attr, &uid);
	gid_to_string(sbbs, uid, group);
	fname = getfname(path);
	if (fname[0] == 0)
		fname = path;
	if (asprintf(&ret, "%s   0 %-8.8s %-8.8s %-8s %-12s %s%s%s", pstr, owner, group, szstr, datestr, fname, pstr[0] == 'l' ? " -> " : "", link ? link : "") == -1)
		return nullptr;
	return ret;
}

static sftp_file_attr_t
get_lib_attrs(sbbs_t *sbbs, int lib)
{
	sftp_file_attr_t attr = sftp_fattr_alloc();

	if (attr == nullptr)
		return nullptr;
	sftp_fattr_set_permissions(attr, S_IFDIR | S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP);
	sftp_fattr_set_uid_gid(attr, 1, static_cast<uint32_t>(lib) | lib_flag);
	return attr;
}

static sftp_file_attr_t
get_dir_attrs(sbbs_t *sbbs, int32_t dir)
{
	sftp_file_attr_t attr = sftp_fattr_alloc();
	uint32_t         perms = S_IFDIR | S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP;

	if (attr == nullptr)
		return nullptr;
	if (user_can_upload(&sbbs->cfg, dir, &sbbs->useron, &sbbs->client, nullptr))
		perms |= S_IWGRP;
	sftp_fattr_set_permissions(attr, perms);
	sftp_fattr_set_uid_gid(attr, 1, static_cast<uint32_t>(dir));
	return attr;
}

static sftp_file_attr_t
get_filebase_attrs(sbbs_t *sbbs, int32_t dir, smbfile_t *file)
{
	sftp_file_attr_t attr = sftp_fattr_alloc();
	uint32_t         perms = S_IFREG | S_IRUSR | S_IWUSR;
	uint32_t         atime;
	uint32_t         mtime;

	if (attr == nullptr)
		return nullptr;
	if (user_can_download(&sbbs->cfg, dir, &sbbs->useron, &sbbs->client, nullptr))
		perms |= S_IRGRP;
	if (user_can_upload(&sbbs->cfg, dir, &sbbs->useron, &sbbs->client, nullptr))
		perms |= S_IWGRP;
	sftp_fattr_set_permissions(attr, perms);
	sftp_fattr_set_size(attr, smb_getfilesize(&file->idx));
	sftp_fattr_set_uid_gid(attr, 0, static_cast<uint32_t>(dir));
	atime = file->hdr.last_downloaded; // Is this a time_t?
	mtime = file->hdr.when_written.time;
	sftp_fattr_set_times(attr, atime, mtime);
	// TODO: How to get user number of uploader if available?  For uid
	//       Answer, from_ext... be sure to check if it's anonymous etc.
	//       Real answer: We don't store the user number of uploader,
	//                    look up the usernumber from uploader's username.

	return attr;
}

static int
find_lib(sbbs_t *sbbs, const char *path, enum sftp_dir_tree tree)
{
	char *p = strdup(path);
	char *c = strchr(p, '/');
	char *exp {};
	int   l;

	if (c)
		*c = 0;
	for (l = 0; l < sbbs->cfg.total_libs; l++) {
		if (!user_can_access_lib(&sbbs->cfg, l, &sbbs->useron, &sbbs->client))
			continue;
		switch (tree) {
			case SFTP_DTREE_FULL:
				exp = expand_slash(sbbs->cfg.lib[l]->lname);
				break;
			case SFTP_DTREE_SHORT:
				exp = expand_slash(sbbs->cfg.lib[l]->sname);
				break;
			case SFTP_DTREE_VIRTUAL:
				exp = expand_slash(sbbs->cfg.lib[l]->vdir);
				break;
		}
		if (exp == nullptr) {
			free(p);
			return -1;
		}
		if (strcmp(p, exp)) {
			free(exp);
			continue;
		}
		free(exp);
		break;
	}
	free(p);
	if (l < sbbs->cfg.total_libs)
		return l;
	return -1;
}

static int
find_dir(sbbs_t *sbbs, const char *path, int lib, enum sftp_dir_tree tree)
{
	char *p = strdup(path);
	char *c;
	char *e;
	int   d;
	char *exp{};

	if (p == nullptr)
		return -1;
	remove_trailing_slash(p);
	c = strchr(p, '/');
	if (c == nullptr || c[1] == 0) {
		free(p);
		return -1;
	}
	c++;
	e = strchr(c, '/');
	if (e != nullptr)
		*e = 0;
	for (d = 0; d < sbbs->cfg.total_dirs; d++) {
		if (sbbs->cfg.dir[d]->lib != lib)
			continue;
		if (!user_can_access_dir(&sbbs->cfg, d, &sbbs->useron, &sbbs->client))
			continue;
		switch (tree) {
			case SFTP_DTREE_FULL:
				exp = expand_slash(sbbs->cfg.dir[d]->lname);
				break;
			case SFTP_DTREE_SHORT:
				exp = expand_slash(sbbs->cfg.dir[d]->sname);
				break;
			case SFTP_DTREE_VIRTUAL:
				exp = expand_slash(sbbs->cfg.dir[d]->vdir);
				break;
		}
		if (exp == nullptr) {
			free(p);
			return -1;
		}
		if (strcmp(c, exp)) {
			free(exp);
			continue;
		}
		free(exp);
		break;
	}
	free(p);
	if (d < sbbs->cfg.total_dirs)
		return d;
	return -1;
}

static struct pathmap *
get_pathmap_ptr(sbbs_t *sbbs, const char *filename)
{
	unsigned sf;
	char     vpath[MAX_PATH + 1];

	for (sf = 0; sf < static_files_sz; sf++) {
		snprintf(vpath, sizeof(vpath), static_files[sf].sftp_patt, sbbs->useron.alias);
		remove_trailing_slash(vpath);
		if (strcmp(vpath, filename) == 0)
			return &static_files[sf];
	}
	return nullptr;
}

// TODO: This should be overhauled as well...
static sftp_file_attr_t
get_attrs(sbbs_t *sbbs, const char *path, char **link)
{
	struct pathmap * pm;
	char             ppath[MAX_PATH + 1];
	sftp_file_attr_t ret;

	if (link)
		*link = nullptr;
	pm = get_pathmap_ptr(sbbs, path);
	if (pm == nullptr) {
		int                lib;
		int                dir;
		const char *       libp;

		if (!is_in_filebase(path))
			return nullptr;
		enum sftp_dir_tree tree = in_tree(path);
		switch (tree) {
			case SFTP_DTREE_FULL:
				libp = path + files_path_len + 1;
				break;
			case SFTP_DTREE_SHORT:
				libp = path + fls_path_len + 1;
				break;
			case SFTP_DTREE_VIRTUAL:
				libp = path + vfiles_path_len + 1;
				break;
			default:
				return nullptr;
		}
		lib = find_lib(sbbs, libp, tree);
		if (lib == -1) {
			return nullptr;
		}
		const char *c = strchr(libp, '/');
		if (c == nullptr || c[1] == 0)
			return get_lib_attrs(sbbs, lib);
		dir = find_dir(sbbs, libp, lib, tree);
		if (dir == -1)
			return nullptr;
		c = strchr(c + 1, '/');
		if (c == nullptr || c[1] == 0)
			return get_dir_attrs(sbbs, dir);
		smb_t     smb{};
		smbfile_t file{};
		if (smb_open_dir(&sbbs->cfg, &smb, dir) != SMB_SUCCESS)
			return nullptr;
		if (smb_findfile(&smb, &c[1], &file) != SMB_SUCCESS) {
			smb_close(&smb);
			return nullptr;
		}
		if (smb_getfile(&smb, &file, file_detail_normal) != SMB_SUCCESS) {
			smb_close(&smb);
			return nullptr;
		}
		ret = get_filebase_attrs(sbbs, dir, &file);
		smb_freefilemem(&file);
		smb_close(&smb);
		return ret;
	}
	if (pm->real_patt)
		snprintf(ppath, sizeof(ppath), pm->real_patt, sbbs->cfg.data_dir, sbbs->useron.number);
	else
		ppath[0] = 0;
	ret = pm->get_attrs(sbbs, ppath);
	if (link && pm->link_patt) {
		if (asprintf(link, pm->link_patt, sbbs->useron.alias) == -1) {
			sftp_fattr_free(ret);
			ret = nullptr;
		}
	}
	return ret;
}

static sftp_file_attr_t
get_attrs(sbbs_t *sbbs, const char *path)
{
	return get_attrs(sbbs, path, nullptr);
}

static void
copy_path(char *p, size_t psz, const char *fp)
{
	char *last;

	strlcpy(p, fp, psz);
	last = strrchr(p, '/');
	if (last == nullptr) {
		return;
	}
	*last = 0;
}

static void
copy_path_from_dir(char *p, size_t psz, const char *fp)
{
	char *last;

	strlcpy(p, fp, psz);
	last = strrchr(p, '/');
	if (last == nullptr) {
		return;
	}
	if (last[1] == 0) {
		*last = 0;
		last = strrchr(p, '/');
		if (last == nullptr)
			return;
	}
	*last = 0;
}

static void
record_transfer(sbbs_t *sbbs, sftp_filedescriptor_t desc, bool upload)
{
	if (desc->dir == -1) {
		sbbs->llprintf(upload ? "U+" : "D-", "%sloaded %s (%" PRId64 " bytes)"
		         , upload ? "up" : "down", desc->local_path, flength(desc->local_path));
		return;
	}
	char *nptr = strrchr(desc->local_path, '/');
	if (nptr != nullptr) {
		file_t file{};
		nptr++;
		file.name = nptr;
		file.dir = desc->dir;
		file.size = flength(desc->local_path);
		file.file_idx.idx.offset = desc->idx_offset;
		file.file_idx.idx.number = desc->idx_number;
		if (upload)
			sbbs->uploadfile(&file);
		else
			sbbs->downloadedfile(&file);
		file.name = nullptr;
		// We shouldn't need to call this, but it doesn't hurt.
		smb_freefilemem(&file);
	}
}

extern "C" {

static bool
sftp_send(uint8_t *buf, size_t len, void *cb_data)
{
	sbbs_t *sbbs = (sbbs_t *)cb_data;
	size_t  sent = 0;
	int     i;
	int     status;

	if (sbbs->sftp_channel == -1)
		return false;
	while (sent < len) {
		pthread_mutex_lock(&sbbs->ssh_mutex);
		status = cryptSetAttribute(sbbs->ssh_session, CRYPT_SESSINFO_SSH_CHANNEL, sbbs->sftp_channel);
		if (cryptStatusError(status)) {
			pthread_mutex_unlock(&sbbs->ssh_mutex);
			return false;
		}
		size_t sendbytes = len - sent;
#define SENDBYTES_MAX 0x2000
		if (sendbytes > SENDBYTES_MAX)
			sendbytes = SENDBYTES_MAX;
		status = cryptSetAttribute(sbbs->ssh_session, CRYPT_OPTION_NET_WRITETIMEOUT, 5);
		if (cryptStatusError(status)) {
			pthread_mutex_unlock(&sbbs->ssh_mutex);
			return false;
		}
		status = cryptPushData(sbbs->ssh_session, (char*)buf + sent, sendbytes, &i);
		if (cryptStatusError(status)) {
			pthread_mutex_unlock(&sbbs->ssh_mutex);
			return false;
		}
		status = cryptFlushData(sbbs->ssh_session);
		if (cryptStatusError(status)) {
			pthread_mutex_unlock(&sbbs->ssh_mutex);
			return false;
		}
		status = cryptSetAttribute(sbbs->ssh_session, CRYPT_OPTION_NET_WRITETIMEOUT, 0);
		if (cryptStatusError(status)) {
			pthread_mutex_unlock(&sbbs->ssh_mutex);
			return false;
		}
		pthread_mutex_unlock(&sbbs->ssh_mutex);
		sent += i;
	}
	return true;
}

static void
sftp_lprint(void *arg, uint32_t errcode, const char *msg)
{
	sbbs_t *sbbs = (sbbs_t *)arg;
	int     level = LOG_DEBUG;

	switch (errcode) {
		case SSH_FX_PERMISSION_DENIED:
			level = LOG_INFO;
			break;
		case SSH_FX_FAILURE:
		case SSH_FX_BAD_MESSAGE:
			level = LOG_WARNING;
			break;
	}

	sbbs->lprintf(level, "SFTP error code %" PRIu32 " (%s) %s", errcode, sftp_get_errcode_name(errcode), msg);
}

static void
sftp_cleanup_callback(void *cb_data)
{
	sbbs_t *         sbbs = (sbbs_t *)cb_data;
	constexpr size_t nfdes = sizeof(sbbs->sftp_filedes) / sizeof(sbbs->sftp_filedes[0]);
	constexpr size_t nddes = sizeof(sbbs->sftp_dirdes) / sizeof(sbbs->sftp_dirdes[0]);

	for (unsigned i = 0; i < nfdes; i++) {
		if (sbbs->sftp_filedes[i] != nullptr) {
			close(sbbs->sftp_filedes[i]->fd);
			if (sbbs->sftp_filedes[i]->created && sbbs->sftp_filedes[i]->local_path) {
				// If we were uploading, delete the incomplete file
				remove(sbbs->sftp_filedes[i]->local_path);
			}
			free(sbbs->sftp_filedes[i]->local_path);
			free(sbbs->sftp_filedes[i]);
			sbbs->sftp_filedes[i] = nullptr;
		}
	}
	for (unsigned i = 0; i < nddes; i++) {
		free(sbbs->sftp_dirdes[i]);
		sbbs->sftp_dirdes[i] = nullptr;
	}
	free(sbbs->sftp_cwd);
}

static bool
sftp_open(sftp_str_t filename, uint32_t flags, sftp_file_attr_t attributes, void *cb_data)
{
	sbbs_t *         sbbs = (sbbs_t *)cb_data;
	constexpr size_t nfdes = sizeof(sbbs->sftp_filedes) / sizeof(sbbs->sftp_filedes[0]);
	unsigned         fdidx;
	mode_t           omode = 0;
	int              oflags = O_BINARY;
	sftp_str_t       handle;
	bool             ret;
	map_path_mode_t  mmode;

	sbbs->lprintf(LOG_DEBUG, "SFTP open(%.*s, %x, %s)", filename->len, filename->c_str, flags, sftp_attr_string(attributes).c_str());

	// See if there's an available file descriptor
	for (fdidx = 0; fdidx < nfdes; fdidx++) {
		if (sbbs->sftp_filedes[fdidx] == nullptr)
			break;
	}
	if (fdidx == nfdes) {
		return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Too many open file descriptors");
	}
	switch (flags & (SSH_FXF_READ | SSH_FXF_WRITE)) {
		case SSH_FXF_READ:
			oflags |= O_RDONLY;
			mmode = MAP_READ;
			break;
		case SSH_FXF_WRITE:
			oflags |= O_WRONLY;
			mmode = MAP_WRITE;
			break;
		case (SSH_FXF_READ | SSH_FXF_WRITE):
			oflags |= O_RDWR;
			mmode = MAP_RDWR;
			break;
		default:
			return sftps_send_error(sbbs->sftp_state, SSH_FX_OP_UNSUPPORTED, "Invalid flags (not read or write)");
	}
	if (flags & SSH_FXF_APPEND)
		oflags |= O_APPEND;
	if (flags & SSH_FXF_CREAT)
		oflags |= O_CREAT;
	if (flags & SSH_FXF_TRUNC)
		oflags |= O_TRUNC;
	if (flags & SSH_FXF_EXCL)
		oflags |= O_EXCL;
	path_map pmap(sbbs, filename->c_str, mmode);
	if (pmap.result() != MAP_TO_FILE)
		return pmap.cleanup();
	if (oflags & O_CREAT) {
		uint32_t perms;
		if (!sftp_fattr_get_permissions(attributes, &perms)) {
			omode = DEFFILEMODE;
		}
		else {
			if (perms & 0444) {
				omode |= S_IREAD;
			}
			if (perms & 0222) {
				omode |= S_IWRITE;
			}
			if (perms & ~(0666)) {
				return sftps_send_error(sbbs->sftp_state, SSH_FX_OP_UNSUPPORTED, "Invalid permissions");
			}
		}
		if (sftp_fattr_get_size(attributes, nullptr)) {
			return sftps_send_error(sbbs->sftp_state, SSH_FX_OP_UNSUPPORTED, "Specifying size in open not supported");
		}
		if (sftp_fattr_get_uid(attributes, nullptr)) {
			return sftps_send_error(sbbs->sftp_state, SSH_FX_OP_UNSUPPORTED, "Specifying uid/gid in open not supported");
		}
		if (sftp_fattr_get_atime(attributes, nullptr)) {
			return sftps_send_error(sbbs->sftp_state, SSH_FX_OP_UNSUPPORTED, "Specifying times in open not supported");
		}
		if (sftp_fattr_get_ext_count(attributes)) {
			return sftps_send_error(sbbs->sftp_state, SSH_FX_OP_UNSUPPORTED, "Specifying extended attributes in open not supported");
		}
	}
	sbbs->sftp_filedes[fdidx] = static_cast<sftp_filedescriptor_t>(calloc(1, sizeof(*sbbs->sftp_filedes[0])));
	if (sbbs->sftp_filedes[fdidx] == nullptr) {
		return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Unable to allocate file handle");
	}
	if (pmap.is_static())
		sbbs->sftp_filedes[fdidx]->dir = -1;
	else {
		sbbs->sftp_filedes[fdidx]->dir = pmap.info.filebase.dir;
		sbbs->sftp_filedes[fdidx]->idx_offset = pmap.info.filebase.offset;
		sbbs->sftp_filedes[fdidx]->idx_number = pmap.info.filebase.idx;
	}
	if (access(pmap.local_path, F_OK) != 0) {
		// File did not exist, and we're creating
		if (oflags & O_CREAT) {
			sbbs->sftp_filedes[fdidx]->created = true;
		}
	}
	if (sbbs->sftp_filedes[fdidx] == nullptr) {
		return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Unable to allocate file handle");
	}
	sbbs->sftp_filedes[fdidx]->local_path = strdup(pmap.local_path);
	if (sbbs->sftp_filedes[fdidx]->local_path == nullptr) {
		free(sbbs->sftp_filedes[fdidx]);
		return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Allocation failure");
	}
	sbbs->sftp_filedes[fdidx]->fd = open(pmap.local_path, oflags, omode);
	if (sbbs->sftp_filedes[fdidx]->fd == -1) {
		free(sbbs->sftp_filedes[fdidx]->local_path);
		free(sbbs->sftp_filedes[fdidx]);
		return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Operation failed");
	}
	handle = sftp_asprintf("%u", fdidx + 1);
	if (handle == nullptr) {
		close(sbbs->sftp_filedes[fdidx]->fd);
		free(sbbs->sftp_filedes[fdidx]->local_path);
		free(sbbs->sftp_filedes[fdidx]);
		return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Out of resources");
	}
	ret = sftps_send_handle(sbbs->sftp_state, handle);
	free_sftp_str(handle);
	return ret;
}

static bool
sftp_close(sftp_str_t handle, void *cb_data)
{
	sbbs_t *sbbs = (sbbs_t *)cb_data;

	sbbs->lprintf(LOG_DEBUG, "SFTP close(%.*s)", handle->len, handle->c_str);
	if (isdigit(handle->c_str[0])) {
		unsigned fidx = parse_file_handle(sbbs, handle);
		if (fidx == UINT_MAX) {
			return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Invalid file handle");
		}
		int      rval = close(sbbs->sftp_filedes[fidx]->fd);
		if (sbbs->sftp_filedes[fidx]->created)
			record_transfer(sbbs, sbbs->sftp_filedes[fidx], true);
		free(sbbs->sftp_filedes[fidx]->local_path);
		free(sbbs->sftp_filedes[fidx]);
		sbbs->sftp_filedes[fidx] = nullptr;
		if (rval)
			return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Close failed");
		else
			return sftps_send_error(sbbs->sftp_state, SSH_FX_OK, "Closed");
	}
	else {
		unsigned didx = parse_dir_handle(sbbs, handle);
		if (didx == UINT_MAX) {
			return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Invalid handle");
		}
		free(sbbs->sftp_dirdes[didx]);
		sbbs->sftp_dirdes[didx] = nullptr;
		return sftps_send_error(sbbs->sftp_state, SSH_FX_OK, "Closed");
	}
}

static bool
sftp_read(sftp_filehandle_t handle, uint64_t offset, uint32_t len, void *cb_data)
{
	sbbs_t * sbbs = (sbbs_t *)cb_data;
	unsigned fidx = parse_file_handle(sbbs, handle);
	ssize_t  rlen;

	sbbs->lprintf(LOG_DEBUG, "SFTP read(%.*s, %" PRIu64 ", %" PRIu32 ")", handle->len, handle->c_str, offset, len);
	if (fidx == UINT_MAX) {
		return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Invalid file handle");
	}
	int fd = sbbs->sftp_filedes[fidx]->fd;
	if (fd == -1) {
		return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Invalid file handle");
	}
	if (lseek(fd, offset, SEEK_SET) == -1) {
		return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Unable to seek to correct position");
	}
	sftp_str_t data = sftp_alloc_str(len);
	if (data == nullptr) {
		return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Unable to allocate buffer");
	}
	rlen = read(fd, data->c_str, len);
	if (rlen == 0) {
		// EOF
		free_sftp_str(data);
		return sftps_send_error(sbbs->sftp_state, SSH_FX_EOF, "End of file");
	}
	if (rlen == -1) {
		// Error
		free_sftp_str(data);
		return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Failed");
	}
	data->len = rlen;
	bool ret = sftps_send_data(sbbs->sftp_state, data);
	free_sftp_str(data);
	/*
	 * A successful transfer is defined as the last byte of the file
	 * being transmitted to the remote.
	 */
	uint8_t byte;
	if (read(fd, &byte, 1) == 0)
		record_transfer(sbbs, sbbs->sftp_filedes[fidx], false);

	return ret;
}

static bool
sftp_write(sftp_filehandle_t handle, uint64_t offset, sftp_str_t data, void *cb_data)
{
	sbbs_t * sbbs = (sbbs_t *)cb_data;
	unsigned fidx = parse_file_handle(sbbs, handle);
	ssize_t  rlen;

	sbbs->lprintf(LOG_DEBUG, "SFTP write(%.*s, %" PRIu64 ", %" PRIu32 ")", handle->len, handle->c_str, offset, data->len);
	if (data->len == 0) {
		return sftps_send_error(sbbs->sftp_state, SSH_FX_OK, "Nothing done, as requested");
	}
	if (fidx == UINT_MAX) {
		return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Invalid file handle");
	}
	int fd = sbbs->sftp_filedes[fidx]->fd;
	if (fd == -1) {
		return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Invalid file handle");
	}
	if (lseek(fd, offset, SEEK_SET) == -1) {
		return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Unable to seek to correct position");
	}
	rlen = write(fd, data->c_str, data->len);
	if (rlen == -1) {
		// Error
		return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Failed");
	}
	if (rlen != data->len) {
		return sftps_send_error(sbbs->sftp_state, SSH_FX_EOF, "Short write... I dunno.");
	}
	return sftps_send_error(sbbs->sftp_state, SSH_FX_OK, "Wrote");
}

static bool
sftp_realpath(sftp_str_t path, void *cb_data)
{
	sbbs_t *sbbs = (sbbs_t *)cb_data;
	char *  rp = sftp_parse_realpath(sbbs, path);
	sbbs->lprintf(LOG_DEBUG, "SFTP realpath(%.*s)", path->len, path->c_str);
	if (rp == nullptr) {
		return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "No idea where that is boss");
	}
	sftp_file_attr_t attr = dummy_attrs();
	if (attr == nullptr) {
		free(rp);
		return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Unable to allocate attribute");
	}
	bool ret = sftps_send_name(sbbs->sftp_state, 1, &rp, &rp, &attr);
	free(rp);
	sftp_fattr_free(attr);

	return ret;
}

static bool
sftp_opendir(sftp_str_t path, void *cb_data)
{
	sbbs_t *         sbbs = (sbbs_t *)cb_data;
	constexpr size_t nddes = sizeof(sbbs->sftp_dirdes) / sizeof(sbbs->sftp_dirdes[0]);
	unsigned         ddidx;
	sftp_str_t       h;

	sbbs->lprintf(LOG_DEBUG, "SFTP opendir(%.*s)", path->len, path->c_str);
	// See if there's an available file descriptor
	for (ddidx = 0; ddidx < nddes; ddidx++) {
		if (sbbs->sftp_dirdes[ddidx] == nullptr)
			break;
	}
	if (ddidx == nddes) {
		return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Too many open file descriptors");
	}
	path_map pmap(sbbs, path->c_str, MAP_READ);
	if (pmap.result() != MAP_TO_DIR)
		return pmap.cleanup();
	sbbs->sftp_dirdes[ddidx] = static_cast<sftp_dirdescriptor_t>(malloc(sizeof(*sbbs->sftp_dirdes[ddidx])));
	sbbs->sftp_dirdes[ddidx]->tree = pmap.tree();
	if (pmap.is_static()) {
		sbbs->sftp_dirdes[ddidx]->is_static = true;
		sbbs->sftp_dirdes[ddidx]->info.rootdir.mapping = pmap.info.rootdir.mapping;
		sbbs->sftp_dirdes[ddidx]->info.rootdir.idx = dot;
	}
	else {
		sbbs->sftp_dirdes[ddidx]->is_static = false;
		sbbs->sftp_dirdes[ddidx]->info.filebase.lib = pmap.info.filebase.lib;
		sbbs->sftp_dirdes[ddidx]->info.filebase.dir = pmap.info.filebase.dir;
		sbbs->sftp_dirdes[ddidx]->info.filebase.idx = dot;
	}
	h = sftp_asprintf("D:%u", ddidx + 1);
	if (h == nullptr) {
		return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Handle allocation failure");
	}
	bool ret = sftps_send_handle(sbbs->sftp_state, h);
	free_sftp_str(h);
	return ret;
}

// TODO: This is still too ugly... should be split into multiple functions.
static bool
sftp_readdir(sftp_dirhandle_t handle, void *cb_data)
{
	sbbs_t *             sbbs = (sbbs_t *)cb_data;
	unsigned             didx = parse_dir_handle(sbbs, handle);
	sftp_file_attr_t     attr;
	sftp_dirdescriptor_t dd;
	char                 tmppath[MAX_PATH + 1];
	char                 cwd[MAX_PATH + 1];
	char *               vpath;
	char *               lname;
	char *               ename;
	struct pathmap *     pm;
	file_names           fn(sbbs);

	sbbs->lprintf(LOG_DEBUG, "SFTP readdir(%.*s)", handle->len, handle->c_str);
	if (didx == UINT_MAX)
		return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Invalid handle");
	dd = sbbs->sftp_dirdes[didx];
	pm = static_cast<struct pathmap *>(dd->info.rootdir.mapping);
	if (dd->is_static) {
		char *link;

		if (dd->info.rootdir.idx == no_more_files) {
			if (fn.entries() > 0)
				return fn.send();
			return sftps_send_error(sbbs->sftp_state, SSH_FX_EOF, "No more files");
		}
		if (dd->info.rootdir.idx == dot) {
			const char *dir = ".";
			snprintf(tmppath, sizeof(tmppath), pm->sftp_patt, sbbs->useron.alias);
			remove_trailing_slash(tmppath);
			if (!fn.generic_dot_entry(strdup(dir), tmppath, dd->info.rootdir.idx))
				return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Unable to add dot dir");
		}
		if (dd->info.rootdir.idx == dotdot) {
			if (pm->sftp_patt[1]) {
				const char *dir = "..";
				snprintf(tmppath, sizeof(tmppath) - 3 /* for dir */, pm->sftp_patt, sbbs->useron.alias);
				tmppath[sizeof(tmppath) - 2] = 0;
				strcat(tmppath, dir);
				if (!fn.generic_dot_realpath_entry(strdup(dir), tmppath, dd->info.rootdir.idx))
					return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Unable to add dotdot dir");
			}
			else
				dd->info.rootdir.idx++;
		}
		if (dd->info.rootdir.idx == 0) {
			unsigned sf;
			for (sf = 0; sf < static_files_sz; sf++) {
				if (&static_files[sf] == pm) {
					dd->info.rootdir.idx = sf;
					break;
				}
			}
			if (sf == static_files_sz)
				return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Corrupt directory handle");
		}
		copy_path(cwd, sizeof(cwd), pm->sftp_patt);
		while (static_files[dd->info.rootdir.idx].sftp_patt != nullptr && fn.entries() < MAX_FILES_PER_READDIR) {
			dd->info.rootdir.idx++;
			if (static_cast<size_t>(dd->info.rootdir.idx) >= static_files_sz)
				break;
			if (static_files[dd->info.rootdir.idx].sftp_patt == nullptr) {
				dd->info.rootdir.idx = no_more_files;
				if (fn.entries() > 0)
					return fn.send();
				return sftps_send_error(sbbs->sftp_state, SSH_FX_EOF, "No more files");
			}
			copy_path_from_dir(tmppath, sizeof(tmppath), static_files[dd->info.rootdir.idx].sftp_patt);
			if (strcmp(cwd, tmppath))
				continue;
			if (static_files[dd->info.rootdir.idx].real_patt) {
				snprintf(tmppath, sizeof tmppath, static_files[dd->info.rootdir.idx].real_patt, sbbs->cfg.data_dir, sbbs->useron.number);
				if (access(tmppath, F_OK))
					continue;
			}
			snprintf(tmppath, sizeof tmppath, static_files[dd->info.rootdir.idx].sftp_patt, sbbs->useron.alias);
			remove_trailing_slash(tmppath);
			attr = get_attrs(sbbs, tmppath, &link);
			if (attr == nullptr) {
				free(link);
				return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Attributes allocation failure");
			}
			lname = get_longname(sbbs, tmppath, link, attr);
			free(link);
			if (lname == nullptr) {
				sftp_fattr_free(attr);
				return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Longname allocation failure");
			}
			vpath = getfname(tmppath);
			if (!fn.add_name(strdup(vpath), lname, attr))
				return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "adding static file");
		}
	}
	else {
		if (dd->info.filebase.lib == -1) {
			// /files/ (ie: list of libs)
			if (dd->info.filebase.idx == no_more_files) {
				if (fn.entries() > 0)
					return fn.send();
				return sftps_send_error(sbbs->sftp_state, SSH_FX_EOF, "No more files");
			}
			if (dd->info.filebase.idx == dot) {
				const char *dir = ".";
				switch (dd->tree) {
					case SFTP_DTREE_FULL:
						strcpy(tmppath, SLASH_FILES);
						break;
					case SFTP_DTREE_SHORT:
						strcpy(tmppath, SLASH_FLS);
						break;
					case SFTP_DTREE_VIRTUAL:
						strcpy(tmppath, SLASH_VFILES);
						break;
				}
				if (!fn.generic_dot_entry(strdup(dir), tmppath, dd->info.filebase.idx))
					return sftps_send_error(sbbs->sftp_state, SSH_FX_EOF, "Error adding topdoot");
			}
			if (dd->info.filebase.idx == dotdot) {
				const char *dir = "..";
				strcpy(tmppath, "/");
				if (!fn.generic_dot_entry(strdup(dir), tmppath, dd->info.filebase.idx))
					return sftps_send_error(sbbs->sftp_state, SSH_FX_EOF, "Error adding topdootdoot");
			}
			while (dd->info.filebase.idx < sbbs->cfg.total_libs && fn.entries() < MAX_FILES_PER_READDIR) {
				if (dd->info.filebase.idx >= sbbs->cfg.total_libs) {
					dd->info.filebase.idx = no_more_files;
					if (fn.entries() > 0)
						return fn.send();
					return sftps_send_error(sbbs->sftp_state, SSH_FX_EOF, "No more files");
				}
				if (!user_can_access_lib(&sbbs->cfg, dd->info.filebase.idx, &sbbs->useron, &sbbs->client)) {
					dd->info.filebase.idx++;
					continue;
				}
				attr = get_lib_attrs(sbbs, dd->info.filebase.idx);
				if (attr == nullptr)
					return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Attributes allocation failure");
				switch (dd->tree) {
					case SFTP_DTREE_FULL:
						ename = expand_slash(sbbs->cfg.lib[dd->info.filebase.idx]->lname);
						break;
					case SFTP_DTREE_SHORT:
						ename = expand_slash(sbbs->cfg.lib[dd->info.filebase.idx]->sname);
						break;
					case SFTP_DTREE_VIRTUAL:
						ename = expand_slash(sbbs->cfg.lib[dd->info.filebase.idx]->vdir);
						break;
					default:
						ename = nullptr;
						break;
				}
				if (ename == nullptr) {
					sftp_fattr_free(attr);
					return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Ename allocation failure");
				}
				lname = get_longname(sbbs, ename, nullptr, attr);
				if (lname == nullptr) {
					free(ename);
					sftp_fattr_free(attr);
					return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Longname allocation failure");
				}
				if (!fn.add_name(ename, lname, attr))
					return sftps_send_error(sbbs->sftp_state, SSH_FX_EOF, "Error adding lib");
				dd->info.filebase.idx++;
			}
		}
		else if (dd->info.filebase.dir == -1) {
			// /files/somelib (ie: list of dirs)
			if (dd->info.filebase.idx == no_more_files) {
				if (fn.entries() > 0)
					return fn.send();
				return sftps_send_error(sbbs->sftp_state, SSH_FX_EOF, "No more files");
			}
			if (dd->info.filebase.idx == dot) {
				const char *dir = ".";
				attr = get_lib_attrs(sbbs, dd->info.filebase.lib);
				if (!fn.generic_dot_attr_entry(strdup(dir), attr, nullptr, dd->info.filebase.idx))
					return sftps_send_error(sbbs->sftp_state, SSH_FX_EOF, "Adding libdoot");
			}
			if (dd->info.filebase.idx == dotdot) {
				const char *dir = "..";
				switch (dd->tree) {
					case SFTP_DTREE_FULL:
						strcpy(tmppath, SLASH_FILES);
						break;
					case SFTP_DTREE_SHORT:
						strcpy(tmppath, SLASH_FLS);
						break;
					case SFTP_DTREE_VIRTUAL:
						strcpy(tmppath, SLASH_VFILES);
						break;
				}
				if (!fn.generic_dot_entry(strdup(dir), tmppath, dd->info.filebase.idx))
					return sftps_send_error(sbbs->sftp_state, SSH_FX_EOF, "Adding libdootdoot");
			}
			while (dd->info.filebase.idx < sbbs->cfg.total_dirs && fn.entries() < MAX_FILES_PER_READDIR) {
				if (dd->info.filebase.idx >= sbbs->cfg.total_dirs) {
					dd->info.filebase.idx = no_more_files;
					if (fn.entries() > 0)
						return fn.send();
					return sftps_send_error(sbbs->sftp_state, SSH_FX_EOF, "No more files");
				}
				if (sbbs->cfg.dir[dd->info.filebase.idx]->lib != dd->info.filebase.lib) {
					dd->info.filebase.idx++;
					continue;
				}
				if (!user_can_access_dir(&sbbs->cfg, dd->info.filebase.idx, &sbbs->useron, &sbbs->client)) {
					dd->info.filebase.idx++;
					continue;
				}
				attr = get_dir_attrs(sbbs, dd->info.filebase.idx);
				if (attr == nullptr)
					return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Attributes allocation failure");
				switch (dd->tree) {
					case SFTP_DTREE_FULL:
						ename = expand_slash(sbbs->cfg.dir[dd->info.filebase.idx]->lname);
						break;
					case SFTP_DTREE_SHORT:
						ename = expand_slash(sbbs->cfg.dir[dd->info.filebase.idx]->sname);
						break;
					case SFTP_DTREE_VIRTUAL:
						ename = expand_slash(sbbs->cfg.dir[dd->info.filebase.idx]->vdir);
						break;
					default:
						sftp_fattr_free(attr);
						return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Invalid tree type");
				}
				if (ename == nullptr) {
					sftp_fattr_free(attr);
					return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "EName allocation failure");
				}
				lname = get_longname(sbbs, ename, nullptr, attr);
				if (lname == nullptr) {
					free(ename);
					sftp_fattr_free(attr);
					return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Longname allocation failure");
				}
				if (!fn.add_name(ename, lname, attr))
					return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Add dir name failure");
				dd->info.filebase.idx++;
			}
		}
		else {
			// /files/somelib/somedir (ie: list of files)
			if (dd->info.filebase.idx == no_more_files) {
				if (fn.entries() > 0)
					return fn.send();
				return sftps_send_error(sbbs->sftp_state, SSH_FX_EOF, "No more files");
			}
			if (dd->info.filebase.idx == dot) {
				const char *dir = ".";
				attr = get_dir_attrs(sbbs, dd->info.filebase.dir);
				if (!fn.generic_dot_attr_entry(strdup(dir), attr, nullptr, dd->info.filebase.idx))
					return sftps_send_error(sbbs->sftp_state, SSH_FX_EOF, "Adding dirdoot");
			}
			if (dd->info.filebase.idx == dotdot) {
				const char *dir = "..";
				attr = get_lib_attrs(sbbs, dd->info.filebase.lib);
				if (!fn.generic_dot_attr_entry(strdup(dir), attr, nullptr, dd->info.filebase.idx))
					return sftps_send_error(sbbs->sftp_state, SSH_FX_EOF, "Adding dirdootdoot");
			}
			// Find the "next"* file number.
			smb_t     smb{};
			idxrec_t  idx{};
			smbfile_t file{};
			if (smb_open_dir(&sbbs->cfg, &smb, dd->info.filebase.dir) != SMB_SUCCESS) {
				return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Can't open dir");
			}
			do  {
				if (dd->info.filebase.idx == 0) {
					if (smb_getfirstidx(&smb, &idx) != SMB_SUCCESS) {
						smb_close(&smb);
						dd->info.filebase.idx = no_more_files;
						if (fn.entries() > 0)
							return fn.send();
						return sftps_send_error(sbbs->sftp_state, SSH_FX_EOF, "No files at all");
					}
					file.hdr.number = idx.number;
				}
				else {
					file.hdr.number = dd->info.filebase.idx;
					if (smb_getmsgidx(&smb, &file) != SMB_SUCCESS) {
						smb_close(&smb);
						return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Can't find previous file in index");
					}
					file.hdr.number = 0;
					file.idx_offset++;
				}
				int result = smb_getmsgidx(&smb, &file);
				if (result == SMB_ERR_HDR_OFFSET) {
					smb_close(&smb);
					dd->info.filebase.idx = no_more_files;
					if (fn.entries() > 0)
						return fn.send();
					return sftps_send_error(sbbs->sftp_state, SSH_FX_EOF, "No more files");
				}
				if (result != SMB_SUCCESS) {
					smb_close(&smb);
					return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Can't find next file in index");
				}
				dd->info.filebase.idx = file.file_idx.idx.number;
				if (!(file.file_idx.idx.attr & MSG_FILE))
					continue;
				if (file.file_idx.idx.attr & (MSG_DELETE | MSG_PRIVATE))
					continue;
				if ((file.file_idx.idx.attr & (MSG_MODERATED | MSG_VALIDATED)) == MSG_MODERATED)
					continue;
				if (file.hdr.auxattr & MSG_NODISP)
					continue;
				if (smb_getfile(&smb, &file, file_detail_normal) != SMB_SUCCESS) {
					smb_close(&smb);
					return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Can't get file header");
				}
				attr = get_filebase_attrs(sbbs, dd->info.filebase.dir, &file);
				if (attr == nullptr) {
					smb_freefilemem(&file);
					smb_close(&smb);
					return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Can't get file attributes");
				}
				strcpy(tmppath, file.name);
				snprintf(cwd, sizeof cwd, "%s/%s", sbbs->cfg.dir[dd->info.filebase.dir]->path, file.name);
				smb_freefilemem(&file);
				if (access(cwd, R_OK)) {
					sftp_fattr_free(attr);
					continue;
				}
				char *lname = get_longname(sbbs, cwd, nullptr, attr);
				if (lname == nullptr) {
					sftp_fattr_free(attr);
					smb_close(&smb);
					return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Can't get file header");
				}
				if (!fn.add_name(strdup(tmppath), lname, attr)) {
					smb_close(&smb);
					return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Can't get file header");
				}
			} while (fn.entries() < MAX_FILES_PER_READDIR);
			smb_close(&smb);
		}
	}
	if (fn.entries() > 0)
		return fn.send();
	return sftps_send_error(sbbs->sftp_state, SSH_FX_EOF, "No more files");
}

static bool
sftp_stat(sftp_str_t path, void *cb_data)
{
	sbbs_t *                  sbbs = (sbbs_t *)cb_data;
	unsigned                  lcnt = 0;

	sbbs->lprintf(LOG_DEBUG, "SFTP stat(%.*s)", path->len, path->c_str);

	std::unique_ptr<path_map> cpmap(new path_map(sbbs, path->c_str, MAP_STAT));

	if (!cpmap->success())
		return cpmap->cleanup();
	while (cpmap->sftp_link_target != nullptr) {
		lcnt++;
		if (lcnt > 50) {
			return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Too many symbolic links");
		}
		std::unique_ptr<path_map> newpmap(new path_map(sbbs, cpmap->sftp_link_target, MAP_STAT));
		if (!newpmap->success())
			return newpmap->cleanup();
		cpmap = std::move(newpmap);
	}
	sftp_file_attr_t attr = get_attrs(sbbs, cpmap->sftp_path);
	if (attr == nullptr)
		return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Unable to allocate attribute");
	bool             ret = sftps_send_attrs(sbbs->sftp_state, attr);
	sftp_fattr_free(attr);

	return ret;
}

static bool
sftp_lstat(sftp_str_t path, void *cb_data)
{
	sbbs_t *         sbbs = (sbbs_t *)cb_data;
	sbbs->lprintf(LOG_DEBUG, "SFTP lstat(%.*s)", path->len, path->c_str);
	path_map         pmap(sbbs, path->c_str, MAP_STAT);
	if (!pmap.success())
		return pmap.cleanup();
	sftp_file_attr_t attr = get_attrs(sbbs, pmap.sftp_path);
	if (attr == nullptr)
		return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Unable to allocate attribute");
	bool             ret = sftps_send_attrs(sbbs->sftp_state, attr);
	sftp_fattr_free(attr);

	return ret;
}

static bool
sftp_readlink(sftp_str_t path, void *cb_data)
{
	sbbs_t *         sbbs = (sbbs_t *)cb_data;
	sbbs->lprintf(LOG_DEBUG, "SFTP readlink(%.*s)", path->len, path->c_str);
	path_map         pmap(sbbs, path->c_str, MAP_STAT);
	if (pmap.result() != MAP_TO_SYMLINK)
		return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Not a symlink");
	sftp_file_attr_t attr = dummy_attrs();
	if (attr == nullptr)
		return sftps_send_error(sbbs->sftp_state, SSH_FX_FAILURE, "Unable to allocate attribute");
	bool             ret = sftps_send_name(sbbs->sftp_state, 1, &pmap.sftp_link_target, &pmap.sftp_link_target, &attr);
	sftp_fattr_free(attr);

	return ret;
}

#if NOTYET
static bool
sftp_fstat(sftp_filehandle_t handle, void *cb_data)
{
	sbbs_t *sbbs = (sbbs_t *)cb_data;
	return true;
}

static bool
sftp_remove(sftp_str_t filename, void *cb_data)
{
	sbbs_t *sbbs = (sbbs_t *)cb_data;
	return true;
}

static bool
sftp_rename(sftp_str_t oldpath, sftp_str_t newpath, void *cb_data)
{
	sbbs_t *sbbs = (sbbs_t *)cb_data;
	return true;
}

static bool
sftp_extended(sftp_str_t request, sftp_rx_pkt_t pkt, void *cb_data)
{
	sbbs_t *sbbs = (sbbs_t *)cb_data;
	return true;
}

static bool
sftp_setstat(sftp_str_t path, sftp_file_attr_t attributes, void *cb_data)
{
	sbbs_t *sbbs = (sbbs_t *)cb_data;
	return true;
}

static bool
sftp_fsetstat(sftp_filehandle_t handle, sftp_file_attr_t attributes, void *cb_data)
{
	sbbs_t *sbbs = (sbbs_t *)cb_data;
	return true;
}

static bool
sftp_mkdir(sftp_str_t path, sftp_file_attr_t attributes, void *cb_data)
{
	sbbs_t *sbbs = (sbbs_t *)cb_data;
	return true;
}

static bool
sftp_rmdir(sftp_str_t path, void *cb_data)
{
	sbbs_t *sbbs = (sbbs_t *)cb_data;
	return true;
}

static bool
sftp_symlink(sftp_str_t linkpath, sftp_str_t targetpath, void *cb_data)
{
	sbbs_t *sbbs = (sbbs_t *)cb_data;
	return true;
}
#endif

}

bool
sbbs_t::init_sftp(int cid)
{
	if (sftp_state != nullptr)
		return true;
	sftp_state = sftps_begin(sftp_send, this);
	if (sftp_state != nullptr) {
		sftp_state->lprint = sftp_lprint;
		sftp_state->cleanup_callback = sftp_cleanup_callback;
		sftp_state->realpath = sftp_realpath;
		sftp_state->open = sftp_open;
		sftp_state->close = sftp_close;
		sftp_state->read = sftp_read;
		sftp_state->write = sftp_write;
		sftp_state->opendir = sftp_opendir;
		sftp_state->readdir = sftp_readdir;
		sftp_state->stat = sftp_stat;
		sftp_state->lstat = sftp_lstat;
		sftp_state->readlink = sftp_readlink;
		sftp_channel = cid;
		lprintf(LOG_INFO, "SFTP initialized on channel %d", cid);
		return true;
	}
	return false;
}

bool
sbbs_t::sftp_end(void)
{
	if (sftp_state)
		sftps_end(sftp_state);
	sftp_state = nullptr;
	return true;
}
