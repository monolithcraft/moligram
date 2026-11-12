<p align="center">
  <img src="resources/branding/logo.png" width="160" alt="Moligram">
</p>

<h1 align="center">Moligram</h1>

<p align="center">Десктопный клиент Telegram от студии Monolith.</p>

Moligram построен на TDLib и Qt Quick. Его главные приоритеты — приватность,
производительность и собственный интерфейс.

Проект на ранней стадии: сейчас готовы каркас приложения и дизайн-система.

## Возможности

Планируются:

- Ghost Mode — не отправлять прочтение, статус «в сети» и «печатает»
- Message History — локальная история удалённых и изменённых сообщений
- Streamer Mode — скрытие личных данных во время трансляции
- Командная палитра и полное управление с клавиатуры

Полный список идей — в [docs/research/ideas.md](docs/research/ideas.md).

## Сборка

Нужны CMake 3.28+, Ninja, компилятор с поддержкой C++23 и Qt 6.8+.

```bash
git clone --recurse-submodules https://github.com/monolithcraft/moligram.git
```

```bash
./scripts/build-tdlib.sh
```

```bash
cmake --preset release
```

```bash
cmake --build --preset release
```

Подробности и список пакетов — в [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md#сборка).

## Документация

- [Принципы разработки](docs/PRINCIPLES.md)
- [Архитектура](docs/ARCHITECTURE.md)
- [Дизайн](docs/DESIGN.md)
- [Исследования](docs/research/README.md)

## Лицензия

Moligram распространяется под лицензией [GNU GPL 3.0 или более поздней версии](LICENSE)
(`GPL-3.0-or-later`).
