#!/usr/bin/env python3
"""Validate the sensor framework's four-layer dependency rules."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


LAYER_PREFIXES = {
    "tool": ("util_",),
    "iface": ("hal_", "host_", "port_", "template_"),
    "proto": ("proto_",),
    "func": ("func_",),
}
ALLOWED = {
    "tool": {"tool"},
    "iface": {"tool", "iface"},
    "proto": {"tool", "iface", "proto"},
    "func": {"tool", "proto", "func"},
}
SYMBOL_RULES = {
    "tool": ("hal_", "host_", "port_", "plat_", "proto_", "func_"),
    "iface": ("proto_", "func_"),
    "proto": ("func_",),
    "func": ("hal_", "host_", "port_", "plat_"),
}
INCLUDE_RE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]')
TOKEN_RE = re.compile(r"\b(?:util|hal|host|port|plat|proto|func)_[A-Za-z0-9_]*")


def classify(path: Path, root: Path) -> str | None:
    relative = path.relative_to(root).as_posix()
    if relative.startswith("src/tool/"):
        return "tool"
    if relative.startswith("src/iface/") or relative.startswith("src/ports/"):
        return "iface"
    if relative.startswith("src/proto/"):
        return "proto"
    if relative.startswith("src/func/"):
        return "func"
    if relative == "examples/host_demo/main.c":
        return "func"
    return None


def inferred_layer(name: str) -> str | None:
    base = Path(name).name
    for layer, prefixes in LAYER_PREFIXES.items():
        if base.startswith(prefixes):
            return layer
    return None


def report(path: Path, line: int, rule: str, detail: str) -> str:
    return f"{path.as_posix()}:{line}: {rule}: {detail}"


def validate(root: Path) -> list[str]:
    errors: list[str] = []
    sources = sorted(
        path for path in (root / "src").rglob("*")
        if path.is_file() and path.suffix in {".c", ".h"}
    )
    demo = root / "examples" / "host_demo" / "main.c"
    if demo.is_file():
        sources.append(demo)

    header_layers: dict[str, set[str]] = {}
    for path in sources:
        layer = classify(path, root)
        if layer is None:
            errors.append(report(path.relative_to(root), 1, "layer-location",
                                 "runtime source is outside a configured layer"))
            continue
        if path.suffix == ".h":
            header_layers.setdefault(path.name, set()).add(layer)

    for name, layers in sorted(header_layers.items()):
        if len(layers) > 1:
            errors.append(f"{name}:1: layer-ownership: header occurs in multiple layers: "
                          f"{', '.join(sorted(layers))}")

    for path in sources:
        layer = classify(path, root)
        if layer is None:
            continue
        relative = path.relative_to(root)
        text = path.read_text(encoding="utf-8")
        for line_number, line in enumerate(text.splitlines(), start=1):
            include = INCLUDE_RE.match(line)
            if include:
                name = Path(include.group(1)).name
                candidates = header_layers.get(name, set())
                target = next(iter(candidates)) if len(candidates) == 1 else inferred_layer(name)
                if target is not None and target not in ALLOWED[layer]:
                    errors.append(report(relative, line_number, "illegal-include",
                                         f"{layer} layer may not include {target} header {name}"))

            code = INCLUDE_RE.sub("", line)
            code = re.sub(r"//.*$", "", code)
            for token in TOKEN_RE.findall(code):
                if token.startswith(SYMBOL_RULES[layer]):
                    errors.append(report(relative, line_number, "forbidden-symbol",
                                         f"{layer} layer may not reference {token}"))
    return errors


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", nargs="?", type=Path,
                        default=Path(__file__).resolve().parents[1])
    args = parser.parse_args(argv)
    root = args.root.resolve()
    errors = validate(root)
    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1
    print(f"layer check passed: {root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
