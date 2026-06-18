# Templix Laravel Volt Production Test Report

- Generated at: `2026-06-18T09:51:56+00:00`
- Laravel: `13.16.1`
- PHP: `8.5.7`
- APP_ENV: `production`
- APP_DEBUG: `false`
- memory_limit: `512M`
- Volt installed: `true`
- Volt version: `v1.10.5`
- Templix loaded: `true`
- OPcache CLI: `true`
- OPcache validate timestamps: `0`
- Array row sets per render: `100, 5000, 50000` synthetic equity quote rows

## Results

| Array rows/render | Case | Renders/s | Rows/s | ms/render | us/row | Output bytes |
|---:|---|---:|---:|---:|---:|---:|
| 100 | `templix_prod_standard` | 11662.72 | 1166271.61 | 0.086 | 0.857 | 29855 |
| 100 | `templix_prod_extreme` | 12707.42 | 1270742.33 | 0.079 | 0.787 | 29855 |
| 100 | `laravel_blade_compiled` | 5716.30 | 571630.05 | 0.175 | 1.749 | 29352 |
| 5000 | `templix_prod_standard` | 301.19 | 1505943.58 | 3.320 | 0.664 | 1488034 |
| 5000 | `templix_prod_extreme` | 305.03 | 1525165.22 | 3.278 | 0.656 | 1488034 |
| 5000 | `laravel_blade_compiled` | 133.60 | 668014.13 | 7.485 | 1.497 | 1463031 |
| 50000 | `templix_prod_standard` | 31.38 | 1568927.70 | 31.869 | 0.637 | 14927014 |
| 50000 | `templix_prod_extreme` | 31.22 | 1561163.09 | 32.027 | 0.641 | 14927014 |
| 50000 | `laravel_blade_compiled` | 13.40 | 670016.31 | 74.625 | 1.493 | 14677011 |

## Modes

- `templix_prod_standard`: Laravel view engine -> `Templix\Engine::render()` -> prod compiled cache.
- `templix_prod_extreme`: prewarmed Templix prod cache directly included with OPcache enabled.
- `laravel_blade_compiled`: Laravel Blade compiler output directly included as the framework baseline.

