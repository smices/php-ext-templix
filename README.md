# Templix

Languages: [English](README.md) | [中文](README.zh-CN.md)

Templix is a PHP 8.x C template extension for accelerating Laravel Volt / Blade-style template rendering. It targets high-frequency rendering workloads such as financial dashboards, real-time risk views, trading operations screens, reporting pages, and other latency-sensitive PHP template generation paths.

Templix templates stay Laravel-compatible by default: use `.blade.php`, `{{ }}`, `{!! !!}`, Blade control directives, and native PHP blocks.

## Status

Current MVP:

- `Templix\Engine`
- `.blade.php` template lookup
- PHP cache file compilation
- `dev` and `prod` modes
- prod compiled-cache fast path
- escaped / raw output
- `@if`, `@elseif`, `@else`, `@endif`
- `@foreach`, `@endforeach`
- `@isset`, `@endisset`, `@empty`, `@endempty`
- native PHP blocks
- Volt-style single-file component smoke coverage

## Build

```bash
phpize
./configure --enable-templix
make
```

The built module is:

```text
modules/templix.so
```

## Standalone Usage

```php
$engine = new Templix\Engine([
    'mode' => 'prod',
    'path' => __DIR__ . '/views',
    'cache' => __DIR__ . '/cache/templix',
]);

echo $engine->render('quotes/ticker', [
    'symbol' => 'BTC-USD',
    'price' => '104250.50',
]);
```

Template file:

```text
views/quotes/ticker.blade.php
```

```blade
<span>{{ $symbol }}</span>
<strong>{{ $price }}</strong>
```

## Laravel Integration

Laravel integration should register Templix as a view engine and delegate `.blade.php` rendering to `Templix\Engine`.

Target shape:

```php
$this->app->singleton(Templix\Engine::class, fn ($app) => new Templix\Engine([
    'mode' => $app->environment('production') ? 'prod' : 'dev',
    'path' => resource_path('views'),
    'cache' => storage_path('framework/views/templix'),
]));

$this->app['view']->addExtension('blade.php', 'templix', fn () =>
    new LaravelTemplixViewEngine($this->app->make(Templix\Engine::class))
);
```

See [docs/laravel-template-engine.md](docs/laravel-template-engine.md).

## Test

```bash
make test TESTS=tests/php8
```

## Benchmark

```bash
php -d extension=modules/templix.so bench/templix_vs_laravel_volt.php --iterations=5000
```

The benchmark compares:

- Templix prod cache rendering for a Volt-style component
- Laravel Blade-style compiled PHP include
- Volt-style compiled PHP include
- the simple Templix baseline fixture

Use `--json` for CI or reports.

## License

MIT License. Copyright (c) 2026 MISSU.LINK.
