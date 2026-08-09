#!/usr/bin/env python3
"""Negative fixtures for the production layer checker."""

from __future__ import annotations

import importlib.util
import sys
from pathlib import Path


def load_checker(path: Path):
    spec = importlib.util.spec_from_file_location("sensor_check_layers", path)
    if spec is None or spec.loader is None:
        raise RuntimeError("unable to load layer checker")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def write(root: Path, relative: str, text: str) -> Path:
    path = root / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")
    return path


def require_matching(errors: list[str], needle: str) -> None:
    if not any(needle in error for error in errors):
        raise AssertionError(f"expected {needle!r} in {errors!r}")


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit("usage: test_check_layers.py CHECKER FIXTURE_ROOT")
    checker = load_checker(Path(sys.argv[1]).resolve())
    fixture_root = Path(sys.argv[2]).resolve()
    fixture_root.mkdir(parents=True, exist_ok=True)

    root = fixture_root / "case_include"
    write(root, "src/func/func_bad_include.c", '#include "hal_i2c.h"\n')
    require_matching(checker.validate(root), "illegal-include")

    root = fixture_root / "case_token"
    write(root, "src/func/func_bad_token.c", "void *bad = hal_i2c_open;\n")
    require_matching(checker.validate(root), "forbidden-symbol")

    root = fixture_root / "case_composition"
    write(root, "src/tool/util_status.h", "/* tool */\n")
    write(root, "src/iface/hal_i2c.h", "/* iface */\n")
    write(root, "src/proto/proto_temp.h", "/* proto */\n")
    write(root, "src/func/func_sensor.h", "/* func */\n")
    demo = write(
        root,
        "examples/host_demo/main.c",
        '#include "util_status.h"\n#include "hal_i2c.h"\n'
        '#include "proto_temp.h"\n#include "func_sensor.h"\n'
        "void compose(void) { util_status_t *a; hal_i2c_t *b; "
        "proto_temp_t *c; func_sensor_t *d; (void)a; (void)b; "
        "(void)c; (void)d; }\n",
    )
    if checker.classify(demo, root) != "composition":
        raise AssertionError("host_demo must be the composition whitelist")
    if checker.classify(root / "examples/other/main.c", root) is not None:
        raise AssertionError("arbitrary example must not become composition")
    errors = checker.validate(root)
    if errors:
        raise AssertionError(f"whitelisted composition rejected: {errors!r}")

    write(
        root,
        "examples/host_demo/main.c",
        "void compose(void) { plat_vendor_init(); port_vendor_global = 1; }\n",
    )
    errors = checker.validate(root)
    require_matching(errors, "forbidden-symbol: composition layer may not reference plat_vendor_init")
    require_matching(errors, "forbidden-symbol: composition layer may not reference port_vendor_global")

    print("layer checker negative fixtures passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
