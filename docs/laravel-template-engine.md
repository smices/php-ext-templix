# Laravel Template Engine Support

Templix must be usable from Laravel without changing application templates away from Blade/Volt-compatible syntax.

The target integration is:

- PHP loads `templix.so`.
- Laravel registers Templix as a view engine.
- Existing `.blade.php` templates remain valid Blade/Volt templates.
- Applications opt into Templix through Laravel configuration or a service provider.
- Templix compiles supported syntax to PHP cache files and relies on OPcache for the hot path.

Templix is not a Laravel fork and does not depend on Laravel internals for standalone rendering. Laravel integration should be a thin adapter over `Templix\Engine`.

## PHP Extension Loading

Development example:

```ini
extension=/absolute/path/to/modules/templix.so
```

Runtime check:

```bash
php --ri templix
```

Expected:

```text
Templix support => enabled
```

## Laravel Service Provider Shape

A Laravel package can register Templix as a view engine through `Illuminate\View\Factory::addExtension`.

Example provider shape:

```php
<?php

namespace Templix\Laravel;

use Illuminate\Support\ServiceProvider;
use Illuminate\View\Engines\EngineResolver;
use Templix\Engine as TemplixEngine;

final class TemplixServiceProvider extends ServiceProvider
{
    public function register(): void
    {
        $this->app->singleton(TemplixEngine::class, function ($app): TemplixEngine {
            return new TemplixEngine([
                'mode' => $app->environment('production') ? 'prod' : 'dev',
                'path' => resource_path('views'),
                'cache' => storage_path('framework/views/templix'),
                'extension' => '.blade.php',
            ]);
        });
    }

    public function boot(): void
    {
        $this->app['view']->addExtension('blade.php', 'templix', function () {
            return new LaravelTemplixViewEngine(
                $this->app->make(TemplixEngine::class)
            );
        });
    }
}
```

The Laravel-facing view engine adapter should implement Laravel's expected engine contract and delegate rendering to `Templix\Engine`.

```php
<?php

namespace Templix\Laravel;

use Illuminate\View\Engines\EngineInterface;
use Templix\Engine as TemplixEngine;

final class LaravelTemplixViewEngine implements EngineInterface
{
    public function __construct(
        private readonly TemplixEngine $templix,
    ) {}

    public function get($path, array $data = []): string
    {
        $template = $this->templateNameFromPath($path);

        return $this->templix->render($template, $data);
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

## Compatibility Rules

Supported Templix syntax must remain valid Blade/Volt syntax:

```blade
{{ $user->name }}
{!! $trustedHtml !!}

@if ($user->vip)
  VIP
@elseif ($user->score > 100)
  Active
@else
  Normal
@endif

@foreach ($users as $user)
  <li>{{ $user->name }}</li>
@endforeach
```

Templix must not introduce custom tags such as `{{= }}` or `{{! }}`.

## Cache And Volt Acceleration Target

The Laravel integration should route production rendering through Templix compiled PHP cache files:

```text
resources/views/users/index.blade.php
  -> storage/framework/views/templix/<compiled-template>.php
  -> OPcache
  -> Templix runtime metadata
```

Production mode should prefer compiled cache files and avoid request-time source checks. Template deployment should run a warmup step before traffic reaches the new release.

Recommended deployment flow:

```bash
php artisan templix:warmup
php artisan optimize
```

The first adapter package can expose `templix:warmup` later. The C extension API should provide the underlying warmup primitive first.

## Testing Priority

Laravel compatibility tests should focus on:

- A `.blade.php` file can be rendered by Templix without changing template syntax.
- Escaped output and raw output match Blade semantics.
- Core control directives compile to executable PHP.
- Volt-style single-file component templates with a native PHP block and `wire:*` attributes render without altering the template.
- Prod mode renders from compiled cache after warmup or first compile.
- Changing source after a prod cache compile does not affect output until warmup/clear.
- Cache files are valid PHP and OPcache-friendly.
- Performance comparison is available through `bench/templix_vs_laravel_volt.php`, covering Templix prod cache, Laravel-style compiled include, Volt-style compiled include, and the current simple Templix render fixture.

Standalone PHPT tests cover the C extension behavior. Laravel package tests should cover service provider registration and `EngineInterface::get()` path mapping.
