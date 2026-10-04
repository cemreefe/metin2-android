// Host test for the shim's MySQL enum semantics on SQLite.
//
//   tests/run.sh
//
// The game stores enum columns as text ('INVENTORY', ...) but queries them
// with the numbers from EWindows, so every number that meets an enum column
// has to be translated. Without that, `WHERE window < 3` matches nothing and
// the server loads an empty inventory.
#include "mysql.h"
#include "sqlite3.h"

#include <cstdio>
#include <string>
#include <sys/stat.h>

namespace
{
const char *const kEnumValues = "INVENTORY,EQUIPMENT,SAFEBOX,MALL,DRAGON_SOUL_INVENTORY,BELT_INVENTORY";

int g_failed = 0;

std::string QueryRows(MYSQL *m, const char *sql)
{
	if (mysql_query(m, sql))
		return std::string("ERROR: ") + mysql_error(m);
	MYSQL_RES *res = mysql_store_result(m);
	if (!res)
		return "<no result>";
	std::string out;
	while (MYSQL_ROW row = mysql_fetch_row(res))
	{
		const unsigned int fields = mysql_num_fields(res);
		for (unsigned int i = 0; i < fields; ++i)
		{
			if (i)
				out += "|";
			out += row[i] ? row[i] : "NULL";
		}
		out += ";";
	}
	mysql_free_result(res);
	return out;
}

void Check(MYSQL *m, const char *sql, const char *want)
{
	const std::string got = QueryRows(m, sql);
	if (got == want)
		return;
	printf("FAIL %s\n  got  %s\n  want %s\n", sql, got.c_str(), want);
	++g_failed;
}

void Exec(MYSQL *m, const char *sql)
{
	if (mysql_query(m, sql))
	{
		printf("FAIL %s\n  %s\n", sql, mysql_error(m));
		++g_failed;
	}
}

bool WriteFixtureDatabase(const std::string &dir)
{
	mkdir(dir.c_str(), 0755);
	sqlite3 *db = nullptr;
	if (sqlite3_open((dir + "/player.sqlite3").c_str(), &db) != SQLITE_OK)
		return false;
	const std::string sql =
		"DROP TABLE IF EXISTS t_enum;"
		"CREATE TABLE t_enum (id INTEGER PRIMARY KEY, owner_id INTEGER,"
		" \"window\" TEXT NOT NULL DEFAULT 'INVENTORY' COLLATE NOCASE, pos INTEGER);"
		"CREATE TABLE IF NOT EXISTS _m2_enum (col TEXT, vals TEXT, is_set INTEGER);"
		"DELETE FROM _m2_enum WHERE col='window';"
		"INSERT INTO _m2_enum (col, vals, is_set) VALUES ('window', '" + std::string(kEnumValues) + "', 0);";
	const bool ok = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK;
	sqlite3_close(db);
	return ok;
}
}

int main(int argc, char **argv)
{
	const std::string dir = argc > 1 ? argv[1] : "sqlite-test";
	if (!WriteFixtureDatabase(dir))
	{
		printf("could not create the fixture database in %s\n", dir.c_str());
		return 1;
	}
	setenv("M2_SQLITE_DIR", dir.c_str(), 1);

	MYSQL *m = mysql_init(nullptr);
	if (!mysql_real_connect(m, "sqlite", "", "", "player", 0, nullptr, 0))
	{
		printf("connect failed: %s\n", mysql_error(m));
		return 1;
	}

	// Rows as the shipped dump stores them: enum names, not numbers.
	Exec(m, "INSERT INTO t_enum (id, owner_id, window, pos) VALUES (1, 2, 'INVENTORY', 0)");
	Exec(m, "INSERT INTO t_enum (id, owner_id, window, pos) VALUES (2, 2, 'EQUIPMENT', 1)");
	Exec(m, "INSERT INTO t_enum (id, owner_id, window, pos) VALUES (3, 2, 'SAFEBOX', 2)");
	Exec(m, "INSERT INTO t_enum (id, owner_id, window, pos) VALUES (4, 2, 'DRAGON_SOUL_INVENTORY', 3)");

	// The server's item load: window < SAFEBOX(3) or window = DRAGON_SOUL_INVENTORY(5).
	Check(m, "SELECT id,window+0 FROM t_enum WHERE owner_id=2 AND (window < 3 or window = 5) ORDER BY id",
			"1|1;2|2;4|5;");

	// A numeric write stores the enum's name, as MySQL does.
	Exec(m, "INSERT INTO t_enum (id, owner_id, window, pos) VALUES (5, 7, 2, 4)");
	Check(m, "SELECT window, window+0 FROM t_enum WHERE id = 5", "EQUIPMENT|2;");

	Exec(m, "UPDATE t_enum SET window=1, pos=9 WHERE id = 5");
	Check(m, "SELECT window, pos FROM t_enum WHERE id = 5", "INVENTORY|9;");

	// A plain column compared with a number keeps its plain comparison.
	Check(m, "SELECT id FROM t_enum WHERE pos = 9", "5;");

	// MySQL orders an enum by its index, not alphabetically.
	Check(m, "SELECT window FROM t_enum WHERE owner_id=2 ORDER BY window",
			"INVENTORY;EQUIPMENT;SAFEBOX;DRAGON_SOUL_INVENTORY;");

	// INSERT ... SET and multi-row VALUES go through the same path.
	Exec(m, "INSERT INTO t_enum SET id=6, owner_id=8, window=3, pos=0");
	Check(m, "SELECT window FROM t_enum WHERE id = 6", "SAFEBOX;");
	Exec(m, "REPLACE INTO t_enum (id, owner_id, window, pos) VALUES (7, 8, 4, 0), (8, 8, 6, 1)");
	Check(m, "SELECT window FROM t_enum WHERE id IN (7, 8) ORDER BY id", "MALL;BELT_INVENTORY;");

	mysql_close(m);
	printf(g_failed ? "%d check(s) failed\n" : "all checks passed\n", g_failed);
	return g_failed ? 1 : 0;
}
