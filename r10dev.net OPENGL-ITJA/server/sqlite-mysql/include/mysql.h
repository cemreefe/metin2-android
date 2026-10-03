/*
 * Minimal MySQL C API surface backed by SQLite, for the embedded (offline)
 * Android server. Only what m2dev-server-src uses is declared here.
 */
#ifndef M2_SQLITE_MYSQL_H
#define M2_SQLITE_MYSQL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef char my_bool;
typedef unsigned long long my_ulonglong;
typedef char **MYSQL_ROW;

struct m2sql_conn;

typedef struct st_mysql
{
	char *host;
	struct m2sql_conn *impl;
} MYSQL;

typedef struct st_mysql_res MYSQL_RES;
typedef struct st_mysql_stmt MYSQL_STMT;

enum enum_field_types
{
	MYSQL_TYPE_DECIMAL, MYSQL_TYPE_TINY, MYSQL_TYPE_SHORT, MYSQL_TYPE_LONG,
	MYSQL_TYPE_FLOAT, MYSQL_TYPE_DOUBLE, MYSQL_TYPE_NULL, MYSQL_TYPE_TIMESTAMP,
	MYSQL_TYPE_LONGLONG, MYSQL_TYPE_INT24, MYSQL_TYPE_DATE, MYSQL_TYPE_TIME,
	MYSQL_TYPE_DATETIME, MYSQL_TYPE_YEAR, MYSQL_TYPE_NEWDATE, MYSQL_TYPE_VARCHAR,
	MYSQL_TYPE_BIT,
	MYSQL_TYPE_BLOB = 252, MYSQL_TYPE_VAR_STRING = 253, MYSQL_TYPE_STRING = 254
};

typedef struct st_mysql_bind
{
	unsigned long *length;
	my_bool *is_null;
	void *buffer;
	my_bool *error;
	enum enum_field_types buffer_type;
	unsigned long buffer_length;
	my_bool is_unsigned;
} MYSQL_BIND;

enum mysql_option
{
	MYSQL_OPT_CONNECT_TIMEOUT,
	MYSQL_SET_CHARSET_NAME,
	MYSQL_OPT_RECONNECT
};

#define CLIENT_MULTI_STATEMENTS (1UL << 16)
#define CLIENT_MULTI_RESULTS (1UL << 17)

MYSQL *mysql_init(MYSQL *mysql);
int mysql_options(MYSQL *mysql, enum mysql_option option, const void *arg);
MYSQL *mysql_real_connect(MYSQL *mysql, const char *host, const char *user, const char *passwd,
	const char *db, unsigned int port, const char *unix_socket, unsigned long clientflag);
void mysql_close(MYSQL *mysql);
int mysql_select_db(MYSQL *mysql, const char *db);
int mysql_ping(MYSQL *mysql);
int mysql_set_character_set(MYSQL *mysql, const char *csname);

int mysql_real_query(MYSQL *mysql, const char *q, unsigned long length);
int mysql_query(MYSQL *mysql, const char *q);
MYSQL_RES *mysql_store_result(MYSQL *mysql);
int mysql_next_result(MYSQL *mysql);
void mysql_free_result(MYSQL_RES *result);
MYSQL_ROW mysql_fetch_row(MYSQL_RES *result);
unsigned long *mysql_fetch_lengths(MYSQL_RES *result);
my_ulonglong mysql_num_rows(MYSQL_RES *res);
unsigned int mysql_num_fields(MYSQL_RES *res);
my_ulonglong mysql_affected_rows(MYSQL *mysql);
my_ulonglong mysql_insert_id(MYSQL *mysql);
unsigned int mysql_errno(MYSQL *mysql);
const char *mysql_error(MYSQL *mysql);
unsigned long mysql_thread_id(MYSQL *mysql);
unsigned long mysql_real_escape_string(MYSQL *mysql, char *to, const char *from, unsigned long length);

/* Prepared statements are not used by db/game; these always fail. */
MYSQL_STMT *mysql_stmt_init(MYSQL *mysql);
int mysql_stmt_prepare(MYSQL_STMT *stmt, const char *query, unsigned long length);
my_bool mysql_stmt_bind_param(MYSQL_STMT *stmt, MYSQL_BIND *bnd);
my_bool mysql_stmt_bind_result(MYSQL_STMT *stmt, MYSQL_BIND *bnd);
int mysql_stmt_execute(MYSQL_STMT *stmt);
int mysql_stmt_store_result(MYSQL_STMT *stmt);
my_ulonglong mysql_stmt_num_rows(MYSQL_STMT *stmt);
int mysql_stmt_fetch(MYSQL_STMT *stmt);
my_bool mysql_stmt_close(MYSQL_STMT *stmt);
unsigned int mysql_stmt_errno(MYSQL_STMT *stmt);
const char *mysql_stmt_error(MYSQL_STMT *stmt);

#ifdef __cplusplus
}
#endif

#endif
