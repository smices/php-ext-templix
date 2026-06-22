--TEST--
Templix compiles Blade PHP, comments, verbatim, and JSON directives
--SKIPIF--
<?php if (!extension_loaded('templix')) die('skip templix extension not loaded'); ?>
--FILE--
<?php
$root = sys_get_temp_dir() . '/templix_blade_php_json_' . getmypid();
$views = $root . '/views';
$cache = $root . '/cache';
mkdir($views, 0777, true);
mkdir($cache, 0777, true);
file_put_contents($views . '/misc.blade.php', <<<'BLADE'
{{-- hidden comment --}}
@php
$sum = $a + $b;
@endphp
{{ $sum }}
@verbatim{{ client.side }}@endverbatim
<script>const payload = @json($payload);</script>
BLADE);

$engine = new Templix\Engine(['mode' => 'dev', 'path' => $views, 'cache' => $cache]);
echo $engine->render('misc', [
    'a' => 2,
    'b' => 3,
    'payload' => ['tag' => '</script>', 'quote' => '"'],
]);
?>
--EXPECT--

5
{{ client.side }}
<script>const payload = {"tag":"\u003C\/script\u003E","quote":"\u0022"};</script>
