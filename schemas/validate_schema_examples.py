#!/usr/bin/env python3
import json
import sys
from pathlib import Path

import jsonschema
from jsonschema.validators import validator_for


def join_path(parts):
    if not parts:
        return "$"
    out = "$"
    for p in parts:
        if isinstance(p, int):
            out += f"[{p}]"
        else:
            # Use dot-notation for simple identifier-like keys, and
            # bracket-notation with JSON-string escaping otherwise.
            if isinstance(p, str) and p and p[0].isalpha() or (isinstance(p, str) and p and p[0] == "_"):
                if all(ch.isalnum() or ch == "_" for ch in p[1:]):
                    out += f".{p}"
                    continue
            out += f"[{json.dumps(p)}]"
    return out


def iter_examples(node, path=None):
    if path is None:
        path = []

    if isinstance(node, dict):
        if "examples" in node and isinstance(node["examples"], list):
            yield path, node, node["examples"]

        for key, value in node.items():
            yield from iter_examples(value, path + [key])

    elif isinstance(node, list):
        for i, value in enumerate(node):
            yield from iter_examples(value, path + [i])


def main():
    if len(sys.argv) != 2:
        print(f"Usage: {Path(sys.argv[0]).name} <schema.json>", file=sys.stderr)
        sys.exit(2)

    schema_path = Path(sys.argv[1])
    with schema_path.open("r", encoding="utf-8") as f:
        root_schema = json.load(f)

    ValidatorClass = validator_for(root_schema)
    ValidatorClass.check_schema(root_schema)

    failures = []

    for schema_path_parts, subschema, examples in iter_examples(root_schema):
        validator = ValidatorClass(subschema)

        for i, example in enumerate(examples):
            errors = sorted(validator.iter_errors(example), key=lambda e: list(e.path))
            for error in errors:
                failures.append(
                    {
                        "schema_path": join_path(schema_path_parts),
                        "example_index": i,
                        "instance_path": join_path(list(error.path)),
                        "message": error.message,
                    }
                )

    if failures:
        print("Invalid embedded examples found:\n")
        for n, failure in enumerate(failures, 1):
            print(f"{n}. Schema location: {failure['schema_path']}")
            print(f"   Example index:   {failure['example_index']}")
            print(f"   Instance path:   {failure['instance_path']}")
            print(f"   Error:           {failure['message']}\n")
        sys.exit(1)

    print("All embedded examples are valid.")
    sys.exit(0)


if __name__ == "__main__":
    main()
