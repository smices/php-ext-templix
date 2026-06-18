# Templix Laravel Volt Production Test Report

- Generated at: `2026-06-18T09:13:34+00:00`
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
| 100 | `templix_prod_standard` | 11126.05 | 1112604.72 | 0.090 | 0.899 | 29855 |
| 100 | `templix_prod_extreme` | 11782.38 | 1178237.95 | 0.085 | 0.849 | 29855 |
| 100 | `laravel_blade_compiled` | 5954.74 | 595474.44 | 0.168 | 1.679 | 29352 |
| 5000 | `templix_prod_standard` | 297.33 | 1486633.68 | 3.363 | 0.673 | 1488034 |
| 5000 | `templix_prod_extreme` | 298.86 | 1494310.41 | 3.346 | 0.669 | 1488034 |
| 5000 | `laravel_blade_compiled` | 134.48 | 672381.16 | 7.436 | 1.487 | 1463031 |
| 50000 | `templix_prod_standard` | 27.87 | 1393589.61 | 35.879 | 0.718 | 14927014 |
| 50000 | `templix_prod_extreme` | 26.97 | 1348461.12 | 37.079 | 0.742 | 14927014 |
| 50000 | `laravel_blade_compiled` | 12.34 | 617096.75 | 81.025 | 1.620 | 14677011 |

## Modes

- `templix_prod_standard`: Laravel view engine -> `Templix\Engine::render()` -> prod compiled cache.
- `templix_prod_extreme`: prewarmed Templix prod cache directly included with OPcache enabled.
- `laravel_blade_compiled`: Laravel Blade compiler output directly included as the framework baseline.

