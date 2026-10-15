PRAGMA foreign_keys = ON;
PRAGMA journal_mode = WAL;
CREATE TABLE IF NOT EXISTS users (
 id INTEGER PRIMARY KEY AUTOINCREMENT,
 username TEXT NOT NULL UNIQUE COLLATE BINARY,
 password_hash TEXT NOT NULL,
 created_at INTEGER NOT NULL
);
-- 游客没有 user_id，身份和历史随会话过期；账号历史属于 user_id。
CREATE TABLE IF NOT EXISTS sessions (
 id_hash TEXT PRIMARY KEY,
 user_id INTEGER REFERENCES users(id) ON DELETE CASCADE,
 csrf TEXT NOT NULL,
 expires_at INTEGER NOT NULL,
 created_at INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS sessions_expiry ON sessions(expires_at);
CREATE TABLE IF NOT EXISTS calculations (
 id INTEGER PRIMARY KEY AUTOINCREMENT,
 user_id INTEGER REFERENCES users(id) ON DELETE CASCADE,
 guest_session TEXT REFERENCES sessions(id_hash) ON DELETE CASCADE,
 expression TEXT NOT NULL,
 variables_json TEXT NOT NULL,
 result_text TEXT NOT NULL,
 duration_ms REAL NOT NULL,
 created_at INTEGER NOT NULL,
 CHECK ((user_id IS NOT NULL AND guest_session IS NULL) OR (user_id IS NULL AND guest_session IS NOT NULL))
);
CREATE INDEX IF NOT EXISTS calculations_user ON calculations(user_id, id DESC);
CREATE INDEX IF NOT EXISTS calculations_guest ON calculations(guest_session, id DESC);
CREATE TABLE IF NOT EXISTS rate_limits (
 bucket TEXT PRIMARY KEY,
 window_start INTEGER NOT NULL,
 attempts INTEGER NOT NULL
);
PRAGMA user_version = 1;
