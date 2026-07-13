# Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

from __future__ import annotations

import os
import pathlib
import shutil
import subprocess

from pybind11.setup_helpers import Pybind11Extension, build_ext
from setuptools import setup
from setuptools.command.sdist import sdist


ROOT = pathlib.Path(__file__).parent.resolve()
PACKAGED_REP_FILE = ROOT / "treeland_windowtree" / "treelandwindowtree.rep"
SOURCE_REP_FILE = (
    ROOT / ".." / ".." / "src" / "modules" / "resource" / "treelandwindowtree.rep"
).resolve()
REP_FILE = PACKAGED_REP_FILE if PACKAGED_REP_FILE.is_file() else SOURCE_REP_FILE


def _command_from_env(name: str, default: str) -> str:
    return os.environ.get(name, default)


def _pkg_config(args: list[str]) -> list[str]:
    output = subprocess.check_output(["pkg-config", *args], text=True)
    return output.split()


class Sdist(sdist):
    def make_release_tree(self, base_dir: str, files: list[str]) -> None:
        super().make_release_tree(base_dir, files)
        packaged_rep_file = pathlib.Path(base_dir) / PACKAGED_REP_FILE.relative_to(ROOT)
        packaged_rep_file.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(SOURCE_REP_FILE, packaged_rep_file)


class BuildExt(build_ext):
    def build_extensions(self) -> None:
        build_temp = pathlib.Path(self.build_temp).resolve()
        generated_dir = build_temp / "generated"
        generated_dir.mkdir(parents=True, exist_ok=True)

        repc = _command_from_env("REPC", "/usr/lib/qt6/libexec/repc")
        moc = _command_from_env("MOC", "/usr/lib/qt6/libexec/moc")
        if not pathlib.Path(repc).exists():
            repc = shutil.which("repc") or repc
        if not pathlib.Path(moc).exists():
            moc = shutil.which("moc") or moc

        cflags = _pkg_config(["--cflags", "Qt6Core", "Qt6RemoteObjects"])
        libs = _pkg_config(["--libs", "Qt6Core", "Qt6RemoteObjects"])
        moc_args = [token for token in cflags if token.startswith(("-I", "-D"))]

        replica_header = generated_dir / "rep_treeland_windowtree_replica.h"
        moc_source = generated_dir / "moc_rep_treeland_windowtree_replica.cpp"

        subprocess.check_call(
            [repc, "-i", "rep", "-o", "replica", str(REP_FILE), str(replica_header)]
        )
        subprocess.check_call([moc, *moc_args, str(replica_header), "-o", str(moc_source)])

        include_dirs: list[str] = [str(generated_dir)]
        extra_compile_args: list[str] = ["-std=c++17", "-fPIC"]
        extra_link_args: list[str] = []
        library_dirs: list[str] = []
        libraries: list[str] = []

        for token in cflags:
            if token.startswith("-I"):
                include_dirs.append(token[2:])
            else:
                extra_compile_args.append(token)

        for token in libs:
            if token.startswith("-L"):
                library_dirs.append(token[2:])
            elif token.startswith("-l"):
                libraries.append(token[2:])
            else:
                extra_link_args.append(token)

        for ext in self.extensions:
            ext.sources.append(str(moc_source))
            ext.include_dirs.extend(include_dirs)
            ext.extra_compile_args.extend(extra_compile_args)
            ext.extra_link_args.extend(extra_link_args)
            ext.library_dirs.extend(library_dirs)
            ext.libraries.extend(libraries)

        super().build_extensions()


ext_modules = [
    Pybind11Extension(
        "treeland_windowtree._core",
        ["src/treeland_windowtree/_core.cpp"],
    ),
]


setup_kwargs = {
    "ext_modules": ext_modules,
    "cmdclass": {"build_ext": BuildExt, "sdist": Sdist},
}


setup(**setup_kwargs)
