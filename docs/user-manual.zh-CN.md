# Templix 使用手册

Templix 是一个 PHP 8.x C 模板扩展，用于加速 `.blade.php` 风格模板渲染。它可以单独使用，也可以注册到 Laravel 中作为 view engine 使用；在 Volt 场景下，重点加速服务端模板生成。

## 编译安装

```bash
phpize
./configure --enable-templix
make
```

构建结果：

```text
modules/templix.so
```

临时加载：

```bash
php -d extension=/absolute/path/to/modules/templix.so --ri templix
```

生产环境建议把扩展写入 PHP ini：

```ini
extension=/absolute/path/to/templix.so
```

## 模板目录

默认模板扩展名是 `.blade.php`。

```text
views/
  quotes/
    ticker.blade.php
cache/
  templix/
```

模板名使用不带扩展名的路径：

```text
quotes/ticker
```

## 单独使用

```php
<?php

$engine = new Templix\Engine([
    'mode' => 'prod',
    'path' => __DIR__ . '/views',
    'cache' => __DIR__ . '/cache/templix',
]);

echo $engine->render('quotes/ticker', [
    'symbol' => 'BTC-USD',
    'price' => '104250.50',
    'change' => 1.25,
]);
```

模板：

```blade
<article>
  <h1>{{ $symbol }}</h1>
  <strong>{{ number_format($price, 2) }}</strong>

  @if ($change >= 0)
    <span class="up">+{{ number_format($change, 2) }}%</span>
  @else
    <span class="down">{{ number_format($change, 2) }}%</span>
  @endif
</article>
```

## 配置项

| 配置 | 默认值 | 说明 |
|---|---|---|
| `mode` | `dev` | `dev` 每次渲染前编译；`prod` 优先使用缓存 |
| `path` | 无 | 模板根目录 |
| `cache` | 无 | 编译缓存目录 |
| `extension` | `.blade.php` | 模板文件扩展名 |

生产环境建议使用 `prod`，并在发布阶段预热缓存。

## 模板语法

自动转义：

```blade
{{ $stock['symbol'] }}
```

原样输出：

```blade
{!! $trustedHtml !!}
```

条件：

```blade
@if ($risk >= 80)
  HIGH
@elseif ($risk >= 50)
  MID
@else
  LOW
@endif
```

循环：

```blade
@foreach ($stocks as $stock)
  <tr>
    <td>{{ $stock['symbol'] }}</td>
    <td>{{ number_format($stock['price'], 2) }}</td>
  </tr>
@endforeach
```

原生 PHP block：

```blade
<?php $renderedAt = $renderedAt ?? 'n/a'; ?>
<time>{{ $renderedAt }}</time>
```

## Laravel 集成

在 Laravel 中注册一个 view engine，把 `.blade.php` 渲染委托给 `Templix\Engine`。

Service Provider 示例：

```php
<?php

namespace App\Providers;

use App\View\Engines\LaravelTemplixViewEngine;
use Illuminate\Support\ServiceProvider;
use Templix\Engine as TemplixEngine;

final class TemplixServiceProvider extends ServiceProvider
{
    public function register(): void
    {
        $this->app->singleton(TemplixEngine::class, fn ($app) => new TemplixEngine([
            'mode' => $app->environment('production') ? 'prod' : 'dev',
            'path' => resource_path('views'),
            'cache' => storage_path('framework/views/templix'),
            'extension' => '.blade.php',
        ]));
    }

    public function boot(): void
    {
        $this->app['view']->addExtension('blade.php', 'templix', fn () =>
            new LaravelTemplixViewEngine($this->app->make(TemplixEngine::class))
        );
    }
}
```

View engine adapter：

```php
<?php

namespace App\View\Engines;

use Illuminate\Contracts\View\Engine as EngineInterface;
use Templix\Engine as TemplixEngine;

final class LaravelTemplixViewEngine implements EngineInterface
{
    public function __construct(private readonly TemplixEngine $templix)
    {
    }

    public function get($path, array $data = []): string
    {
        return $this->templix->render($this->templateNameFromPath($path), $data);
    }

    private function templateNameFromPath(string $path): string
    {
        $views = realpath(resource_path('views'));
        $real = realpath($path);

        if ($views === false || $real === false || !str_starts_with($real, $views . DIRECTORY_SEPARATOR)) {
            throw new \InvalidArgumentException('Template path is outside Laravel views directory.');
        }

        $relative = substr($real, strlen($views) + 1);
        $relative = str_replace(DIRECTORY_SEPARATOR, '/', $relative);

        return preg_replace('/\.blade\.php$/', '', $relative);
    }
}
```

注册 Provider 后，Laravel 的 `view('market.quotes', $data)->render()` 会进入 Templix view engine。

## Volt 场景

Volt 风格文件可以保留原生 PHP block 和 HTML / `wire:*` 属性：

```blade
<?php
$title = $title ?? 'Market Board';
?>

<section wire:poll.1s>
  <h1>{{ $title }}</h1>

  @foreach ($stocks as $stock)
    <div wire:key="stock-{{ $stock['symbol'] }}">
      {{ $stock['symbol'] }} {{ number_format($stock['price'], 2) }}
    </div>
  @endforeach
</section>
```

Templix 负责模板生成加速；Livewire / Volt 的交互协议仍由 Laravel 应用本身处理。

## 生产建议

- 开发环境用 `mode=dev`。
- 生产环境用 `mode=prod`。
- 缓存目录放在 `storage/framework/views/templix` 或等价可写目录。
- 发布后先预热模板，再切流量。
- 打开 OPcache，并关闭生产环境 timestamp 校验或按发布策略刷新 OPcache。

## 测试

扩展测试：

```bash
make test TESTS=tests/php8
```

真实 Laravel / Volt 生产测试：

```bash
ROW_SETS=100,5000,50000 ITERATIONS=5000 MEMORY_LIMIT=512M scripts/production-laravel-volt-test.sh
```

`ROW_SETS` 是每次渲染传给模板的数组行数；`ITERATIONS` 是重复渲染次数。

## 常见问题

确认扩展已加载：

```bash
php --ri templix
```

确认缓存目录可写：

```bash
mkdir -p storage/framework/views/templix
```

生产模式下模板修改未生效：

```text
prod 模式优先使用缓存。清理 Templix 缓存或重新预热模板。
```

