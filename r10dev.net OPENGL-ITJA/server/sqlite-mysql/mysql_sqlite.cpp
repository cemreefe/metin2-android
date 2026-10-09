// SQLite implementation of the MySQL C API subset used by m2dev-server-src.
//
// Each MySQL "database" (account, player, common, log, ...) is a separate
// SQLite file <M2_SQLITE_DIR>/<name>.sqlite3. A connection opens an in-memory
// main database and ATTACHes the selected database first, followed by every
// other known database, so both `table` and `db.table` references resolve.
//
// Queries are rewritten from the MySQL dialect the server emits (backslash
// escapes, INSERT DELAYED/IGNORE, ON DUPLICATE KEY UPDATE, enum `col+0`,
// DATE_ADD, NOW(), PASSWORD(), user variables, ...) before being prepared.

#include "mysql.h"
#include "errmsg.h"
#include "mysqld_error.h"
#include "sqlite3.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <map>
#include <mutex>
#include <regex>
#include <string>
#include <sys/stat.h>
#include <vector>

#ifndef M2_SQLITE_DEFAULT_DIR
#define M2_SQLITE_DEFAULT_DIR "sqlite"
#endif

namespace
{
const char *const kKnownDatabases[] = { "account", "player", "common", "log", "hotbackup" };

struct ResultSet
{
	bool hasColumns = false;
	unsigned int numFields = 0;
	std::vector<std::vector<std::string>> rows;
	std::vector<std::vector<bool>> nulls;
	my_ulonglong affected = 0;
	my_ulonglong insertId = 0;
};

struct EnumDef
{
	std::string values;
	int isSet = 0;
};
}

struct m2sql_conn
{
	sqlite3 *db = nullptr;
	std::string dir;
	std::string selected;
	std::vector<std::string> schemas;
	std::map<std::string, EnumDef> enumColumns;
	std::vector<ResultSet> results;
	size_t current = 0;
	unsigned int err = 0;
	std::string errmsg;
	std::recursive_mutex mtx;
};

struct st_mysql_res
{
	ResultSet set;
	size_t cursor = 0;
	std::vector<char *> rowPtrs;
	std::vector<unsigned long> lengths;
};

struct st_mysql_stmt
{
	int unused;
};

namespace
{
// ---------------------------------------------------------------- helpers

void SetError(m2sql_conn *c, unsigned int code, const std::string &msg)
{
	c->err = code;
	c->errmsg = msg;
}

unsigned int MapSqliteError(int rc)
{
	switch (rc & 0xff)
	{
		case SQLITE_CONSTRAINT:
			return ER_DUP_ENTRY;
		case SQLITE_BUSY:
		case SQLITE_LOCKED:
			return ER_LOCK_WAIT_TIMEOUT;
		case SQLITE_NOMEM:
			return CR_OUT_OF_MEMORY;
		case SQLITE_CANTOPEN:
			return ER_BAD_DB_ERROR;
		default:
			return ER_PARSE_ERROR;
	}
}

bool FileExists(const std::string &path)
{
	struct stat st;
	return stat(path.c_str(), &st) == 0;
}

std::string Lower(std::string s)
{
	for (auto &ch : s)
		ch = static_cast<char>(tolower(static_cast<unsigned char>(ch)));
	return s;
}

std::string Trim(const std::string &s)
{
	size_t b = s.find_first_not_of(" \t\r\n");
	if (b == std::string::npos)
		return "";
	size_t e = s.find_last_not_of(" \t\r\n");
	return s.substr(b, e - b + 1);
}

std::string QuoteText(const std::string &s)
{
	std::string out = "'";
	for (char ch : s)
	{
		if (ch == '\'')
			out += "''";
		else
			out += ch;
	}
	out += "'";
	return out;
}

// ---------------------------------------------------------------- SHA1 (for PASSWORD())

struct Sha1
{
	uint32_t h[5] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };

	static uint32_t Rol(uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }

	void Block(const unsigned char *p)
	{
		uint32_t w[80];
		for (int i = 0; i < 16; ++i)
			w[i] = (uint32_t)p[i * 4] << 24 | (uint32_t)p[i * 4 + 1] << 16 | (uint32_t)p[i * 4 + 2] << 8 | p[i * 4 + 3];
		for (int i = 16; i < 80; ++i)
			w[i] = Rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
		uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
		for (int i = 0; i < 80; ++i)
		{
			uint32_t f, k;
			if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; }
			else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
			else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
			else { f = b ^ c ^ d; k = 0xCA62C1D6; }
			uint32_t t = Rol(a, 5) + f + e + k + w[i];
			e = d; d = c; c = Rol(b, 30); b = a; a = t;
		}
		h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
	}

	void Digest(const unsigned char *data, size_t len, unsigned char out[20])
	{
		std::vector<unsigned char> m(data, data + len);
		uint64_t bits = (uint64_t)len * 8;
		m.push_back(0x80);
		while (m.size() % 64 != 56)
			m.push_back(0);
		for (int i = 7; i >= 0; --i)
			m.push_back((unsigned char)(bits >> (i * 8)));
		for (size_t i = 0; i < m.size(); i += 64)
			Block(&m[i]);
		for (int i = 0; i < 5; ++i)
		{
			out[i * 4] = (unsigned char)(h[i] >> 24);
			out[i * 4 + 1] = (unsigned char)(h[i] >> 16);
			out[i * 4 + 2] = (unsigned char)(h[i] >> 8);
			out[i * 4 + 3] = (unsigned char)h[i];
		}
	}
};

// ---------------------------------------------------------------- SQL functions

void FmtLocal(time_t t, char *buf, size_t n, const char *fmt)
{
	struct tm tmv;
	localtime_r(&t, &tmv);
	strftime(buf, n, fmt, &tmv);
}

bool ParseDateTime(const char *s, time_t *out)
{
	struct tm tmv;
	memset(&tmv, 0, sizeof(tmv));
	int n = sscanf(s, "%d-%d-%d %d:%d:%d", &tmv.tm_year, &tmv.tm_mon, &tmv.tm_mday, &tmv.tm_hour, &tmv.tm_min, &tmv.tm_sec);
	if (n < 3 || tmv.tm_year <= 0)
		return false;
	tmv.tm_year -= 1900;
	tmv.tm_mon -= 1;
	tmv.tm_isdst = -1;
	*out = mktime(&tmv);
	return true;
}

void FnNow(sqlite3_context *ctx, int, sqlite3_value **)
{
	char buf[32];
	FmtLocal(time(nullptr), buf, sizeof(buf), "%Y-%m-%d %H:%M:%S");
	sqlite3_result_text(ctx, buf, -1, SQLITE_TRANSIENT);
}

void FnCurDate(sqlite3_context *ctx, int, sqlite3_value **)
{
	char buf[32];
	FmtLocal(time(nullptr), buf, sizeof(buf), "%Y-%m-%d");
	sqlite3_result_text(ctx, buf, -1, SQLITE_TRANSIENT);
}

void FnUnixTimestamp(sqlite3_context *ctx, int argc, sqlite3_value **argv)
{
	if (argc == 0)
	{
		sqlite3_result_int64(ctx, (sqlite3_int64)time(nullptr));
		return;
	}
	if (sqlite3_value_type(argv[0]) == SQLITE_NULL)
	{
		sqlite3_result_null(ctx);
		return;
	}
	time_t t;
	const char *s = (const char *)sqlite3_value_text(argv[0]);
	sqlite3_result_int64(ctx, s && ParseDateTime(s, &t) ? (sqlite3_int64)t : 0);
}

void FnFromUnixtime(sqlite3_context *ctx, int, sqlite3_value **argv)
{
	if (sqlite3_value_type(argv[0]) == SQLITE_NULL)
	{
		sqlite3_result_null(ctx);
		return;
	}
	char buf[32];
	FmtLocal((time_t)sqlite3_value_int64(argv[0]), buf, sizeof(buf), "%Y-%m-%d %H:%M:%S");
	sqlite3_result_text(ctx, buf, -1, SQLITE_TRANSIENT);
}

void FnTimestampDiffSeconds(sqlite3_context *ctx, int, sqlite3_value **argv)
{
	time_t a = 0, b = 0;
	const char *sa = (const char *)sqlite3_value_text(argv[0]);
	const char *sb = (const char *)sqlite3_value_text(argv[1]);
	if (!sa || !sb || !ParseDateTime(sa, &a) || !ParseDateTime(sb, &b))
	{
		sqlite3_result_null(ctx);
		return;
	}
	sqlite3_result_int64(ctx, (sqlite3_int64)(a - b));
}

void FnDateAddSeconds(sqlite3_context *ctx, int, sqlite3_value **argv)
{
	time_t t;
	const char *s = (const char *)sqlite3_value_text(argv[0]);
	if (!s || !ParseDateTime(s, &t))
	{
		sqlite3_result_null(ctx);
		return;
	}
	char buf[32];
	FmtLocal(t + (time_t)sqlite3_value_int64(argv[1]), buf, sizeof(buf), "%Y-%m-%d %H:%M:%S");
	sqlite3_result_text(ctx, buf, -1, SQLITE_TRANSIENT);
}

void FnInetAton(sqlite3_context *ctx, int, sqlite3_value **argv)
{
	const char *s = reinterpret_cast<const char *>(sqlite3_value_text(argv[0]));
	unsigned a, b, c, d;
	char tail;
	if (!s || sscanf(s, "%u.%u.%u.%u%c", &a, &b, &c, &d, &tail) != 4 || a > 255 || b > 255 || c > 255 || d > 255)
	{
		sqlite3_result_null(ctx);
		return;
	}
	sqlite3_result_int64(ctx, (sqlite3_int64)((a << 24) | (b << 16) | (c << 8) | d));
}

void FnInetNtoa(sqlite3_context *ctx, int, sqlite3_value **argv)
{
	if (sqlite3_value_type(argv[0]) == SQLITE_NULL)
	{
		sqlite3_result_null(ctx);
		return;
	}
	const uint32_t v = (uint32_t)sqlite3_value_int64(argv[0]);
	char buf[16];
	snprintf(buf, sizeof(buf), "%u.%u.%u.%u", v >> 24, (v >> 16) & 255, (v >> 8) & 255, v & 255);
	sqlite3_result_text(ctx, buf, -1, SQLITE_TRANSIENT);
}

void FnPassword(sqlite3_context *ctx, int, sqlite3_value **argv)
{
	const unsigned char *s = sqlite3_value_text(argv[0]);
	int len = sqlite3_value_bytes(argv[0]);
	if (!s || len == 0)
	{
		sqlite3_result_text(ctx, "", 0, SQLITE_STATIC);
		return;
	}
	unsigned char d1[20], d2[20];
	Sha1().Digest(s, (size_t)len, d1);
	Sha1().Digest(d1, sizeof(d1), d2);
	char out[42];
	out[0] = '*';
	for (int i = 0; i < 20; ++i)
		snprintf(out + 1 + i * 2, 3, "%02X", d2[i]);
	sqlite3_result_text(ctx, out, 41, SQLITE_TRANSIENT);
}

std::vector<std::string> SplitValues(const char *values)
{
	std::vector<std::string> out;
	std::string cur;
	for (const char *p = values; *p; ++p)
	{
		if (*p == ',')
		{
			out.push_back(cur);
			cur.clear();
		}
		else
			cur += *p;
	}
	out.push_back(cur);
	return out;
}

bool IsIntegerText(const char *s)
{
	if (!s || !*s)
		return false;
	if (*s == '-')
		++s;
	for (; *s; ++s)
		if (!isdigit((unsigned char)*s))
			return false;
	return true;
}

// m2_enum_num(value, 'A,B,C', is_set): MySQL `enum_col+0` semantics.
void FnEnumNum(sqlite3_context *ctx, int, sqlite3_value **argv)
{
	if (sqlite3_value_type(argv[0]) == SQLITE_NULL)
	{
		sqlite3_result_null(ctx);
		return;
	}
	const char *v = (const char *)sqlite3_value_text(argv[0]);
	if (sqlite3_value_type(argv[0]) == SQLITE_INTEGER || IsIntegerText(v))
	{
		sqlite3_result_int64(ctx, sqlite3_value_int64(argv[0]));
		return;
	}
	auto values = SplitValues((const char *)sqlite3_value_text(argv[1]));
	bool isSet = sqlite3_value_int(argv[2]) != 0;
	sqlite3_int64 result = 0;
	if (!isSet)
	{
		for (size_t i = 0; i < values.size(); ++i)
			if (strcasecmp(values[i].c_str(), v) == 0)
				result = (sqlite3_int64)i + 1;
	}
	else
	{
		for (const auto &member : SplitValues(v))
			for (size_t i = 0; i < values.size(); ++i)
				if (strcasecmp(values[i].c_str(), member.c_str()) == 0)
					result |= (sqlite3_int64)1 << i;
	}
	sqlite3_result_int64(ctx, result);
}

// m2_enum_text(value, 'A,B,C', is_set): what MySQL stores when an integer is
// written into an enum/set column.
void FnEnumText(sqlite3_context *ctx, int, sqlite3_value **argv)
{
	const char *v = (const char *)sqlite3_value_text(argv[0]);
	if (sqlite3_value_type(argv[0]) == SQLITE_NULL || !(sqlite3_value_type(argv[0]) == SQLITE_INTEGER || IsIntegerText(v)))
	{
		sqlite3_result_value(ctx, argv[0]);
		return;
	}
	auto values = SplitValues((const char *)sqlite3_value_text(argv[1]));
	bool isSet = sqlite3_value_int(argv[2]) != 0;
	sqlite3_int64 n = sqlite3_value_int64(argv[0]);
	std::string out;
	if (!isSet)
	{
		if (n >= 1 && (size_t)n <= values.size())
			out = values[(size_t)n - 1];
	}
	else
	{
		for (size_t i = 0; i < values.size() && i < 63; ++i)
			if (n & ((sqlite3_int64)1 << i))
			{
				if (!out.empty())
					out += ",";
				out += values[i];
			}
	}
	sqlite3_result_text(ctx, out.c_str(), (int)out.size(), SQLITE_TRANSIENT);
}

void RegisterFunctions(sqlite3 *db)
{
	const int flags = SQLITE_UTF8;
	sqlite3_create_function(db, "now", 0, flags, nullptr, FnNow, nullptr, nullptr);
	sqlite3_create_function(db, "sysdate", 0, flags, nullptr, FnNow, nullptr, nullptr);
	sqlite3_create_function(db, "current_timestamp", 0, flags, nullptr, FnNow, nullptr, nullptr);
	sqlite3_create_function(db, "curdate", 0, flags, nullptr, FnCurDate, nullptr, nullptr);
	sqlite3_create_function(db, "unix_timestamp", 0, flags, nullptr, FnUnixTimestamp, nullptr, nullptr);
	sqlite3_create_function(db, "unix_timestamp", 1, flags, nullptr, FnUnixTimestamp, nullptr, nullptr);
	sqlite3_create_function(db, "from_unixtime", 1, flags, nullptr, FnFromUnixtime, nullptr, nullptr);
	sqlite3_create_function(db, "m2_diff_seconds", 2, flags, nullptr, FnTimestampDiffSeconds, nullptr, nullptr);
	sqlite3_create_function(db, "m2_add_seconds", 2, flags, nullptr, FnDateAddSeconds, nullptr, nullptr);
	sqlite3_create_function(db, "password", 1, flags | SQLITE_DETERMINISTIC, nullptr, FnPassword, nullptr, nullptr);
	sqlite3_create_function(db, "inet_aton", 1, flags | SQLITE_DETERMINISTIC, nullptr, FnInetAton, nullptr, nullptr);
	sqlite3_create_function(db, "inet_ntoa", 1, flags | SQLITE_DETERMINISTIC, nullptr, FnInetNtoa, nullptr, nullptr);
	sqlite3_create_function(db, "m2_enum_num", 3, flags | SQLITE_DETERMINISTIC, nullptr, FnEnumNum, nullptr, nullptr);
	sqlite3_create_function(db, "m2_enum_text", 3, flags | SQLITE_DETERMINISTIC, nullptr, FnEnumText, nullptr, nullptr);
}

// ---------------------------------------------------------------- lexer

// Splits MySQL SQL into code and literal parts. Literals are replaced in the
// code by "\x01<index>\x01" so regex rewrites never touch string contents.
struct Lexed
{
	std::string code;
	std::vector<std::string> literals;
};

std::string SqliteLiteral(const std::string &bytes)
{
	if (bytes.find('\0') != std::string::npos)
	{
		static const char *hex = "0123456789ABCDEF";
		std::string out = "X'";
		for (unsigned char ch : bytes)
		{
			out += hex[ch >> 4];
			out += hex[ch & 15];
		}
		out += "'";
		return out;
	}
	return QuoteText(bytes);
}

Lexed Lex(const char *q, size_t len)
{
	Lexed out;
	size_t i = 0;
	auto addLiteral = [&](const std::string &sqliteText) {
		out.code += '\x01';
		out.code += std::to_string(out.literals.size());
		out.code += '\x01';
		out.literals.push_back(sqliteText);
	};

	while (i < len)
	{
		char ch = q[i];
		if (ch == '\'' || ch == '"')
		{
			char quote = ch;
			std::string bytes;
			++i;
			while (i < len)
			{
				char c = q[i];
				if (c == '\\' && i + 1 < len)
				{
					char n = q[i + 1];
					switch (n)
					{
						case '0': bytes += '\0'; break;
						case 'n': bytes += '\n'; break;
						case 'r': bytes += '\r'; break;
						case 't': bytes += '\t'; break;
						case 'b': bytes += '\b'; break;
						case 'Z': bytes += '\x1a'; break;
						case '%': bytes += "\\%"; break;
						case '_': bytes += "\\_"; break;
						default: bytes += n; break;
					}
					i += 2;
					continue;
				}
				if (c == quote)
				{
					if (i + 1 < len && q[i + 1] == quote)
					{
						bytes += quote;
						i += 2;
						continue;
					}
					++i;
					break;
				}
				bytes += c;
				++i;
			}
			addLiteral(SqliteLiteral(bytes));
			continue;
		}
		if (ch == '`')
		{
			size_t end = i + 1;
			while (end < len && q[end] != '`')
				++end;
			out.code += '"';
			out.code.append(q + i + 1, end - i - 1);
			out.code += '"';
			i = end + 1;
			continue;
		}
		if (ch == '#' || (ch == '-' && i + 2 < len && q[i + 1] == '-' && isspace((unsigned char)q[i + 2])))
		{
			while (i < len && q[i] != '\n')
				++i;
			continue;
		}
		if (ch == '/' && i + 1 < len && q[i + 1] == '*')
		{
			size_t end = i + 2;
			while (end + 1 < len && !(q[end] == '*' && q[end + 1] == '/'))
				++end;
			i = end + 2;
			out.code += ' ';
			continue;
		}
		if (ch == '\0')
			break;
		out.code += ch;
		++i;
	}
	return out;
}

std::string Unlex(const std::string &code, const std::vector<std::string> &literals)
{
	std::string out;
	out.reserve(code.size());
	for (size_t i = 0; i < code.size(); ++i)
	{
		if (code[i] == '\x01')
		{
			size_t end = code.find('\x01', i + 1);
			out += literals[(size_t)atoi(code.c_str() + i + 1)];
			i = end;
		}
		else
			out += code[i];
	}
	return out;
}

// Split on top-level ';' (literals are already masked).
std::vector<std::string> SplitStatements(const std::string &code)
{
	std::vector<std::string> out;
	std::string cur;
	for (char ch : code)
	{
		if (ch == ';')
		{
			out.push_back(cur);
			cur.clear();
		}
		else
			cur += ch;
	}
	out.push_back(cur);
	std::vector<std::string> nonEmpty;
	for (auto &s : out)
	{
		size_t b = s.find_first_not_of(" \t\r\n");
		if (b != std::string::npos)
			nonEmpty.push_back(s.substr(b));
	}
	return nonEmpty;
}

// ---------------------------------------------------------------- rewriter

using std::regex;
using std::regex_replace;
const auto kIcase = std::regex::ECMAScript | std::regex::icase;

std::string ReplaceCallback(const std::string &in, const regex &re, const std::function<std::string(const std::smatch &)> &fn)
{
	std::string out;
	auto begin = std::sregex_iterator(in.begin(), in.end(), re);
	auto end = std::sregex_iterator();
	size_t last = 0;
	for (auto it = begin; it != end; ++it)
	{
		out.append(in, last, (size_t)it->position(0) - last);
		out += fn(*it);
		last = (size_t)(it->position(0) + it->length(0));
	}
	out.append(in, last, std::string::npos);
	return out;
}

const char *IntervalUnitSeconds(const std::string &unitIn)
{
	std::string unit = Lower(unitIn);
	if (unit == "second") return "1";
	if (unit == "minute") return "60";
	if (unit == "hour") return "3600";
	if (unit == "day") return "86400";
	if (unit == "week") return "604800";
	if (unit == "month") return "2592000";
	if (unit == "year") return "31536000";
	return nullptr;
}

// ------------------------------------------------------------ enum columns
// MySQL enums are stored as text here, so SQLite compares them against the
// numbers the game uses by storage class: text always sorts above integers and
// `WHERE window < 3` matches nothing. Every place a number meets an enum column
// is therefore translated to the matching enum semantics.

const EnumDef *FindEnumColumn(m2sql_conn *c, const std::string &name)
{
	std::string bare = name;
	bare.erase(std::remove(bare.begin(), bare.end(), '"'), bare.end());
	size_t dot = bare.rfind('.');
	if (dot != std::string::npos)
		bare = bare.substr(dot + 1);
	auto it = c->enumColumns.find(Lower(Trim(bare)));
	return it == c->enumColumns.end() ? nullptr : &it->second;
}

std::string EnumCall(const char *fn, const EnumDef &def, const std::string &arg)
{
	return std::string(fn) + "(" + arg + ", '" + def.values + "', " + std::to_string(def.isSet) + ")";
}

std::vector<std::string> SplitTopLevel(const std::string &in)
{
	std::vector<std::string> out;
	std::string cur;
	int depth = 0;
	for (char ch : in)
	{
		if (ch == '(')
			++depth;
		else if (ch == ')')
			--depth;
		if (ch == ',' && depth == 0)
		{
			out.push_back(cur);
			cur.clear();
		}
		else
			cur += ch;
	}
	out.push_back(cur);
	return out;
}

// enum_col = <number> inside a SET clause: store what MySQL would store.
std::string RewriteEnumAssignments(m2sql_conn *c, const std::string &in)
{
	static const regex reSet(R"re(\bSET\b)re", kIcase);
	static const regex reWhere(R"re(\bWHERE\b)re", kIcase);
	static const regex reAssign(R"re(("?\w+"?)\s*=\s*(-?\d+)\b)re", kIcase);
	std::string out;
	size_t last = 0;
	for (auto it = std::sregex_iterator(in.begin(), in.end(), reSet); it != std::sregex_iterator(); ++it)
	{
		size_t start = (size_t)(it->position(0) + it->length(0));
		if (start < last)
			continue;
		size_t end = in.size();
		std::string rest = in.substr(start);
		std::smatch mw;
		if (std::regex_search(rest, mw, reWhere))
			end = start + (size_t)mw.position(0);
		out.append(in, last, start - last);
		out += ReplaceCallback(in.substr(start, end - start), reAssign, [c](const std::smatch &mm) {
			const EnumDef *def = FindEnumColumn(c, mm[1].str());
			if (!def)
				return mm[0].str();
			return mm[1].str() + " = " + EnumCall("m2_enum_text", *def, mm[2].str());
		});
		last = end;
	}
	out.append(in, last, std::string::npos);
	return out;
}

// INSERT INTO t (a, window, b) VALUES (1, 2, 3), (...)
std::string RewriteEnumInsertValues(m2sql_conn *c, const std::string &in)
{
	static const regex reHead(R"re(^(\s*(?:INSERT|REPLACE)(?:\s+OR\s+\w+)?\s+INTO\s+(?:"?\w+"?\.)?"?\w+"?\s*\(([^()]*)\)\s*VALUES\s*))re", kIcase);
	static const regex reNumber(R"re(^\s*(-?\d+)\s*$)re");
	std::smatch m;
	if (!std::regex_search(in, m, reHead) || m.position(0) != 0)
		return in;

	std::vector<const EnumDef *> defs;
	bool any = false;
	for (const std::string &col : SplitTopLevel(m[2].str()))
	{
		const EnumDef *def = FindEnumColumn(c, col);
		defs.push_back(def);
		any = any || def != nullptr;
	}
	if (!any)
		return in;

	std::string out = m[1].str();
	size_t pos = (size_t)m.length(0);
	while (pos < in.size() && in[pos] == '(')
	{
		int depth = 0;
		size_t start = pos;
		for (; pos < in.size(); ++pos)
		{
			if (in[pos] == '(')
				++depth;
			else if (in[pos] == ')' && --depth == 0)
			{
				++pos;
				break;
			}
		}
		if (depth != 0)
			return in;
		std::vector<std::string> vals = SplitTopLevel(in.substr(start + 1, pos - start - 2));
		if (vals.size() != defs.size())
			return in;
		std::string tuple;
		for (size_t i = 0; i < vals.size(); ++i)
		{
			std::smatch mn;
			bool isEnumNumber = defs[i] && std::regex_match(vals[i], mn, reNumber);
			tuple += (i ? ", " : "") + (isEnumNumber ? EnumCall("m2_enum_text", *defs[i], mn[1].str()) : vals[i]);
		}
		out += "(" + tuple + ")";
		size_t skip = pos;
		while (skip < in.size() && isspace((unsigned char)in[skip]))
			++skip;
		if (skip < in.size() && in[skip] == ',')
		{
			out += ", ";
			pos = skip + 1;
			while (pos < in.size() && isspace((unsigned char)in[pos]))
				++pos;
		}
		else
			break;
	}
	out.append(in, pos, std::string::npos);
	return out;
}

// Returns false if the statement should be skipped (no-op in SQLite).
bool RewriteStatement(m2sql_conn *c, std::string &s, Lexed &lx)
{
	static const regex reSetNames(R"re(^\s*SET\s+(NAMES|CHARACTER\s+SET|SESSION|GLOBAL|AUTOCOMMIT|FOREIGN_KEY_CHECKS|UNIQUE_CHECKS|SQL_MODE|TIME_ZONE)\b)re", kIcase);
	static const regex reLockTables(R"re(^\s*(LOCK|UNLOCK)\s+TABLES?\b)re", kIcase);
	static const regex reUse(R"re(^\s*USE\s+"?(\w+)"?\s*$)re", kIcase);
	static const regex reSetVar(R"re(^\s*SET\s+@(\w+)\s*:?=\s*([\s\S]*)$)re", kIcase);
	static const regex reShowCreate(R"re(^\s*SHOW\s+CREATE\s+TABLE\s+"?(\w+)"?\s*$)re", kIcase);
	static const regex reShowTables(R"re(^\s*SHOW\s+TABLES\b[\s\S]*$)re", kIcase);
	static const regex reTruncate(R"re(^\s*TRUNCATE\s+(TABLE\s+)?)re", kIcase);
	static const regex reInsertDelayed(R"re(^\s*INSERT\s+(LOW_PRIORITY\s+|DELAYED\s+|HIGH_PRIORITY\s+)+)re", kIcase);
	static const regex reInsertIgnore(R"re(^\s*INSERT\s+IGNORE\s+)re", kIcase);
	static const regex reReplaceNoInto(R"re(^\s*REPLACE\s+(?!INTO\b))re", kIcase);
	static const regex reInsertNoInto(R"re(^\s*INSERT(\s+OR\s+\w+)?\s+(?!INTO\b))re", kIcase);
	// MySQL: inserting 0 into an AUTO_INCREMENT column auto-assigns; sqlite stores 0 literally.
	// Rewrite a leading literal 0 to NULL when the column list starts with `id`.
	static const regex reInsertZeroId(R"re((\b(?:INSERT|REPLACE)\s+(?:OR\s+\w+\s+)?INTO\s+(?:"?\w+"?\.)?"?\w+"?\s*\(\s*"?id"?\s*,[\s\S]*?\)\s*VALUES\s*\()\s*0\b)re", kIcase);
	static const regex reInsertSet(R"re(^(\s*(?:INSERT|REPLACE)(?:\s+OR\s+\w+)?\s+INTO\s+(?:"?\w+"?\.)?"?\w+"?)\s+SET\s+([\s\S]*?)(\s+ON\s+DUPLICATE\s+KEY\s+UPDATE\s+[\s\S]*)?$)re", kIcase);
	static const regex reOnDup(R"re(\bON\s+DUPLICATE\s+KEY\s+UPDATE\b)re", kIcase);
	static const regex reValuesFn(R"re(\bVALUES\s*\(\s*"?(\w+)"?\s*\))re", kIcase);
	static const regex reDual(R"re(\s+FROM\s+DUAL\b)re", kIcase);
	static const regex reCastUnsigned(R"re(\bAS\s+(UNSIGNED|SIGNED)(\s+INTEGER|\s+INT)?\b)re", kIcase);
	static const regex reCollate(R"re(\bCOLLATE\s+"?\w+"?)re", kIcase);
	static const regex reBinaryOp(R"re(\bBINARY\s+(?=[\w\x01"(]))re", kIcase);
	static const regex reDateAdd(R"re(\b(DATE_ADD|DATE_SUB|ADDDATE|SUBDATE)\s*\(\s*((?:[^(),]|\([^()]*\))+?)\s*,\s*INTERVAL\s+(-?[\w\x01.]+|\([^()]*\))\s+(SECOND|MINUTE|HOUR|DAY|WEEK|MONTH|YEAR)\s*\))re", kIcase);
	static const regex reTimestampDiff(R"re(\bTIMESTAMPDIFF\s*\(\s*(SECOND|MINUTE|HOUR|DAY)\s*,\s*((?:[^(),]|\([^()]*\))+?)\s*,\s*((?:[^(),]|\([^()]*\))+?)\s*\))re", kIcase);
	static const regex reExprInterval(R"re(((?:\bNOW\s*\(\s*\)|\bCURRENT_TIMESTAMP\b(?:\s*\(\s*\))?|"?\w+"?))\s*([+-])\s*INTERVAL\s+(-?[\w\x01.]+|\([^()]*\))\s+(SECOND|MINUTE|HOUR|DAY|WEEK|MONTH|YEAR)\b)re", kIcase);
	static const regex reEnumPlus0(R"re(((?:"?\w+"?\.)?"?(\w+)"?)\s*\+\s*0\b)re", kIcase);
	static const regex reEnumCompare(R"re(((?:"?\w+"?\.)?"?(\w+)"?)\s*(<=|>=|<>|!=|=|<|>)\s*(-?\d+)\b)re", kIcase);
	static const regex reEnumOrderBy(R"re(\b(ORDER\s+BY)\s+((?:"?\w+"?\.)?"?\w+"?)(?!\s*\())re", kIcase);
	static const regex reUserVar(R"re(@(\w+))re", kIcase);
	static const regex reLimitOffset(R"re(\bLIMIT\s+(\d+)\s*,\s*(\d+))re", kIcase);
	static const regex reIf(R"re(\bIF\s*\()re", kIcase);
	static const regex reCreateEngine(R"re(\)\s*(ENGINE|TYPE)\s*=[\s\S]*$)re", kIcase);
	static const regex reForUpdate(R"re(\s+FOR\s+UPDATE\s*$)re", kIcase);
	static const regex reFoundRows(R"re(\bSQL_CALC_FOUND_ROWS\b)re", kIcase);

	if (std::regex_search(s, reSetNames) || std::regex_search(s, reLockTables))
		return false;

	std::smatch m;
	if (std::regex_match(s, m, reUse))
	{
		c->selected = Lower(m[1].str());
		return false;
	}

	if (std::regex_match(s, m, reSetVar))
	{
		s = "INSERT OR REPLACE INTO temp._m2_vars(k, v) VALUES ('" + Lower(m[1].str()) + "', (" + m[2].str() + "))";
	}

	if (std::regex_match(s, m, reShowCreate))
	{
		std::string t = m[1].str();
		std::string q;
		for (const auto &schema : c->schemas)
		{
			if (!q.empty())
				q += " UNION ALL ";
			q += "SELECT name, sql FROM \"" + schema + "\".sqlite_master WHERE type='table' AND name='" + t + "'";
		}
		s = "SELECT * FROM (" + q + ") LIMIT 1";
		return true;
	}

	if (std::regex_match(s, reShowTables))
	{
		s = "SELECT name FROM \"" + c->selected + "\".sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%' AND name NOT LIKE '\\_m2\\_%' ESCAPE '\\'";
		return true;
	}

	s = regex_replace(s, reTruncate, "DELETE FROM ");
	s = regex_replace(s, reInsertDelayed, "INSERT ");
	s = regex_replace(s, reInsertIgnore, "INSERT OR IGNORE ");
	s = regex_replace(s, reReplaceNoInto, "REPLACE INTO ");
	s = regex_replace(s, reInsertNoInto, "INSERT$1 INTO ");
	s = regex_replace(s, reInsertZeroId, "$1NULL");

	// INSERT INTO t SET a=1, b=2  ->  INSERT INTO t (a, b) VALUES (1, 2)
	if (std::regex_match(s, m, reInsertSet))
	{
		std::string head = m[1].str(), assigns = m[2].str(), tail = m[3].str();
		std::vector<std::string> cols, vals;
		int depth = 0;
		std::string cur;
		auto flush = [&]() {
			size_t eq = cur.find('=');
			if (eq != std::string::npos)
			{
				cols.push_back(cur.substr(0, eq));
				vals.push_back(cur.substr(eq + 1));
			}
			cur.clear();
		};
		for (char ch : assigns)
		{
			if (ch == '(') ++depth;
			if (ch == ')') --depth;
			if (ch == ',' && depth == 0)
				flush();
			else
				cur += ch;
		}
		flush();
		std::string cs, vs;
		for (size_t i = 0; i < cols.size(); ++i)
		{
			cs += (i ? ", " : "") + cols[i];
			vs += (i ? ", " : "") + vals[i];
		}
		s = head + " (" + cs + ") VALUES (" + vs + ")" + tail;
	}

	if (std::regex_search(s, reOnDup))
	{
		s = regex_replace(s, reOnDup, "ON CONFLICT DO UPDATE SET");
		s = regex_replace(s, reValuesFn, "excluded.\"$1\"");
	}

	s = regex_replace(s, reDual, "");
	s = regex_replace(s, reCastUnsigned, "AS INTEGER");
	s = regex_replace(s, reCollate, "COLLATE NOCASE");
	s = regex_replace(s, reBinaryOp, "");
	s = regex_replace(s, reForUpdate, "");
	s = regex_replace(s, reFoundRows, "");
	s = regex_replace(s, reLimitOffset, "LIMIT $2 OFFSET $1");
	s = regex_replace(s, reIf, "IIF(");

	s = ReplaceCallback(s, reDateAdd, [](const std::smatch &mm) {
		const char *unit = IntervalUnitSeconds(mm[4].str());
		std::string fn = Lower(mm[1].str());
		std::string sign = (fn == "date_sub" || fn == "subdate") ? "-" : "";
		return "m2_add_seconds(" + mm[2].str() + ", " + sign + "(" + mm[3].str() + ") * " + unit + ")";
	});
	s = ReplaceCallback(s, reTimestampDiff, [](const std::smatch &mm) {
		const char *unit = IntervalUnitSeconds(mm[1].str());
		return "(m2_diff_seconds(" + mm[3].str() + ", " + mm[2].str() + ") / " + unit + ")";
	});
	s = ReplaceCallback(s, reExprInterval, [](const std::smatch &mm) {
		const char *unit = IntervalUnitSeconds(mm[4].str());
		return "m2_add_seconds(" + mm[1].str() + ", " + mm[2].str() + "(" + mm[3].str() + ") * " + unit + ")";
	});

	if (!c->enumColumns.empty())
	{
		s = ReplaceCallback(s, reEnumPlus0, [c](const std::smatch &mm) {
			auto it = c->enumColumns.find(Lower(mm[2].str()));
			if (it == c->enumColumns.end())
				return mm[0].str();
			return "m2_enum_num(" + mm[1].str() + ", '" + it->second.values + "', " + std::to_string(it->second.isSet) + ")";
		});
		// Writes first, so their numbers are gone before the comparison pass.
		s = RewriteEnumInsertValues(c, s);
		s = RewriteEnumAssignments(c, s);
		s = ReplaceCallback(s, reEnumCompare, [c](const std::smatch &mm) {
			const EnumDef *def = FindEnumColumn(c, mm[1].str());
			if (!def)
				return mm[0].str();
			return EnumCall("m2_enum_num", *def, mm[1].str()) + " " + mm[3].str() + " " + mm[4].str();
		});
		s = ReplaceCallback(s, reEnumOrderBy, [c](const std::smatch &mm) {
			const EnumDef *def = FindEnumColumn(c, mm[2].str());
			if (!def)
				return mm[0].str();
			return mm[1].str() + " " + EnumCall("m2_enum_num", *def, mm[2].str());
		});
	}

	s = ReplaceCallback(s, reUserVar, [](const std::smatch &mm) {
		return "(SELECT v FROM temp._m2_vars WHERE k='" + Lower(mm[1].str()) + "')";
	});

	(void)lx;
	return true;
}

// ---------------------------------------------------------------- execution

bool ExecuteOne(m2sql_conn *c, const std::string &sql, ResultSet &rs)
{
	sqlite3_stmt *stmt = nullptr;
	const char *tail = nullptr;
	int rc = sqlite3_prepare_v2(c->db, sql.c_str(), (int)sql.size(), &stmt, &tail);
	if (rc != SQLITE_OK)
	{
		SetError(c, MapSqliteError(rc), std::string(sqlite3_errmsg(c->db)) + " [sqlite: " + sql.substr(0, 512) + "]");
		return false;
	}
	if (!stmt)
		return true;

	int ncols = sqlite3_column_count(stmt);
	rs.hasColumns = ncols > 0;
	rs.numFields = (unsigned int)ncols;
	sqlite3_int64 changesBefore = sqlite3_total_changes64(c->db);

	for (;;)
	{
		rc = sqlite3_step(stmt);
		if (rc == SQLITE_ROW)
		{
			std::vector<std::string> row((size_t)ncols);
			std::vector<bool> nulls((size_t)ncols, false);
			for (int i = 0; i < ncols; ++i)
			{
				int type = sqlite3_column_type(stmt, i);
				if (type == SQLITE_NULL)
				{
					nulls[(size_t)i] = true;
					continue;
				}
				if (type == SQLITE_FLOAT)
				{
					char buf[64];
					snprintf(buf, sizeof(buf), "%.9g", sqlite3_column_double(stmt, i));
					row[(size_t)i] = buf;
					continue;
				}
				const void *data = (type == SQLITE_BLOB) ? sqlite3_column_blob(stmt, i) : (const void *)sqlite3_column_text(stmt, i);
				int bytes = sqlite3_column_bytes(stmt, i);
				if (data && bytes > 0)
					row[(size_t)i].assign((const char *)data, (size_t)bytes);
			}
			rs.rows.push_back(std::move(row));
			rs.nulls.push_back(std::move(nulls));
			continue;
		}
		if (rc == SQLITE_DONE)
			break;
		SetError(c, MapSqliteError(sqlite3_extended_errcode(c->db)), std::string(sqlite3_errmsg(c->db)) + " [sqlite: " + sql.substr(0, 512) + "]");
		sqlite3_finalize(stmt);
		return false;
	}

	bool readOnly = sqlite3_stmt_readonly(stmt) != 0;
	sqlite3_finalize(stmt);

	if (rs.hasColumns && readOnly)
		rs.affected = rs.rows.size();
	else
	{
		rs.affected = (my_ulonglong)(sqlite3_total_changes64(c->db) - changesBefore);
		std::string head = Lower(sql.substr(0, 16));
		if ((head.rfind("insert", 0) == 0 || head.rfind("replace", 0) == 0) && rs.affected > 0)
			rs.insertId = (my_ulonglong)sqlite3_last_insert_rowid(c->db);
	}
	return true;
}

void LoadEnumColumns(m2sql_conn *c)
{
	c->enumColumns.clear();
	for (const auto &schema : c->schemas)
	{
		std::string q = "SELECT col, vals, is_set FROM \"" + schema + "\"._m2_enum";
		sqlite3_stmt *stmt = nullptr;
		if (sqlite3_prepare_v2(c->db, q.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
			continue;
		while (sqlite3_step(stmt) == SQLITE_ROW)
		{
			std::string col = Lower((const char *)sqlite3_column_text(stmt, 0));
			if (c->enumColumns.count(col))
				continue;
			EnumDef def;
			def.values = (const char *)sqlite3_column_text(stmt, 1);
			def.isSet = sqlite3_column_int(stmt, 2);
			c->enumColumns[col] = def;
		}
		sqlite3_finalize(stmt);
	}
}

bool AttachAll(m2sql_conn *c, const std::string &selected)
{
	std::vector<std::string> order;
	if (!selected.empty())
		order.push_back(Lower(selected));
	for (const char *name : kKnownDatabases)
		if (Lower(selected) != name)
			order.push_back(name);

	c->schemas.clear();
	for (const auto &name : order)
	{
		std::string path = c->dir + "/" + name + ".sqlite3";
		if (!FileExists(path))
		{
			if (name == Lower(selected))
			{
				SetError(c, ER_BAD_DB_ERROR, "Unknown database '" + name + "' (" + path + ")");
				return false;
			}
			continue;
		}
		std::string q = "ATTACH DATABASE " + QuoteText(path) + " AS \"" + name + "\"";
		char *err = nullptr;
		if (sqlite3_exec(c->db, q.c_str(), nullptr, nullptr, &err) != SQLITE_OK)
		{
			SetError(c, ER_BAD_DB_ERROR, err ? err : "attach failed");
			sqlite3_free(err);
			return false;
		}
		std::string pragma = "PRAGMA \"" + name + "\".synchronous=NORMAL; PRAGMA \"" + name + "\".journal_mode=WAL;";
		sqlite3_exec(c->db, pragma.c_str(), nullptr, nullptr, nullptr);
		c->schemas.push_back(name);
	}
	c->selected = Lower(selected);
	LoadEnumColumns(c);
	return true;
}
}

// ---------------------------------------------------------------- C API

extern "C" {

MYSQL *mysql_init(MYSQL *mysql)
{
	if (!mysql)
	{
		mysql = (MYSQL *)calloc(1, sizeof(MYSQL));
		if (!mysql)
			return nullptr;
	}
	if (!mysql->impl)
		mysql->impl = new m2sql_conn();
	return mysql;
}

int mysql_options(MYSQL *, enum mysql_option, const void *)
{
	return 0;
}

MYSQL *mysql_real_connect(MYSQL *mysql, const char *host, const char *, const char *, const char *db,
	unsigned int, const char *, unsigned long)
{
	if (!mysql || !mysql->impl)
		return nullptr;
	m2sql_conn *c = mysql->impl;
	std::lock_guard<std::recursive_mutex> lock(c->mtx);

	const char *dir = getenv("M2_SQLITE_DIR");
	c->dir = (dir && *dir) ? dir : M2_SQLITE_DEFAULT_DIR;

	if (c->db)
	{
		sqlite3_close(c->db);
		c->db = nullptr;
	}

	int rc = sqlite3_open_v2(":memory:", &c->db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
	if (rc != SQLITE_OK)
	{
		SetError(c, CR_CONNECTION_ERROR, "sqlite3_open failed");
		return nullptr;
	}
	sqlite3_busy_timeout(c->db, 15000);
	RegisterFunctions(c->db);
	sqlite3_exec(c->db, "PRAGMA temp_store=MEMORY; CREATE TEMP TABLE _m2_vars(k TEXT PRIMARY KEY, v)", nullptr, nullptr, nullptr);

	if (!AttachAll(c, db ? db : ""))
	{
		sqlite3_close(c->db);
		c->db = nullptr;
		return nullptr;
	}

	static char kHost[] = "sqlite";
	(void)host;
	mysql->host = kHost;
	SetError(c, 0, "");
	return mysql;
}

void mysql_close(MYSQL *mysql)
{
	if (!mysql || !mysql->impl)
		return;
	{
		std::lock_guard<std::recursive_mutex> lock(mysql->impl->mtx);
		if (mysql->impl->db)
			sqlite3_close(mysql->impl->db);
	}
	delete mysql->impl;
	mysql->impl = nullptr;
	mysql->host = nullptr;
}

int mysql_select_db(MYSQL *mysql, const char *db)
{
	if (!mysql || !mysql->impl || !mysql->impl->db)
		return 1;
	m2sql_conn *c = mysql->impl;
	std::lock_guard<std::recursive_mutex> lock(c->mtx);
	for (const auto &schema : c->schemas)
	{
		std::string q = "DETACH DATABASE \"" + schema + "\"";
		sqlite3_exec(c->db, q.c_str(), nullptr, nullptr, nullptr);
	}
	return AttachAll(c, db ? db : "") ? 0 : 1;
}

int mysql_ping(MYSQL *mysql)
{
	return (mysql && mysql->impl && mysql->impl->db) ? 0 : 1;
}

int mysql_set_character_set(MYSQL *, const char *)
{
	return 0;
}

int mysql_real_query(MYSQL *mysql, const char *q, unsigned long length)
{
	if (!mysql || !mysql->impl)
		return 1;
	m2sql_conn *c = mysql->impl;
	std::lock_guard<std::recursive_mutex> lock(c->mtx);
	c->results.clear();
	c->current = 0;
	SetError(c, 0, "");
	if (!c->db)
	{
		SetError(c, CR_SERVER_GONE_ERROR, "not connected");
		return 1;
	}

	Lexed lx = Lex(q, length);
	for (auto &stmtCode : SplitStatements(lx.code))
	{
		std::string s = stmtCode;
		ResultSet rs;
		if (RewriteStatement(c, s, lx))
		{
			std::string sql = Unlex(s, lx.literals);
			if (!ExecuteOne(c, sql, rs))
			{
				c->results.push_back(std::move(rs));
				return 1;
			}
		}
		c->results.push_back(std::move(rs));
	}
	if (c->results.empty())
		c->results.emplace_back();
	return 0;
}

int mysql_query(MYSQL *mysql, const char *q)
{
	return mysql_real_query(mysql, q, (unsigned long)strlen(q));
}

MYSQL_RES *mysql_store_result(MYSQL *mysql)
{
	if (!mysql || !mysql->impl)
		return nullptr;
	m2sql_conn *c = mysql->impl;
	std::lock_guard<std::recursive_mutex> lock(c->mtx);
	if (c->err || c->current >= c->results.size() || !c->results[c->current].hasColumns)
		return nullptr;
	MYSQL_RES *res = new st_mysql_res();
	res->set.hasColumns = true;
	res->set.numFields = c->results[c->current].numFields;
	res->set.rows.swap(c->results[c->current].rows);
	res->set.nulls.swap(c->results[c->current].nulls);
	res->rowPtrs.resize(res->set.numFields);
	res->lengths.resize(res->set.numFields);
	return res;
}

int mysql_next_result(MYSQL *mysql)
{
	if (!mysql || !mysql->impl)
		return -1;
	m2sql_conn *c = mysql->impl;
	std::lock_guard<std::recursive_mutex> lock(c->mtx);
	if (c->current + 1 < c->results.size())
	{
		++c->current;
		return 0;
	}
	return -1;
}

void mysql_free_result(MYSQL_RES *result)
{
	delete result;
}

MYSQL_ROW mysql_fetch_row(MYSQL_RES *res)
{
	if (!res || res->cursor >= res->set.rows.size())
		return nullptr;
	auto &row = res->set.rows[res->cursor];
	auto &nulls = res->set.nulls[res->cursor];
	for (size_t i = 0; i < row.size(); ++i)
	{
		res->rowPtrs[i] = nulls[i] ? nullptr : &row[i][0];
		res->lengths[i] = (unsigned long)row[i].size();
	}
	++res->cursor;
	return res->rowPtrs.empty() ? nullptr : res->rowPtrs.data();
}

unsigned long *mysql_fetch_lengths(MYSQL_RES *res)
{
	return res ? res->lengths.data() : nullptr;
}

my_ulonglong mysql_num_rows(MYSQL_RES *res)
{
	return res ? (my_ulonglong)res->set.rows.size() : 0;
}

unsigned int mysql_num_fields(MYSQL_RES *res)
{
	return res ? res->set.numFields : 0;
}

my_ulonglong mysql_affected_rows(MYSQL *mysql)
{
	if (!mysql || !mysql->impl)
		return (my_ulonglong)-1;
	m2sql_conn *c = mysql->impl;
	std::lock_guard<std::recursive_mutex> lock(c->mtx);
	if (c->err || c->current >= c->results.size())
		return (my_ulonglong)-1;
	return c->results[c->current].affected;
}

my_ulonglong mysql_insert_id(MYSQL *mysql)
{
	if (!mysql || !mysql->impl)
		return 0;
	m2sql_conn *c = mysql->impl;
	std::lock_guard<std::recursive_mutex> lock(c->mtx);
	if (c->current >= c->results.size())
		return 0;
	return c->results[c->current].insertId;
}

unsigned int mysql_errno(MYSQL *mysql)
{
	return (mysql && mysql->impl) ? mysql->impl->err : CR_UNKNOWN_ERROR;
}

const char *mysql_error(MYSQL *mysql)
{
	return (mysql && mysql->impl) ? mysql->impl->errmsg.c_str() : "not initialised";
}

unsigned long mysql_thread_id(MYSQL *)
{
	return 1;
}

unsigned long mysql_real_escape_string(MYSQL *, char *to, const char *from, unsigned long length)
{
	char *start = to;
	for (unsigned long i = 0; i < length; ++i)
	{
		char ch = from[i];
		const char *esc = nullptr;
		switch (ch)
		{
			case '\0': esc = "\\0"; break;
			case '\n': esc = "\\n"; break;
			case '\r': esc = "\\r"; break;
			case '\\': esc = "\\\\"; break;
			case '\'': esc = "\\'"; break;
			case '"': esc = "\\\""; break;
			case '\x1a': esc = "\\Z"; break;
		}
		if (esc)
		{
			*to++ = esc[0];
			*to++ = esc[1];
		}
		else
			*to++ = ch;
	}
	*to = '\0';
	return (unsigned long)(to - start);
}

MYSQL_STMT *mysql_stmt_init(MYSQL *)
{
	return new st_mysql_stmt();
}

int mysql_stmt_prepare(MYSQL_STMT *, const char *, unsigned long) { return 1; }
my_bool mysql_stmt_bind_param(MYSQL_STMT *, MYSQL_BIND *) { return 1; }
my_bool mysql_stmt_bind_result(MYSQL_STMT *, MYSQL_BIND *) { return 1; }
int mysql_stmt_execute(MYSQL_STMT *) { return 1; }
int mysql_stmt_store_result(MYSQL_STMT *) { return 1; }
my_ulonglong mysql_stmt_num_rows(MYSQL_STMT *) { return 0; }
int mysql_stmt_fetch(MYSQL_STMT *) { return 1; }
my_bool mysql_stmt_close(MYSQL_STMT *stmt) { delete stmt; return 0; }
unsigned int mysql_stmt_errno(MYSQL_STMT *) { return CR_UNKNOWN_ERROR; }
const char *mysql_stmt_error(MYSQL_STMT *) { return "prepared statements are not supported by the SQLite backend"; }

}
