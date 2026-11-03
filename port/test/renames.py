"""Names the shared corpus recorded before roadmap S11.4 (port/corpus and log/p2 captures: .vec files, raw capture keys,
segment frames.json / checkpoints.json, framehash.json) and the names the port and its tests use since then.

The table is re/port/corpus_aliases.tsv (kind, module, addr, corpus_name, port_name): functions by their hooks.csv names, GameState
objects by their symbols. A corpus file is never rewritten: its readers translate at the boundary.
"""
from __future__ import annotations

import csv
import functools
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[2]
TABLE = ROOT / "re" / "port" / "corpus_aliases.tsv"


@functools.lru_cache(maxsize=None)
def _rows() -> tuple:
    if not TABLE.exists():
        return ()
    with TABLE.open(newline="", encoding="utf-8") as f:
        return tuple(csv.DictReader(f, delimiter="\t"))


@functools.lru_cache(maxsize=None)
def _maps() -> tuple[dict, dict, dict]:
    to_port, to_corpus, syms = {}, {}, {}
    for r in _rows():
        if r["kind"] == "global":
            syms[r["corpus_name"]] = r["port_name"]
        else:
            to_port[r["corpus_name"]] = r["port_name"]
            to_corpus[r["port_name"]] = r["corpus_name"]
    return to_port, to_corpus, syms


def port_name(name: str) -> str:
    """The port's (registry) name of a function the corpus names `name`."""
    return _maps()[0].get(name, name)


def corpus_name(name: str) -> str:
    """The corpus name (.vec stem, raw capture key prefix) of the port function `name`."""
    return _maps()[1].get(name, name)


def symbol(sym: str) -> str:
    """The GameState symbol of an object the corpus names `sym`."""
    return _maps()[2].get(sym, sym)


def symbols(syms: list[str]) -> list[str]:
    return [symbol(s) for s in syms]


def hashes(d: dict) -> dict:
    """A {symbol: value} map with its keys translated."""
    return {symbol(k): v for k, v in d.items()}
