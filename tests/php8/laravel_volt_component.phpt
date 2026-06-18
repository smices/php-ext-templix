--TEST--
Templix renders a Laravel Volt-style single-file component template
--SKIPIF--
<?php if (!extension_loaded('templix')) die('skip templix extension not loaded'); ?>
--FILE--
<?php
$root = sys_get_temp_dir() . '/templix_laravel_volt_' . getmypid();
$views = $root . '/views/pages/post';
$cache = $root . '/cache';
mkdir($views, 0777, true);
mkdir($cache, 0777, true);

$template = <<<'BLADE'
<?php
new class {
    public string $title = '';
};
?>
<form wire:submit="save">
@if ($post->published)
  <h1>{{ $post->title }}</h1>
@else
  <h1>Draft</h1>
@endif
@foreach ($tags as $tag)
  <span wire:key="tag-{{ $tag }}">{{ $tag }}</span>
@endforeach
</form>
BLADE;

file_put_contents($views . '/create.blade.php', $template);

$engine = new Templix\Engine(['mode' => 'prod', 'path' => $root . '/views', 'cache' => $cache]);
$post = (object) ['published' => true, 'title' => '<Volt Ready>'];
echo preg_replace('/\s+/', ' ', trim($engine->render('pages/post/create', [
    'post' => $post,
    'tags' => ['php', 'laravel'],
])));
?>
--EXPECT--
<form wire:submit="save"> <h1>&lt;Volt Ready&gt;</h1> <span wire:key="tag-php">php</span> <span wire:key="tag-laravel">laravel</span> </form>
