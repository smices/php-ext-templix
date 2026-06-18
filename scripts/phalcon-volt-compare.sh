#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PHP_BIN="${PHP_BIN:-php}"
PHPIZE_BIN="${PHPIZE_BIN:-phpize}"
WORK_ROOT="${WORK_ROOT:-/private/tmp}"
PHALCON_EXTENSION="${PHALCON_EXTENSION:-/private/tmp/templix-phalcon-compare/build/phalcon/modules/phalcon.so}"
PHALCON_VERSION="${PHALCON_VERSION:-5.12.1}"
PHALCON_BUILD_ROOT="${PHALCON_BUILD_ROOT:-$WORK_ROOT/templix-phalcon-compare}"
ROW_SETS="${ROW_SETS:-100,5000,50000}"
ITERATIONS="${ITERATIONS:-50}"
WARMUPS="${WARMUPS:-1}"
MEMORY_LIMIT="${MEMORY_LIMIT:-512M}"
APP_DIR="${APP_DIR:-}"
REPORT_DIR="${REPORT_DIR:-$ROOT_DIR/reports}"

mkdir -p "$REPORT_DIR"

REPORT_MD="$REPORT_DIR/phalcon-volt-compare-report.md"
REPORT_JSON="$REPORT_DIR/phalcon-volt-compare-report.json"

log() {
  printf '[templix-phalcon-compare] %s\n' "$*"
}

php_with_extensions() {
  local php_args=(
    -d "extension=$ROOT_DIR/modules/templix.so"
    -d opcache.enable_cli=1
    -d opcache.validate_timestamps=0
    -d "memory_limit=$MEMORY_LIMIT"
  )

  if [[ -n "$PHALCON_EXTENSION" ]] && ! "$PHP_BIN" -r 'exit(extension_loaded("phalcon") ? 0 : 1);' >/dev/null 2>&1; then
    php_args+=(-d "extension=$PHALCON_EXTENSION")
  fi

  "$PHP_BIN" "${php_args[@]}" "$@"
}

build_templix() {
  log "Building Templix extension"
  if [[ ! -x "$ROOT_DIR/configure" ]]; then
    (cd "$ROOT_DIR" && "$PHPIZE_BIN" >/dev/null)
  fi
  (cd "$ROOT_DIR" && ./configure --enable-templix >/dev/null)
  (cd "$ROOT_DIR" && make -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)" >/dev/null)
}

ensure_phalcon() {
  if "$PHP_BIN" -m | grep -qi '^phalcon$'; then
    PHALCON_EXTENSION=""
    return
  fi

  if [[ -f "$PHALCON_EXTENSION" ]]; then
    return
  fi

  local tarball="$WORK_ROOT/cphalcon-v$PHALCON_VERSION.tar.gz"
  local build_dir="$PHALCON_BUILD_ROOT"

  log "Downloading Phalcon v$PHALCON_VERSION"
  mkdir -p "$build_dir"
  if [[ ! -f "$tarball" ]]; then
    curl -L --fail --show-error --output "$tarball" \
      "https://github.com/phalcon/cphalcon/archive/refs/tags/v$PHALCON_VERSION.tar.gz"
  fi

  if [[ ! -f "$build_dir/build/phalcon/config.m4" ]]; then
    tar -xzf "$tarball" -C "$build_dir" --strip-components=1
  fi

  log "Building Phalcon extension"
  (
    cd "$build_dir/build/phalcon"
    "$PHPIZE_BIN" >/dev/null
    ./configure --enable-phalcon >/dev/null
    make -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)" >/dev/null
  )

  if [[ ! -f "$PHALCON_EXTENSION" ]]; then
    printf 'Phalcon build finished but extension was not found: %s\n' "$PHALCON_EXTENSION" >&2
    exit 1
  fi
}

prepare_app() {
  if [[ -z "$APP_DIR" ]]; then
    APP_DIR="$(mktemp -d "$WORK_ROOT/templix-phalcon-volt.XXXXXX")"
  fi

  mkdir -p \
    "$APP_DIR/views/templix/market" \
    "$APP_DIR/views/volt/market" \
    "$APP_DIR/cache/templix" \
    "$APP_DIR/cache/volt" \
    "$APP_DIR/scripts"

  cat > "$APP_DIR/views/templix/market/quotes.blade.php" <<'BLADE'
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

  cat > "$APP_DIR/views/volt/market/quotes.volt" <<'VOLT'
<?php $renderedAt = $renderedAt ?? 'n/a'; ?>
<section data-report="market-quotes" data-count="{{ e(count) }}" data-rendered-at="{{ e(renderedAt) }}">
  <header>
    <h1>{{ e(marketName) }}</h1>
    <strong>{{ e(count) }}</strong>
  </header>
  <table>
    <thead>
      <tr>
        <th>Symbol</th><th>Name</th><th>Price</th><th>Change</th><th>Volume</th><th>Risk</th>
      </tr>
    </thead>
    <tbody>
    {% for stock in stocks %}
      <tr data-symbol="{{ e(stock['symbol']) }}" data-risk="{{ e(stock['risk']) }}">
        <td>{{ e(stock['symbol']) }}</td>
        <td>{{ e(stock['name']) }}</td>
        <td>{{ number_format(stock['price'], 2) }}</td>
        {% if stock['change'] >= 0 %}
          <td class="up">+{{ number_format(stock['change'], 2) }}%</td>
        {% else %}
          <td class="down">{{ number_format(stock['change'], 2) }}%</td>
        {% endif %}
        <td>{{ number_format(stock['volume']) }}</td>
        {% if stock['risk'] >= 80 %}
          <td class="risk-high">HIGH</td>
        {% elseif stock['risk'] >= 50 %}
          <td class="risk-mid">MID</td>
        {% else %}
          <td class="risk-low">LOW</td>
        {% endif %}
      </tr>
    {% endfor %}
    </tbody>
  </table>
  {% if count(stocks) == 0 %}
    <p>No market data.</p>
  {% endif %}
</section>
VOLT

  cat > "$APP_DIR/scripts/run-compare.php" <<'PHP'
<?php

declare(strict_types=1);

use Phalcon\Mvc\View\Engine\Volt\Compiler as VoltCompiler;
use Templix\Engine as TemplixEngine;

$args = getopt('', [
    'app-dir:',
    'row-sets::',
    'iterations::',
    'warmups::',
    'report-json::',
    'report-md::',
]);

$appDir = (string) $args['app-dir'];
$rowSets = parseRowSets((string) ($args['row-sets'] ?? '100,5000,50000'));
$iterations = max(1, (int) ($args['iterations'] ?? 50));
$warmups = max(0, (int) ($args['warmups'] ?? 1));
$reportJson = $args['report-json'] ?? null;
$reportMd = $args['report-md'] ?? null;

if (!extension_loaded('templix')) {
    throw new RuntimeException('templix extension is not loaded.');
}
if (!extension_loaded('phalcon')) {
    throw new RuntimeException('phalcon extension is not loaded.');
}

$templix = new TemplixEngine([
    'mode' => 'prod',
    'path' => $appDir . '/views/templix',
    'cache' => $appDir . '/cache/templix',
]);

$voltCompiler = new VoltCompiler();
$voltCompiler->setOptions([
    'path' => $appDir . '/cache/volt/',
    'extension' => '.php',
    'separator' => '_',
    'stat' => false,
]);
$voltCompiler->addFunction('number_format', 'number_format');
$voltCompiler->addFunction('count', 'count');
$voltCompiler->addFunction('e', function (string $resolvedArgs): string {
    return 'htmlspecialchars(' . $resolvedArgs . ', ENT_QUOTES | ENT_SUBSTITUTE, "UTF-8")';
});
$voltCompiler->compile($appDir . '/views/volt/market/quotes.volt');
$voltCompiledPath = $voltCompiler->getCompiledTemplatePath();

$compileData = buildData($rowSets[0]);
$templix->render('market/quotes', $compileData);

$templixCompiledPath = $appDir . '/cache/templix/market_quotes.blade.php.php';
if (!is_file($templixCompiledPath)) {
    throw new RuntimeException('Templix compiled cache not found.');
}
if (!is_file($voltCompiledPath)) {
    throw new RuntimeException('Volt compiled cache not found.');
}

if (function_exists('opcache_compile_file')) {
    @opcache_compile_file($templixCompiledPath);
    @opcache_compile_file($voltCompiledPath);
}

$scenarios = [];
foreach ($rowSets as $rows) {
    $data = buildData($rows);
    $templixSample = $templix->render('market/quotes', $data);
    $voltSample = renderCompiled($voltCompiledPath, $data);
    $expectedHash = hash('sha256', normalizeHtml($templixSample));
    assertSameHash("phalcon_volt_compiled rows={$rows}", $expectedHash, $voltSample);

    $cases = [
        'templix_prod_standard' => fn (): string => $templix->render('market/quotes', $data),
        'templix_prod_extreme' => fn (): string => renderCompiled($templixCompiledPath, $data),
        'phalcon_volt_compiled' => fn (): string => renderCompiled($voltCompiledPath, $data),
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
        $seconds = $elapsedNs / 1_000_000_000;
        $peakAfter = memory_get_peak_usage(true);

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
        'results' => $results,
    ];
}

$report = [
    'generated_at' => gmdate('c'),
    'environment' => [
        'php_version' => PHP_VERSION,
        'memory_limit' => ini_get('memory_limit'),
        'templix_loaded' => extension_loaded('templix'),
        'phalcon_loaded' => extension_loaded('phalcon'),
        'phalcon_version' => phpversion('phalcon'),
        'opcache_cli_enabled' => (bool) ini_get('opcache.enable_cli'),
        'opcache_validate_timestamps' => ini_get('opcache.validate_timestamps'),
    ],
    'dataset' => [
        'type' => 'synthetic_equity_quotes',
        'row_sets' => $rowSets,
        'fields' => ['symbol', 'name', 'price', 'change', 'volume', 'risk'],
    ],
    'cache' => [
        'templix_compiled_path' => $templixCompiledPath,
        'templix_compiled_bytes' => filesize($templixCompiledPath),
        'phalcon_volt_compiled_path' => $voltCompiledPath,
        'phalcon_volt_compiled_bytes' => filesize($voltCompiledPath),
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
    $lines[] = '# Templix vs Phalcon Volt Report';
    $lines[] = '';
    $lines[] = '- Generated at: `' . $report['generated_at'] . '`';
    $lines[] = '- PHP: `' . $report['environment']['php_version'] . '`';
    $lines[] = '- memory_limit: `' . $report['environment']['memory_limit'] . '`';
    $lines[] = '- Templix loaded: `' . ($report['environment']['templix_loaded'] ? 'true' : 'false') . '`';
    $lines[] = '- Phalcon loaded: `' . ($report['environment']['phalcon_loaded'] ? 'true' : 'false') . '`';
    $lines[] = '- Phalcon version: `' . ($report['environment']['phalcon_version'] ?: 'n/a') . '`';
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
    $lines[] = '## Notes';
    $lines[] = '';
    $lines[] = '- `templix_prod_standard`: `Templix\\Engine::render()` in prod mode.';
    $lines[] = '- `templix_prod_extreme`: prewarmed Templix compiled PHP cache directly included.';
    $lines[] = '- `phalcon_volt_compiled`: Phalcon Volt compiled PHP cache directly included.';
    $lines[] = '- Templix and Volt use their native template syntax, with equivalent HTML output and SHA-256 normalized output checks.';
    $lines[] = '';

    return implode(PHP_EOL, $lines) . PHP_EOL;
}
PHP
}

ensure_phalcon
build_templix
prepare_app

log "Running comparison row_sets=$ROW_SETS iterations=$ITERATIONS warmups=$WARMUPS"
php_with_extensions "$APP_DIR/scripts/run-compare.php" \
  --app-dir="$APP_DIR" \
  --row-sets="$ROW_SETS" \
  --iterations="$ITERATIONS" \
  --warmups="$WARMUPS" \
  --report-json="$REPORT_JSON" \
  --report-md="$REPORT_MD"

log "Report written to $REPORT_MD"
log "JSON written to $REPORT_JSON"
log "Work dir: $APP_DIR"
