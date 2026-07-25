/*
 * Linux registry shim: persist RegistryClass data in renegade.conf (INI format).
 * Runtime/debug keys go to renegade.state so user-edited renegade.conf is not rewritten on exit.
 * Override path with RENEGADE_CONF or legacy RENEGADE_REGISTRY_INI.
 */
#include "winreg.h"
#include "winerror.h"

#include "linux_registry_alloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>

#ifndef ERROR_NO_MORE_ITEMS
#define ERROR_NO_MORE_ITEMS 259L
#endif
#ifndef ERROR_FILE_NOT_FOUND
#define ERROR_FILE_NOT_FOUND 2L
#endif
#ifndef ERROR_INVALID_HANDLE
#define ERROR_INVALID_HANDLE 6L
#endif
#ifndef ERROR_MORE_DATA
#define ERROR_MORE_DATA 234L
#endif
#ifndef ERROR_INVALID_PARAMETER
#define ERROR_INVALID_PARAMETER 87L
#endif
#ifndef ERROR_OUTOFMEMORY
#define ERROR_OUTOFMEMORY 14L
#endif

struct LinuxRegValue {
	DWORD type;
	RegBytes data;
};

struct LinuxRegSection {
	RegMap<RegString, LinuxRegValue> values;
};

/*
 * Lazy singletons: cRegistryBool globals may call Reg* during static init
 * before other translation-unit globals are constructed.
 */
static RegMap<RegString, LinuxRegSection> &linux_registry_sections(void)
{
	return linux_reg_lazy_singleton<RegMap<RegString, LinuxRegSection>>();
}

static RegMap<HKEY, RegString> &linux_registry_open_keys(void)
{
	return linux_reg_lazy_singleton<RegMap<HKEY, RegString>>();
}

static uintptr_t &linux_registry_next_handle(void)
{
	static uintptr_t *next = nullptr;
	if (next == nullptr) {
		void *mem = std::malloc(sizeof(uintptr_t));
		if (mem == nullptr) {
			throw std::bad_alloc();
		}
		next = new (mem) uintptr_t(1);
	}
	return *next;
}

static bool &linux_registry_loaded_flag(void)
{
	return linux_reg_lazy_singleton<bool>();
}

static bool &linux_registry_conf_dirty_flag(void)
{
	static bool *dirty = nullptr;
	if (dirty == nullptr) {
		void *mem = std::malloc(sizeof(bool));
		if (mem == nullptr) {
			throw std::bad_alloc();
		}
		dirty = new (mem) bool(false);
	}
	return *dirty;
}

static bool &linux_registry_state_dirty_flag(void)
{
	static bool *dirty = nullptr;
	if (dirty == nullptr) {
		void *mem = std::malloc(sizeof(bool));
		if (mem == nullptr) {
			throw std::bad_alloc();
		}
		dirty = new (mem) bool(false);
	}
	return *dirty;
}

static RegMap<RegString, LinuxRegSection> &linux_registry_conf_snapshot(void)
{
	return linux_reg_lazy_singleton<RegMap<RegString, LinuxRegSection>>();
}

static bool linux_registry_section_is_state(const RegString &section)
{
	const size_t debug_suffix = 6;
	if (section.size() >= debug_suffix &&
		section.compare(section.size() - debug_suffix, debug_suffix, "/Debug") == 0) {
		return true;
	}
	return section.find("/Debug/") != RegString::npos;
}

static void linux_registry_mark_section_dirty(const RegString &section)
{
	if (linux_registry_section_is_state(section)) {
		linux_registry_state_dirty_flag() = true;
	} else {
		linux_registry_conf_dirty_flag() = true;
	}
}

static bool linux_registry_sections_equal(
	const RegMap<RegString, LinuxRegValue> &a, const RegMap<RegString, LinuxRegValue> &b)
{
	if (a.size() != b.size()) {
		return false;
	}
	for (const auto &it : a) {
		const auto found = b.find(it.first);
		if (found == b.end() || found->second.type != it.second.type || found->second.data != it.second.data) {
			return false;
		}
	}
	return true;
}

static void linux_registry_update_conf_snapshot(void)
{
	linux_registry_conf_snapshot().clear();
	for (const auto &section_it : linux_registry_sections()) {
		if (!linux_registry_section_is_state(section_it.first)) {
			linux_registry_conf_snapshot()[section_it.first] = section_it.second;
		}
	}
}

static bool linux_registry_conf_matches_snapshot(void)
{
	for (const auto &section_it : linux_registry_conf_snapshot()) {
		const auto found = linux_registry_sections().find(section_it.first);
		if (found == linux_registry_sections().end() ||
			!linux_registry_sections_equal(section_it.second.values, found->second.values)) {
			return false;
		}
	}
	for (const auto &section_it : linux_registry_sections()) {
		if (linux_registry_section_is_state(section_it.first)) {
			continue;
		}
		if (linux_registry_conf_snapshot().find(section_it.first) == linux_registry_conf_snapshot().end()) {
			return false;
		}
	}
	return true;
}

static void linux_registry_normalize_path(const char *in, char *out, size_t out_cap)
{
	if (out == NULL || out_cap == 0) {
		return;
	}
	out[0] = '\0';
	if (in == NULL) {
		return;
	}
	size_t j = 0;
	for (const char *p = in; *p != '\0' && j + 1 < out_cap; ++p) {
		out[j++] = (*p == '\\') ? '/' : *p;
	}
	out[j] = '\0';
}

static RegString linux_registry_make_path(const char *normalized)
{
	if (normalized == NULL || normalized[0] == '\0') {
		return RegString();
	}
	return RegString(normalized);
}

static const char *linux_registry_default_path(void)
{
	return "generals.conf";
}

static const char *linux_registry_legacy_path(void)
{
	return "renegade.conf";
}

static bool linux_registry_path_has_payload(const char *path)
{
	FILE *fp = fopen(path, "r");
	if (fp == NULL) {
		return false;
	}
	int c = 0;
	bool found = false;
	while ((c = fgetc(fp)) != EOF) {
		if (c != ' ' && c != '\t' && c != '\r' && c != '\n' && c != ';' && c != '#') {
			found = true;
			break;
		}
	}
	fclose(fp);
	return found;
}

static void linux_registry_normalize_section_name(RegString &section)
{
	if (section == "Render") {
		section = "Software/Westwood/Renegade/Render";
	}
}

static void linux_registry_get_exe_dir(char *exe_dir, size_t exe_dir_size)
{
	exe_dir[0] = '\0';
	char exe_path[1024];
	const ssize_t exe_len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
	if (exe_len <= 0) {
		return;
	}
	exe_path[exe_len] = '\0';
	char *slash = strrchr(exe_path, '/');
	if (slash != NULL) {
		*slash = '\0';
		strncpy(exe_dir, exe_path, exe_dir_size - 1);
		exe_dir[exe_dir_size - 1] = '\0';
	}
}

static bool linux_registry_resolve_path(const char *candidate, char *resolved, size_t resolved_size)
{
	char normalized[PATH_MAX];
	strncpy(normalized, candidate, sizeof(normalized) - 1);
	normalized[sizeof(normalized) - 1] = '\0';
	if (realpath(normalized, resolved) != NULL) {
		resolved[resolved_size - 1] = '\0';
		return true;
	}
	strncpy(resolved, candidate, resolved_size - 1);
	resolved[resolved_size - 1] = '\0';
	return false;
}

static bool linux_registry_pick_conf_path(char *path, size_t path_size, bool for_write)
{
	const char *from_env = getenv("RENEGADE_CONF");
	if (from_env == NULL || from_env[0] == '\0') {
		from_env = getenv("RENEGADE_REGISTRY_INI");
	}
	if (from_env != NULL && from_env[0] != '\0') {
		linux_registry_resolve_path(from_env, path, path_size);
		return true;
	}

	char exe_dir[1024];
	linux_registry_get_exe_dir(exe_dir, sizeof(exe_dir));

	static const char *exe_relative_paths[] = {
		"../../../game/generals.conf",
		"../../game/generals.conf",
		"../game/generals.conf",
		"generals.conf",
		"../../../game/renegade.conf",
		"../../game/renegade.conf",
		"../game/renegade.conf",
		"renegade.conf",
		NULL,
	};

	if (exe_dir[0] != '\0') {
		for (int i = 0; exe_relative_paths[i] != NULL; ++i) {
			char candidate[PATH_MAX];
			snprintf(candidate, sizeof(candidate), "%s/%s", exe_dir, exe_relative_paths[i]);
			char resolved[PATH_MAX];
			linux_registry_resolve_path(candidate, resolved, sizeof(resolved));
			if (linux_registry_path_has_payload(resolved)) {
				strncpy(path, resolved, path_size - 1);
				path[path_size - 1] = '\0';
				return true;
			}
		}

		if (for_write) {
			for (int i = 0; exe_relative_paths[i] != NULL; ++i) {
				char candidate[PATH_MAX];
				snprintf(candidate, sizeof(candidate), "%s/%s", exe_dir, exe_relative_paths[i]);
				char resolved[PATH_MAX];
				if (linux_registry_resolve_path(candidate, resolved, sizeof(resolved))) {
					strncpy(path, resolved, path_size - 1);
					path[path_size - 1] = '\0';
					return true;
				}
			}
		}
	}

	if (linux_registry_path_has_payload(linux_registry_default_path())) {
		strncpy(path, linux_registry_default_path(), path_size - 1);
		path[path_size - 1] = '\0';
		return true;
	}

	if (for_write && exe_dir[0] != '\0') {
		char candidate[PATH_MAX];
		snprintf(candidate, sizeof(candidate), "%s/../../game/%s", exe_dir, linux_registry_default_path());
		char resolved[PATH_MAX];
		if (linux_registry_resolve_path(candidate, resolved, sizeof(resolved))) {
			strncpy(path, resolved, path_size - 1);
			path[path_size - 1] = '\0';
			return true;
		}
		snprintf(candidate, sizeof(candidate), "%s/../game/%s", exe_dir, linux_registry_default_path());
		if (linux_registry_resolve_path(candidate, resolved, sizeof(resolved))) {
			strncpy(path, resolved, path_size - 1);
			path[path_size - 1] = '\0';
			return true;
		}
	}

	strncpy(path, linux_registry_default_path(), path_size - 1);
	path[path_size - 1] = '\0';
	return false;
}

static const char *linux_registry_file_path(void)
{
	static char path[1024];
	linux_registry_pick_conf_path(path, sizeof(path), true);
	return path;
}

static const char *linux_registry_state_file_path(void)
{
	static char path[1024];
	const char *conf = linux_registry_file_path();
	const size_t len = strlen(conf);
	if (len >= 5 && strcmp(conf + len - 5, ".conf") == 0) {
		snprintf(path, sizeof(path), "%.*s.state", (int)(len - 5), conf);
	} else {
		snprintf(path, sizeof(path), "%s.state", conf);
	}
	return path;
}

static void linux_registry_trim(RegString &s)
{
	while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ' || s.back() == '\t')) {
		s.pop_back();
	}
	size_t start = 0;
	while (start < s.size() && (s[start] == ' ' || s[start] == '\t')) {
		++start;
	}
	if (start > 0) {
		s.erase(0, start);
	}
}

static bool linux_registry_section_is_render(const RegString &section)
{
	const char *suffix = "/Render";
	const size_t suffix_len = strlen(suffix);
	return section.size() >= suffix_len &&
		section.compare(section.size() - suffix_len, suffix_len, suffix) == 0;
}

static void linux_registry_map_render_setting_key(RegString &name, DWORD &type)
{
	struct Entry {
		const char *alias;
		const char *canonical;
		bool dword;
	};
	static const Entry entries[] = {
		{"Width", "RenderDeviceWidth", true},
		{"Height", "RenderDeviceHeight", true},
		{"Depth", "RenderDeviceDepth", true},
		{"Windowed", "RenderDeviceWindowed", true},
		{"TextureDepth", "RenderDeviceTextureDepth", true},
		{"Device", "RenderDeviceName", false},
		{"RenderDevice", "RenderDeviceName", false},
	};
	for (const Entry &entry : entries) {
		if (name == entry.alias) {
			name = entry.canonical;
			if (entry.dword) {
				type = REG_DWORD;
			}
			return;
		}
	}
}

static bool linux_registry_try_parse_dword(const RegString &value, DWORD &dword)
{
	char *end = NULL;
	const unsigned long parsed = strtoul(value.c_str(), &end, 10);
	if (end == value.c_str() || (end != NULL && *end != '\0')) {
		return false;
	}
	dword = (DWORD)parsed;
	return true;
}
static bool linux_registry_store_value(
	const RegString &section, const RegString &name, DWORD type, const RegBytes &data)
{
	LinuxRegValue &val = linux_registry_sections()[section].values[name];
	if (val.type == type && val.data == data) {
		return false;
	}
	val.type = type;
	val.data = data;
	return true;
}

static bool linux_registry_load_file(const char *path)
{
	FILE *fp = fopen(path, "r");
	if (fp == NULL) {
		return false;
	}

	RegString section;
	char line[4096];
	while (fgets(line, sizeof(line), fp) != NULL) {
		RegString raw(line);
		linux_registry_trim(raw);
		if (raw.empty() || raw[0] == ';' || raw[0] == '#') {
			continue;
		}
		if (raw[0] == '[') {
			const size_t end = raw.find(']');
			if (end != RegString::npos && end > 1) {
				RegString section_name = raw.substr(1, end - 1);
				char normalized[512];
				linux_registry_normalize_path(section_name.c_str(), normalized, sizeof(normalized));
				section = normalized;
				linux_registry_normalize_section_name(section);
			}
			continue;
		}
		if (section.empty()) {
			continue;
		}

		const size_t eq = raw.find('=');
		if (eq == RegString::npos) {
			continue;
		}

		RegString key = raw.substr(0, eq);
		RegString value = raw.substr(eq + 1);
		linux_registry_trim(key);
		linux_registry_trim(value);

		DWORD type = REG_SZ;
		RegString name = key;
		if (strncmp(key.c_str(), "STRING_", 7) == 0) {
			name = key.substr(7);
			type = REG_SZ;
		} else if (strncmp(key.c_str(), "DWORD_", 6) == 0) {
			name = key.substr(6);
			type = REG_DWORD;
		} else if (strncmp(key.c_str(), "BIN_", 4) == 0) {
			continue;
		}

		if (linux_registry_section_is_render(section)) {
			linux_registry_map_render_setting_key(name, type);
		}

		RegBytes bytes;
		if (type == REG_DWORD) {
			DWORD dword = 0;
			if (!linux_registry_try_parse_dword(value, dword)) {
				continue;
			}
			const unsigned char *src = (const unsigned char *)&dword;
			bytes.assign(src, src + sizeof(dword));
		} else if (linux_registry_section_is_render(section) && name != "RenderDeviceName") {
			DWORD dword = 0;
			if (linux_registry_try_parse_dword(value, dword)) {
				type = REG_DWORD;
				const unsigned char *src = (const unsigned char *)&dword;
				bytes.assign(src, src + sizeof(dword));
			} else {
				bytes.assign(value.begin(), value.end());
				bytes.push_back('\0');
			}
		} else {
			bytes.assign(value.begin(), value.end());
			bytes.push_back('\0');
		}
		linux_registry_store_value(section, name, type, bytes);
	}

	fclose(fp);
	return true;
}

static void linux_registry_load(void)
{
	if (linux_registry_loaded_flag()) {
		return;
	}
	linux_registry_loaded_flag() = true;

	char path[1024];
	const bool path_exists = linux_registry_pick_conf_path(path, sizeof(path), false);
	bool loaded = path_exists && linux_registry_load_file(path);
	if (!loaded) {
		const char *legacy = linux_registry_legacy_path();
		if (strcmp(path, legacy) != 0 && linux_registry_load_file(legacy)) {
			linux_registry_conf_dirty_flag() = true;
			loaded = true;
		}
	}

	linux_registry_load_file(linux_registry_state_file_path());
	linux_registry_update_conf_snapshot();
}

static bool linux_path_has_ini_big(const char *dir)
{
	if (dir == NULL || dir[0] == '\0') {
		return false;
	}
	char candidate[PATH_MAX];
	snprintf(candidate, sizeof(candidate), "%s/INI.big", dir);
	struct stat st;
	return stat(candidate, &st) == 0 && S_ISREG(st.st_mode);
}

bool Linux_FindGameInstallDirectory(char *path, size_t path_size)
{
	if (path == NULL || path_size == 0) {
		return false;
	}

	const char *from_env = getenv("GENERALS_GAME_DIR");
	if (from_env == NULL || from_env[0] == '\0') {
		from_env = getenv("RENEGADE_GAME_DIR");
	}
	if (from_env != NULL && from_env[0] != '\0') {
		char resolved[PATH_MAX];
		linux_registry_resolve_path(from_env, resolved, sizeof(resolved));
		if (linux_path_has_ini_big(resolved)) {
			strncpy(path, resolved, path_size - 1);
			path[path_size - 1] = '\0';
			return true;
		}
	}

	char exe_dir[1024];
	linux_registry_get_exe_dir(exe_dir, sizeof(exe_dir));

	static const char *relative_game_dirs[] = {
		"../../../game",
		"../../game",
		"../game",
		".",
		NULL,
	};

	for (int i = 0; relative_game_dirs[i] != NULL; ++i) {
		char candidate[PATH_MAX];
		if (exe_dir[0] != '\0') {
			snprintf(candidate, sizeof(candidate), "%s/%s", exe_dir, relative_game_dirs[i]);
		} else {
			strncpy(candidate, relative_game_dirs[i], sizeof(candidate) - 1);
			candidate[sizeof(candidate) - 1] = '\0';
		}

		char resolved[PATH_MAX];
		linux_registry_resolve_path(candidate, resolved, sizeof(resolved));
		if (linux_path_has_ini_big(resolved)) {
			strncpy(path, resolved, path_size - 1);
			path[path_size - 1] = '\0';
			return true;
		}
	}

	return false;
}

void Linux_Registry_Reload_For_Working_Directory(void)
{
	linux_registry_open_keys().clear();
	linux_registry_sections().clear();
	linux_registry_conf_snapshot().clear();
	linux_registry_loaded_flag() = false;
	linux_registry_conf_dirty_flag() = false;
	linux_registry_state_dirty_flag() = false;
	linux_registry_load();
}

static void linux_registry_write_sections(FILE *fp, bool state_only)
{
	for (const auto &section_it : linux_registry_sections()) {
		if (linux_registry_section_is_state(section_it.first) != state_only) {
			continue;
		}
		if (section_it.second.values.empty()) {
			continue;
		}
		fprintf(fp, "[%s]\n", section_it.first.c_str());
		if (!state_only && linux_registry_section_is_render(section_it.first)) {
			fprintf(fp, "; Screen resolution: RenderDeviceWidth x RenderDeviceHeight\n");
		}
		for (const auto &value_it : section_it.second.values) {
			const LinuxRegValue &val = value_it.second;
			if (val.type == REG_DWORD && val.data.size() >= sizeof(DWORD)) {
				DWORD dword = 0;
				memcpy(&dword, val.data.data(), sizeof(dword));
				fprintf(fp, "DWORD_%s=%lu\n", value_it.first.c_str(), (unsigned long)dword);
			} else if (val.type == REG_SZ && !val.data.empty()) {
				fprintf(fp, "STRING_%s=%s\n", value_it.first.c_str(), (const char *)val.data.data());
			}
		}
		fprintf(fp, "\n");
	}
}

static void linux_registry_save_conf(void)
{
	if (!linux_registry_conf_dirty_flag()) {
		return;
	}
	if (linux_registry_conf_matches_snapshot()) {
		linux_registry_conf_dirty_flag() = false;
		return;
	}

	FILE *fp = fopen(linux_registry_file_path(), "w");
	if (fp == NULL) {
		return;
	}
	linux_registry_write_sections(fp, false);
	fclose(fp);
	linux_registry_update_conf_snapshot();
	linux_registry_conf_dirty_flag() = false;
}

static void linux_registry_save_state(void)
{
	if (!linux_registry_state_dirty_flag()) {
		return;
	}

	FILE *fp = fopen(linux_registry_state_file_path(), "w");
	if (fp == NULL) {
		return;
	}
	linux_registry_write_sections(fp, true);
	fclose(fp);
	linux_registry_state_dirty_flag() = false;
}

static void linux_registry_save(void)
{
	linux_registry_save_conf();
	linux_registry_save_state();
}

static RegString linux_registry_full_path(HKEY root, LPCSTR sub)
{
	char buf[512];
	buf[0] = '\0';
	if (sub != NULL && sub[0] != '\0') {
		linux_registry_normalize_path(sub, buf, sizeof(buf));
	}
	(void)root;
	return linux_registry_make_path(buf);
}

static HKEY linux_registry_open_handle(const RegString &path, bool create)
{
	linux_registry_load();

	if (!create && linux_registry_sections().find(path) == linux_registry_sections().end()) {
		return NULL;
	}

	if (create) {
		(void)linux_registry_sections()[path];
	}

	const HKEY handle = (HKEY)(uintptr_t)linux_registry_next_handle()++;
	linux_registry_open_keys()[handle] = path;
	return handle;
}

static LinuxRegSection *linux_registry_section_for(HKEY key)
{
	const auto it = linux_registry_open_keys().find(key);
	if (it == linux_registry_open_keys().end()) {
		return NULL;
	}
	return &linux_registry_sections()[it->second];
}

extern "C" {

LONG RegOpenKeyExA(HKEY root, LPCSTR sub, DWORD opts, DWORD access, HKEY *out)
{
	(void)opts;
	(void)access;
	if (out == NULL) {
		return ERROR_INVALID_PARAMETER;
	}

	linux_registry_load();
	const RegString path = linux_registry_full_path(root, sub);
	HKEY handle = linux_registry_open_handle(path, false);
	if (handle == NULL) {
		*out = NULL;
		return ERROR_FILE_NOT_FOUND;
	}

	*out = handle;
	return ERROR_SUCCESS;
}

LONG RegCreateKeyExA(
	HKEY root, LPCSTR sub, DWORD reserved, LPSTR cls, DWORD opts, DWORD access,
	LPVOID security, HKEY *out, DWORD *disp)
{
	(void)reserved;
	(void)cls;
	(void)opts;
	(void)access;
	(void)security;
	if (out == NULL) {
		return ERROR_INVALID_PARAMETER;
	}

	linux_registry_load();
	const RegString path = linux_registry_full_path(root, sub);
	const bool existed = linux_registry_sections().find(path) != linux_registry_sections().end();
	HKEY handle = linux_registry_open_handle(path, true);
	if (handle == NULL) {
		return ERROR_OUTOFMEMORY;
	}

	if (disp != NULL) {
		*disp = existed ? 2u : 1u;
	}
	*out = handle;
	return ERROR_SUCCESS;
}

LONG RegEnumKeyExA(HKEY, DWORD, LPSTR, LPDWORD, LPDWORD, LPSTR, LPDWORD, void *)
{
	return ERROR_NO_MORE_ITEMS;
}

LONG RegEnumValueA(
	HKEY key, DWORD index, LPSTR name, LPDWORD name_len, LPDWORD reserved,
	LPDWORD type, LPBYTE data, LPDWORD data_len)
{
	(void)reserved;
	LinuxRegSection *section = linux_registry_section_for(key);
	if (section == NULL) {
		return ERROR_INVALID_HANDLE;
	}
	if (index >= section->values.size()) {
		return ERROR_NO_MORE_ITEMS;
	}

	size_t i = 0;
	for (const auto &it : section->values) {
		if (i++ != index) {
			continue;
		}

		if (name != NULL && name_len != NULL && *name_len > 0) {
			strncpy(name, it.first.c_str(), *name_len);
			name[*name_len - 1] = '\0';
			*name_len = (DWORD)strlen(name);
		}
		if (type != NULL) {
			*type = it.second.type;
		}
		if (data != NULL && data_len != NULL) {
			const size_t to_copy = (it.second.data.size() < *data_len) ? it.second.data.size() : *data_len;
			if (to_copy > 0) {
				memcpy(data, it.second.data.data(), to_copy);
			}
			*data_len = (DWORD)it.second.data.size();
		}
		return ERROR_SUCCESS;
	}

	return ERROR_NO_MORE_ITEMS;
}

LONG RegQueryInfoKeyA(
	HKEY key, LPSTR cls, LPDWORD cls_len, LPDWORD reserved, LPDWORD subkeys,
	LPDWORD max_sub, LPDWORD max_cls, LPDWORD values, LPDWORD max_val, LPDWORD max_data,
	LPDWORD sec, void *mtime)
{
	(void)cls;
	(void)cls_len;
	(void)reserved;
	(void)subkeys;
	(void)max_sub;
	(void)max_cls;
	(void)sec;
	(void)mtime;

	LinuxRegSection *section = linux_registry_section_for(key);
	if (section == NULL) {
		return ERROR_INVALID_HANDLE;
	}

	DWORD count = 0;
	DWORD max_name = 0;
	DWORD max_size = 0;
	for (const auto &it : section->values) {
		++count;
		const DWORD name_len = (DWORD)it.first.size();
		const DWORD data_len = (DWORD)it.second.data.size();
		if (name_len > max_name) {
			max_name = name_len;
		}
		if (data_len > max_size) {
			max_size = data_len;
		}
	}

	if (values != NULL) {
		*values = count;
	}
	if (max_val != NULL) {
		*max_val = max_name;
	}
	if (max_data != NULL) {
		*max_data = max_size;
	}
	return ERROR_SUCCESS;
}

LONG RegQueryValueExA(HKEY key, LPCSTR name, LPDWORD reserved, LPDWORD type, LPBYTE data, LPDWORD data_len)
{
	(void)reserved;
	LinuxRegSection *section = linux_registry_section_for(key);
	if (section == NULL || name == NULL) {
		return ERROR_INVALID_HANDLE;
	}

	const auto it = section->values.find(name);
	if (it == section->values.end()) {
		if (data_len != NULL) {
			*data_len = 0;
		}
		return ERROR_FILE_NOT_FOUND;
	}

	if (type != NULL) {
		*type = it->second.type;
	}
	if (data_len != NULL) {
		if (data == NULL) {
			*data_len = (DWORD)it->second.data.size();
			return ERROR_SUCCESS;
		}
		const DWORD need = (DWORD)it->second.data.size();
		if (*data_len < need) {
			*data_len = need;
			return ERROR_MORE_DATA;
		}
		if (need > 0) {
			memcpy(data, it->second.data.data(), need);
		}
		*data_len = need;
	}
	return ERROR_SUCCESS;
}

LONG RegSetValueExA(HKEY key, LPCSTR name, DWORD reserved, DWORD type, const BYTE *data, DWORD data_len)
{
	(void)reserved;
	LinuxRegSection *section = linux_registry_section_for(key);
	if (section == NULL || name == NULL || data == NULL || data_len == 0) {
		return ERROR_INVALID_HANDLE;
	}

	RegBytes bytes(data, data + data_len);
	if (linux_registry_store_value(linux_registry_open_keys()[key], name, type, bytes)) {
		linux_registry_mark_section_dirty(linux_registry_open_keys()[key]);
	}
	return ERROR_SUCCESS;
}

LONG RegDeleteValueA(HKEY key, LPCSTR name)
{
	LinuxRegSection *section = linux_registry_section_for(key);
	if (section == NULL || name == NULL) {
		return ERROR_INVALID_HANDLE;
	}
	if (section->values.erase(name) == 0) {
		return ERROR_FILE_NOT_FOUND;
	}
	linux_registry_mark_section_dirty(linux_registry_open_keys()[key]);
	return ERROR_SUCCESS;
}

LONG RegCloseKey(HKEY key)
{
	linux_registry_open_keys().erase(key);
	return ERROR_SUCCESS;
}

void Linux_Registry_Flush(void)
{
	linux_registry_save();
}

LONG RegDeleteKeyA(HKEY, LPCSTR)
{
	return ERROR_FILE_NOT_FOUND;
}

LONG RegQueryValueExW(HKEY, LPCWSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD)
{
	return ERROR_FILE_NOT_FOUND;
}

LONG RegSetValueExW(HKEY, LPCWSTR, DWORD, DWORD, const BYTE *, DWORD)
{
	return ERROR_FILE_NOT_FOUND;
}

} /* extern "C" */
