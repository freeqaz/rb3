#!/usr/bin/env python3
"""
MCP client for Ghidra pyghidra-mcp server (RB3 Wii).

Handles session initialization, JSON-RPC formatting, and response parsing.

Points to port 8001 (RB3). DC3 uses port 8000.
"""

import json
import os
import sys
from pathlib import Path
from typing import Any, Optional

try:
    import requests
except ImportError:
    print("Error: requests library not found. Install with: pip install requests", file=sys.stderr)
    sys.exit(1)


# Default configuration — port 8001 for RB3 (DC3 is on 8000)
MCP_URL = "http://127.0.0.1:8001/mcp"
SESSION_CACHE_FILE = Path("/tmp/claude/ghidra_mcp_session_rb3.txt")
DEFAULT_BINARY = None  # Resolved dynamically via list_binaries()


class MCPError(Exception):
    """Exception raised for MCP-related errors."""
    pass


class MCPClient:
    """Client for communicating with Ghidra MCP server (RB3)."""

    def __init__(self, url: str = MCP_URL, binary: Optional[str] = DEFAULT_BINARY):
        self.url = url
        self._binary = binary
        self._binary_resolved = binary is not None
        self.session_id: Optional[str] = None
        self._request_id = 0

    @property
    def binary(self) -> str:
        """Get binary name, resolving dynamically if needed."""
        if not self._binary_resolved:
            self._resolve_binary()
        return self._binary or "band_r_wii.elf"  # Fallback

    def _resolve_binary(self) -> None:
        """Resolve binary name by querying list_binaries."""
        if self._binary_resolved:
            return

        try:
            if not self.session_id:
                self.initialize()

            binaries = self.list_binaries()

            if isinstance(binaries, dict) and "binaries" in binaries:
                binary_list = binaries["binaries"]
            elif isinstance(binaries, list):
                binary_list = binaries
            else:
                binary_list = []

            # Look for the ELF first, then DOL
            for pattern in ["band_r_wii.elf", "band_r_wii", "main.dol"]:
                for binary in binary_list:
                    name = binary.get("name", "") if isinstance(binary, dict) else str(binary)
                    if pattern in name:
                        self._binary = name.lstrip("/")
                        self._binary_resolved = True
                        return

            # If no match found, use first available binary
            if binary_list:
                first = binary_list[0]
                name = first.get("name", "") if isinstance(first, dict) else str(first)
                self._binary = name.lstrip("/")

            self._binary_resolved = True

        except MCPError:
            self._binary = "band_r_wii.elf"
            self._binary_resolved = True

    def _next_id(self) -> int:
        self._request_id += 1
        return self._request_id

    def _load_cached_session(self) -> Optional[str]:
        try:
            if SESSION_CACHE_FILE.exists():
                return SESSION_CACHE_FILE.read_text().strip()
        except (IOError, OSError):
            pass
        return None

    def _save_session(self, session_id: str) -> None:
        try:
            SESSION_CACHE_FILE.parent.mkdir(parents=True, exist_ok=True)
            SESSION_CACHE_FILE.write_text(session_id)
        except (IOError, OSError) as e:
            print(f"Warning: Could not cache session: {e}", file=sys.stderr)

    def _parse_sse_response(self, response: requests.Response) -> dict:
        """Parse SSE-formatted response and extract JSON data."""
        content = response.text

        data_line = None
        for line in content.split('\n'):
            line = line.strip()
            if line.startswith('data:'):
                data_line = line[5:].strip()
                break

        if data_line:
            try:
                return json.loads(data_line)
            except json.JSONDecodeError as e:
                raise MCPError(f"Failed to parse SSE data as JSON: {e}\nData: {data_line}")

        try:
            return response.json()
        except json.JSONDecodeError as e:
            raise MCPError(f"Failed to parse response as JSON: {e}\nResponse: {content[:500]}")

    def _make_request(self, method: str, params: Optional[dict] = None, timeout: int = 60) -> dict:
        headers = {
            "Content-Type": "application/json",
            "Accept": "application/json, text/event-stream",
        }

        if self.session_id:
            headers["mcp-session-id"] = self.session_id

        payload = {
            "jsonrpc": "2.0",
            "id": self._next_id(),
            "method": method,
        }
        if params:
            payload["params"] = params

        try:
            response = requests.post(self.url, headers=headers, json=payload, timeout=timeout)
        except requests.exceptions.ConnectionError:
            raise MCPError(
                f"Could not connect to MCP server at {self.url}. "
                f"Is pyghidra-mcp running? Check with: ./tools/ghidra/pyghidra-service.sh status"
            )
        except requests.exceptions.Timeout:
            raise MCPError("Request timed out")
        except requests.exceptions.RequestException as e:
            raise MCPError(f"Request failed: {e}")

        new_session = response.headers.get("mcp-session-id")
        if new_session:
            self.session_id = new_session
            self._save_session(new_session)

        if response.status_code != 200:
            raise MCPError(f"HTTP {response.status_code}: {response.text[:500]}")

        return self._parse_sse_response(response)

    def _send_initialized_notification(self) -> None:
        headers = {
            "Content-Type": "application/json",
            "Accept": "application/json, text/event-stream",
        }
        if self.session_id:
            headers["mcp-session-id"] = self.session_id

        payload = {
            "jsonrpc": "2.0",
            "method": "notifications/initialized",
        }

        try:
            requests.post(self.url, headers=headers, json=payload, timeout=10)
        except requests.exceptions.RequestException:
            pass

    def initialize(self, force: bool = False) -> str:
        """Initialize MCP session. Returns session ID."""
        if not force:
            cached = self._load_cached_session()
            if cached:
                self.session_id = cached
                try:
                    result = self._make_request("initialize", {
                        "protocolVersion": "2024-11-05",
                        "capabilities": {},
                        "clientInfo": {"name": "ghidra-rb3-cli", "version": "1.0"}
                    })
                    if "error" not in result and self.session_id:
                        self._send_initialized_notification()
                        return self.session_id
                except MCPError:
                    pass
                self.session_id = None

        result = self._make_request("initialize", {
            "protocolVersion": "2024-11-05",
            "capabilities": {},
            "clientInfo": {"name": "ghidra-rb3-cli", "version": "1.0"}
        })

        if "error" in result:
            raise MCPError(f"Initialize failed: {result['error']}")

        if not self.session_id:
            raise MCPError("Server did not return session ID")

        self._send_initialized_notification()
        return self.session_id

    def call_tool(self, tool_name: str, arguments: dict, timeout: int = 60) -> Any:
        if not self.session_id:
            self.initialize()

        result = self._make_request("tools/call", {
            "name": tool_name,
            "arguments": arguments
        }, timeout=timeout)

        if "error" in result:
            error = result["error"]
            if isinstance(error, dict):
                msg = error.get("message", str(error))
            else:
                msg = str(error)
            raise MCPError(f"Tool call failed: {msg}")

        result_data = result.get("result", {})

        if result_data.get("isError"):
            content = result_data.get("content", [])
            if isinstance(content, list) and len(content) > 0:
                first = content[0]
                if isinstance(first, dict) and "text" in first:
                    raise MCPError(first["text"])
            raise MCPError("Tool returned an error")

        if "structuredContent" in result_data:
            return result_data["structuredContent"]
        elif "content" in result_data:
            content = result_data["content"]
            if isinstance(content, list) and len(content) > 0:
                first = content[0]
                if isinstance(first, dict) and "text" in first:
                    try:
                        return json.loads(first["text"])
                    except json.JSONDecodeError:
                        return first["text"]
                return first
            return content

        return result_data

    # ---- Convenience methods ----

    def decompile_function(self, name_or_address: str) -> dict:
        """Decompile a function by name or address."""
        return self.call_tool("decompile_function", {
            "binary_name": self.binary,
            "name_or_address": name_or_address
        })

    def search_symbols(self, query: str, offset: int = 0, limit: int = 25) -> dict:
        """Search for symbols by name."""
        return self.call_tool("search_symbols_by_name", {
            "binary_name": self.binary,
            "query": query,
            "offset": offset,
            "limit": limit
        })

    def search_strings(self, query: str, limit: int = 100) -> dict:
        """Search for strings in the binary."""
        return self.call_tool("search_strings", {
            "binary_name": self.binary,
            "query": query,
            "limit": limit
        })

    def search_code(self, query: str, limit: int = 5) -> dict:
        """Search for code patterns."""
        return self.call_tool("search_code", {
            "binary_name": self.binary,
            "query": query,
            "limit": limit
        })

    def list_xrefs(self, name_or_address: str) -> dict:
        """List cross-references to/from a symbol or address."""
        return self.call_tool("list_cross_references", {
            "binary_name": self.binary,
            "name_or_address": name_or_address
        })

    def get_callgraph(self, function_name: str, direction: str = "calling") -> dict:
        """Generate call graph for a function."""
        return self.call_tool("gen_callgraph", {
            "binary_name": self.binary,
            "function_name": function_name,
            "direction": direction
        })

    def list_exports(self, query: str = ".*", offset: int = 0, limit: int = 25) -> dict:
        return self.call_tool("list_exports", {
            "binary_name": self.binary,
            "query": query,
            "offset": offset,
            "limit": limit
        })

    def list_imports(self, query: str = ".*", offset: int = 0, limit: int = 25) -> dict:
        return self.call_tool("list_imports", {
            "binary_name": self.binary,
            "query": query,
            "offset": offset,
            "limit": limit
        })

    def read_bytes(self, address: str, size: int = 32) -> dict:
        return self.call_tool("read_bytes", {
            "binary_name": self.binary,
            "address": address,
            "size": size
        })

    def list_structures(self, query: str = ".*", offset: int = 0, limit: int = 100) -> dict:
        return self.call_tool("list_structures", {
            "binary_name": self.binary,
            "query": query,
            "offset": offset,
            "limit": limit
        })

    def list_binaries(self) -> dict:
        """List all project binaries."""
        return self.call_tool("list_project_binaries", {})


def create_client(binary: str = DEFAULT_BINARY) -> MCPClient:
    """Create and initialize an MCP client."""
    client = MCPClient(binary=binary)
    client.initialize()
    return client


if __name__ == "__main__":
    try:
        client = create_client()
        print(f"Connected to RB3 MCP server, session: {client.session_id}")
        binaries = client.list_binaries()
        print(f"Available binaries: {json.dumps(binaries, indent=2)}")
    except MCPError as e:
        print(f"Error: {e}", file=sys.stderr)
        print("\nTroubleshooting:", file=sys.stderr)
        print("  1. Check if service is running: ./tools/ghidra/pyghidra-service.sh status", file=sys.stderr)
        print("  2. Start service if needed: ./tools/ghidra/pyghidra-service.sh start", file=sys.stderr)
        print("  3. View logs: ./tools/ghidra/pyghidra-service.sh logs", file=sys.stderr)
        print(f"  4. Verify port {MCP_URL} is correct (RB3 uses port 8001)", file=sys.stderr)
        sys.exit(1)
