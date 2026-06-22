--TEST--
Templix extended Blade directives match Laravel Blade output
--SKIPIF--
<?php
if (!extension_loaded('templix')) die('skip templix extension not loaded');
$app = getenv('TEMPLIX_LARAVEL_APP') ?: '/private/tmp/templix-laravel-prod-test.QUFPaf';
if (!is_file($app . '/artisan') || !is_file($app . '/vendor/autoload.php')) {
    die('skip Laravel app not available; set TEMPLIX_LARAVEL_APP');
}
?>
--FILE--
<?php
$appDir = getenv('TEMPLIX_LARAVEL_APP') ?: '/private/tmp/templix-laravel-prod-test.QUFPaf';

require $appDir . '/vendor/autoload.php';
$app = require $appDir . '/bootstrap/app.php';
$app->make(Illuminate\Contracts\Console\Kernel::class)->bootstrap();

$root = sys_get_temp_dir() . '/templix_laravel_blade_parity_' . getmypid();
$views = $root . '/views';
$cache = $root . '/cache';
mkdir($views, 0777, true);
mkdir($cache, 0777, true);

$cases = [
    'control' => [
        <<<'BLADE'
@if ($risk >= 80)HIGH
@elseif ($risk >= 50)MID
@else
LOW
@endif
@unless ($hidden)VISIBLE
@endunless
@isset($user)<b>{{ $user['name'] }}</b>@endisset
@empty($alerts)NO ALERTS
@endempty
@for ($i = 0; $i < 3; $i++){{ $i }}
@endfor
@while ($count < 2){{ $count }}
@php $count++; @endphp
@endwhile
@forelse ($items as $item){{ $item }}
@empty
EMPTY
@endforelse
@switch($market)
@case('HK')HK
@break
@case('US')US
@break
@default
OTHER
@endswitch
@foreach ($numbers as $number)
@continue($number === 2){{ $number }}
@break($number === 3)
@endforeach
BLADE,
        [
            'risk' => 55,
            'hidden' => false,
            'user' => ['name' => '<Ada>'],
            'alerts' => [],
            'count' => 0,
            'items' => [],
            'market' => 'US',
            'numbers' => [1, 2, 3, 4],
        ],
    ],
    'attributes' => [
        <<<'BLADE'
<tr @class(['row', 'up' => $up, 'down' => !$up]) @style(['color: green' => $up, 'display: none' => false])>
<option @selected($selected)>{{ $label }}</option>
<input @checked($checked) @disabled($disabled) @readonly($readonly) @required($required)>
BLADE,
        [
            'up' => true,
            'selected' => true,
            'checked' => true,
            'disabled' => false,
            'readonly' => true,
            'required' => true,
            'label' => '<Buy>',
        ],
    ],
    'helpers' => [
        <<<'BLADE'
{{-- hidden comment --}}
@php
$sum = $a + $b;
@endphp
{{ $sum }}
@verbatim{{ client.side }}@endverbatim
<script>const payload = @json($payload);</script>
BLADE,
        [
            'a' => 2,
            'b' => 3,
            'payload' => ['tag' => '</script>', 'quote' => '"'],
        ],
    ],
];

foreach ($cases as $name => [$source, $data]) {
    $blade = renderLaravelBlade($app, $source, $data, $cache);
    $templix = renderTemplix($name, $source, $data, $views, $cache);
    $bladeComparable = $name === 'control' ? normalizeControlOutput($blade) : $blade;
    $templixComparable = $name === 'control' ? normalizeControlOutput($templix) : $templix;

    if ($bladeComparable !== $templixComparable) {
        echo "Mismatch: {$name}\n";
        echo "--- Laravel ---\n{$blade}\n";
        echo "--- Templix ---\n{$templix}\n";
        exit(1);
    }

    echo "OK {$name}\n";
}

function renderLaravelBlade($app, string $source, array $data, string $cache): string
{
    $compiledPath = $cache . '/laravel_' . md5($source) . '.php';
    file_put_contents($compiledPath, $app->make('blade.compiler')->compileString($source));

    extract($data, EXTR_SKIP);
    $__env = $app->make('view');

    ob_start();
    include $compiledPath;
    return ob_get_clean();
}

function renderTemplix(string $name, string $source, array $data, string $views, string $cache): string
{
    file_put_contents($views . '/' . $name . '.blade.php', $source);

    $engine = new Templix\Engine([
        'mode' => 'dev',
        'path' => $views,
        'cache' => $cache . '/templix',
    ]);

    if (!is_dir($cache . '/templix')) {
        mkdir($cache . '/templix', 0777, true);
    }
    return $engine->render($name, $data);
}

function normalizeControlOutput(string $output): string
{
    return preg_replace('/\s+/', '', $output);
}
?>
--EXPECT--
OK control
OK attributes
OK helpers
