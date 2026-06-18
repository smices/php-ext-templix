<?php
/*
 * php-ext-templix
 *
 * Copyright (c) 2026 MISSU.LINK
 * SPDX-License-Identifier: MIT
 */

declare(strict_types=1);

if (!extension_loaded('templix')) {
    fwrite(STDERR, "The templix extension must be loaded with -d extension=modules/templix.so\n");
    exit(1);
}

$iterations = 5000;
$json = false;

foreach ($argv as $arg) {
    if (str_starts_with($arg, '--iterations=')) {
        $iterations = max(1, (int) substr($arg, strlen('--iterations=')));
    }
    if ($arg === '--json') {
        $json = true;
    }
}

$root = sys_get_temp_dir() . '/templix_bench_' . getmypid();
$views = $root . '/views';
$cache = $root . '/cache';
mkdir($views, 0777, true);
mkdir($cache, 0777, true);

$template = <<<'BLADE'
<article wire:key="post-{{ $post->id }}">
@if ($post->published)
  <h1>{{ $post->title }}</h1>
@else
  <h1>Draft</h1>
@endif
@foreach ($tags as $tag)
  <span wire:key="tag-{{ $tag }}">{{ $tag }}</span>
@endforeach
</article>
BLADE;

$simpleTemplate = 'Hello {{ $name }} {!! $html !!}';

file_put_contents($views . '/volt-card.blade.php', $template);
file_put_contents($views . '/simple.blade.php', $simpleTemplate);

$post = (object) ['id' => 42, 'published' => true, 'title' => '<Volt Fast Path>'];
$data = ['post' => $post, 'tags' => ['php', 'laravel', 'volt']];
$simpleData = ['name' => '<Ada>', 'html' => '<strong>OK</strong>'];

$templix = new Templix\Engine(['mode' => 'prod', 'path' => $views, 'cache' => $cache]);
$expected = normalize($templix->render('volt-card', $data));
$simpleExpected = $templix->render('simple', $simpleData);

$laravelCompiled = $cache . '/laravel_compiled.php';
$voltCompiled = $cache . '/volt_compiled.php';

file_put_contents($laravelCompiled, <<<'PHP'
<?php
extract($__data, EXTR_SKIP);
?>
<article wire:key="post-<?= htmlspecialchars((string) $post->id, ENT_QUOTES | ENT_SUBSTITUTE, 'UTF-8') ?>">
<?php if ($post->published): ?>
  <h1><?= htmlspecialchars((string) $post->title, ENT_QUOTES | ENT_SUBSTITUTE, 'UTF-8') ?></h1>
<?php else: ?>
  <h1>Draft</h1>
<?php endif; ?>
<?php foreach ($tags as $tag): ?>
  <span wire:key="tag-<?= htmlspecialchars((string) $tag, ENT_QUOTES | ENT_SUBSTITUTE, 'UTF-8') ?>"><?= htmlspecialchars((string) $tag, ENT_QUOTES | ENT_SUBSTITUTE, 'UTF-8') ?></span>
<?php endforeach; ?>
</article>
PHP);

file_put_contents($voltCompiled, <<<'PHP'
<?php
extract($__data, EXTR_SKIP);
?>
<article wire:key="post-<?= htmlspecialchars((string) $post->id, ENT_QUOTES | ENT_SUBSTITUTE, 'UTF-8') ?>">
<?php if ($post->published): ?>
  <h1><?= htmlspecialchars((string) $post->title, ENT_QUOTES | ENT_SUBSTITUTE, 'UTF-8') ?></h1>
<?php endif; ?>
<?php foreach ($tags as $tag): ?>
  <span wire:key="tag-<?= htmlspecialchars((string) $tag, ENT_QUOTES | ENT_SUBSTITUTE, 'UTF-8') ?>"><?= htmlspecialchars((string) $tag, ENT_QUOTES | ENT_SUBSTITUTE, 'UTF-8') ?></span>
<?php endforeach; ?>
</article>
PHP);

$cases = [
    'templix_prod_volt_component' => function () use ($templix, $data): string {
        return $templix->render('volt-card', $data);
    },
    'laravel_blade_compiled_include' => function () use ($laravelCompiled, $data): string {
        return include_render($laravelCompiled, $data);
    },
    'volt_compiled_include' => function () use ($voltCompiled, $data): string {
        return include_render($voltCompiled, $data);
    },
    'user_render_basic_templix' => function () use ($templix, $simpleData): string {
        return $templix->render('simple', $simpleData);
    },
];

$results = [];

foreach ($cases as $name => $case) {
    $sample = $case();
    if ($name === 'user_render_basic_templix') {
        assert_same($simpleExpected, $sample, $name);
    } else {
        assert_same($expected, normalize($sample), $name);
    }

    $start = hrtime(true);
    for ($i = 0; $i < $iterations; $i++) {
        $case();
    }
    $elapsedNs = hrtime(true) - $start;
    $seconds = $elapsedNs / 1_000_000_000;
    $results[$name] = [
        'iterations' => $iterations,
        'seconds' => $seconds,
        'ops_per_second' => $iterations / max($seconds, 0.000001),
        'microseconds_per_render' => ($elapsedNs / 1000) / $iterations,
    ];
}

uasort($results, static fn (array $a, array $b): int => $a['microseconds_per_render'] <=> $b['microseconds_per_render']);

if ($json) {
    echo json_encode($results, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES) . "\n";
    exit(0);
}

printf("%-34s %12s %14s %12s\n", 'case', 'iterations', 'ops/sec', 'us/render');
foreach ($results as $name => $row) {
    printf(
        "%-34s %12d %14.2f %12.3f\n",
        $name,
        $row['iterations'],
        $row['ops_per_second'],
        $row['microseconds_per_render']
    );
}

function include_render(string $compiled, array $data): string
{
    $__data = $data;
    ob_start();
    include $compiled;

    return ob_get_clean();
}

function normalize(string $html): string
{
    return preg_replace('/\s+/', ' ', trim($html));
}

function assert_same(string $expected, string $actual, string $case): void
{
    if ($expected !== $actual) {
        fwrite(STDERR, "Benchmark case output mismatch: {$case}\n");
        fwrite(STDERR, "Expected: {$expected}\nActual: {$actual}\n");
        exit(2);
    }
}
