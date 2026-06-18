# Templix Laravel Volt Production Test Report

- Generated at: `2026-06-18T09:25:04+00:00`
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
| 100 | `templix_prod_standard` | 11843.78 | 1184378.05 | 0.084 | 0.844 | 29855 |
| 100 | `templix_prod_extreme` | 12056.79 | 1205678.65 | 0.083 | 0.829 | 29855 |
| 100 | `laravel_blade_compiled` | 5802.68 | 580268.01 | 0.172 | 1.723 | 29352 |
| 5000 | `templix_prod_standard` | 315.49 | 1577455.01 | 3.170 | 0.634 | 1488034 |
| 5000 | `templix_prod_extreme` | 314.56 | 1572801.02 | 3.179 | 0.636 | 1488034 |
| 5000 | `laravel_blade_compiled` | 134.32 | 671589.94 | 7.445 | 1.489 | 1463031 |
| 50000 | `templix_prod_standard` | 30.46 | 1522759.47 | 32.835 | 0.657 | 14927014 |
| 50000 | `templix_prod_extreme` | 30.63 | 1531336.19 | 32.651 | 0.653 | 14927014 |
| 50000 | `laravel_blade_compiled` | 13.16 | 658074.05 | 75.979 | 1.520 | 14677011 |

## Modes

- `templix_prod_standard`: Laravel view engine -> `Templix\Engine::render()` -> prod compiled cache.
- `templix_prod_extreme`: prewarmed Templix prod cache directly included with OPcache enabled.
- `laravel_blade_compiled`: Laravel Blade compiler output directly included as the framework baseline.

