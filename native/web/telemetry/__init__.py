"""Session-telemetry SQLite ingest store (SESSION_TELEMETRY_DESIGN.md §5 / D3).

The DB + writer thread live in ``db.py`` (importable by trace-report.py). The dev
server (native/web/server.py) wires the HTTP routes around it.
"""
