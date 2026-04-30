# Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

from __future__ import annotations

import argparse
import json

from . import WindowTreeClient


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--url", default="local:org.deepin.dde.treeland.debug")
    parser.add_argument("--name", default="WindowTree")
    parser.add_argument("--timeout-ms", type=int, default=30000)
    parser.add_argument("--cursor", action="store_true")
    args = parser.parse_args()

    client = WindowTreeClient(args.url, args.name, args.timeout_ms)
    data = client.cursor_position() if args.cursor else client.get_full_layout_tree()
    print(json.dumps(data, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
