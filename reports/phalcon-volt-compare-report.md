# Templix vs Phalcon Volt Report

- Generated at: `2026-06-18T09:52:52+00:00`
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
| 100 | `templix_prod_standard` | 12106.17 | 1210617.11 | 0.083 | 0.826 | 29855 |
| 100 | `templix_prod_extreme` | 13262.46 | 1326245.52 | 0.075 | 0.754 | 29855 |
| 100 | `phalcon_volt_compiled` | 10303.53 | 1030352.54 | 0.097 | 0.971 | 29352 |
| 5000 | `templix_prod_standard` | 319.62 | 1598084.01 | 3.129 | 0.626 | 1488034 |
| 5000 | `templix_prod_extreme` | 318.76 | 1593815.57 | 3.137 | 0.627 | 1488034 |
| 5000 | `phalcon_volt_compiled` | 247.14 | 1235709.53 | 4.046 | 0.809 | 1463031 |
| 50000 | `templix_prod_standard` | 29.80 | 1489862.71 | 33.560 | 0.671 | 14927014 |
| 50000 | `templix_prod_extreme` | 29.64 | 1482244.42 | 33.733 | 0.675 | 14927014 |
| 50000 | `phalcon_volt_compiled` | 24.62 | 1230970.98 | 40.618 | 0.812 | 14677011 |

## Notes

- `templix_prod_standard`: `Templix\Engine::render()` in prod mode.
- `templix_prod_extreme`: prewarmed Templix compiled PHP cache directly included.
- `phalcon_volt_compiled`: Phalcon Volt compiled PHP cache directly included.
- Templix and Volt use their native template syntax, with equivalent HTML output and SHA-256 normalized output checks.

