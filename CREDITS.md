# Credits

Every non-Valve asset in this repository must have a row in the table below.

| File path | Asset | Author | Source URL | License |
| --- | --- | --- | --- | --- |
| `example/path/asset.ext` (EXAMPLE, not a real asset) | Example asset | Example Author | https://example.com/asset | CC0 |

## Tools

Installed into the gitignored `tools/venv` by `tools/setup_venv.sh`; not shipped in the mod.

| Tool | Used for | License |
| --- | --- | --- |
| [Pillow](https://python-pillow.org/) | Resizing and DXT compression in `tools/img2vtf.py` | MIT-CMU (HPND) |
| [no_vtf](https://git.sr.ht/~b5327157/no_vtf) | Not used: it only decodes VTFs (and fails to start with current click), so `img2vtf.py` writes VTFs itself. Listed for the record. | GPL-3.0-only |
