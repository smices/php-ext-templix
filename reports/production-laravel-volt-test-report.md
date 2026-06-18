# Templix Laravel Volt Production Test Report

- Generated at: `2026-06-18T08:16:18+00:00`
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
| `templix_prod_standard` | 24.62 | 1230870.65 | 40.622 | 0.812 | 14927014 |
| `templix_prod_extreme` | 27.11 | 1355550.81 | 36.885 | 0.738 | 14927014 |
| `laravel_blade_compiled` | 13.28 | 663886.46 | 75.314 | 1.506 | 14677011 |

## Modes

- `templix_prod_standard`: Laravel view engine -> `Templix\Engine::render()` -> prod compiled cache.
- `templix_prod_extreme`: prewarmed Templix prod cache directly included with OPcache enabled.
- `laravel_blade_compiled`: Laravel Blade compiler output directly included as the framework baseline.

