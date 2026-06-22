# Templix 兼容性报告

生成日期：2026-06-18

## 目标

Templix 目标是加速 Laravel Volt / Blade 风格模板在生产环境中的渲染，重点覆盖金融行情、实时风控、交易运营、报表生成等一次渲染大量数组数据的场景。

Templix 模板默认使用 `.blade.php`，让现有 Laravel / Volt 风格模板在不改基本语法的前提下进入 Templix 编译缓存和 OPcache 热路径。

## 运行环境

已验证环境：

| 项目 | 结果 |
|---|---|
| PHP | 8.5.7 |
| Laravel | 13.16.1 |
| livewire/volt | v1.10.5 |
| OPcache CLI | enabled |
| APP_ENV | production |
| APP_DEBUG | false |
| Templix extension | loaded |

扩展目标是 PHP 8.x。不同 PHP 小版本需要按本机 `phpize` / `php-config` 重新编译。

## 语法兼容

当前已支持：

| 能力 | 状态 | 说明 |
|---|---|---|
| `.blade.php` 模板文件 | 支持 | 默认扩展名，可配置 |
| `{{ $value }}` | 支持 | 自动 HTML 转义 |
| `{!! $html !!}` | 支持 | 原样输出 |
| `@if` / `@elseif` / `@else` / `@endif` | 支持 | 编译为 PHP 控制流 |
| `@foreach` / `@endforeach` | 支持 | 编译为 PHP 循环 |
| `@for` / `@endfor` | 支持 | 编译为 PHP 循环 |
| `@while` / `@endwhile` | 支持 | 编译为 PHP 循环 |
| `@forelse` / `@empty` / `@endforelse` | 支持 | 编译为空集合感知循环 |
| `@switch` / `@case` / `@default` / `@endswitch` | 支持 | 编译为 PHP switch |
| `@break` / `@continue` | 支持 | 支持可选条件 |
| `@unless` / `@endunless` | 支持 | 编译为取反条件 |
| `@isset` / `@endisset` | 支持 | 编译为 PHP 条件 |
| `@empty` / `@endempty` | 支持 | 编译为 PHP 条件 |
| `@class` / `@style` | 支持 | 由扩展内置 helper 生成属性 |
| `@checked` / `@selected` / `@disabled` / `@readonly` / `@required` | 支持 | 编译为布尔 HTML 属性 |
| `@php` / `@endphp` | 支持 | 编译为 PHP block |
| `@verbatim` / `@endverbatim` | 支持 | 保留内部模板文本 |
| `@json` | 支持 | 使用 PHP `json_encode` 和 HEX 安全选项 |
| Blade 注释 `{{-- --}}` | 支持 | 编译时移除 |
| 原生 `<?php ... ?>` block | 支持 | 适合 Volt 风格文件头状态定义 |
| HTML / `wire:*` 属性 | 支持 | 作为普通模板文本保留 |
| `number_format()` 默认分隔符输出 | 支持并优化 | 安全跳过多余转义 |

## Laravel 兼容

Templix 通过 Laravel view engine 适配层接入：

```text
resources/views/*.blade.php
  -> Laravel View Engine
  -> Templix\Engine
  -> storage/framework/views/templix/*.php
  -> OPcache
```

已验证：

- `.blade.php` 模板可由 Templix 渲染。
- `Templix\Engine::render()` 可接收 Laravel view data 数组。
- prod 模式会优先使用已编译缓存。
- 编译缓存为 PHP 文件，可被 OPcache 使用。
- Laravel 生产配置下可以运行真实基准脚本。

Laravel 接入方式见 [user-manual.zh-CN.md](user-manual.zh-CN.md) 和 [laravel-template-engine.md](laravel-template-engine.md)。

## Volt 兼容

已验证 Volt 风格模板能力：

- 单文件组件中的原生 PHP block 可保留并执行。
- Volt / Livewire 常见 HTML 属性不会被 Templix 改写。
- 生产测试安装并检查 `livewire/volt`。
- 大数组股票行情模板在真实 Laravel + Volt 依赖环境中完成渲染和结果一致性校验。

Templix 的重点是加速 Volt / Blade 风格模板的生成路径，不接管 Livewire 的前端交互协议。

## 模式兼容

| 模式 | 说明 |
|---|---|
| `dev` | 渲染前编译模板，适合开发期 |
| `prod` | 已存在缓存时直接使用缓存，适合生产 |
| prod 标准模式 | Laravel view engine 调用 `Templix\Engine::render()` |
| prod 极致模式 | 预热后直接 include Templix 编译缓存，用于测量模板主体极限 |

## 测试覆盖

PHPT 覆盖：

| 测试文件 | 覆盖点 |
|---|---|
| `render_basic.phpt` | 基本渲染、自动转义、原样输出 |
| `escape_function.phpt` | C 转义函数与模板输出转义 |
| `blade_control_directives.phpt` | `@if` / `@foreach` |
| `blade_extended_control_directives.phpt` | `@unless` / `@for` / `@while` / `@forelse` / `@switch` / `@break` / `@continue` |
| `laravel_blade_condition_directives.phpt` | `@isset` / `@empty` |
| `blade_attribute_directives.phpt` | `@class` / `@style` / 布尔属性指令 |
| `blade_php_json_verbatim_directives.phpt` | `@php` / `@verbatim` / `@json` / Blade 注释 |
| `laravel_blade_directive_parity.phpt` | 真实 Laravel Blade 对照：控制流、属性指令、模板辅助 |
| `native_php.phpt` | 原生 PHP block |
| `laravel_volt_component.phpt` | Volt 风格单文件组件 |
| `volt_prod_cache_fast_path.phpt` | prod 缓存优先路径 |
| `number_format_escape_optimization.phpt` | `number_format()` 安全优化边界 |
| `benchmark_laravel_volt_smoke.phpt` | 基准结构 smoke |
| `engine_construct.phpt` | `Templix\Engine` 构造 |

执行：

```bash
make test TESTS=tests/php8
```

当前结果：16/16 PASS。

## 生产性能验证

真实生产脚本：

```bash
ROW_SETS=100,5000,50000 ITERATIONS=5000 MEMORY_LIMIT=512M scripts/production-laravel-volt-test.sh
```

最新报告文件：

```text
reports/production-laravel-volt-test-report.md
reports/production-laravel-volt-test-report.json
```

当前已记录结果：

| Array rows/render | Templix 最快 | Laravel Blade compiled | 提升 |
|---:|---:|---:|---:|
| 100 | 0.083 ms/render | 0.172 ms/render | 约 2.08x |
| 5000 | 3.170 ms/render | 7.445 ms/render | 约 2.35x |
| 50000 | 32.651 ms/render | 75.979 ms/render | 约 2.33x |

## 已知边界

- 当前覆盖的是 Blade / Volt 常用生成语法，不是完整 Blade 编译器替代品。
- 未实现 Blade 组件、slot、layout、include、stack、auth、csrf 等高级指令。
- 自定义 helper 的返回值默认仍按 `{{ }}` 规则转义。
- `prod` 模式建议在部署时预热缓存，避免首个请求承担编译成本。
- 极大输出会受 PHP 输出缓冲和最终 HTML 字符串大小影响。
