# Templix Laravel Volt Production Test Report

- Generated at: `2026-06-18T10:02:41+00:00`
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
| 100 | `templix_prod_standard` | 11846.12 | 1184611.80 | 0.084 | 0.844 | 29855 |
| 100 | `templix_prod_extreme` | 12486.73 | 1248673.28 | 0.080 | 0.801 | 29855 |
| 100 | `laravel_blade_compiled` | 5783.69 | 578369.00 | 0.173 | 1.729 | 29352 |
| 5000 | `templix_prod_standard` | 322.94 | 1614704.57 | 3.097 | 0.619 | 1488034 |
| 5000 | `templix_prod_extreme` | 324.01 | 1620072.49 | 3.086 | 0.617 | 1488034 |
| 5000 | `laravel_blade_compiled` | 139.18 | 695885.10 | 7.185 | 1.437 | 1463031 |
| 50000 | `templix_prod_standard` | 31.63 | 1581585.62 | 31.614 | 0.632 | 14927014 |
| 50000 | `templix_prod_extreme` | 31.75 | 1587368.78 | 31.499 | 0.630 | 14927014 |
| 50000 | `laravel_blade_compiled` | 13.58 | 678753.86 | 73.664 | 1.473 | 14677011 |

## Modes

- `templix_prod_standard`: Laravel view engine -> `Templix\Engine::render()` -> prod compiled cache.
- `templix_prod_extreme`: prewarmed Templix prod cache directly included with OPcache enabled.
- `laravel_blade_compiled`: Laravel Blade compiler output directly included as the framework baseline.

