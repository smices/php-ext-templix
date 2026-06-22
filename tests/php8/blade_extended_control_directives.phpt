--TEST--
Templix compiles extended Blade control directives
--SKIPIF--
<?php if (!extension_loaded('templix')) die('skip templix extension not loaded'); ?>
--FILE--
<?php
$root = sys_get_temp_dir() . '/templix_blade_extended_control_' . getmypid();
$views = $root . '/views';
$cache = $root . '/cache';
mkdir($views, 0777, true);
mkdir($cache, 0777, true);
file_put_contents($views . '/control.blade.php', <<<'BLADE'
@unless ($hidden)VISIBLE@endunless
@for ($i = 0; $i < 3; $i++){{ $i }}@endfor
@while ($count < 2){{ $count }}<?php $count++; ?>@endwhile
@forelse ($items as $item){{ $item }}@empty EMPTY@endforelse
@switch($risk)@case('high')HIGH@break@case('mid')MID@break@defaultLOW@endswitch
@foreach ($numbers as $number)@continue($number === 2){{ $number }}@break($number === 3)@endforeach
BLADE);

$engine = new Templix\Engine(['mode' => 'dev', 'path' => $views, 'cache' => $cache]);
echo $engine->render('control', [
    'hidden' => false,
    'count' => 0,
    'items' => [],
    'risk' => 'mid',
    'numbers' => [1, 2, 3, 4],
]);
?>
--EXPECT--
VISIBLE
012
01
 EMPTY
MID
13
