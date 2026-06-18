#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PHP_BIN="${PHP_BIN:-php}"
PHPIZE_BIN="${PHPIZE_BIN:-phpize}"
ROWS="${ROWS:-10000}"
ROW_SETS="${ROW_SETS:-100,5000,50000}"
ITERATIONS="${ITERATIONS:-50}"
WARMUPS="${WARMUPS:-1}"
MEMORY_LIMIT="${MEMORY_LIMIT:-512M}"
APP_DIR="${APP_DIR:-}"
WORK_ROOT="${WORK_ROOT:-/private/tmp}"
COMPOSER_HOME="${COMPOSER_HOME:-$WORK_ROOT/templix-composer-home}"
COMPOSER_BIN="${COMPOSER_BIN:-}"
REPORT_DIR="${REPORT_DIR:-$ROOT_DIR/reports}"
KEEP_APP="${KEEP_APP:-0}"

mkdir -p "$REPORT_DIR" "$COMPOSER_HOME"

REPORT_MD="$REPORT_DIR/production-laravel-volt-test-report.md"
REPORT_JSON="$REPORT_DIR/production-laravel-volt-test-report.json"

log() {
  printf '[templix-prod-test] %s\n' "$*"
}

ensure_composer() {
  if [[ -n "$COMPOSER_BIN" ]]; then
    return
  fi

  if command -v composer >/dev/null 2>&1; then
    COMPOSER_BIN="$(command -v composer)"
    return
  fi

  COMPOSER_BIN="$WORK_ROOT/templix-composer.phar"
  if [[ -f "$COMPOSER_BIN" ]]; then
    return
  fi

  log "Downloading Composer to $COMPOSER_BIN"
  curl -sS https://getcomposer.org/installer -o "$WORK_ROOT/templix-composer-setup.php"
  COMPOSER_HOME="$COMPOSER_HOME" "$PHP_BIN" "$WORK_ROOT/templix-composer-setup.php" \
    --install-dir="$WORK_ROOT" \
    --filename="$(basename "$COMPOSER_BIN")"
}

composer_cmd() {
  COMPOSER_HOME="$COMPOSER_HOME" COMPOSER_PROCESS_TIMEOUT=900 "$PHP_BIN" "$COMPOSER_BIN" "$@"
}

php_with_templix() {
  "$PHP_BIN" \
    -d "extension=$ROOT_DIR/modules/templix.so" \
    -d opcache.enable_cli=1 \
    -d opcache.validate_timestamps=0 \
    -d "memory_limit=$MEMORY_LIMIT" \
    "$@"
}

build_extension() {
  log "Building Templix extension"
  if [[ ! -x "$ROOT_DIR/configure" ]]; then
    if ! command -v "$PHPIZE_BIN" >/dev/null 2>&1; then
      printf 'phpize not found. Set PHPIZE_BIN=/path/to/phpize.\n' >&2
      exit 1
    fi

    (cd "$ROOT_DIR" && "$PHPIZE_BIN" >/dev/null)
  fi
  (cd "$ROOT_DIR" && ./configure --enable-templix >/dev/null)
  (cd "$ROOT_DIR" && make -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)" >/dev/null)
  php_with_templix --ri templix >/dev/null
}

create_or_reuse_laravel_app() {
  if [[ -z "$APP_DIR" ]]; then
    APP_DIR="$(mktemp -d "$WORK_ROOT/templix-laravel-prod.XXXXXX")"
  fi

  if [[ -f "$APP_DIR/artisan" ]]; then
    log "Reusing Laravel app at $APP_DIR"
  else
    log "Creating Laravel app at $APP_DIR"
    composer_cmd create-project laravel/laravel "$APP_DIR" "^13.0" \
      --no-interaction \
      --no-audit \
      --no-progress
  fi

  if ! (cd "$APP_DIR" && composer_cmd show livewire/volt >/dev/null 2>&1); then
    log "Installing livewire/volt"
    (cd "$APP_DIR" && composer_cmd require livewire/volt --no-interaction --no-audit --no-progress)
  fi

  log "Installing production dependencies"
  (cd "$APP_DIR" && composer_cmd install --no-dev --optimize-autoloader --no-interaction --no-progress)
}

write_laravel_fixture() {
  log "Writing Laravel production fixture"
  mkdir -p \
    "$APP_DIR/app/View/Engines" \
    "$APP_DIR/app/Providers" \
    "$APP_DIR/resources/views/market" \
    "$APP_DIR/storage/framework/views/templix" \
    "$APP_DIR/storage/framework/views/templix-extreme" \
    "$APP_DIR/scripts"

  cat > "$APP_DIR/app/View/Engines/LaravelTemplixViewEngine.php" <<'PHP'
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
PHP

  cat > "$APP_DIR/app/Providers/TemplixProductionTestServiceProvider.php" <<'PHP'
<?php

namespace App\Providers;

use App\View\Engines\LaravelTemplixViewEngine;
use Illuminate\Support\ServiceProvider;
use Templix\Engine as TemplixEngine;

final class TemplixProductionTestServiceProvider extends ServiceProvider
{
    public function register(): void
    {
        $this->app->singleton(TemplixEngine::class, fn ($app) => new TemplixEngine([
            'mode' => 'prod',
            'path' => resource_path('views'),
            'cache' => storage_path('framework/views/templix'),
        ]));
    }

    public function boot(): void
    {
        $this->app['view']->addExtension('blade.php', 'templix', fn () =>
            new LaravelTemplixViewEngine($this->app->make(TemplixEngine::class))
        );
    }
}
PHP

  cat > "$APP_DIR/bootstrap/providers.php" <<'PHP'
<?php

use App\Providers\AppServiceProvider;
use App\Providers\TemplixProductionTestServiceProvider;

return [
    AppServiceProvider::class,
    TemplixProductionTestServiceProvider::class,
];
PHP

  cat > "$APP_DIR/resources/views/market/quotes.blade.php" <<'BLADE'
<?php $renderedAt = $renderedAt ?? 'n/a'; ?>
<section data-report="market-quotes" data-count="{{ $count }}" data-rendered-at="{{ $renderedAt }}">
  <header>
    <h1>{{ $marketName }}</h1>
    <strong>{{ $count }}</strong>
  </header>
  <table>
    <thead>
      <tr>
        <th>Symbol</th><th>Name</th><th>Price</th><th>Change</th><th>Volume</th><th>Risk</th>
      </tr>
    </thead>
    <tbody>
    @foreach ($stocks as $stock)
      <tr data-symbol="{{ $stock['symbol'] }}" data-risk="{{ $stock['risk'] }}">
        <td>{{ $stock['symbol'] }}</td>
        <td>{{ $stock['name'] }}</td>
        <td>{{ number_format($stock['price'], 2) }}</td>
        @if ($stock['change'] >= 0)
          <td class="up">+{{ number_format($stock['change'], 2) }}%</td>
        @else
          <td class="down">{{ number_format($stock['change'], 2) }}%</td>
        @endif
        <td>{{ number_format($stock['volume']) }}</td>
        @if ($stock['risk'] >= 80)
          <td class="risk-high">HIGH</td>
        @elseif ($stock['risk'] >= 50)
          <td class="risk-mid">MID</td>
        @else
          <td class="risk-low">LOW</td>
        @endif
      </tr>
    @endforeach
    </tbody>
  </table>
  @empty($stocks)
    <p>No market data.</p>
  @endempty
</section>
BLADE

  cat > "$APP_DIR/scripts/templix-production-benchmark.php" <<'PHP'
<?php

declare(strict_types=1);

use Templix\Engine as TemplixEngine;

require __DIR__ . '/../vendor/autoload.php';

$app = require __DIR__ . '/../bootstrap/app.php';
$app->make(Illuminate\Contracts\Console\Kernel::class)->bootstrap();

$args = getopt('', [
    'rows::',
    'row-sets::',
    'iterations::',
    'warmups::',
    'report-json::',
    'report-md::',
]);

$rowSetInput = (string) ($args['row-sets'] ?? ($args['rows'] ?? '10000'));
$rowSets = parseRowSets($rowSetInput);
$iterations = max(1, (int) ($args['iterations'] ?? 5));
$warmups = max(0, (int) ($args['warmups'] ?? 1));
$reportJson = $args['report-json'] ?? null;
$reportMd = $args['report-md'] ?? null;

$templix = $app->make(TemplixEngine::class);
$templixExtreme = new TemplixEngine([
    'mode' => 'prod',
    'path' => resource_path('views'),
    'cache' => storage_path('framework/views/templix-extreme'),
]);
$viewName = 'market.quotes';
$templateName = 'market/quotes';
$compiledPath = storage_path('framework/views/templix/market_quotes.blade.php.php');
$extremeCompiledPath = storage_path('framework/views/templix-extreme/market_quotes.blade.php.php');
$sourcePath = resource_path('views/market/quotes.blade.php');
$bladeCompiledPath = storage_path('framework/views/laravel_blade_market_quotes.php');

if (!class_exists(Livewire\Volt\Volt::class)) {
    throw new RuntimeException('livewire/volt is not installed.');
}

$compileData = buildData($rowSets[0]);
$standardSample = view($viewName, $compileData)->render();
$templix->render($templateName, $compileData);
$templixExtreme->render($templateName, $compileData);

if (!is_file($compiledPath)) {
    throw new RuntimeException("Templix compiled cache not found: {$compiledPath}");
}
if (!is_file($extremeCompiledPath)) {
    throw new RuntimeException("Templix extreme compiled cache not found: {$extremeCompiledPath}");
}

if (function_exists('opcache_compile_file')) {
    @opcache_compile_file($compiledPath);
    @opcache_compile_file($extremeCompiledPath);
}

$bladeCompiler = app('blade.compiler');
file_put_contents($bladeCompiledPath, $bladeCompiler->compileString(file_get_contents($sourcePath)));
if (function_exists('opcache_compile_file')) {
    @opcache_compile_file($bladeCompiledPath);
}

$scenarios = [];
foreach ($rowSets as $rows) {
    $data = buildData($rows);
    $standardSample = view($viewName, $data)->render();
    $extremeSample = renderCompiled($extremeCompiledPath, $data);
    $bladeSample = renderCompiled($bladeCompiledPath, $data);
    $expectedHash = hash('sha256', normalizeHtml($standardSample));

    assertSameHash("templix_prod_extreme rows={$rows}", $expectedHash, $extremeSample);
    assertSameHash("laravel_blade_compiled rows={$rows}", $expectedHash, $bladeSample);

    $cases = [
        'templix_prod_standard' => fn (): string => view($viewName, $data)->render(),
        'templix_prod_extreme' => fn (): string => renderCompiled($extremeCompiledPath, $data),
        'laravel_blade_compiled' => fn (): string => renderCompiled($bladeCompiledPath, $data),
    ];

    $results = [];
    foreach ($cases as $name => $case) {
        for ($i = 0; $i < $warmups; $i++) {
            $case();
        }

        gc_collect_cycles();
        $memoryBefore = memory_get_usage(true);
        $peakBefore = memory_get_peak_usage(true);
        $start = hrtime(true);

        $last = '';
        for ($i = 0; $i < $iterations; $i++) {
            $last = $case();
        }

        $elapsedNs = hrtime(true) - $start;
        $peakAfter = memory_get_peak_usage(true);
        $seconds = $elapsedNs / 1_000_000_000;

        $results[$name] = [
            'iterations' => $iterations,
            'rows_per_render' => $rows,
            'total_rows_rendered' => $rows * $iterations,
            'seconds' => $seconds,
            'renders_per_second' => $iterations / max($seconds, 0.000001),
            'rows_per_second' => ($rows * $iterations) / max($seconds, 0.000001),
            'milliseconds_per_render' => ($elapsedNs / 1_000_000) / $iterations,
            'microseconds_per_row' => ($elapsedNs / 1000) / ($iterations * $rows),
            'output_bytes' => strlen($last),
            'output_sha256' => hash('sha256', normalizeHtml($last)),
            'memory_before_bytes' => $memoryBefore,
            'peak_memory_delta_bytes' => max(0, $peakAfter - $peakBefore),
        ];
    }

    $scenarios[] = [
        'rows' => $rows,
        'dataset' => [
            'type' => 'synthetic_equity_quotes',
            'rows' => $rows,
            'fields' => ['symbol', 'name', 'price', 'change', 'volume', 'risk'],
        ],
        'results' => $results,
    ];
}

$report = [
    'generated_at' => gmdate('c'),
    'environment' => [
        'app_env' => app()->environment(),
        'app_debug' => (bool) config('app.debug'),
        'php_version' => PHP_VERSION,
        'memory_limit' => ini_get('memory_limit'),
        'laravel_version' => app()->version(),
        'volt_installed' => class_exists(Livewire\Volt\Volt::class),
        'volt_version' => class_exists(Composer\InstalledVersions::class)
            ? Composer\InstalledVersions::getPrettyVersion('livewire/volt')
            : null,
        'templix_loaded' => extension_loaded('templix'),
        'opcache_cli_enabled' => (bool) ini_get('opcache.enable_cli'),
        'opcache_validate_timestamps' => ini_get('opcache.validate_timestamps'),
    ],
    'dataset' => [
        'type' => 'synthetic_equity_quotes',
        'row_sets' => $rowSets,
        'fields' => ['symbol', 'name', 'price', 'change', 'volume', 'risk'],
    ],
    'cache' => [
        'templix_compiled_path' => $compiledPath,
        'templix_compiled_bytes' => filesize($compiledPath),
        'templix_extreme_compiled_path' => $extremeCompiledPath,
        'templix_extreme_compiled_bytes' => filesize($extremeCompiledPath),
        'laravel_blade_compiled_path' => $bladeCompiledPath,
        'laravel_blade_compiled_bytes' => filesize($bladeCompiledPath),
    ],
    'scenarios' => $scenarios,
];

if ($reportJson) {
    file_put_contents($reportJson, json_encode($report, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES) . PHP_EOL);
}

if ($reportMd) {
    file_put_contents($reportMd, markdownReport($report));
}

echo json_encode($report, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES) . PHP_EOL;

function parseRowSets(string $input): array
{
    $rows = [];
    foreach (explode(',', $input) as $value) {
        $rowCount = max(1, (int) trim($value));
        $rows[$rowCount] = $rowCount;
    }

    return array_values($rows);
}

function buildData(int $rows): array
{
    return [
        'marketName' => 'Production Equity Risk Board',
        'count' => $rows,
        'renderedAt' => gmdate('c'),
        'stocks' => generateStocks($rows),
    ];
}

function generateStocks(int $rows): array
{
    $sectors = ['Banking', 'Insurance', 'Brokerage', 'Energy', 'Semiconductor', 'Cloud', 'Biotech'];
    $stocks = [];

    for ($i = 0; $i < $rows; $i++) {
        $price = 10 + (($i * 7919) % 500000) / 100;
        $change = ((($i * 1543) % 2001) - 1000) / 100;
        $risk = ($i * 37) % 100;
        $stocks[] = [
            'symbol' => sprintf('EQ%05d', $i),
            'name' => $sectors[$i % count($sectors)] . ' Holdings ' . $i,
            'price' => $price,
            'change' => $change,
            'volume' => 100000 + (($i * 104729) % 90000000),
            'risk' => $risk,
        ];
    }

    return $stocks;
}

function renderCompiled(string $path, array $data): string
{
    extract($data, EXTR_SKIP);
    $__env = app('view');
    ob_start();
    include $path;

    return ob_get_clean();
}

function normalizeHtml(string $html): string
{
    return preg_replace('/\s+/', ' ', trim($html));
}

function assertSameHash(string $name, string $expectedHash, string $html): void
{
    $actual = hash('sha256', normalizeHtml($html));
    if ($actual !== $expectedHash) {
        throw new RuntimeException("Output mismatch for {$name}: {$actual} != {$expectedHash}");
    }
}

function markdownReport(array $report): string
{
    $lines = [];
    $lines[] = '# Templix Laravel Volt Production Test Report';
    $lines[] = '';
    $lines[] = '- Generated at: `' . $report['generated_at'] . '`';
    $lines[] = '- Laravel: `' . $report['environment']['laravel_version'] . '`';
    $lines[] = '- PHP: `' . $report['environment']['php_version'] . '`';
    $lines[] = '- APP_ENV: `' . $report['environment']['app_env'] . '`';
    $lines[] = '- APP_DEBUG: `' . ($report['environment']['app_debug'] ? 'true' : 'false') . '`';
    $lines[] = '- memory_limit: `' . $report['environment']['memory_limit'] . '`';
    $lines[] = '- Volt installed: `' . ($report['environment']['volt_installed'] ? 'true' : 'false') . '`';
    $lines[] = '- Volt version: `' . ($report['environment']['volt_version'] ?? 'n/a') . '`';
    $lines[] = '- Templix loaded: `' . ($report['environment']['templix_loaded'] ? 'true' : 'false') . '`';
    $lines[] = '- OPcache CLI: `' . ($report['environment']['opcache_cli_enabled'] ? 'true' : 'false') . '`';
    $lines[] = '- OPcache validate timestamps: `' . $report['environment']['opcache_validate_timestamps'] . '`';
    $lines[] = '- Array row sets per render: `' . implode(', ', $report['dataset']['row_sets']) . '` synthetic equity quote rows';
    $lines[] = '';
    $lines[] = '## Results';
    $lines[] = '';
    $lines[] = '| Array rows/render | Case | Renders/s | Rows/s | ms/render | us/row | Output bytes |';
    $lines[] = '|---:|---|---:|---:|---:|---:|---:|';

    foreach ($report['scenarios'] as $scenario) {
        foreach ($scenario['results'] as $case => $row) {
            $lines[] = sprintf(
                '| %d | `%s` | %.2f | %.2f | %.3f | %.3f | %d |',
                $scenario['rows'],
                $case,
                $row['renders_per_second'],
                $row['rows_per_second'],
                $row['milliseconds_per_render'],
                $row['microseconds_per_row'],
                $row['output_bytes'],
            );
        }
    }

    $lines[] = '';
    $lines[] = '## Modes';
    $lines[] = '';
    $lines[] = '- `templix_prod_standard`: Laravel view engine -> `Templix\\Engine::render()` -> prod compiled cache.';
    $lines[] = '- `templix_prod_extreme`: prewarmed Templix prod cache directly included with OPcache enabled.';
    $lines[] = '- `laravel_blade_compiled`: Laravel Blade compiler output directly included as the framework baseline.';
    $lines[] = '';

    return implode(PHP_EOL, $lines) . PHP_EOL;
}
PHP
}

configure_production() {
  log "Configuring Laravel production environment"
  (
    cd "$APP_DIR"
    "$PHP_BIN" -r '
      $env = file_get_contents(".env");
      $pairs = [
        "APP_ENV" => "production",
        "APP_DEBUG" => "false",
        "LOG_CHANNEL" => "stderr",
        "CACHE_STORE" => "array",
        "SESSION_DRIVER" => "array",
        "QUEUE_CONNECTION" => "sync",
      ];
      foreach ($pairs as $key => $value) {
        if (preg_match("/^{$key}=.*$/m", $env)) {
          $env = preg_replace("/^{$key}=.*$/m", "{$key}={$value}", $env);
        } else {
          $env .= PHP_EOL . "{$key}={$value}";
        }
      }
      file_put_contents(".env", $env);
    '
    php_with_templix artisan config:clear >/dev/null
    php_with_templix artisan optimize:clear >/dev/null
    rm -rf storage/framework/views/templix storage/framework/views/templix-extreme
    mkdir -p storage/framework/views/templix storage/framework/views/templix-extreme
    php_with_templix artisan config:cache >/dev/null
  )
}

run_production_benchmark() {
  log "Running production benchmark row_sets=$ROW_SETS iterations=$ITERATIONS warmups=$WARMUPS"
  rm -f "$REPORT_JSON" "$REPORT_MD"
  (
    cd "$APP_DIR"
    APP_ENV=production APP_DEBUG=false php_with_templix \
      scripts/templix-production-benchmark.php \
      --row-sets="$ROW_SETS" \
      --iterations="$ITERATIONS" \
      --warmups="$WARMUPS" \
      --report-json="$REPORT_JSON" \
      --report-md="$REPORT_MD"
  )

  if [[ ! -s "$REPORT_JSON" || ! -s "$REPORT_MD" ]]; then
    printf 'Production benchmark did not generate reports.\n' >&2
    exit 1
  fi
}

main() {
  ensure_composer
  build_extension
  create_or_reuse_laravel_app
  write_laravel_fixture
  configure_production
  run_production_benchmark

  log "Report written to $REPORT_MD"
  log "JSON written to $REPORT_JSON"
  log "Laravel app: $APP_DIR"

  if [[ "$KEEP_APP" != "1" && -z "${APP_DIR:-}" ]]; then
    :
  fi
}

main "$@"
