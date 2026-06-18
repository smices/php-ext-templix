# Templix vs Phalcon Volt Report

- Generated at: `2026-06-18T10:03:11+00:00`
- PHP: `8.5.7`
- memory_limit: `512M`
- Templix loaded: `true`
- Phalcon loaded: `true`
- Phalcon version: `5.12.1`
- OPcache CLI: `true`
- OPcache validate timestamps: `0`
- Array row sets per render: `100, 5000, 50000` synthetic equity quote rows

## Results

| Array rows/render | Case | Renders/s | Rows/s | ms/render | us/row | Output bytes |
|---:|---|---:|---:|---:|---:|---:|
| 100 | `templix_prod_standard` | 12830.79 | 1283079.39 | 0.078 | 0.779 | 29855 |
| 100 | `templix_prod_extreme` | 12957.56 | 1295756.40 | 0.077 | 0.772 | 29855 |
| 100 | `phalcon_volt_compiled` | 10508.62 | 1050861.71 | 0.095 | 0.952 | 29352 |
| 5000 | `templix_prod_standard` | 325.97 | 1629870.52 | 3.068 | 0.614 | 1488034 |
| 5000 | `templix_prod_extreme` | 324.49 | 1622455.66 | 3.082 | 0.616 | 1488034 |
| 5000 | `phalcon_volt_compiled` | 256.27 | 1281329.03 | 3.902 | 0.780 | 1463031 |
| 50000 | `templix_prod_standard` | 31.41 | 1570695.80 | 31.833 | 0.637 | 14927014 |
| 50000 | `templix_prod_extreme` | 31.44 | 1572028.75 | 31.806 | 0.636 | 14927014 |
| 50000 | `phalcon_volt_compiled` | 25.11 | 1255336.54 | 39.830 | 0.797 | 14677011 |

## Notes

- `templix_prod_standard`: `Templix\Engine::render()` in prod mode.
- `templix_prod_extreme`: prewarmed Templix compiled PHP cache directly included.
- `phalcon_volt_compiled`: Phalcon Volt compiled PHP cache directly included.
- Templix and Volt use their native template syntax, with equivalent HTML output and SHA-256 normalized output checks.

