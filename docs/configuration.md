# Configuration

Lunify can be configured with a JSON file passed via `-c`/`--config`, or by
CLI flags. CLI flags override nothing by default: a config file replaces the
defaults, then any explicit flags change individual options.

## Levels

The `level` field resets every toggle to a sensible baseline before explicit
fields are applied.

| Option | Level 1 | Level 2 (default) | Level 3 |
| --- | --- | --- | --- |
| removeComments | on | on | on |
| removeWhitespace | on | on | on |
| stripTypes | on | on | on |
| shortenNumbers | on | on | on |
| renameLocals | off | on | on |
| constantFold | off | on | on |
| deadCodeElim | off | on | on |
| inlineLocals | off | on | on |
| deadParams | off | on | on |
| renameGlobals | off | off | off |
| encodeStrings | off | off | on |
| stripDebug | off | off | off |

## Fields

| Field | Type | Default | Description |
| --- | --- | --- | --- |
| `level` | number | 2 | Baseline level (1-3). |
| `removeComments` | bool | true | Strip line and long comments. When false, Lunify falls back to token-level minification so comments survive. |
| `removeWhitespace` | bool | true | Remove redundant whitespace between tokens. |
| `stripTypes` | bool | true | Remove Luau type annotations and `type` statements. |
| `shortenNumbers` | bool | true | Shorten number literals. |
| `renameLocals` | bool | true | Rename locals to short names by usage frequency. |
| `constantFold` | bool | true | Fold constant expressions. |
| `deadCodeElim` | bool | true | Remove unreachable code and unused locals/functions. |
| `inlineLocals` | bool | true | Inline single-use literal locals. |
| `deadParams` | bool | true | Remove unused trailing function parameters. |
| `renameGlobals` | bool | false | Also rename globals that are both read and written. |
| `encodeStrings` | bool | false | XOR-encode string literals with an inline `_D` decoder. |
| `stripDebug` | bool | false | Remove `print(...)`/`warn(...)` call statements. |
| `finalNewline` | bool | true | Ensure the output ends with a newline. |
| `renamePrefix` | string | "" | Prefix for renamed locals (`p0`, `p1`, ...). Empty means short-letter names. |
| `keep` | string array | [] | Identifiers that must never be renamed or inlined. |

## Example

```json
{
  "level": 2,
  "stripDebug": true,
  "keep": ["_G", "module"],
  "renamePrefix": "v"
}
```

## CLI flags

| Flag | Effect |
| --- | --- |
| `-c, --config <file>` | Load a JSON config file. |
| `-o, --output <file>` | Write output to a file instead of stdout. |
| `-l, --level <1-3>` | Set the compression level. |
| `--rename-locals` | Enable local renaming. |
| `--constant-fold` | Enable constant folding. |
| `--dead-code-elim` | Enable dead code elimination. |
| `--inline-locals` | Enable local inlining. |
| `--dead-params` | Enable dead parameter removal. |
| `--rename-globals` | Enable global renaming. |
| `--encode-strings` | Enable string encoding. |
| `--strip-debug` | Remove `print`/`warn` calls. |
| `--keep-comments` | Keep comments (forces token-level mode). |
| `--no-shorten-numbers` | Do not shorten numbers. |
| `--no-final-newline` | Do not append a trailing newline. |
| `--rename-prefix <s>` | Prefix for renamed locals. |
| `--keep <name>` | Preserve an identifier (repeatable). |
| `-V, --version` | Print the version. |
| `-h, --help` | Print help. |

Flag order matters: flags listed after `-c`/`--config` override the config,
flags listed before are overwritten by it.
