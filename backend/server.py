"""MINAX HTTP API. Local: python backend/server.py. Server: waitress-serve --call backend.server:create_app."""
from __future__ import annotations
import argparse
from contextlib import contextmanager
import hashlib
import hmac
import json
import math
import os
import re
import secrets
import sqlite3
import subprocess
import sys
import threading
import time
import webbrowser
from pathlib import Path
from flask import Flask, request, jsonify, send_from_directory, make_response
from werkzeug.exceptions import HTTPException

ROOT = Path(__file__).resolve().parents[1]
FUNCTIONS = [
    {"name": n, "args": a, "description": d}
    for n, a, d in [
        ("sin", 1, "Sine / 正弦，弧度"), ("cos", 1, "Cosine / 余弦，弧度"),
        ("tan", 1, "Tangent / 正切，弧度"), ("exp", 1, "Exponential / 指数"),
        ("log", 1, "Natural logarithm / 自然对数"), ("sqrt", 1, "Square root / 平方根"),
        ("abs", 1, "Absolute value / 绝对值"), ("pow", 2, "Power / 幂"),
        ("min", 2, "Minimum / 较小值"), ("max", 2, "Maximum / 较大值")]]
IDENTIFIER = re.compile(r"[A-Za-z_][A-Za-z0-9_]*\Z")
USERNAME = re.compile(r"[A-Za-z0-9_]{3,32}\Z")

class ApiError(Exception):
    def __init__(self, message, code="BAD_REQUEST", status=400):
        self.message, self.code, self.status = message, code, status

def create_app(overrides=None):
    cfg = {
        "WEB_DIR": os.environ.get("MINAX_WEB_DIR", str(ROOT / "frontend" / "dist")),
        "DB_PATH": os.environ.get("MINAX_DB_PATH", str(ROOT / "backend" / "data" / "minax.sqlite3")),
        "NATIVE_EXE": os.environ.get("MINAX_NATIVE_EXE", str(ROOT / "bin" / ("minax_calculator.exe" if os.name == "nt" else "minax_calculator"))),
        "SECURE_COOKIE": os.environ.get("MINAX_SECURE_COOKIE", "0") == "1",
        "ALLOWED_ORIGINS": [s.rstrip("/") for s in os.environ.get("MINAX_ALLOWED_ORIGINS", "").split(",") if s],
        "NATIVE_TIMEOUT": 8,
        "RATE_LIMIT": True,
    }
    cfg.update(overrides or {})
    app = Flask(__name__, static_folder=None)
    app.config["MAX_CONTENT_LENGTH"] = 131072
    app.config.update(cfg)
    dbpath = Path(cfg["DB_PATH"])
    dbpath.parent.mkdir(parents=True, exist_ok=True)
    @contextmanager
    def db():
        conn = sqlite3.connect(dbpath, timeout=10)
        conn.row_factory = sqlite3.Row
        try:
            with conn:
                yield conn
        finally:
            conn.close()
    with db() as con:
        con.executescript("""
        PRAGMA journal_mode=WAL;
        CREATE TABLE IF NOT EXISTS users(id INTEGER PRIMARY KEY, name TEXT UNIQUE NOT NULL, salt TEXT NOT NULL, password TEXT NOT NULL);
        CREATE TABLE IF NOT EXISTS sessions(token TEXT PRIMARY KEY, user_id INTEGER, expires REAL NOT NULL);
        CREATE TABLE IF NOT EXISTS history(id INTEGER PRIMARY KEY, owner TEXT NOT NULL, payload TEXT NOT NULL, created REAL NOT NULL);
        CREATE INDEX IF NOT EXISTS history_owner ON history(owner,id);
        """)
    lock = threading.Lock()
    rate = {}
    workers = threading.BoundedSemaphore(2)
    def limited(bucket, limit, seconds):
        if not app.config["RATE_LIMIT"]:
            return
        now = time.monotonic()
        key = (bucket, request.remote_addr)
        with lock:
            for k in list(rate):
                if not rate[k] or now - rate[k][-1] > 600:
                    del rate[k]
            entries = [t for t in rate.get(key, []) if now - t < seconds]
            if len(entries) >= limit or (key not in rate and len(rate) >= 10000):
                raise ApiError("Too many requests. Try again later.", "RATE_LIMIT", 429)
            rate[key] = entries + [now]
    def body():
        data = request.get_json(silent=True)
        if not isinstance(data, dict):
            raise ApiError("Expected a JSON object.")
        return data
    def session(required=True):
        token = request.cookies.get("minax_session", "")
        hashed = hashlib.sha256(token.encode()).hexdigest()
        with db() as con:
            row = con.execute(
                "SELECT sessions.*, users.name FROM sessions LEFT JOIN users ON users.id=sessions.user_id WHERE token=? AND expires>?",
                (hashed, time.time())).fetchone()
        if not row:
            if required:
                raise ApiError("Please sign in or continue as guest.", "UNAUTHENTICATED", 401)
            return None
        return {"id": row["user_id"], "name": row["name"] or "Guest",
                "guest": row["user_id"] is None,
                "owner": "u:" + str(row["user_id"]) if row["user_id"] else "g:" + hashed,
                "token": hashed}
    def public_user(s):
        return {k: s[k] for k in ("id", "name", "guest")}
    def new_session(uid, name):
        token = secrets.token_urlsafe(32)
        hashed = hashlib.sha256(token.encode()).hexdigest()
        age = 86400 if uid is None else 604800
        old = request.cookies.get("minax_session", "")
        with db() as con:
            con.execute("DELETE FROM sessions WHERE expires<=?", (time.time(),))
            if old:
                con.execute("DELETE FROM sessions WHERE token=?", (hashlib.sha256(old.encode()).hexdigest(),))
            con.execute("INSERT INTO sessions VALUES(?,?,?)", (hashed, uid, time.time()+age))
        response = jsonify(user={"id": uid, "name": name, "guest": uid is None})
        response.set_cookie("minax_session", token, max_age=age, httponly=True,
                            secure=cfg["SECURE_COOKIE"], samesite="Lax", path="/")
        return response
    def password_hash(password, salt):
        return hashlib.scrypt(password.encode(), salt=bytes.fromhex(salt), n=16384, r=8, p=1).hex()

    @app.before_request
    def guard():
        if not request.path.startswith("/api/"):
            return None
        origin = request.headers.get("Origin")
        own = request.host_url.rstrip("/")
        if origin and origin != own and origin not in cfg["ALLOWED_ORIGINS"]:
            raise ApiError("Origin not allowed.", "ORIGIN_DENIED", 403)
        if request.method == "OPTIONS":
            return "", 204
        if request.method in ("POST", "DELETE", "PUT"):
            # Cookie + 非简单自定义头 + Origin 校验，阻止跨站表单触发操作。
            if request.headers.get("X-Minax-Client") != "web":
                raise ApiError("Missing client request header.", "CSRF_REJECTED", 403)
            if not request.is_json:
                raise ApiError("Use application/json.")
    @app.after_request
    def headers(response):
        response.headers["X-Content-Type-Options"] = "nosniff"
        response.headers["X-Frame-Options"] = "DENY"
        response.headers["Referrer-Policy"] = "same-origin"
        if request.path.startswith("/api/"):
            response.headers["Cache-Control"] = "no-store"
        origin = request.headers.get("Origin")
        if origin in cfg["ALLOWED_ORIGINS"]:
            response.headers["Access-Control-Allow-Origin"] = origin
            response.headers["Access-Control-Allow-Credentials"] = "true"
            response.headers["Vary"] = "Origin"
            response.headers["Access-Control-Allow-Headers"] = "Content-Type, X-Minax-Client"
            response.headers["Access-Control-Allow-Methods"] = "GET, POST, DELETE, OPTIONS"
        return response
    @app.errorhandler(ApiError)
    def api_error(error):
        return jsonify(error={"code": error.code, "message": error.message}), error.status
    @app.errorhandler(HTTPException)
    def http_error(error):
        return jsonify(error={"code": "HTTP_ERROR", "message": error.description}), error.code
    @app.errorhandler(Exception)
    def internal_error(error):
        app.logger.exception("Unhandled request failure")
        return jsonify(error={"code": "INTERNAL_ERROR", "message": "Internal server error."}), 500

    @app.get("/api/health")
    def health():
        return jsonify(status="ok", evaluator=Path(cfg["NATIVE_EXE"]).is_file(), optimizer=False,
                       minaParser=False, version="0.1.0", functions=FUNCTIONS)
    @app.get("/api/auth/me")
    def me():
        s = session(False)
        return jsonify(user=public_user(s) if s else None)
    @app.post("/api/auth/guest")
    def guest():
        limited("auth", 30, 300)
        return new_session(None, "Guest")
    @app.post("/api/auth/register")
    def register():
        limited("auth", 30, 300)
        data = body()
        name, password = data.get("username"), data.get("password")
        if not isinstance(name, str) or not USERNAME.fullmatch(name):
            raise ApiError("Username: 3-32 letters, digits or underscores.")
        if not isinstance(password, str) or not 8 <= len(password) <= 128:
            raise ApiError("Password must contain 8-128 characters.")
        salt = secrets.token_hex(16)
        hashed = password_hash(password, salt)
        try:
            with db() as con:
                cur = con.execute("INSERT INTO users(name,salt,password) VALUES(?,?,?)",
                                  (name, salt, hashed))
                uid = cur.lastrowid
        except sqlite3.IntegrityError:
            raise ApiError("Username is already taken.", "USERNAME_EXISTS", 409)
        return new_session(uid, name)
    @app.post("/api/auth/login")
    def login():
        limited("auth", 30, 300)
        data = body()
        name, password = data.get("username"), data.get("password")
        if not isinstance(name, str) or not isinstance(password, str) or len(password)>128:
            raise ApiError("Invalid username or password.", "INVALID_CREDENTIALS", 401)
        with db() as con:
            row = con.execute("SELECT * FROM users WHERE name=?", (name,)).fetchone()
        candidate = password_hash(password, row["salt"] if row else "0"*32)
        if not row or not hmac.compare_digest(candidate, row["password"]):
            raise ApiError("Invalid username or password.", "INVALID_CREDENTIALS", 401)
        return new_session(row["id"], row["name"])
    @app.post("/api/auth/logout")
    def logout():
        s = session(False)
        if s:
            with db() as con:
                con.execute("DELETE FROM sessions WHERE token=?", (s["token"],))
        response = jsonify(ok=True)
        response.delete_cookie("minax_session", path="/", secure=cfg["SECURE_COOKIE"], httponly=True, samesite="Lax")
        return response
    @app.get("/api/history")
    def history():
        s = session()
        with db() as con:
            rows = con.execute("SELECT id,payload,created FROM history WHERE owner=? ORDER BY id DESC LIMIT 30",
                               (s["owner"],)).fetchall()
        return jsonify(items=[{"id":r["id"],"created":r["created"],**json.loads(r["payload"])} for r in rows])
    @app.delete("/api/history")
    def delete_history():
        s = session()
        with db() as con:
            con.execute("DELETE FROM history WHERE owner=?", (s["owner"],))
        return jsonify(ok=True)
    @app.post("/api/optimize")
    def optimize():
        session()
        raise ApiError("The optimizer is not implemented yet.", "NOT_IMPLEMENTED", 501)
    @app.post("/api/evaluate")
    def evaluate():
        s = session()
        limited("evaluate", 60, 60)
        data = body()
        expression = data.get("expression")
        variables = data.get("variables")
        if not isinstance(expression, str) or not expression.strip() or len(expression)>8192:
            raise ApiError("Expression must contain 1-8192 characters.")
        if any(ord(ch)<32 and ch not in "\t\n\r" for ch in expression):
            raise ApiError("Invalid control character in expression.")
        # 一行传给 CLI，按已有编译器的空白 Token 约定规范化。
        expression = " ".join(expression.split())
        tokens = expression.split()
        if len(tokens)>512:
            raise ApiError("Expression exceeds 512 tokens.")
        depth = 0
        for tok in tokens:
            depth += (tok == "(") - (tok == ")")
            if depth>64:
                raise ApiError("Parenthesis nesting exceeds 64.")
        if not isinstance(variables, list) or len(variables)>100:
            raise ApiError("Provide at most 100 variables.")
        names, lines, checked = set(), [], []
        for item in variables:
            if not isinstance(item, dict):
                raise ApiError("Invalid variable entry.")
            name, value = item.get("name"), item.get("value")
            if not isinstance(name, str) or not IDENTIFIER.fullmatch(name) or name in names:
                raise ApiError("Invalid or duplicate variable name.")
            if isinstance(value, bool) or not isinstance(value, (int,float)):
                raise ApiError("Variable values must be numbers.")
            try:
                finite = math.isfinite(value)
            except OverflowError:
                finite = False
            if not finite:
                raise ApiError("Variable values must be finite.")
            names.add(name)
            lines.append(name + " " + format(float(value), ".17g"))
            checked.append({"name":name,"value":value})
        exe = Path(cfg["NATIVE_EXE"]).resolve()
        if not exe.is_file():
            raise ApiError("C++ evaluator executable not found. Build minax_calculator first.",
                           "EVALUATOR_UNAVAILABLE", 503)
        if not workers.acquire(blocking=False):
            raise ApiError("Evaluator is busy. Try again shortly.", "BUSY", 429)
        started = time.perf_counter()
        try:
            payload = str(len(lines)) + "\n" + "\n".join(lines)
            if lines: payload += "\n"
            payload += expression + "\n"
            proc = subprocess.run([str(exe)], input=payload, text=True, encoding="utf-8",
                                  errors="replace", capture_output=True, cwd=exe.parent,
                                  timeout=cfg["NATIVE_TIMEOUT"], shell=False,
                                  creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
        except subprocess.TimeoutExpired:
            raise ApiError("Evaluation timed out.", "EVALUATION_TIMEOUT", 408)
        except OSError:
            raise ApiError("Evaluator could not start. Check executable and runtime DLLs.",
                           "EVALUATOR_UNAVAILABLE", 503)
        finally:
            workers.release()
        if proc.returncode != 0:
            diagnostic = (proc.stderr.strip() or "Native evaluator failed.")[:3000]
            raise ApiError(diagnostic, "EVALUATION_FAILED", 422)
        matches = re.findall(r"^Result = ([^\r\n]+)$", proc.stdout, flags=re.MULTILINE)
        if len(matches)!=1:
            raise ApiError("Unexpected evaluator output; use the provided root main.cpp.",
                           "NATIVE_PROTOCOL_ERROR", 502)
        try: value = float(matches[0])
        except ValueError:
            raise ApiError("Invalid native result.", "NATIVE_PROTOCOL_ERROR", 502)
        if not math.isfinite(value):
            raise ApiError("Non-finite native result.", "EVALUATION_FAILED", 422)
        result = {"expression":expression,"variables":checked,"value":value,
                  "displayValue":matches[0].strip(),
                  "durationMs":round((time.perf_counter()-started)*1000,2)}
        with db() as con:
            con.execute("INSERT INTO history(owner,payload,created) VALUES(?,?,?)",
                        (s["owner"], json.dumps(result,ensure_ascii=False),time.time()))
            con.execute("DELETE FROM history WHERE owner=? AND id NOT IN "
                        "(SELECT id FROM history WHERE owner=? ORDER BY id DESC LIMIT 30)",
                        (s["owner"],s["owner"]))
        return jsonify(result)
    @app.get("/")
    def index():
        web = Path(cfg["WEB_DIR"])
        if not (web/"index.html").is_file():
            raise ApiError("Frontend is not built. Run npm run build in frontend.", "WEB_NOT_BUILT", 503)
        return send_from_directory(web, "index.html")
    @app.get("/<path:filename>")
    def static_file(filename):
        if filename.startswith("api/"):
            raise ApiError("Endpoint not found.", "NOT_FOUND", 404)
        response = make_response(send_from_directory(cfg["WEB_DIR"], filename))
        if filename == "config.js":
            response.headers["Cache-Control"] = "no-store"
        return response
    return app

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default=os.environ.get("MINAX_HOST","127.0.0.1"))
    parser.add_argument("--port", type=int, default=int(os.environ.get("MINAX_PORT","8787")))
    parser.add_argument("--open-browser", action="store_true")
    args = parser.parse_args()
    # 打包后从 exe 同目录读取 web/native，数据放用户可写目录。
    if getattr(sys,"frozen",False):
        base=Path(sys.executable).resolve().parent
        os.environ.setdefault("MINAX_WEB_DIR",str(base/"web"))
        os.environ.setdefault("MINAX_NATIVE_EXE",str(base/"native"/"minax_calculator.exe"))
        data=Path(os.environ.get("LOCALAPPDATA",str(Path.home())))/"MINAX"
        os.environ.setdefault("MINAX_DB_PATH",str(data/"minax.sqlite3"))
    from waitress import create_server
    server=create_server(create_app(),host=args.host,port=args.port,threads=8)
    url="http://127.0.0.1:"+str(server.effective_port)
    print("MINAX_READY:"+url,flush=True)
    if args.open_browser:
        webbrowser.open(url)
    server.run()

if __name__ == "__main__":
    main()

