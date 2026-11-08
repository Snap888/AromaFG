# Анализ Snap888/Snap_AI_Engine — ветка SnapFrameBackendV1

**Проверенная ревизия:** `1b09a28caa85ced25b73c47dc6cc2dd6f59be944` (7 ноября 2026).  
**Источник:** [Snap_AI_Engine / SnapFrameBackendV1](https://github.com/Snap888/Snap_AI_Engine/tree/SnapFrameBackendV1).

## Что это

Репозиторий объединяет portable SnapFrame lifecycle/API, Vulkan host interop, compute frame-generation backend, адаптеры хостов и отдельный детерминированный Snap AI policy controller. Это не единый универсальный compositor: X11, Wayland и DXVK integration paths различаются и должны проверяться отдельно.

## Место в стеке

| Компонент | Роль | Значение для AromaFG |
|---|---|---|
| Portable C ABI + backend callbacks | Управление жизненным циклом и вызов backend | Высокое |
| Vulkan interop | Borrowed host resources + command recording | Критически высокое |
| Flow / consistency / warp shaders | FG compute pipeline | Высокое, только как алгоритмический ориентир |
| Snap AI policy | Детерминированное управление workload | Высокое |
| Frame telemetry | Passive present accounting | Высокое |
| X11 / Wayland / DXVK adapters | Host-specific integration | Среднее; не переносить напрямую |
| Legacy DXVK host | Привязка к частному форку DXVK | Низкое как переносимый код |

## Релевантность

| Аспект | Оценка |
|---|---|
| Прямой Frame Generation | Да |
| Optical flow / motion / warping | Да |
| Adreno + Turnip | Высокая архитектурная релевантность; физическая валидация всё равно обязательна |
| Нужна ли зависимость | Нет |
| Что использовать | ABI patterns, ownership contract, flow reuse, confidence/consistency semantics, bounded telemetry, validation discipline |

## Лучшие идеи для AromaFG

1. **Portable core отделён от backend.** C ABI управляет frame ordering, config, lifecycle, bypass и ошибками; GPU backend подключается через versioned callbacks.
2. **Vulkan host ownership.** Host передаёт device/queue/command buffer/source/output slots. Backend записывает команды, но не submit’ит host command buffer, не ждёт GPU и не владеет WSI.
3. **Flow reuse.** Один forward/backward solve на source pair; multiplier добавляет только отдельные temporal synthesis phases. Для ×3/×4 запрещено повторять один midpoint.
4. **Чёткая семантика событий.** Compute completion, acquire success, successful present и scanout — разные события. Только успешный host present учитывается как delivered.
5. **Bounded async telemetry.** GPU reductions в малый staging ring, CPU читает только после завершения query; никакого full-frame readback или blocking render path.
6. **Deterministic adaptive policy.** Сглаживание сигналов, hysteresis, ограничение workload, safe bypass при invalid telemetry/scene cut. Решение применяется к следующему безопасному frame.
7. **Candidate reconstruction.** Несколько геометрически допустимых source candidates, score по confidence и photometric disagreement, bounded softmax; если все ненадёжны — резкий real-frame fallback.
8. **Validation as product feature.** Ground-truth midpoint, edge/occlusion/temporal metrics, p50/p95 timings, A/B parity и отдельные host/device acceptance gates.

## Что не переносить

- Не копировать исходники SnapFrame, DXVK legacy host или shaders. В AromaFG реализовать собственные clean-room варианты по общим принципам.
- Не переносить Snap AI как «нейросеть»: в источнике controller детерминированный, признаки — proxy, а не ground-truth artifact probability.
- Не переносить host-specific X11/Wayland timeouts как универсальные константы.
- Не считать DXVK adapter универсальным: в источнике он зависит от конкретного DXVK fork/private classes.
- Не считать Winlator runtime интегрированным: документ источника прямо указывает, что runtime adapter отсутствует.
- Не считать CI/portable tests доказательством физической работы на Turnip/Adreno.
- Не использовать неизвестные FP16 constants как model weights; provenance и контракт не установлены.
- Не утверждать HDR support по внутренним FP16 intermediate images.

## Итог

| Направление | Код | Алгоритм/идея | Зависимость | Влияние |
|---|---|---|---|---|
| Versioned portable C ABI | Новая реализация | Да | Нет | Критическое |
| Borrowed Vulkan interop | Новая реализация | Да | Нет | Критическое |
| Flow reuse across phases | Новая реализация | Да | Нет | Критическое |
| Confidence + FB consistency | Новые шейдеры | Да | Нет | Высокое |
| Candidate scoring/fallback | Новая реализация после baseline | Да | Нет | Высокое |
| Async GPU reductions | Новая реализация | Да | Нет | Высокое |
| Passive present telemetry | Новая реализация | Да | Нет | Высокое |
| Snap AI controller | AromaFG policy с собственным ABI | Да | Нет | Высокое |
| Host adapters | Отдельно для каждого host | Архитектурные идеи | Нет | Среднее |
| Legacy DXVK host | Нет | Только boundary lessons | Нет | Низкое |

**Вывод:** обновить ROADMAP, architecture, integration, Engineer Mode и testing. Анализ относится к указанной ревизии; статус upstream может измениться.