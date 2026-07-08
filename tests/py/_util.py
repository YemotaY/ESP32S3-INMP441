"""Shared helpers for the plain-script Python tests (no pytest dependency)."""
import os
import sys

# Make `kwslib` importable from tests/py/.
_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(_ROOT, "kws-framework"))


class Checker:
    def __init__(self, name):
        self.name = name
        self.checks = 0
        self.fails = 0

    def check(self, cond, msg=""):
        self.checks += 1
        if not cond:
            self.fails += 1
            print(f"  FAIL: {msg}")

    def close(self):
        status = "FAILED" if self.fails else "ok"
        print(f"{status}: {self.name}: {self.checks} checks, {self.fails} failures")
        return 1 if self.fails else 0
