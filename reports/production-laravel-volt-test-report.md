# Templix Laravel Volt Production Test Report

- Generated at: `2026-06-18T08:49:31+00:00`
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
- Dataset: `50000` synthetic equity quote rows

## Results

| Case | Renders/s | Rows/s | ms/render | us/row | Output bytes |
|---|---:|---:|---:|---:|---:|
| `templix_prod_standard` | 29.33 | 1466706.97 | 34.090 | 0.682 | 14927014 |
| `templix_prod_extreme` | 29.30 | 1464869.39 | 34.133 | 0.683 | 14927014 |
| `laravel_blade_compiled` | 13.49 | 674490.13 | 74.130 | 1.483 | 14677011 |

## Modes

- `templix_prod_standard`: Laravel view engine -> `Templix\Engine::render()` -> prod compiled cache.
- `templix_prod_extreme`: prewarmed Templix prod cache directly included with OPcache enabled.
- `laravel_blade_compiled`: Laravel Blade compiler output directly included as the framework baseline.

