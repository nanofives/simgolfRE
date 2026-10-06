One Python file per C3 batch, loaded by `re/frida/hooks_registry.py`. Each defines
`HOOKS = {"name": dict(module=..., addr=..., abi=..., ret=..., args=[...], fixture=..., state=[...], vectors=[...])}`
in the format documented at the top of hooks_registry.py. Names must be unique across all files (checked at import).
