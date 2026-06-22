# Templix

语言：[English](README.md) | [中文](README.zh-CN.md)

Templix 是一个 PHP 8.x C 模板扩展，目标是加速 Laravel Volt / Blade 风格模板渲染。它面向高频渲染场景，例如金融行情看板、实时风控视图、交易运营页面、报表页面，以及其他对延迟敏感的 PHP 模板生成路径。

Templix 默认保持 Laravel 兼容：使用 `.blade.php`、`{{ }}`、`{!! !!}`、Blade 控制指令和原生 PHP block。

## 当前状态

当前 MVP：

- `Templix\Engine`
- `.blade.php` 模板查找
- 编译 PHP 缓存文件
- `dev` / `prod` 模式
- prod compiled-cache fast path
- 转义输出 / 原始输出
- `@if`、`@elseif`、`@else`、`@endif`
- `@foreach`、`@endforeach`
- `@for`、`@while`、`@forelse`、`@switch`
- `@isset`、`@endisset`、`@empty`、`@endempty`
- `@class`、`@style`、`@checked`、`@selected`、`@disabled`、`@readonly`、`@required`
- `@php`、`@verbatim`、`@json`、Blade 注释
- 原生 PHP block
- Volt 风格单文件组件 smoke 测试

## 编译

```bash
phpize
./configure --enable-templix
make
```

构建产物：

```text
modules/templix.so
```

单次命令加载：

```bash
php -d extension=/absolute/path/to/modules/templix.so --ri templix
```

生产环境写入 PHP 配置：

```ini
extension=/absolute/path/to/templix.so
```

## 单独使用

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

模板文件：

```text
views/quotes/ticker.blade.php
```

```blade
<span>{{ $symbol }}</span>
<strong>{{ $price }}</strong>
```

## 与 Laravel 集成

Laravel 集成方式是注册一个 view engine，把 `.blade.php` 渲染委托给 `Templix\Engine`。

目标形态：

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

详细说明：

- [Templix 使用手册](docs/user-manual.zh-CN.md)
- [Templix 兼容性报告](docs/compatibility-report.zh-CN.md)
- [Laravel template engine support](docs/laravel-template-engine.md)

## 测试

```bash
make test TESTS=tests/php8
```

## 性能对比

```bash
php -d extension=modules/templix.so bench/templix_vs_laravel_volt.php --iterations=5000
```

benchmark 对比：

- Templix prod cache 渲染 Volt 风格组件
- Laravel Blade 风格 compiled PHP include
- Volt 风格 compiled PHP include
- 简单 Templix 基线模板

CI 或报告需要结构化结果时使用 `--json`。

## 生产 Laravel / Volt 测试

脚本会创建或复用真实 Laravel 应用，安装 `livewire/volt`，使用生产配置、
OPcache CLI，并覆盖小量、中等、大量三档传入模板的股票行情数组：

```bash
ROW_SETS=100,5000,50000 ITERATIONS=5000 MEMORY_LIMIT=512M scripts/production-laravel-volt-test.sh
```

`ROW_SETS` 是每次渲染传给模板的数组行数；`ITERATIONS` 只是重复渲染采样次数。

报告输出：

```text
reports/production-laravel-volt-test-report.md
reports/production-laravel-volt-test-report.json
```

对比项包括 `templix_prod_standard`、`templix_prod_extreme` 和
`laravel_blade_compiled`。

## 许可证

MIT License. Copyright (c) 2026 MISSU.LINK.
